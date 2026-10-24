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

#include "GnssAntennaInfo.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <cmath>
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
GnssAntennaInfo::GnssAntennaInfo() {
    GnssBackend::get().setHandler(0x7a0a, [this](const std::vector<uint8_t>& data) {
        protocol::Reader reader(data);
        const auto count = reader.get<uint8_t>();
        if (count > 16) return;
        std::vector<IGnssAntennaInfoCallback::GnssAntennaInfo> infos;
        for (unsigned i = 0; i < count; ++i) {
            IGnssAntennaInfoCallback::GnssAntennaInfo info;
            double frequency = reader.get<double>() * 1000000.0;
            if (!std::isfinite(frequency) || frequency < 0 || frequency >= 9.22e18) return;
            info.carrierFrequencyHz = static_cast<int64_t>(frequency);
            auto& coord = info.phaseCenterOffsetCoordinateMillimeters;
            coord.x = reader.get<double>();
            coord.xUncertainty = reader.get<double>();
            coord.y = reader.get<double>();
            coord.yUncertainty = reader.get<double>();
            coord.z = reader.get<double>();
            coord.zUncertainty = reader.get<double>();
            infos.push_back(std::move(info));
        }
        std::shared_ptr<IGnssAntennaInfoCallback> callback;
        {
            std::lock_guard lock(mMutex);
            callback = mCallback;
        }
        if (reader.ok() && callback) callback->gnssAntennaInfoCb(infos);
    });
}

GnssAntennaInfo::~GnssAntennaInfo() {
    GnssBackend::get().setHandler(0x7a0a, {});
}

ndk::ScopedAStatus GnssAntennaInfo::setCallback(
        const std::shared_ptr<IGnssAntennaInfoCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    std::lock_guard lock(mMutex);
    if (!mCallback) {
        mCallback = callback;
        mRequestSent = false;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssAntennaInfo::close() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
    mRequestSent = false;
    return ndk::ScopedAStatus::ok();
}

void GnssAntennaInfo::onConnected() {
    std::lock_guard lock(mMutex);
    mRequestSent = false;
}

void GnssAntennaInfo::onSvStatus() {
    std::lock_guard lock(mMutex);
    if (mRequestSent) return;
    if (protocol::readConfigInt("GlueLayer_AntennaInfoCap", 0))
        GnssBackend::get().send(0x8004, {static_cast<uint8_t>(mCallback != nullptr)});
    mRequestSent = true;
}
}  // namespace aidl::android::hardware::gnss
