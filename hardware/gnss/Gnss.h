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

#pragma once

#include <aidl/android/hardware/gnss/BnAGnss.h>
#include <aidl/android/hardware/gnss/BnAGnssRil.h>
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <aidl/android/hardware/gnss/BnGnssAntennaInfo.h>
#include <aidl/android/hardware/gnss/BnGnssBatching.h>
#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>
#include <aidl/android/hardware/gnss/BnGnssDebug.h>
#include <aidl/android/hardware/gnss/BnGnssMeasurementInterface.h>
#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>
#include <aidl/android/hardware/gnss/BnGnssPsds.h>
#include <aidl/android/hardware/gnss/gnss_assistance/BnGnssAssistanceInterface.h>
#include <aidl/android/hardware/gnss/measurement_corrections/BnMeasurementCorrectionsInterface.h>
#include <aidl/android/hardware/gnss/visibility_control/BnGnssVisibilityControl.h>
#include <mutex>
#include <condition_variable>
#include "GnssConfiguration.h"
#include "GnssMeasurementInterface.h"
#include "GnssPowerIndication.h"
#include "GnssSensors.h"

namespace aidl::android::hardware::gnss {
namespace visibility_control {
struct GnssVisibilityControl;
}
namespace measurement_corrections {
struct MeasurementCorrectionsInterface;
}

class Gnss : public BnGnss {
  public:
    Gnss();
    ~Gnss() override;
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback>& callback) override;
    ndk::ScopedAStatus start() override;
    ndk::ScopedAStatus stop() override;
    ndk::ScopedAStatus close() override;

    ndk::ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs,
                                  int uncertaintyMs) override;
    ndk::ScopedAStatus injectLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus injectBestLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus deleteAidingData(GnssAidingData aidingDataFlags) override;
    ndk::ScopedAStatus setPositionMode(const PositionModeOptions& options) override;
    ndk::ScopedAStatus startSvStatus() override;
    ndk::ScopedAStatus stopSvStatus() override;
    ndk::ScopedAStatus startNmea() override;
    ndk::ScopedAStatus stopNmea() override;

    ndk::ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds>* iGnssPsds) override;
    ndk::ScopedAStatus getExtensionGnssConfiguration(
            std::shared_ptr<IGnssConfiguration>* iGnssConfiguration) override;
    ndk::ScopedAStatus getExtensionGnssPowerIndication(
            std::shared_ptr<IGnssPowerIndication>* iGnssPowerIndication) override;
    ndk::ScopedAStatus getExtensionGnssMeasurement(
            std::shared_ptr<IGnssMeasurementInterface>* iGnssMeasurement) override;
    ndk::ScopedAStatus getExtensionGnssBatching(
            std::shared_ptr<IGnssBatching>* iGnssBatching) override;
    ndk::ScopedAStatus getExtensionGnssGeofence(
            std::shared_ptr<IGnssGeofence>* iGnssGeofence) override;
    ndk::ScopedAStatus getExtensionGnssNavigationMessage(
            std::shared_ptr<IGnssNavigationMessageInterface>* iGnssNavigationMessage) override;
    ndk::ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss>* iAGnss) override;
    ndk::ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* iAGnssRil) override;
    ndk::ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* iGnssDebug) override;
    ndk::ScopedAStatus getExtensionGnssVisibilityControl(
            std::shared_ptr<android::hardware::gnss::visibility_control::IGnssVisibilityControl>*
                    iGnssVisibilityControl) override;
    ndk::ScopedAStatus getExtensionGnssAntennaInfo(
            std::shared_ptr<IGnssAntennaInfo>* iGnssAntennaInfo) override;
    ndk::ScopedAStatus getExtensionMeasurementCorrections(
            std::shared_ptr<android::hardware::gnss::measurement_corrections::
                                    IMeasurementCorrectionsInterface>* iMeasurementCorrections)
            override;
    ndk::ScopedAStatus getExtensionGnssAssistanceInterface(
            std::shared_ptr<android::hardware::gnss::gnss_assistance::IGnssAssistanceInterface>*
                    iGnssAssistanceInterface) override;

    std::shared_ptr<GnssLocation> getLastLocation() const;
    std::vector<IGnssCallback::GnssSvInfo> getLastSvStatus() const;

  private:
    friend struct GnssDebug;
    void onConnected(bool fresh);
    void acknowledge(uint8_t mask);
    void processPendingCommands();
    void onLocation(const std::vector<uint8_t>& payload);
    void onSvStatus(const std::vector<uint8_t>& payload);
    void onNmea(const std::vector<uint8_t>& payload);
    void reportStatus(IGnssCallback::GnssStatusValue status);
    void reportEngineStatus(bool active);
    void sendPositionMode();
    void ensureMeasurementInterface();
    std::shared_ptr<IGnssCallback> getCallback() const;

    std::shared_ptr<IGnssCallback> mCallback;
    std::shared_ptr<GnssConfiguration> mGnssConfiguration;
    std::shared_ptr<GnssPowerIndication> mGnssPowerIndication;
    std::shared_ptr<GnssMeasurementInterface> mGnssMeasurementInterface;
    std::shared_ptr<struct AGnss> mAGnss;
    std::shared_ptr<struct AGnssRil> mAGnssRil;
    std::shared_ptr<struct GnssPsds> mGnssPsds;
    std::shared_ptr<struct GnssBatching> mGnssBatching;
    std::shared_ptr<struct GnssGeofence> mGnssGeofence;
    std::shared_ptr<struct GnssNavigationMessageInterface> mGnssNavigation;
    std::shared_ptr<struct GnssDebug> mGnssDebug;
    std::shared_ptr<struct GnssAntennaInfo> mGnssAntennaInfo;
    std::shared_ptr<visibility_control::GnssVisibilityControl> mGnssVisibilityControl;
    std::shared_ptr<measurement_corrections::MeasurementCorrectionsInterface> mCorrections;
    bool mIsActive = false;
    bool mEverConnected = false;
    uint16_t mAidingData = 0;
    uint64_t mSvReportCount = 0;
    bool mSensorsPending = false;
    GnssSensors mSensors;
    bool mIsEngineOn = false;
    bool mEngineStatusSet = false;
    IGnssCallback::GnssStatusValue mGnssStatus = IGnssCallback::GnssStatusValue::NONE;
    bool mIsSvStatusActive = true;
    bool mIsNmeaActive = true;
    bool mPositionModeSet = false;
    PositionModeOptions mPositionMode;
    int64_t mLastInjectionSeconds = 0;
    uint32_t mLastNmeaTime = 0;
    uint32_t mLastTowMs = 0;
    std::shared_ptr<GnssLocation> mLastLocation;
    std::vector<IGnssCallback::GnssSvInfo> mLastSvStatus;
    mutable std::recursive_mutex mMutex;
    std::mutex mCommandMutex;
    std::mutex mPendingMutex;
    std::condition_variable mAckCondition;
    bool mStartPending = false;
    bool mStopPending = false;
    bool mAckReceived = false;
};

}  // namespace aidl::android::hardware::gnss
