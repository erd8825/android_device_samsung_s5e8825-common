/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssPowerIndication.h"
namespace aidl::android::hardware::gnss {
ndk::ScopedAStatus GnssPowerIndication::setCallback(
    const std::shared_ptr<IGnssPowerIndicationCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  {
    std::lock_guard lock(mMutex);
    mCallback = callback;
  }
  callback->setCapabilitiesCb(0x3f);
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssPowerIndication::requestGnssPowerStats() {
  return ndk::ScopedAStatus::ok();
}
void GnssPowerIndication::clearCallback() {
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
}
} // namespace aidl::android::hardware::gnss
