/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnAGnssRil.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct AGnssRil : public BnAGnssRil {
  AGnssRil();
  ~AGnssRil();
  ndk::ScopedAStatus
  setCallback(const std::shared_ptr<IAGnssRilCallback> &callback) override;
  ndk::ScopedAStatus setRefLocation(const AGnssRefLocation &location) override;
  ndk::ScopedAStatus setSetId(SetIdType type,
                              const std::string &setid) override;
  ndk::ScopedAStatus
  updateNetworkState(const NetworkAttributes &attributes) override;
  ndk::ScopedAStatus injectNiSuplMessageData(const std::vector<uint8_t> &data,
                                             int slotIndex) override;
  void clearCallback();
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IAGnssRilCallback> mCallback;
  bool mCellConnected = false;
  bool mWifiConnected = false;
  std::vector<uint8_t> mNetworkState;
  std::vector<uint8_t> mNetworkAvailable;
};
} // namespace aidl::android::hardware::gnss
