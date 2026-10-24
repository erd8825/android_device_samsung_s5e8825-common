/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnGnssBatching.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssBatching : public BnGnssBatching {
  GnssBatching();
  ~GnssBatching();
  ndk::ScopedAStatus
  init(const std::shared_ptr<IGnssBatchingCallback> &callback) override;
  ndk::ScopedAStatus getBatchSize(int *size) override;
  ndk::ScopedAStatus start(const Options &options) override;
  ndk::ScopedAStatus flush() override;
  ndk::ScopedAStatus stop() override;
  ndk::ScopedAStatus cleanup() override;
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IGnssBatchingCallback> mCallback;
  std::vector<uint8_t> mRequest;
  std::vector<GnssLocation> mLocations;
};
} // namespace aidl::android::hardware::gnss
