/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnGnssAntennaInfo.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssAntennaInfo : public BnGnssAntennaInfo {
  GnssAntennaInfo();
  ~GnssAntennaInfo();
  ndk::ScopedAStatus setCallback(
      const std::shared_ptr<IGnssAntennaInfoCallback> &callback) override;
  ndk::ScopedAStatus close() override;
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IGnssAntennaInfoCallback> mCallback;
};
} // namespace aidl::android::hardware::gnss
