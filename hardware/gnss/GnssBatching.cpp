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

#include "GnssBatching.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <utils/SystemClock.h>
#include <climits>
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
namespace {
ndk::ScopedAStatus send(uint16_t id, const std::vector<uint8_t>& data) {
    GnssBackend::get().send(id, data);
    return ndk::ScopedAStatus::ok();
}
}  // namespace

GnssBatching::GnssBatching() {
    GnssBackend::get().setHandler(0x7c0a, [this](const std::vector<uint8_t>& data) {
        protocol::Reader reader(data);
        reader.get<uint16_t>();
        GnssLocation location;
        location.latitudeDegrees = reader.get<double>();
        location.longitudeDegrees = reader.get<double>();
        location.altitudeMeters = reader.get<double>();
        location.speedMetersPerSec = reader.get<float>();
        location.bearingDegrees = reader.get<float>();
        location.horizontalAccuracyMeters = reader.get<float>();
        const uint64_t low = reader.get<uint32_t>();
        const uint64_t high = reader.get<uint32_t>();
        location.timestampMillis = low | (high << 32);
        location.verticalAccuracyMeters = reader.get<float>();
        location.speedAccuracyMetersPerSecond = reader.get<float>();
        location.bearingAccuracyDegrees = reader.get<float>();
        const auto remaining = reader.get<uint16_t>();
        if (!reader.ok()) return;
        location.gnssLocationFlags = 0x77 | (location.speedMetersPerSec != 0 ? 0x88 : 0);
        location.elapsedRealtime.flags =
                static_cast<int32_t>(ElapsedRealtime::HAS_TIMESTAMP_NS) |
                static_cast<int32_t>(ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS);
        location.elapsedRealtime.timestampNs = ::android::elapsedRealtimeNano();
        location.elapsedRealtime.timeUncertaintyNs = 1000;
        std::vector<GnssLocation> locations;
        bool complete = false;
        {
            std::lock_guard lock(mMutex);
            if (mLocations.empty()) {
                const int size = protocol::readConfigInt("GlueLayer_BatchingCbSize", 200);
                mCallbackSize = size >= 0 && size <= 200 ? size : 200;
            }
            if (mCallbackSize == 0) return;
            if (remaining == 200)
                mLocations.clear();
            else
                mLocations.push_back(location);
            if (remaining == 0 || remaining == 200 || mLocations.size() == mCallbackSize) {
                locations.swap(mLocations);
                complete = true;
            }
        }
        if (complete) {
            GnssBackend::get().postCallback([this, locations = std::move(locations)] {
                std::shared_ptr<IGnssBatchingCallback> callback;
                {
                    std::lock_guard lock(mMutex);
                    callback = mCallback;
                }
                if (callback) callback->gnssLocationBatchCb(locations);
            });
        }
    });
}

GnssBatching::~GnssBatching() {
    GnssBackend::get().setHandler(0x7c0a, {});
}

ndk::ScopedAStatus GnssBatching::init(const std::shared_ptr<IGnssBatchingCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(-38);
    std::lock_guard lock(mMutex);
    mCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssBatching::getBatchSize(int* size) {
    *size = 200;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssBatching::start(const Options& options) {
    const auto period = static_cast<int32_t>(options.periodNanos / 1000000);
    protocol::Writer data;
    data.put<uint8_t>(2);
    data.put<uint32_t>(period);
    data.put<uint8_t>((options.flags & 1) != 0);
    {
        std::lock_guard lock(mMutex);
        if (!mCallback) return ndk::ScopedAStatus::ok();
    }
    return send(0x6604, data.data());
}

ndk::ScopedAStatus GnssBatching::flush() {
    {
        std::lock_guard lock(mMutex);
        if (!mCallback) return ndk::ScopedAStatus::ok();
    }
    return send(0x8204, {0});
}

ndk::ScopedAStatus GnssBatching::stop() {
    {
        std::lock_guard lock(mMutex);
        if (!mCallback) return ndk::ScopedAStatus::ok();
    }
    return send(0x6704, {2});
}

ndk::ScopedAStatus GnssBatching::cleanup() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
