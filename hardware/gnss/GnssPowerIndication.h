/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssPowerIndication : public BnGnssPowerIndication {
  ndk::ScopedAStatus setCallback(
      const std::shared_ptr<IGnssPowerIndicationCallback> &callback) override;
  ndk::ScopedAStatus requestGnssPowerStats() override;
  void clearCallback();

private:
  std::mutex mMutex;
  std::shared_ptr<IGnssPowerIndicationCallback> mCallback;
};
} // namespace aidl::android::hardware::gnss
