/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
