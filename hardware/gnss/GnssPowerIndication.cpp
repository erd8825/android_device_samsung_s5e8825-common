/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "GnssPowerIndication.h"

#include <aidl/android/hardware/gnss/GnssPowerStats.h>
#include <utils/SystemClock.h>
#include "GnssBackend.h"
#include "GnssProtocol.h"

namespace aidl::android::hardware::gnss {

GnssPowerIndication::GnssPowerIndication() {
    GnssBackend::get().setHandler(0x850a, [this](const auto& payload) {
        protocol::Reader reader(payload);
        GnssPowerStats stats;
        stats.totalEnergyMilliJoule = reader.f64();
        stats.singlebandTrackingModeEnergyMilliJoule = reader.f64();
        stats.multibandTrackingModeEnergyMilliJoule = reader.f64();
        stats.singlebandAcquisitionModeEnergyMilliJoule = reader.f64();
        stats.multibandAcquisitionModeEnergyMilliJoule = reader.f64();
        if (!reader.ok()) return;
        GnssBackend::get().postCallback([this, stats = std::move(stats)]() mutable {
            stats.elapsedRealtime.flags = 0;
            stats.elapsedRealtime.timestampNs = ::android::elapsedRealtimeNano();
            stats.elapsedRealtime.timeUncertaintyNs = 1000;
            std::shared_ptr<IGnssPowerIndicationCallback> callback;
            {
                std::lock_guard lock(mMutex);
                callback = mCallback;
            }
            if (callback) callback->gnssPowerStatsCb(stats);
        });
    });
}

GnssPowerIndication::~GnssPowerIndication() {
    GnssBackend::get().setHandler(0x850a, {});
}

ndk::ScopedAStatus GnssPowerIndication::setCallback(
        const std::shared_ptr<IGnssPowerIndicationCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(-38);
    {
        std::lock_guard lock(mMutex);
        mCallback = callback;
    }
    callback->setCapabilitiesCb(0x3f);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssPowerIndication::requestGnssPowerStats() {
    GnssBackend::get().send(0x8404, {0});
    return ndk::ScopedAStatus::ok();
}

void GnssPowerIndication::clearCallback() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
}

}  // namespace aidl::android::hardware::gnss
