/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GnssGeofence.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <utils/SystemClock.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
namespace {
GnssLocation readLocation(protocol::Reader& reader) {
    reader.get<uint16_t>();
    GnssLocation location;
    location.gnssLocationFlags = 0x1f;
    location.latitudeDegrees = reader.get<double>();
    location.longitudeDegrees = reader.get<double>();
    location.altitudeMeters = reader.get<double>();
    location.speedMetersPerSec = reader.get<float>();
    location.bearingDegrees = reader.get<float>();
    location.horizontalAccuracyMeters = reader.get<float>();
    location.verticalAccuracyMeters = reader.get<float>();
    location.speedAccuracyMetersPerSecond = reader.get<float>();
    location.bearingAccuracyDegrees = reader.get<float>();
    const uint64_t low = reader.get<uint32_t>();
    const uint64_t high = reader.get<uint32_t>();
    location.timestampMillis = low | (high << 32);
    return location;
}
unsigned hashId(uint32_t id) {
    auto value = (id * 0x1001 ^ (id * 0x1001) >> 22) * 0x11;
    value = (value ^ value >> 9) * 0x401;
    value = (value ^ value >> 2) * 0x81;
    return (((value ^ value >> 12) >> 3) * 0x9e3779b1) % 512;
}

float radiusCentimeters(double radius) {
    const auto value = radius * 100;
    const uint32_t converted = std::isnan(value) || value <= 0 ? 0
                               : value >= 0x1p32 ? std::numeric_limits<uint32_t>::max()
                                                 : static_cast<uint32_t>(value);
    return static_cast<float>(converted);
}
}  // namespace

GnssGeofence::GnssGeofence() {
    for (auto id : {0x0312, 0x0412, 0x0512, 0x0612})
        GnssBackend::get().setHandler(
                id, [this, id](const std::vector<uint8_t>& data) { response(id, data); });
    GnssBackend::get().setHandler(0x0112, [this](const std::vector<uint8_t>& data) {
        protocol::Reader reader(data);
        auto location = readLocation(reader);
        auto masks = reader.bytes(192);
        if (!reader.ok()) return;
        std::array<int, 512> ids;
        {
            std::lock_guard lock(mMutex);
            ids = mIds;
        }
        for (size_t i = 0; i < ids.size(); ++i) {
            int transition = 0;
            for (int j = 0; j < 3; ++j)
                if (masks[i / 8 * 3 + j] & (1 << (i % 8))) transition = 1 << j;
            if (transition) {
                GnssBackend::get().postCallback([this, id = ids[i], location, transition] {
                    std::shared_ptr<IGnssGeofenceCallback> callback;
                    {
                        std::lock_guard lock(mMutex);
                        callback = mCallback;
                    }
                    if (callback) callback->gnssGeofenceTransitionCb(id, location, transition, 0);
                });
            }
        }
    });
    GnssBackend::get().setHandler(0x0212, [this](const std::vector<uint8_t>& data) {
        protocol::Reader reader(data);
        auto available = reader.get<uint32_t>();
        auto location = readLocation(reader);
        if (available != 2) location = GnssLocation{};
        if (!reader.ok()) return;
        GnssBackend::get().postCallback([this, available, location] {
            std::shared_ptr<IGnssGeofenceCallback> callback;
            {
                std::lock_guard lock(mMutex);
                callback = mCallback;
            }
            if (callback) callback->gnssGeofenceStatusCb(available, location);
        });
    });
}

GnssGeofence::~GnssGeofence() {
    for (auto id : {0x0112, 0x0212, 0x0312, 0x0412, 0x0512, 0x0612})
        GnssBackend::get().setHandler(id, {});
}

ndk::ScopedAStatus GnssGeofence::setCallback(
        const std::shared_ptr<IGnssGeofenceCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(-38);
    std::lock_guard lock(mMutex);
    const auto now = ::android::elapsedRealtime();
    if (now > mLastInitMillis + 1000) {
        mIds.fill(-1);
        mHashCount = 0;
        mLastInitMillis = now;
        if (protocol::readConfigInt("gf_enabled", 0)) {
            protocol::Writer data;
            data.put<uint32_t>(512);
            GnssBackend::get().send(0x0410, data.data());
        }
    }
    mCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssGeofence::addGeofence(int id, double latitude, double longitude,
                                             double radius, int lastTransition, int transitions,
                                             int responsiveness, int unknownTimer) {
    int error = 0;
    int slot = -1;
    {
        std::lock_guard lock(mMutex);
        if (!protocol::readConfigInt("gf_enabled", 0))
            error = IGnssGeofenceCallback::ERROR_GENERIC;
        else if (mHashCount == 512)
            error = IGnssGeofenceCallback::ERROR_TOO_MANY_GEOFENCES;
        else {
            const unsigned hash = hashId(id);
            for (unsigned i = 0; i < mIds.size(); ++i) {
                const unsigned index = (hash + i) % mIds.size();
                if (mIds[index] == -1) {
                    slot = index;
                    mIds[index] = id;
                    ++mHashCount;
                    break;
                }
                if (mIds[index] == id) {
                    error = IGnssGeofenceCallback::ERROR_ID_EXISTS;
                    break;
                }
            }
            if (slot == -1 && !error) error = IGnssGeofenceCallback::ERROR_TOO_MANY_GEOFENCES;
        }
    }
    if (error) {
        reportError(0x0110, id, error);
        return ndk::ScopedAStatus::ok();
    }
    protocol::Writer data;
    data.put<uint32_t>(slot);
    data.put<double>(latitude);
    data.put<double>(longitude);
    data.put<float>(radiusCentimeters(radius));
    data.put<uint8_t>(lastTransition);
    data.put<uint8_t>(transitions);
    data.put<uint32_t>(
            std::max(responsiveness, protocol::readConfigInt("notifResponsivenessMs", 0)));
    data.put<uint32_t>(unknownTimer);
    GnssBackend::get().send(0x0110, data.data());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssGeofence::command(uint16_t commandId, int id, int transitions) {
    int slot = -1;
    {
        std::lock_guard lock(mMutex);
        const auto hash = hashId(id);
        for (size_t i = 0; i < mIds.size(); ++i) {
            const auto index = (hash + i) % mIds.size();
            if (mIds[index] == id) {
                slot = index;
                break;
            }
        }
    }
    int error = slot == -1 ? IGnssGeofenceCallback::ERROR_ID_UNKNOWN : 0;
    if (!protocol::readConfigInt("gf_enabled", 0)) error = IGnssGeofenceCallback::ERROR_GENERIC;
    protocol::Writer data;
    data.put<uint32_t>(slot);
    if (commandId == 0x0310) data.put<uint8_t>(transitions);
    if (!error) GnssBackend::get().send(commandId, data.data());
    if (error) reportError(commandId, id, error);
    return ndk::ScopedAStatus::ok();
}

void GnssGeofence::reportError(uint16_t commandId, int id, int error) {
    GnssBackend::get().postCallback([this, commandId, id, error] {
        std::shared_ptr<IGnssGeofenceCallback> callback;
        {
            std::lock_guard lock(mMutex);
            callback = mCallback;
        }
        if (!callback) return;
        if (commandId == 0x0110) callback->gnssGeofenceAddCb(id, error);
        if (commandId == 0x0210) callback->gnssGeofencePauseCb(id, error);
        if (commandId == 0x0310) callback->gnssGeofenceResumeCb(id, error);
        if (commandId == 0x0410) callback->gnssGeofenceRemoveCb(id, error);
    });
}

ndk::ScopedAStatus GnssGeofence::pauseGeofence(int id) {
    return command(0x0210, id);
}

ndk::ScopedAStatus GnssGeofence::resumeGeofence(int id, int transitions) {
    return command(0x0310, id, transitions);
}

ndk::ScopedAStatus GnssGeofence::removeGeofence(int id) {
    return command(0x0410, id);
}

void GnssGeofence::response(uint16_t commandId, const std::vector<uint8_t>& data) {
    protocol::Reader reader(data);
    auto slot = reader.get<uint32_t>();
    auto status = reader.get<int32_t>();
    if (!reader.ok()) return;
    int id;
    {
        std::lock_guard lock(mMutex);
        id = slot < mIds.size() ? mIds[slot] : IGnssGeofenceCallback::ERROR_ID_UNKNOWN;
        if (commandId == 0x0412 || (commandId == 0x0312 && status != 0)) {
            if (slot < mIds.size() && mIds[slot] != -1) {
                mIds[slot] = -1;
                --mHashCount;
            } else {
                id = slot == 512 ? static_cast<int>(IGnssGeofenceCallback::ERROR_GENERIC)
                                 : static_cast<int>(IGnssGeofenceCallback::ERROR_ID_UNKNOWN);
            }
        }
    }
    if (commandId == 0x0412 && id == IGnssGeofenceCallback::ERROR_GENERIC) return;
    GnssBackend::get().postCallback([this, commandId, id, status] {
        std::shared_ptr<IGnssGeofenceCallback> callback;
        {
            std::lock_guard lock(mMutex);
            callback = mCallback;
        }
        if (!callback) return;
        if (commandId == 0x0312) callback->gnssGeofenceAddCb(id, status);
        if (commandId == 0x0412) callback->gnssGeofenceRemoveCb(id, status);
        if (commandId == 0x0512) callback->gnssGeofencePauseCb(id, status);
        if (commandId == 0x0612) callback->gnssGeofenceResumeCb(id, status);
    });
}

void GnssGeofence::clearCallback() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
}

}  // namespace aidl::android::hardware::gnss
