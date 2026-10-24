/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/visibility_control/BnGnssVisibilityControl.h>
#include <mutex>
namespace aidl::android::hardware::gnss::visibility_control {
struct GnssVisibilityControl : public BnGnssVisibilityControl {
  GnssVisibilityControl();
  ~GnssVisibilityControl();
  ndk::ScopedAStatus
  enableNfwLocationAccess(const std::vector<std::string> &packages) override;
  ndk::ScopedAStatus setCallback(
      const std::shared_ptr<IGnssVisibilityControlCallback> &callback) override;
  void clearCallback();
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IGnssVisibilityControlCallback> mCallback;
  std::vector<uint8_t> mPackages;
};
} // namespace aidl::android::hardware::gnss::visibility_control
