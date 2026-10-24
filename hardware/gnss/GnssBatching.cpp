/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssBatching.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <climits>
#include <utils/SystemClock.h>
namespace aidl::android::hardware::gnss {
namespace {
ndk::ScopedAStatus send(uint16_t id, const std::vector<uint8_t> &data) {
  return GnssBackend::get().send(id, data)
             ? ndk::ScopedAStatus::ok()
             : ndk::ScopedAStatus::fromServiceSpecificError(
                   IGnss::ERROR_GENERIC);
}
} // namespace
GnssBatching::GnssBatching() {
  GnssBackend::get().setHandler(0x7c0a, [this](
                                            const std::vector<uint8_t> &data) {
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
    if (!reader.ok() || remaining > 200)
      return;
    location.gnssLocationFlags =
        0x77 | (location.speedMetersPerSec != 0 ? 0x88 : 0);
    location.elapsedRealtime.flags = ElapsedRealtime::HAS_TIMESTAMP_NS |
                                     ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS;
    location.elapsedRealtime.timestampNs = ::android::elapsedRealtimeNano();
    location.elapsedRealtime.timeUncertaintyNs = 1000;
    std::vector<GnssLocation> locations;
    std::shared_ptr<IGnssBatchingCallback> callback;
    {
      std::lock_guard lock(mMutex);
      if (!mCallback)
        return;
      if (remaining == 200)
        mLocations.clear();
      else
        mLocations.push_back(location);
      if (remaining == 0 || remaining == 200 || mLocations.size() == 200) {
        locations.swap(mLocations);
        callback = mCallback;
      }
    }
    if (callback)
      callback->gnssLocationBatchCb(locations);
  });
}
GnssBatching::~GnssBatching() { GnssBackend::get().setHandler(0x7c0a, {}); }
ndk::ScopedAStatus
GnssBatching::init(const std::shared_ptr<IGnssBatchingCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  mCallback = callback;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssBatching::getBatchSize(int *size) {
  *size = 200;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssBatching::start(const Options &options) {
  const auto period = options.periodNanos / 1000000;
  if (period <= 0 || period > INT_MAX)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  protocol::Writer data;
  data.put<uint8_t>(2);
  data.put<uint32_t>(period);
  data.put<uint8_t>((options.flags & 1) != 0);
  {
    std::lock_guard lock(mMutex);
    if (!mCallback)
      return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    mRequest = data.data();
  }
  return send(0x6604, data.data());
}
ndk::ScopedAStatus GnssBatching::flush() { return send(0x8204, {0}); }
ndk::ScopedAStatus GnssBatching::stop() {
  {
    std::lock_guard lock(mMutex);
    mRequest.clear();
  }
  return send(0x6704, {2});
}
ndk::ScopedAStatus GnssBatching::cleanup() {
  stop();
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
  mLocations.clear();
  return ndk::ScopedAStatus::ok();
}
void GnssBatching::onConnected() {
  std::vector<uint8_t> request;
  {
    std::lock_guard lock(mMutex);
    request = mRequest;
  }
  if (!request.empty())
    GnssBackend::get().send(0x6604, request);
}
} // namespace aidl::android::hardware::gnss
