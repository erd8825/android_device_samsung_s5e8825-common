/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssAssistanceInterface.h"
namespace aidl::android::hardware::gnss::gnss_assistance {
ndk::ScopedAStatus GnssAssistanceInterface::setCallback(
    const std::shared_ptr<IGnssAssistanceCallback> &) {
  return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
ndk::ScopedAStatus
GnssAssistanceInterface::injectGnssAssistance(const GnssAssistance &) {
  return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
} // namespace aidl::android::hardware::gnss::gnss_assistance
