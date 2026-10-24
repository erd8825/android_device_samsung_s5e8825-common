/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "GNSS_MEMS"

#include "GnssSensors.h"

#include <android/looper.h>
#include <android/sensor.h>
#include <dlfcn.h>
#include <log/log.h>
#include <time.h>
#include <unistd.h>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>
#include "GnssBackend.h"
#include "GnssDiagnostics.h"
#include "GnssProtocol.h"

namespace aidl::android::hardware::gnss {
namespace {

struct Sample {
    std::array<int16_t, 6> values{};
    uint64_t timeMs;
};

struct Sensor {
    int type;
    int mask;
    ASensor const* sensor = nullptr;
    ASensorEventQueue* queue = nullptr;
    bool enabled = false;
    uint64_t lastTimeMs = 0;
    uint32_t spam = 0;
    uint32_t rateLimited = 0;
    uint32_t outOfRange = 0;
    uint32_t total = 0;
    std::vector<Sample> samples{};
    std::function<void()> onReady{};
};

int sensorReady(int, int, void* data) {
    static_cast<Sensor*>(data)->onReady();
    return 1;
}

uint64_t timeMs() {
    timespec now{};
    clock_gettime(CLOCK_BOOTTIME, &now);
    return now.tv_sec * 1000ULL + now.tv_nsec / 1000000;
}

int16_t sampleValue(float value, bool pressure) {
    if (std::isnan(value)) return 0;
    if (pressure) {
        const int32_t converted = value >= 0x1p31f    ? std::numeric_limits<int32_t>::max()
                                  : value <= -0x1p31f ? std::numeric_limits<int32_t>::min()
                                                      : static_cast<int32_t>(value);
        return static_cast<int16_t>(converted);
    }
    value = std::rint(value);
    const int64_t converted = value >= 0x1p63f    ? std::numeric_limits<int64_t>::max()
                              : value <= -0x1p63f ? std::numeric_limits<int64_t>::min()
                                                  : static_cast<int64_t>(value);
    return static_cast<int16_t>(converted);
}

}  // namespace

GnssSensors::~GnssSensors() {
    std::lock_guard lifecycle(mLifecycleMutex);
    mActive = false;
    mRunning = false;
    {
        std::lock_guard lock(mMutex);
        if (mWake) mWake();
    }
    mCondition.notify_all();
    if (mThread.joinable()) mThread.join();
}

void GnssSensors::init() {
    std::lock_guard lifecycle(mLifecycleMutex);
    std::unique_lock lock(mMutex);
    if (mInitialized || mRunning) return;
    if (mThread.joinable()) mThread.join();
    mRunning = true;
    mInitializing = true;
    mThread = std::thread(&GnssSensors::run, this);
    mCondition.wait(lock, [this] { return !mInitializing; });
    GnssDiagnostics::get().logText(7, "TGT_MEMS %s: End: tid %d", "TargetMemsInit", gettid());
}

void GnssSensors::start() {
    std::lock_guard lifecycle(mLifecycleMutex);
    const auto now = timeMs();
    mLastSendMs = mLastReportMs = now;
    if (!mRunning) return;
    mActive = true;
    mCondition.notify_all();
    GnssDiagnostics::get().logText(7, "TGT_MEMS %s: Entry", "TargetMemsStart");
}

void GnssSensors::stop() {
    std::lock_guard lifecycle(mLifecycleMutex);
    mActive = false;
    std::unique_lock lock(mMutex);
    if (mWake) mWake();
    if (!mCondition.wait_for(lock, std::chrono::milliseconds(500), [this] { return !mEnabled; })) {
        ALOGI("TGT_MEMS TargetMemsStop: timeout waiting for ALooper_wake");
        GnssDiagnostics::get().logText(7, "TGT_MEMS %s: timeout waiting for ALooper_wake",
                                       "TargetMemsStop");
    }
}

void GnssSensors::disableConfig() {
    stop();
    start();
}

void GnssSensors::run() {
    auto finish = [this] {
        std::lock_guard lock(mMutex);
        mWake = nullptr;
        mEnabled = false;
        mInitializing = false;
        mRunning = false;
        mCondition.notify_all();
    };
    if (!mLibrary) mLibrary = dlopen("/vendor/lib64/libsensorndkbridge.so", RTLD_NOW | RTLD_LOCAL);
    if (!mLibrary) {
        ALOGW("Cannot load sensor bridge: %s", dlerror());
        GnssDiagnostics::get().logText(7, "Loading Samsung MEMS Lib (%s) failed!",
                                       "/vendor/lib64/libsensorndkbridge.so");
        finish();
        return;
    }
    auto getManager = reinterpret_cast<decltype(&ASensorManager_getInstanceForPackage)>(
            dlsym(mLibrary, "ASensorManager_getInstanceForPackage"));
    auto getLegacyManager =
            reinterpret_cast<ASensorManager* (*)()>(dlsym(mLibrary, "ASensorManager_getInstance"));
    auto getSensor = reinterpret_cast<decltype(&ASensorManager_getDefaultSensor)>(
            dlsym(mLibrary, "ASensorManager_getDefaultSensor"));
    auto prepare = reinterpret_cast<decltype(&ALooper_prepare)>(dlsym(mLibrary, "ALooper_prepare"));
    auto forThread =
            reinterpret_cast<decltype(&ALooper_forThread)>(dlsym(mLibrary, "ALooper_forThread"));
    auto createQueue = reinterpret_cast<decltype(&ASensorManager_createEventQueue)>(
            dlsym(mLibrary, "ASensorManager_createEventQueue"));
    auto destroyQueue = reinterpret_cast<decltype(&ASensorManager_destroyEventQueue)>(
            dlsym(mLibrary, "ASensorManager_destroyEventQueue"));
    auto enable = reinterpret_cast<decltype(&ASensorEventQueue_enableSensor)>(
            dlsym(mLibrary, "ASensorEventQueue_enableSensor"));
    auto disable = reinterpret_cast<decltype(&ASensorEventQueue_disableSensor)>(
            dlsym(mLibrary, "ASensorEventQueue_disableSensor"));
    auto setRate = reinterpret_cast<decltype(&ASensorEventQueue_setEventRate)>(
            dlsym(mLibrary, "ASensorEventQueue_setEventRate"));
    auto pollOnce =
            reinterpret_cast<decltype(&ALooper_pollOnce)>(dlsym(mLibrary, "ALooper_pollOnce"));
    auto wake = reinterpret_cast<decltype(&ALooper_wake)>(dlsym(mLibrary, "ALooper_wake"));
    auto getEvents = reinterpret_cast<decltype(&ASensorEventQueue_getEvents)>(
            dlsym(mLibrary, "ASensorEventQueue_getEvents"));
    if ((!getManager && !getLegacyManager) || !getSensor || !prepare || !forThread ||
        !createQueue || !enable || !disable || !setRate || !pollOnce || !wake || !getEvents) {
        ALOGW("Sensor bridge is missing required entry points");
        finish();
        return;
    }
    const int mask = readConfigInt("Chip_Configuration_mems_data_Configuration", 1);
    if (!(mask & 13)) {
        finish();
        return;
    }
    auto* manager = getManager ? getManager(nullptr) : getLegacyManager();
    auto* looper = manager ? forThread() : nullptr;
    if (manager && !looper) looper = prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    {
        std::lock_guard lock(mMutex);
        if (looper) mWake = [wake, looper] { wake(looper); };
    }
    std::array<Sensor, 5> sensors{{{ASENSOR_TYPE_ACCELEROMETER, 1},
                                   {0, 0},
                                   {ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED, 4},
                                   {ASENSOR_TYPE_PRESSURE, 8},
                                   {0, 0}}};
    auto flush = [&] {
        const auto now = timeMs();
        if (now - mLastSendMs >= 10) {
            Writer data;
            for (const auto& sensor : sensors) {
                data.u32(0).u8(sensor.samples.size());
                for (const auto& sample : sensor.samples) {
                    for (auto value : sample.values) data.u16(value);
                    data.u32(sample.timeMs >> 32).u32(sample.timeMs);
                }
            }
            GnssBackend::get().send(0x3348, data.data(), 2);
            mLastSendMs = now;
        } else {
            for (auto& sensor : sensors) sensor.spam += sensor.samples.size();
        }
        for (auto& sensor : sensors) sensor.samples.clear();
    };
    auto readEvents = [&](Sensor& sensor) {
        if (!sensor.enabled) return;
        ASensorEvent events[2];
        const ssize_t count = getEvents(sensor.queue, events, 2);
        if (count <= 0 || count > 2) return;
        const auto now = timeMs();
        if (now - mLastReportMs > 1000) {
            for (size_t i = 0; i < sensors.size(); ++i) {
                auto& item = sensors[i];
                if (!item.spam && !item.rateLimited && !item.outOfRange) continue;
                ALOGI("TGT_MEMS %s: dropped %d events (spam) %d events (rate limit) %d events "
                      "(out of range) out of %d total for sensor %d since last report",
                      "SCSCMemsReportSensorDrops", item.spam, item.rateLimited, item.outOfRange,
                      item.total + 1, static_cast<int>(i));
                GnssDiagnostics::get().logText(7,
                                               "TGT_MEMS %s: dropped %d events (spam) %d events "
                                               "(rate limit) %d events (out of range) out of %d "
                                               "total for sensor %d since last report",
                                               "SCSCMemsReportSensorDrops", item.spam,
                                               item.rateLimited, item.outOfRange, item.total + 1,
                                               static_cast<int>(i));
                item.spam = item.rateLimited = item.outOfRange = item.total = 0;
                mLastReportMs = now;
            }
        }
        for (ssize_t i = 0; i < count; ++i) {
            const auto& event = events[i];
            if (event.type != sensor.type) continue;
            if (sensor.samples.size() == 5) flush();
            const bool acceleration = sensor.type == ASENSOR_TYPE_ACCELEROMETER;
            const bool pressure = sensor.type == ASENSOR_TYPE_PRESSURE;
            if (acceleration && (event.acceleration.status < 1 || event.acceleration.status > 3))
                continue;
            ++sensor.total;
            Sample sample;
            sample.timeMs =
                    static_cast<int64_t>(static_cast<uint64_t>(event.timestamp) + 500000) / 1000000;
            if (sensor.lastTimeMs && sample.timeMs - sensor.lastTimeMs < 8) {
                ++sensor.rateLimited;
                continue;
            }
            const float limit = acceleration ? 63.998047f : 15.999512f;
            const int values = pressure ? 1 : acceleration ? 3 : 6;
            bool valid = true;
            for (int axis = 0; axis < values; ++axis) {
                float value = event.data[axis];
                if (!pressure && axis < 3 && !(std::abs(value) <= limit)) {
                    valid = false;
                    break;
                }
                value = pressure ? (value - 1000) * 256 : value * (acceleration ? 512 : 2048);
                sample.values[axis] = sampleValue(value, pressure);
            }
            if (!valid) {
                ++sensor.outOfRange;
                continue;
            }
            sensor.samples.push_back(sample);
            sensor.lastTimeMs = sample.timeMs;
            bool complete = true;
            for (const auto& item : sensors) {
                if (item.enabled && item.samples.size() < 5) complete = false;
            }
            if (complete) flush();
        }
    };
    bool available = false;
    for (auto& sensor : sensors) {
        if (!looper || !(mask & sensor.mask)) continue;
        sensor.sensor = getSensor(manager, sensor.type);
        if (!sensor.sensor) continue;
        sensor.onReady = [&, item = &sensor] { readEvents(*item); };
        sensor.queue = createQueue(manager, looper, 3, sensorReady, &sensor);
        available |= sensor.queue != nullptr;
    }
    {
        std::lock_guard lock(mMutex);
        mInitialized = (mask & 13) != 0;
        mInitializing = false;
        mCondition.notify_all();
    }
    while (mRunning && available) {
        {
            std::unique_lock lock(mMutex);
            mCondition.wait(lock, [this] { return mActive || !mRunning; });
            if (!mRunning) break;
            mEnabled = true;
        }
        for (auto& sensor : sensors) {
            sensor.samples.clear();
            sensor.lastTimeMs = 0;
            if (!sensor.queue) continue;
            enable(sensor.queue, sensor.sensor);
            setRate(sensor.queue, sensor.sensor, 8000);
            sensor.enabled = true;
            mLastSendMs = 0;
        }
        while (mRunning && mActive) {
            if (pollOnce(100, nullptr, nullptr, nullptr) == ALOOPER_POLL_WAKE) {
                mActive = false;
                break;
            }
        }
        for (auto& sensor : sensors) {
            if (sensor.queue) disable(sensor.queue, sensor.sensor);
            sensor.enabled = false;
        }
        {
            std::lock_guard lock(mMutex);
            mEnabled = false;
            mCondition.notify_all();
        }
    }
    for (const auto& sensor : sensors) {
        if (sensor.queue && destroyQueue) destroyQueue(manager, sensor.queue);
    }
    finish();
}

}  // namespace aidl::android::hardware::gnss
