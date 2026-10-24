/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssGeofence.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <algorithm>
#include <cmath>
namespace aidl::android::hardware::gnss {
namespace {
GnssLocation readLocation(protocol::Reader &reader) {
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
} // namespace
GnssGeofence::GnssGeofence() {
  mIds.fill(-1);
  for (auto id : {0x0312, 0x0412, 0x0512, 0x0612})
    GnssBackend::get().setHandler(
        id,
        [this, id](const std::vector<uint8_t> &data) { response(id, data); });
  GnssBackend::get().setHandler(
      0x0112, [this](const std::vector<uint8_t> &data) {
        protocol::Reader reader(data);
        auto location = readLocation(reader);
        auto masks = reader.bytes(192);
        if (!reader.ok())
          return;
        std::shared_ptr<IGnssGeofenceCallback> callback;
        std::array<int, 512> ids;
        {
          std::lock_guard lock(mMutex);
          callback = mCallback;
          ids = mIds;
        }
        if (!callback)
          return;
        for (size_t i = 0; i < ids.size(); ++i) {
          int transition = 0;
          for (int j = 0; j < 3; ++j)
            if (masks[i / 8 * 3 + j] & (1 << (i % 8)))
              transition = 1 << j;
          if (ids[i] != -1 && transition)
            callback->gnssGeofenceTransitionCb(ids[i], location, transition,
                                               location.timestampMillis);
        }
      });
  GnssBackend::get().setHandler(
      0x0212, [this](const std::vector<uint8_t> &data) {
        protocol::Reader reader(data);
        auto available = reader.get<uint32_t>();
        auto location = readLocation(reader);
        if (available != 2)
          location = GnssLocation{};
        std::shared_ptr<IGnssGeofenceCallback> callback;
        {
          std::lock_guard lock(mMutex);
          callback = mCallback;
        }
        if (reader.ok() && callback)
          callback->gnssGeofenceStatusCb(available, location);
      });
}
GnssGeofence::~GnssGeofence() {
  for (auto id : {0x0112, 0x0212, 0x0312, 0x0412, 0x0512, 0x0612})
    GnssBackend::get().setHandler(id, {});
}
ndk::ScopedAStatus GnssGeofence::setCallback(
    const std::shared_ptr<IGnssGeofenceCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  mCallback = callback;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus
GnssGeofence::addGeofence(int id, double latitude, double longitude,
                          double radius, int lastTransition, int transitions,
                          int responsiveness, int unknownTimer) {
  if (id == -1 || !std::isfinite(latitude) || std::abs(latitude) > 90 ||
      !std::isfinite(longitude) || std::abs(longitude) > 180 ||
      !std::isfinite(radius) || radius <= 0 || radius > 42949672)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::shared_ptr<IGnssGeofenceCallback> callback;
  int error = 0;
  int slot = -1;
  {
    std::lock_guard lock(mMutex);
    callback = mCallback;
    if (!callback)
      return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    if (!protocol::readConfigInt("gf_enabled", 0))
      error = IGnssGeofenceCallback::ERROR_GENERIC;
    else if ((transitions & ~7) != 0 || transitions == 0)
      error = IGnssGeofenceCallback::ERROR_INVALID_TRANSITION;
    else if (std::find(mIds.begin(), mIds.end(), id) != mIds.end())
      error = IGnssGeofenceCallback::ERROR_ID_EXISTS;
    else {
      const unsigned hash = hashId(id);
      for (unsigned i = 0; i < mIds.size(); ++i) {
        const unsigned index = (hash + i) % mIds.size();
        if (mIds[index] == -1) {
          slot = index;
          mIds[index] = id;
          break;
        }
      }
      if (slot == -1)
        error = IGnssGeofenceCallback::ERROR_TOO_MANY_GEOFENCES;
    }
  }
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
  if (error) {
    callback->gnssGeofenceAddCb(id, error);
    return ndk::ScopedAStatus::ok();
  }
  protocol::Writer data;
  data.put<uint32_t>(slot);
  data.put<double>(latitude);
  data.put<double>(longitude);
  data.put<float>(static_cast<float>(static_cast<uint32_t>(radius * 100)));
  data.put<uint8_t>(lastTransition);
  data.put<uint8_t>(transitions);
  data.put<uint32_t>(responsiveness);
  data.put<uint32_t>(unknownTimer);
  {
    std::lock_guard lock(mMutex);
    mRequests[slot] = data.data();
  }
  if (!GnssBackend::get().send(0x0110, data.data())) {
    {
      std::lock_guard lock(mMutex);
      mIds[slot] = -1;
      mRequests.erase(slot);
    }
    callback->gnssGeofenceAddCb(id, IGnssGeofenceCallback::ERROR_GENERIC);
  }
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssGeofence::command(uint16_t commandId, int id,
                                         int transitions) {
  if (id == -1)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::shared_ptr<IGnssGeofenceCallback> callback;
  int slot = -1;
  {
    std::lock_guard lock(mMutex);
    callback = mCallback;
    for (size_t i = 0; i < mIds.size(); ++i)
      if (mIds[i] == id) {
        slot = i;
        break;
      }
  }
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
  int error = slot == -1 ? IGnssGeofenceCallback::ERROR_ID_UNKNOWN : 0;
  if (!protocol::readConfigInt("gf_enabled", 0))
    error = IGnssGeofenceCallback::ERROR_GENERIC;
  protocol::Writer data;
  data.put<uint32_t>(slot);
  if (commandId == 0x0310)
    data.put<uint8_t>(transitions);
  if (!error && !GnssBackend::get().send(commandId, data.data()))
    error = IGnssGeofenceCallback::ERROR_GENERIC;
  if (error) {
    if (commandId == 0x0210)
      callback->gnssGeofencePauseCb(id, error);
    if (commandId == 0x0310)
      callback->gnssGeofenceResumeCb(id, error);
    if (commandId == 0x0410)
      callback->gnssGeofenceRemoveCb(id, error);
  }
  return ndk::ScopedAStatus::ok();
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
void GnssGeofence::response(uint16_t commandId,
                            const std::vector<uint8_t> &data) {
  protocol::Reader reader(data);
  auto slot = reader.get<uint32_t>();
  auto status = reader.get<int32_t>();
  if (!reader.ok() || slot >= mIds.size())
    return;
  int id;
  std::shared_ptr<IGnssGeofenceCallback> callback;
  {
    std::lock_guard lock(mMutex);
    id = mIds[slot];
    callback = mCallback;
    if (commandId == 0x0412 || (commandId == 0x0312 && status != 0)) {
      mIds[slot] = -1;
      mRequests.erase(slot);
    }
  }
  if (id == -1 || !callback)
    return;
  if (commandId == 0x0312)
    callback->gnssGeofenceAddCb(id, status);
  if (commandId == 0x0412)
    callback->gnssGeofenceRemoveCb(id, status);
  if (commandId == 0x0512)
    callback->gnssGeofencePauseCb(id, status);
  if (commandId == 0x0612)
    callback->gnssGeofenceResumeCb(id, status);
}
void GnssGeofence::clearCallback() {
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
  mIds.fill(-1);
  mRequests.clear();
}
void GnssGeofence::onConnected() {
  std::map<int, std::vector<uint8_t>> requests;
  {
    std::lock_guard lock(mMutex);
    requests = mRequests;
  }
  for (const auto &[slot, data] : requests)
    GnssBackend::get().send(0x0110, data);
}
} // namespace aidl::android::hardware::gnss
