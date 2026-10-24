/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnAGnss.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
using AGnssType = IAGnssCallback::AGnssType;
struct AGnss : public BnAGnss {
  AGnss();
  ~AGnss();
  ndk::ScopedAStatus
  setCallback(const std::shared_ptr<IAGnssCallback> &callback) override;
  ndk::ScopedAStatus dataConnClosed() override;
  ndk::ScopedAStatus dataConnFailed() override;
  ndk::ScopedAStatus setServer(AGnssType type, const std::string &hostname,
                               int port) override;
  ndk::ScopedAStatus dataConnOpen(int64_t networkHandle, const std::string &apn,
                                  ApnIpType apnIpType) override;
  void clearCallback();
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IAGnssCallback> mCallback;
  std::vector<uint8_t> mServer;
};
} // namespace aidl::android::hardware::gnss
