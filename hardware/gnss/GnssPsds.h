/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnGnssPsds.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssPsds : public BnGnssPsds {
  GnssPsds();
  ~GnssPsds();
  ndk::ScopedAStatus
  setCallback(const std::shared_ptr<IGnssPsdsCallback> &callback) override;
  ndk::ScopedAStatus injectPsdsData(PsdsType type,
                                    const std::vector<uint8_t> &data) override;
  void clearCallback();

private:
  std::mutex mMutex;
  std::shared_ptr<IGnssPsdsCallback> mCallback;
};
} // namespace aidl::android::hardware::gnss
