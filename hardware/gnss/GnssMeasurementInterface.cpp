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

#define LOG_TAG "GnssMeasIfaceAidl"

#include "GnssMeasurementInterface.h"

#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <log/log.h>
#include <time.h>

namespace aidl::android::hardware::gnss {
namespace {

uint64_t readTime(protocol::Reader &reader) {
  uint64_t high = reader.u32();
  return (high << 32) | reader.u32();
}

uint64_t receivedTime(uint64_t time, uint32_t state,
                      GnssConstellationType constellation) {
  switch (constellation) {
  case GnssConstellationType::GPS:
  case GnssConstellationType::QZSS:
  case GnssConstellationType::IRNSS:
    if (state & (0x8 | 0x4000))
      return time;
    if (state & 0x4)
      return time % 6000000000ULL;
    if (state & 0x2)
      return time % 20000000ULL;
    if (state & 0x1)
      return time % 1000000ULL;
    return 0;
  case GnssConstellationType::GLONASS:
    if (state & (0x80 | 0x8000))
      return time;
    if (state & 0x40)
      return time % 2000000000ULL;
    if (state & 0x2)
      return time % 20000000ULL;
    if (state & 0x20)
      return time % 10000000ULL;
    if (state & 0x1)
      return time % 1000000ULL;
    return 0;
  case GnssConstellationType::BEIDOU:
    if (state & (0x8 | 0x4000))
      return time;
    if (state & 0x4)
      return time % 6000000000ULL;
    if (state & 0x200)
      return time % 600000000ULL;
    if (state & 0x2)
      return time % 20000000ULL;
    if (state & 0x100)
      return time % 2000000ULL;
    if (state & 0x1)
      return time % 1000000ULL;
    return 0;
  case GnssConstellationType::GALILEO:
    if (state & (0x8 | 0x4000))
      return time;
    if (state & 0x1000)
      return time % 2000000000ULL;
    if (state & 0x800)
      return time % 100000000ULL;
    if (state & 0x400)
      return time % 4000000ULL;
    return time;
  case GnssConstellationType::SBAS:
    if (state & 0x2000)
      time %= 1000000000ULL;
    if (state & 0x20)
      time %= 2000000ULL;
    return state & 1 ? time % 1000000ULL : 0;
  default:
    return time;
  }
}

void useDefaultFrequency(GnssMeasurement &measurement) {
  if (measurement.flags & GnssMeasurement::HAS_CARRIER_FREQUENCY)
    return;
  switch (measurement.signalType.constellation) {
  case GnssConstellationType::GPS:
  case GnssConstellationType::SBAS:
  case GnssConstellationType::QZSS:
  case GnssConstellationType::GALILEO:
    measurement.signalType.carrierFrequencyHz = 1575420000;
    break;
  case GnssConstellationType::GLONASS:
    measurement.signalType.carrierFrequencyHz = 1602000000;
    break;
  case GnssConstellationType::BEIDOU:
    measurement.signalType.carrierFrequencyHz = 1561098000;
    break;
  case GnssConstellationType::IRNSS:
    measurement.signalType.carrierFrequencyHz = 1176450000;
    break;
  default:
    return;
  }
  measurement.flags |= GnssMeasurement::HAS_CARRIER_FREQUENCY;
}

} // namespace

GnssMeasurementInterface::GnssMeasurementInterface()
    : mRfPathLossDb(protocol::readConfigInt("RfPathLossDb_Ap", 5)) {
  auto &backend = GnssBackend::get();
  backend.setHandler(0x790a,
                     [this](const auto &payload) { readHeader(payload); });
  backend.setHandler(0x720a,
                     [this](const auto &payload) { readMeasurement(payload); });
}

GnssMeasurementInterface::~GnssMeasurementInterface() {
  GnssBackend::get().setHandler(0x790a, {});
  GnssBackend::get().setHandler(0x720a, {});
  close();
}

ndk::ScopedAStatus GnssMeasurementInterface::setCallback(
    const std::shared_ptr<IGnssMeasurementCallback> &callback,
    bool enableFullTracking, bool enableCorrVecOutputs) {
  Options options;
  options.enableFullTracking = enableFullTracking;
  options.enableCorrVecOutputs = enableCorrVecOutputs;
  options.intervalMs = 1000;
  return setCallbackWithOptions(callback, options);
}

ndk::ScopedAStatus GnssMeasurementInterface::setCallbackWithOptions(
    const std::shared_ptr<IGnssMeasurementCallback> &callback,
    const Options &options) {
  if (callback == nullptr)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  bool wasEnabled = mCallback != nullptr || mNavigationEnabled;
  mCallback = callback;
  mFullTracking = options.enableFullTracking;
  mIntervalMs = options.intervalMs > 0 ? options.intervalMs : 1000;
  mReportedCount = 0;
  mPendingData = nullptr;
  updateRequest(wasEnabled);
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssMeasurementInterface::close() {
  std::lock_guard lock(mMutex);
  bool wasEnabled = mCallback != nullptr || mNavigationEnabled;
  mCallback = nullptr;
  mPendingData = nullptr;
  mIntervalMs = 1000;
  mReportedCount = 0;
  updateRequest(wasEnabled);
  return ndk::ScopedAStatus::ok();
}

void GnssMeasurementInterface::setLocationInterval(int intervalMs) {
  std::lock_guard lock(mMutex);
  mLocationIntervalMs = intervalMs > 0 ? intervalMs : 1000;
}

void GnssMeasurementInterface::setLocationEnabled(bool enabled) {
  std::lock_guard lock(mMutex);
  mLocationEnabled = enabled;
}

void GnssMeasurementInterface::setNavigationEnabled(bool enabled) {
  std::lock_guard lock(mMutex);
  bool wasEnabled = mCallback != nullptr || mNavigationEnabled;
  mNavigationEnabled = enabled;
  updateRequest(wasEnabled);
}

void GnssMeasurementInterface::updateRequest(bool wasEnabled) {
  bool enabled = mCallback != nullptr || mNavigationEnabled;
  if (enabled == wasEnabled)
    return;
  if (enabled) {
    GnssBackend::get().send(0x6604,
                            protocol::Writer().u8(1).u32(0).u8(0).data());
  } else {
    GnssBackend::get().send(0x6704, {1});
  }
}

void GnssMeasurementInterface::onConnected() {
  std::lock_guard lock(mMutex);
  mPendingData = nullptr;
  if (mCallback != nullptr || mNavigationEnabled) {
    GnssBackend::get().send(0x6604,
                            protocol::Writer().u8(1).u32(0).u8(0).data());
  }
}

std::shared_ptr<GnssData> GnssMeasurementInterface::getLastData() const {
  std::lock_guard lock(mMutex);
  if (mLastData == nullptr)
    return nullptr;
  return std::make_shared<GnssData>(*mLastData);
}

void GnssMeasurementInterface::readHeader(const std::vector<uint8_t> &payload) {
  protocol::Reader reader(payload);
  uint8_t type = reader.u8();
  reader.skip(3);
  auto data = std::make_shared<GnssData>();
  data->clock.gnssClockFlags = reader.u16();
  data->clock.leapSecond = reader.get<int16_t>();
  data->clock.timeNs = readTime(reader);
  data->clock.timeUncertaintyNs = reader.f32();
  data->clock.fullBiasNs = readTime(reader);
  data->clock.biasNs = reader.f32();
  data->clock.biasUncertaintyNs = reader.f32();
  data->clock.driftNsps = reader.f32();
  data->clock.driftUncertaintyNsps = reader.f32();
  data->clock.hwClockDiscontinuityCount = reader.u16();
  const size_t expected = reader.u8();
  if (!reader.ok() || (type != 10 && type != 11))
    return;
  data->clock.referenceSignalTypeForIsb.constellation =
      GnssConstellationType::GPS;
  data->clock.referenceSignalTypeForIsb.carrierFrequencyHz = 1575420000;
  data->clock.referenceSignalTypeForIsb.codeType = "C";
  timespec now{};
  clock_gettime(CLOCK_BOOTTIME, &now);
  data->elapsedRealtime.flags = ElapsedRealtime::HAS_TIMESTAMP_NS |
                                ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS;
  data->elapsedRealtime.timestampNs = now.tv_sec * 1000000000LL + now.tv_nsec;
  data->elapsedRealtime.timeUncertaintyNs = 100000;

  std::shared_ptr<GnssData> previous;
  {
    std::lock_guard lock(mMutex);
    if ((type == 10 && mCallback != nullptr) ||
        (type == 11 && mCallback == nullptr && !mNavigationEnabled))
      return;
    previous = std::move(mPendingData);
    mPendingData = std::move(data);
    mPendingData->isFullTracking = mFullTracking;
    mExpectedMeasurements = expected;
    mPendingData->measurements.reserve(expected);
    mPendingData->gnssAgcs.reserve(expected);
  }
  if (previous != nullptr && !previous->measurements.empty())
    reportMeasurement(std::move(previous));
}

void GnssMeasurementInterface::readMeasurement(
    const std::vector<uint8_t> &payload) {
  static constexpr std::array<const char *, 29> codeTypes = {
      "C", "L", "L", "Q", "C", "B", "C", "B", "I", "P", "I", "I", "P", "C", "Q",
      "Q", "Q", "C", "Q", "C", "Z", "B", "L", "L", "X", "Q", "Z", "X", "A"};
  protocol::Reader reader(payload);
  GnssMeasurement measurement;
  measurement.flags = reader.u32() & ~GnssMeasurement::HAS_CORRELATION_VECTOR;
  measurement.svid = reader.u8();
  uint8_t codeType = reader.u8();
  measurement.signalType.codeType =
      codeType < codeTypes.size() ? codeTypes[codeType] : "C";
  measurement.signalType.constellation =
      static_cast<GnssConstellationType>(reader.u8());
  measurement.multipathIndicator =
      static_cast<GnssMultipathIndicator>(reader.u8());
  measurement.state = reader.u16();
  measurement.receivedSvTimeInNs =
      receivedTime(readTime(reader), measurement.state,
                   measurement.signalType.constellation);
  measurement.receivedSvTimeUncertaintyInNs = readTime(reader);
  measurement.basebandCN0DbHz = reader.f32();
  measurement.antennaCN0DbHz = measurement.basebandCN0DbHz + mRfPathLossDb;
  measurement.pseudorangeRateMps = reader.f32();
  measurement.pseudorangeRateUncertaintyMps = reader.f32();
  measurement.accumulatedDeltaRangeState = reader.u16();
  measurement.accumulatedDeltaRangeM = reader.f64();
  measurement.accumulatedDeltaRangeUncertaintyM =
      std::abs(measurement.pseudorangeRateMps) * 0.7071;
  const float frequency = reader.f32();
  measurement.signalType.carrierFrequencyHz = frequency;
  reader.skip(12);
  measurement.snrDb = reader.f32();
  measurement.agcLevelDb = reader.f32();
  measurement.fullInterSignalBiasNs = reader.f64();
  measurement.fullInterSignalBiasUncertaintyNs = reader.f64();
  measurement.satelliteInterSignalBiasNs = reader.f64();
  measurement.satelliteInterSignalBiasUncertaintyNs = reader.f64();
  auto &pvt = measurement.satellitePvt;
  pvt.flags = reader.u16();
  pvt.tropoDelayMeters = reader.f32();
  pvt.ionoDelayMeters = reader.f64();
  pvt.satPosEcef.posXMeters = reader.f64();
  pvt.satPosEcef.posYMeters = reader.f64();
  pvt.satPosEcef.posZMeters = reader.f64();
  reader.f32();
  pvt.satPosEcef.ureMeters = 300;
  pvt.satVelEcef.velXMps = reader.f64();
  pvt.satVelEcef.velYMps = reader.f64();
  pvt.satVelEcef.velZMps = reader.f64();
  reader.f32();
  pvt.satVelEcef.ureRateMps = 1;
  pvt.satClockInfo.satHardwareCodeBiasMeters = reader.f64();
  pvt.satClockInfo.satTimeCorrectionMeters = reader.f64();
  pvt.satClockInfo.satClkDriftMps = reader.f32();
  uint16_t remaining = reader.u16();
  if (!reader.ok())
    return;
  useDefaultFrequency(measurement);
  GnssData::GnssAgc agc;
  agc.agcLevelDb = measurement.agcLevelDb;
  agc.constellation = measurement.signalType.constellation;
  if (std::isfinite(frequency) && frequency >= 0 &&
      frequency < double(INT64_MAX)) {
    agc.carrierFrequencyHz = static_cast<int64_t>(frequency);
  }
  std::shared_ptr<GnssData> completed;
  {
    std::lock_guard lock(mMutex);
    if (mPendingData == nullptr ||
        mPendingData->measurements.size() >= mExpectedMeasurements)
      return;
    mPendingData->measurements.push_back(std::move(measurement));
    mPendingData->gnssAgcs.push_back(std::move(agc));
    if (remaining == 0 ||
        mPendingData->measurements.size() == mExpectedMeasurements) {
      completed = std::move(mPendingData);
    }
  }
  if (completed != nullptr)
    reportMeasurement(std::move(completed));
}

void GnssMeasurementInterface::reportMeasurement(
    std::shared_ptr<GnssData> data) {
  std::shared_ptr<IGnssMeasurementCallback> callback;
  {
    std::lock_guard lock(mMutex);
    if (data->measurements.empty())
      return;
    mLastData = data;
    int interval = mIntervalMs;
    if (mLocationEnabled)
      interval = std::min(interval, mLocationIntervalMs);
    uint64_t sampling = std::max(1, interval / 1000);
    if (mReportedCount++ % sampling == 0)
      callback = mCallback;
  }
  if (callback != nullptr) {
    auto status = callback->gnssMeasurementCb(*data);
    if (!status.isOk())
      ALOGE("gnssMeasurementCb failed");
  }
}

} // namespace aidl::android::hardware::gnss
