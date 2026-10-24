/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnGnssGeofence.h>
#include <array>
#include <map>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssGeofence : public BnGnssGeofence {
  GnssGeofence();
  ~GnssGeofence();
  ndk::ScopedAStatus
  setCallback(const std::shared_ptr<IGnssGeofenceCallback> &callback) override;
  ndk::ScopedAStatus addGeofence(int id, double latitude, double longitude,
                                 double radius, int lastTransition,
                                 int transitions, int responsiveness,
                                 int unknownTimer) override;
  ndk::ScopedAStatus pauseGeofence(int id) override;
  ndk::ScopedAStatus resumeGeofence(int id, int transitions) override;
  ndk::ScopedAStatus removeGeofence(int id) override;
  void clearCallback();
  void onConnected();

private:
  ndk::ScopedAStatus command(uint16_t command, int id, int transitions = 0);
  void response(uint16_t id, const std::vector<uint8_t> &data);
  std::mutex mMutex;
  std::shared_ptr<IGnssGeofenceCallback> mCallback;
  std::array<int, 512> mIds;
  std::map<int, std::vector<uint8_t>> mRequests;
};
} // namespace aidl::android::hardware::gnss
