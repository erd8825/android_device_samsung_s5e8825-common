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

#include "MeasurementCorrectionsInterface.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss::measurement_corrections {
MeasurementCorrectionsInterface::MeasurementCorrectionsInterface() {
    GnssBackend::get().setHandler(0x800a, [this](const std::vector<uint8_t>& data) {
        std::shared_ptr<IMeasurementCorrectionsCallback> callback;
        {
            std::lock_guard lock(mMutex);
            callback = mCallback;
        }
        if (!data.empty() && data[0] > 0 && data[0] <= 7 && callback)
            callback->setCapabilitiesCb(data[0]);
    });
}

MeasurementCorrectionsInterface::~MeasurementCorrectionsInterface() {
    GnssBackend::get().setHandler(0x800a, {});
}

ndk::ScopedAStatus MeasurementCorrectionsInterface::setCallback(
        const std::shared_ptr<IMeasurementCorrectionsCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    {
        std::lock_guard lock(mMutex);
        mCallback = callback;
    }
    onConnected();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus MeasurementCorrectionsInterface::setCorrections(
        const MeasurementCorrections& corrections) {
    if (!std::isfinite(corrections.latitudeDegrees) || std::abs(corrections.latitudeDegrees) > 90 ||
        !std::isfinite(corrections.longitudeDegrees) ||
        std::abs(corrections.longitudeDegrees) > 180 ||
        !std::isfinite(corrections.altitudeMeters) ||
        !std::isfinite(corrections.horizontalPositionUncertaintyMeters) ||
        !std::isfinite(corrections.verticalPositionUncertaintyMeters) ||
        (corrections.hasEnvironmentBearing &&
         (!std::isfinite(corrections.environmentBearingDegrees) ||
          !std::isfinite(corrections.environmentBearingUncertaintyDegrees))))
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    protocol::Writer data;
    auto position = [](double value, double scale) {
        return static_cast<int32_t>(
                std::clamp(value / scale, static_cast<double>(std::numeric_limits<int32_t>::min()),
                           static_cast<double>(std::numeric_limits<int32_t>::max())));
    };
    auto scaled = [](double value) {
        return static_cast<uint16_t>(std::clamp(value, 0.0, 65535.0));
    };
    data.put<int32_t>(position(corrections.latitudeDegrees, 4.19095158577e-08));
    data.put<int32_t>(position(corrections.longitudeDegrees, 8.381903171539e-08));
    data.put<uint16_t>(scaled(corrections.altitudeMeters * 10 + 5000));
    data.put<uint16_t>(scaled(corrections.horizontalPositionUncertaintyMeters * 10));
    data.put<uint16_t>(scaled(corrections.verticalPositionUncertaintyMeters * 10));
    data.put<uint32_t>(
            (static_cast<uint64_t>(corrections.toaGpsNanosecondsOfWeek) & 0x3ffffffffffffULL) /
            1000000000);
    data.put<uint8_t>(corrections.hasEnvironmentBearing);
    data.put<uint16_t>(corrections.hasEnvironmentBearing
                               ? scaled(corrections.environmentBearingDegrees * 100)
                               : 0);
    data.put<uint16_t>(corrections.hasEnvironmentBearing
                               ? scaled(corrections.environmentBearingUncertaintyDegrees * 100)
                               : 0);
    const auto count = std::min<size_t>(64, corrections.satCorrections.size());
    data.put<uint32_t>(count);
    for (size_t i = 0; i < count; ++i) {
        const auto& sat = corrections.satCorrections[i];
        if ((sat.singleSatCorrectionFlags & 1) && !std::isfinite(sat.probSatIsLos))
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        uint8_t svid = sat.svid;
        switch (sat.constellation) {
            case GnssConstellationType::GPS:
            case GnssConstellationType::SBAS:
            case GnssConstellationType::QZSS:
                break;
            case GnssConstellationType::GLONASS:
                if (svid) svid += sat.svid <= 24 ? 64 : -23;
                break;
            case GnssConstellationType::BEIDOU:
                if (svid) svid += 138;
                break;
            case GnssConstellationType::GALILEO:
                if (svid) svid -= 54;
                break;
            case GnssConstellationType::IRNSS:
                if (svid) svid += 100;
                break;
            default:
                svid = 0;
                break;
        }
        data.put<uint8_t>(sat.singleSatCorrectionFlags);
        data.put<uint8_t>(svid);
        data.put<uint8_t>((sat.singleSatCorrectionFlags & 1)
                                  ? std::clamp(sat.probSatIsLos, 0.0f, 1.0f) * 100
                                  : 0);
    }
    return GnssBackend::get().send(0x8104, data.data())
                   ? ndk::ScopedAStatus::ok()
                   : ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_GENERIC);
}

void MeasurementCorrectionsInterface::clearCallback() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
}

void MeasurementCorrectionsInterface::onConnected() {
    bool enabled;
    {
        std::lock_guard lock(mMutex);
        enabled = mCallback != nullptr;
    }
    if (enabled)
        GnssBackend::get().send(
                0x800a,
                {static_cast<uint8_t>(protocol::readConfigInt("GlueLayer_MeasCorrCap", 0))});
}
}  // namespace aidl::android::hardware::gnss::measurement_corrections
