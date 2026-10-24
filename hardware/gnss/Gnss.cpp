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

#define LOG_TAG "GnssAidl"

#include "Gnss.h"
#include <log/log.h>
#include <utils/SystemClock.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "AGnss.h"
#include "AGnssRil.h"
#include "GnssAntennaInfo.h"
#include "GnssBackend.h"
#include "GnssBatching.h"
#include "GnssDebug.h"
#include "GnssGeofence.h"
#include "GnssNavigationMessageInterface.h"
#include "GnssProtocol.h"
#include "GnssPsds.h"
#include "GnssVisibilityControl.h"
#include "MeasurementCorrectionsInterface.h"

namespace aidl::android::hardware::gnss {
using ndk::ScopedAStatus;

Gnss::Gnss() {
    mAGnssRil = SharedRefBase::make<AGnssRil>();
    ensureMeasurementInterface();
    auto& backend = GnssBackend::get();
    backend.setHandler(0x770a, [this](const auto& p) {
        if (p.empty() || (p[0] & 0xf) != 0xa) return;
        acknowledge(p[0] & 0xf0);
        std::lock_guard lock(mMutex);
        if (mIsActive && mSensorsPending) {
            mSensorsPending = false;
            mSensors.start();
        }
    });
    backend.setHandler(0x780a, [this](const auto&) { acknowledge(0xc0); });
    backend.setHandler(0x0104, [this](const auto&) {
        std::lock_guard lock(mMutex);
        mSensors.disableConfig();
    });
    backend.setHandler(0x650a, [this](const auto& p) { onLocation(p); });
    backend.setHandler(0x710a, [this](const auto& p) { onSvStatus(p); });
    backend.setHandler(0x670a, [this](const auto& p) { onNmea(p); });
    backend.setHandler(0x6e0a, [this](const auto&) {
        if (auto callback = getCallback()) callback->gnssRequestTimeCb();
    });
    backend.setHandler(0x760a, [this](const auto& p) {
        if (auto callback = getCallback(); callback && !p.empty()) {
            callback->gnssRequestLocationCb(p[0] == 0, p[0] != 0);
        }
    });
    backend.setHandler(0x0218, [this](const auto& p) {
        if (p.empty()) return;
        if (p[0] == 3) {
            reportEngineStatus(true);
        } else if (p[0] == 4) {
            reportEngineStatus(false);
        }
    });
    backend.setHandler(0x01c9, [](const auto&) {
        std::vector<uint8_t> version(256);
        const std::string name = "LineageOS GNSS";
        std::copy(name.begin(), name.end(), version.begin());
        GnssBackend::get().send(0x02c9, version);
    });
    backend.setReadyHandler([this] { acknowledge(0x40); });
    backend.setConnectedHandler([this](bool fresh) { onConnected(fresh); });
    backend.start();
}

Gnss::~Gnss() {
    GnssBackend::get().stop();
    close();
    for (auto id : {0x650a, 0x710a, 0x670a, 0x6e0a, 0x760a, 0x0218, 0x01c9, 0x770a, 0x780a, 0x0104}) {
        GnssBackend::get().setHandler(id, nullptr);
    }
    GnssBackend::get().setConnectedHandler(nullptr);
    GnssBackend::get().setReadyHandler(nullptr);
}

std::shared_ptr<IGnssCallback> Gnss::getCallback() const {
    std::lock_guard lock(mMutex);
    return mCallback;
}

ScopedAStatus Gnss::setCallback(const std::shared_ptr<IGnssCallback>& callback) {
    if (!callback) return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    {
        std::lock_guard lock(mMutex);
        mCallback = callback;
    }
    int capabilities = 0xe3de;
    if (readConfigInt("scheduling_enabled", 1))
        capabilities |= IGnssCallback::CAPABILITY_SCHEDULING;
    if (readConfigInt("gf_enabled", 0)) capabilities |= IGnssCallback::CAPABILITY_GEOFENCING;
    if (readConfigInt("GlueLayer_AntennaInfoCap", 0))
        capabilities |= IGnssCallback::CAPABILITY_ANTENNA_INFO;
    if (readConfigInt("GlueLayer_MeasCorrCap", 0))
        capabilities |= IGnssCallback::CAPABILITY_MEASUREMENT_CORRECTIONS;
    callback->gnssSetCapabilitiesCb(capabilities);
    IGnssCallback::GnssSystemInfo info;
    info.yearOfHw = static_cast<uint16_t>(readConfigInt("GlueLayer_YearOfHW", 0));
    info.name = "S.LSI," + readChipType() + ",LineageOS";
    struct Signal {
        int mask;
        GnssConstellationType constellation;
        double frequency;
        const char* code;
    };
    const Signal signals[] = {
            {1, GnssConstellationType::GPS, 1575420000, "C"},
            {0x80, GnssConstellationType::GPS, 1176450000, "X"},
            {0x10, GnssConstellationType::SBAS, 1575420000, "C"},
            {2, GnssConstellationType::GLONASS, 1602000000, "C"},
            {4, GnssConstellationType::GALILEO, 1575420000, "C"},
            {0x100, GnssConstellationType::GALILEO, 1176450000, "X"},
            {8, GnssConstellationType::BEIDOU, 1561098000, "I"},
            {0x20, GnssConstellationType::QZSS, 1575420000, "C"},
            {0x40, GnssConstellationType::IRNSS, 1176450000, "X"},
    };
    std::vector<GnssSignalType> types;
    const uint16_t constraint = std::strtoul(
            readConfigString("Chip_Configuration_GNSSConstConstraintDef", "0x3f").c_str(),
            nullptr, 0);
    for (const auto& signal : signals) {
        if (constraint & signal.mask) {
            GnssSignalType type;
            type.constellation = signal.constellation;
            type.carrierFrequencyHz = signal.frequency;
            type.codeType = signal.code;
            types.push_back(type);
        }
    }
    callback->gnssSetSignalTypeCapabilitiesCb(types);
    callback->gnssSetSystemInfoCb(info);
    GnssBackend::get().send(0x6504, {1});
    return ScopedAStatus::ok();
}

void Gnss::onConnected(bool fresh) {
    std::lock_guard lock(mMutex);
    const bool restored = mEverConnected;
    GnssBackend::get().send(0x6504, {static_cast<uint8_t>(!restored || mCallback != nullptr)});
    mEverConnected = true;
    if (mGnssConfiguration) mGnssConfiguration->onConnected();
    if (mAGnss) mAGnss->onConnected();
    if (mAGnssRil) mAGnssRil->onConnected();
    if (mGnssMeasurementInterface) mGnssMeasurementInterface->onConnected();
    if (fresh && mGnssAntennaInfo) mGnssAntennaInfo->onConnected();
    if (mGnssVisibilityControl) mGnssVisibilityControl->onConnected();
    if (mGnssStatus == IGnssCallback::GnssStatusValue::SESSION_BEGIN) {
        mSensorsPending = true;
        if (mPositionModeSet) sendPositionMode();
        GnssBackend::get().send(0x6604, Writer().u8(0).u32(0).u8(0).data());
    }
    if (restored && (mAidingData & 2)) {
        GnssBackend::get().send(0x7c04, Writer().u16(mAidingData).data());
        mAidingData = 0;
    }
}

void Gnss::acknowledge(uint8_t mask) {
    std::lock_guard lock(mPendingMutex);
    if ((mask & 0x80) && mStopPending) {
        mStopPending = false;
        mAckReceived = true;
    } else if ((mask & 0x40) && mStartPending) {
        mStartPending = false;
        mAckReceived = true;
    }
    if (mAckReceived) mAckCondition.notify_all();
}

void Gnss::processPendingCommands() {
    std::unique_lock commandLock(mCommandMutex, std::try_to_lock);
    if (!commandLock.owns_lock()) return;
    while (true) {
        bool stopping;
        {
            std::lock_guard lock(mPendingMutex);
            if (!mStopPending && !mStartPending) return;
            stopping = mStopPending;
            mAckReceived = false;
        }
        {
            std::lock_guard lock(mMutex);
            if (stopping) {
                mIsActive = false;
                mSensorsPending = false;
                mSensors.stop();
                if (mGnssMeasurementInterface) mGnssMeasurementInterface->setLocationEnabled(false);
                GnssBackend::get().send(0x6704, {0});
            } else {
                mIsActive = true;
                mSensorsPending = true;
                mAGnssRil->onStart();
                if (mGnssMeasurementInterface) mGnssMeasurementInterface->setLocationEnabled(true);
                if (mPositionModeSet) sendPositionMode();
                GnssBackend::get().send(0x6604, Writer().u8(0).u32(0).u8(0).data());
            }
        }
        reportEngineStatus(!stopping);
        std::unique_lock lock(mPendingMutex);
        if (!mAckCondition.wait_for(lock, std::chrono::seconds(5), [this] { return mAckReceived; })) {
            ALOGW("Timed out waiting for GNSS %s acknowledgement", stopping ? "stop" : "start");
            if (mStopPending) mStopPending = false;
            else mStartPending = false;
        }
        mAckReceived = false;
    }
}

void Gnss::onLocation(const std::vector<uint8_t>& payload) {
    Reader reader(payload);
    reader.u16();
    GnssLocation location;
    location.latitudeDegrees = reader.f64();
    location.longitudeDegrees = reader.f64();
    location.altitudeMeters = reader.f64();
    location.speedMetersPerSec = reader.f32();
    location.bearingDegrees = reader.f32();
    location.horizontalAccuracyMeters = reader.f32();
    uint64_t low = reader.u32();
    uint64_t high = reader.u32();
    location.timestampMillis = low | (high << 32);
    reader.u32();
    reader.u32();
    reader.u16();
    const uint32_t tow = reader.u32();
    location.verticalAccuracyMeters = reader.f32();
    location.speedAccuracyMetersPerSecond = reader.f32();
    location.bearingAccuracyDegrees = reader.f32();
    if (!reader.ok()) return;
    location.gnssLocationFlags = 0x77 | (location.speedMetersPerSec != 0 ? 0x88 : 0);
    location.elapsedRealtime.flags = static_cast<int>(ElapsedRealtime::HAS_TIMESTAMP_NS) |
                                     static_cast<int>(ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS);
    location.elapsedRealtime.timestampNs = ::android::elapsedRealtimeNano();
    location.elapsedRealtime.timeUncertaintyNs = 1000;
    std::shared_ptr<IGnssCallback> callback;
    {
        std::lock_guard lock(mMutex);
        if (mGnssStatus == IGnssCallback::GnssStatusValue::ENGINE_OFF) {
            GnssBackend::get().send(0x6704, {0});
            return;
        }
        mLastLocation = std::make_shared<GnssLocation>(location);
        mLastTowMs = tow;
        callback = mCallback;
    }
    if (callback) {
        callback->gnssLocationCb(location);
        std::lock_guard lock(mMutex);
        if (mGnssMeasurementInterface) mGnssMeasurementInterface->noteLocationReported();
    }
}

void Gnss::onSvStatus(const std::vector<uint8_t>& payload) {
    Reader reader(payload);
    if (reader.u8() != 10) return;
    reader.skip(3);
    const auto count = reader.u8();
    if (count > 64) return;
    std::vector<IGnssCallback::GnssSvInfo> satellites;
    const auto loss = readConfigInt("RfPathLossDb_Ap", 5);
    for (int i = 0; i < count; ++i) {
        IGnssCallback::GnssSvInfo satellite;
        satellite.svid = reader.u8();
        const auto constellation = reader.u8();
        satellite.constellation = constellation < 8
                                          ? static_cast<GnssConstellationType>(constellation)
                                          : GnssConstellationType::UNKNOWN;
        satellite.svFlag = reader.u8();
        const float cn0 = reader.f32();
        satellite.cN0Dbhz = std::min(63.0f, cn0 + loss);
        satellite.basebandCN0DbHz = std::min(63.0f, cn0);
        satellite.elevationDegrees = reader.f32();
        satellite.azimuthDegrees = reader.f32();
        satellite.carrierFrequencyHz = reader.f32();
        if (!(satellite.svFlag & 8)) {
            switch (satellite.constellation) {
                case GnssConstellationType::GPS:
                case GnssConstellationType::SBAS:
                case GnssConstellationType::QZSS:
                case GnssConstellationType::GALILEO:
                    satellite.carrierFrequencyHz = 1575420000;
                    break;
                case GnssConstellationType::GLONASS:
                    satellite.carrierFrequencyHz = 1602000000;
                    break;
                case GnssConstellationType::BEIDOU:
                    satellite.carrierFrequencyHz = 1561098000;
                    break;
                case GnssConstellationType::IRNSS:
                    satellite.carrierFrequencyHz = 1176450000;
                    break;
                default:
                    break;
            }
            if (satellite.constellation != GnssConstellationType::UNKNOWN)
                satellite.svFlag |= 8;
        }
        if (cn0 > 0) satellites.push_back(satellite);
    }
    if (!reader.ok()) return;
    std::shared_ptr<IGnssCallback> callback;
    {
        std::lock_guard lock(mMutex);
        mLastSvStatus = satellites;
        if (mIsSvStatusActive && mCallback &&
            (!satellites.empty() || readConfigBool("hidl_reporting_zero_svs", false))) {
            const uint32_t rate = mGnssMeasurementInterface
                                          ? mGnssMeasurementInterface->getSvStatusSamplingRate()
                                          : mIsActive
                                                    ? std::max(1, mPositionMode.minIntervalMs / 1000)
                                                    : 1;
            if (mSvReportCount++ % rate == 0) callback = mCallback;
        }
        if (mGnssAntennaInfo &&
            (!satellites.empty() || readConfigBool("hidl_reporting_zero_svs", false)))
            mGnssAntennaInfo->onSvStatus();
    }
    if (callback) callback->gnssSvStatusCb(satellites);
}

void Gnss::onNmea(const std::vector<uint8_t>& payload) {
    Reader reader(payload);
    const uint32_t time = reader.u32();
    const uint64_t low = reader.u32();
    const uint64_t high = reader.u32();
    const size_t length = reader.u8();
    auto nmea = reader.string(256);
    if (!reader.ok()) return;
    if (nmea.size() > length) nmea.resize(length);
    std::shared_ptr<IGnssCallback> callback;
    {
        std::lock_guard lock(mMutex);
        if (!mIsNmeaActive) return;
        if (time + 3 < mLastNmeaTime) return;
        mLastNmeaTime = time;
        callback = mCallback;
    }
    if (callback) callback->gnssNmeaCb(low | (high << 32), nmea);
}

ScopedAStatus Gnss::start() {
    {
        std::lock_guard lock(mPendingMutex);
        mStartPending = true;
    }
    processPendingCommands();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stop() {
    {
        std::lock_guard lock(mPendingMutex);
        mStartPending = false;
        mStopPending = true;
    }
    processPendingCommands();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::close() {
    std::lock_guard lock(mMutex);
    {
        std::lock_guard pendingLock(mPendingMutex);
        mStartPending = mStopPending = false;
        mAckReceived = true;
        mAckCondition.notify_all();
    }
    mCallback = nullptr;
    GnssBackend::get().send(0x6804, {0});
    if (mIsActive) {
        GnssBackend::get().send(0x6704, {0});
        reportEngineStatus(false);
    }
    mIsActive = false;
    mSensorsPending = false;
    mSensors.stop();
    if (mGnssNavigation) mGnssNavigation->close();
    if (mGnssMeasurementInterface) {
        mGnssMeasurementInterface->setLocationEnabled(false);
        mGnssMeasurementInterface->close();
    }
    if (mGnssAntennaInfo) mGnssAntennaInfo->close();
    if (mGnssBatching) mGnssBatching->cleanup();
    if (mGnssPsds) mGnssPsds->clearCallback();
    if (mCorrections) mCorrections->clearCallback();
    if (mGnssPowerIndication) mGnssPowerIndication->clearCallback();
    if (mGnssGeofence) mGnssGeofence->clearCallback();
    return ScopedAStatus::ok();
}

void Gnss::reportStatus(IGnssCallback::GnssStatusValue status) {
    {
        std::lock_guard lock(mMutex);
        mGnssStatus = status;
    }
    if (auto callback = getCallback()) callback->gnssStatusCb(status);
}

void Gnss::reportEngineStatus(bool active) {
    {
        std::lock_guard lock(mMutex);
        if (mEngineStatusSet && mIsEngineOn == active) return;
        mEngineStatusSet = true;
        mIsEngineOn = active;
    }
    reportStatus(active ? IGnssCallback::GnssStatusValue::ENGINE_ON
                        : IGnssCallback::GnssStatusValue::SESSION_END);
    reportStatus(active ? IGnssCallback::GnssStatusValue::SESSION_BEGIN
                        : IGnssCallback::GnssStatusValue::ENGINE_OFF);
}

ScopedAStatus Gnss::injectTime(int64_t time, int64_t reference, int uncertainty) {
    Writer request;
    request.u32(static_cast<uint64_t>(time) >> 32)
            .u32(time)
            .u32(static_cast<uint64_t>(reference) >> 32)
            .u32(reference)
            .u32(uncertainty);
    GnssBackend::get().send(0x6e04, request.data());
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectLocation(const GnssLocation& location) {
    if (!(location.gnssLocationFlags & GnssLocation::HAS_LAT_LONG)) return ScopedAStatus::ok();
    std::lock_guard lock(mMutex);
    const int64_t seconds = ::android::elapsedRealtimeNano() / 1000000000;
    if (seconds < mLastInjectionSeconds + readConfigInt("loc_injection_interval", 1))
        return ScopedAStatus::ok();
    Writer request;
    request.f64(location.latitudeDegrees)
            .f64(location.longitudeDegrees)
            .f32(location.gnssLocationFlags & GnssLocation::HAS_HORIZONTAL_ACCURACY
                         ? location.horizontalAccuracyMeters
                         : 150000);
    GnssBackend::get().send(0x7b04, request.data());
    mLastInjectionSeconds = seconds;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectBestLocation(const GnssLocation& location) {
    return injectLocation(location);
}

ScopedAStatus Gnss::deleteAidingData(GnssAidingData flags) {
    Writer request;
    std::lock_guard lock(mMutex);
    mAidingData = static_cast<uint16_t>(flags);
    request.u16(mAidingData);
    GnssBackend::get().send(0x6904, request.data());
    return ScopedAStatus::ok();
}

void Gnss::sendPositionMode() {
    Writer request;
    request.u32(static_cast<uint32_t>(mPositionMode.mode))
            .u32(static_cast<uint32_t>(mPositionMode.recurrence))
            .u32(mPositionMode.minIntervalMs)
            .u32(mPositionMode.preferredAccuracyMeters)
            .u32(mPositionMode.preferredTimeMs)
            .u8(mPositionMode.lowPowerMode);
    GnssBackend::get().send(0x6a04, request.data());
}

ScopedAStatus Gnss::setPositionMode(const PositionModeOptions& options) {
    std::lock_guard lock(mMutex);
    mPositionMode = options;
    if (mGnssMeasurementInterface)
        mGnssMeasurementInterface->setLocationInterval(options.minIntervalMs);
    mPositionModeSet = true;
    sendPositionMode();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::startSvStatus() {
    std::lock_guard lock(mMutex);
    mIsSvStatusActive = true;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stopSvStatus() {
    std::lock_guard lock(mMutex);
    mIsSvStatusActive = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::startNmea() {
    std::lock_guard lock(mMutex);
    mIsNmeaActive = true;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stopNmea() {
    std::lock_guard lock(mMutex);
    mIsNmeaActive = false;
    return ScopedAStatus::ok();
}

std::shared_ptr<GnssLocation> Gnss::getLastLocation() const {
    std::lock_guard lock(mMutex);
    return mLastLocation;
}

std::vector<IGnssCallback::GnssSvInfo> Gnss::getLastSvStatus() const {
    std::lock_guard lock(mMutex);
    return mLastSvStatus;
}

void Gnss::ensureMeasurementInterface() {
    if (!mGnssMeasurementInterface) {
        mGnssMeasurementInterface = SharedRefBase::make<GnssMeasurementInterface>();
        mGnssMeasurementInterface->setLocationEnabled(mIsActive);
        mGnssMeasurementInterface->setLocationInterval(mPositionMode.minIntervalMs);
    }
}

#define EXTENSION(method, member, concrete, interface)               \
    ScopedAStatus Gnss::method(std::shared_ptr<interface>* result) { \
        std::lock_guard lock(mMutex);                                \
        if (!member) member = SharedRefBase::make<concrete>();       \
        *result = member;                                            \
        return ScopedAStatus::ok();                                  \
    }
EXTENSION(getExtensionPsds, mGnssPsds, GnssPsds, IGnssPsds)
EXTENSION(getExtensionGnssConfiguration, mGnssConfiguration, GnssConfiguration, IGnssConfiguration)
EXTENSION(getExtensionGnssPowerIndication, mGnssPowerIndication, GnssPowerIndication,
          IGnssPowerIndication)
EXTENSION(getExtensionGnssBatching, mGnssBatching, GnssBatching, IGnssBatching)
EXTENSION(getExtensionGnssGeofence, mGnssGeofence, GnssGeofence, IGnssGeofence)
EXTENSION(getExtensionAGnssRil, mAGnssRil, AGnssRil, IAGnssRil)
EXTENSION(getExtensionGnssAntennaInfo, mGnssAntennaInfo, GnssAntennaInfo, IGnssAntennaInfo)
EXTENSION(getExtensionGnssVisibilityControl, mGnssVisibilityControl,
          visibility_control::GnssVisibilityControl, visibility_control::IGnssVisibilityControl)
EXTENSION(getExtensionMeasurementCorrections, mCorrections,
          measurement_corrections::MeasurementCorrectionsInterface,
          measurement_corrections::IMeasurementCorrectionsInterface)
#undef EXTENSION

ScopedAStatus Gnss::getExtensionAGnss(std::shared_ptr<IAGnss>* result) {
    std::lock_guard lock(mMutex);
    if (!mAGnss) mAGnss = SharedRefBase::make<AGnss>(mAGnssRil);
    *result = mAGnss;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssMeasurement(
        std::shared_ptr<IGnssMeasurementInterface>* result) {
    std::lock_guard lock(mMutex);
    ensureMeasurementInterface();
    *result = mGnssMeasurementInterface;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssNavigationMessage(
        std::shared_ptr<IGnssNavigationMessageInterface>* result) {
    std::lock_guard lock(mMutex);
    ensureMeasurementInterface();
    if (!mGnssNavigation)
        mGnssNavigation =
                SharedRefBase::make<GnssNavigationMessageInterface>(mGnssMeasurementInterface);
    *result = mGnssNavigation;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* result) {
    std::lock_guard lock(mMutex);
    if (!mGnssDebug) mGnssDebug = SharedRefBase::make<GnssDebug>(ref<Gnss>());
    *result = mGnssDebug;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssAssistanceInterface(
        std::shared_ptr<gnss_assistance::IGnssAssistanceInterface>* result) {
    *result = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace aidl::android::hardware::gnss
