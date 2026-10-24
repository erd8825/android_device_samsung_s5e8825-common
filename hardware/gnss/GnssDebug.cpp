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

#include "GnssDebug.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <utils/SystemClock.h>
#include "Gnss.h"
#include "GnssProtocol.h"

namespace aidl::android::hardware::gnss {

ndk::ScopedAStatus GnssDebug::getDebugData(DebugData* data) {
    *data = {};
    auto gnss = mGnss.lock();
    if (!gnss) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    std::shared_ptr<GnssLocation> location;
    std::shared_ptr<GnssData> measurements;
    uint32_t tow;
    {
        std::lock_guard lock(gnss->mMutex);
        location = gnss->mLastLocation;
        tow = gnss->mLastTowMs;
        if (gnss->mGnssMeasurementInterface) measurements = gnss->mGnssMeasurementInterface->getLastData();
    }
    double ageSeconds = 0;
    data->time.timeEstimateMs = std::max<int64_t>(std::time(nullptr) * 1000LL, 1483228800000LL);
    data->time.timeUncertaintyNs = 1000000;
    data->time.frequencyUncertaintyNsPerSec = std::clamp(
            static_cast<float>(readConfigInt("Chip_Configuration_RefClkUncertainty", 0)), 0.1f, 999.9f);
    if (location) {
        ageSeconds = std::max(0.0, (::android::elapsedRealtimeNano() - location->elapsedRealtime.timestampNs) / 1e9);
        data->position = {.valid = true,
                          .latitudeDegrees = location->latitudeDegrees,
                          .longitudeDegrees = location->longitudeDegrees,
                          .altitudeMeters = static_cast<float>(location->altitudeMeters),
                          .speedMetersPerSec = static_cast<float>(location->speedMetersPerSec),
                          .bearingDegrees = static_cast<float>(location->bearingDegrees),
                          .horizontalAccuracyMeters = location->horizontalAccuracyMeters,
                          .verticalAccuracyMeters = location->verticalAccuracyMeters,
                          .speedAccuracyMetersPerSecond = location->speedAccuracyMetersPerSecond,
                          .bearingAccuracyDegrees = location->bearingAccuracyDegrees,
                          .ageSeconds = static_cast<float>(ageSeconds)};
        data->time.timeEstimateMs = location->timestampMillis + static_cast<int64_t>(ageSeconds * 1000);
    }
    if (measurements) {
        for (const auto& measurement : measurements->measurements) {
            SatelliteData satellite;
            satellite.svid = measurement.svid;
            satellite.constellation = measurement.signalType.constellation;
            satellite.ephemerisType = SatelliteEphemerisType::EPHEMERIS;
            satellite.ephemerisSource = SatellitePvt::SatelliteEphemerisSource::DEMODULATED;
            satellite.ephemerisHealth = SatelliteEphemerisHealth::GOOD;
            if (location && (measurement.flags & GnssMeasurement::HAS_SATELLITE_PVT)) {
                double age = std::remainder(tow / 1000.0 + ageSeconds - measurement.satellitePvt.timeOfEphemerisSeconds, 604800.0);
                if (std::abs(age) < 7200) satellite.ephemerisAgeSeconds = age;
            }
            data->satelliteDataArray.push_back(satellite);
        }
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
