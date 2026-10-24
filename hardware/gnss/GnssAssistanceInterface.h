/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/gnss_assistance/BnGnssAssistanceInterface.h>
namespace aidl::android::hardware::gnss::gnss_assistance {
struct GnssAssistanceInterface : public BnGnssAssistanceInterface {
  ndk::ScopedAStatus setCallback(
      const std::shared_ptr<IGnssAssistanceCallback> &callback) override;
  ndk::ScopedAStatus
  injectGnssAssistance(const GnssAssistance &assistance) override;
};
} // namespace aidl::android::hardware::gnss::gnss_assistance
