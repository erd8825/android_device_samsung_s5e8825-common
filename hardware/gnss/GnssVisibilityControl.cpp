/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "GnssVisibilityControl.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <algorithm>
namespace aidl::android::hardware::gnss::visibility_control {
GnssVisibilityControl::GnssVisibilityControl() {
  GnssBackend::get().setHandler(0x0117, [this](
                                            const std::vector<uint8_t> &data) {
    protocol::Reader reader(data);
    IGnssVisibilityControlCallback::NfwNotification notification;
    notification.proxyAppPackageName = reader.string(64);
    notification.protocolStack =
        static_cast<decltype(notification.protocolStack)>(
            reader.get<uint8_t>());
    notification.otherProtocolStackName = reader.string(64);
    notification.requestor =
        static_cast<decltype(notification.requestor)>(reader.get<uint8_t>());
    notification.requestorId = reader.string(64);
    notification.responseType =
        static_cast<decltype(notification.responseType)>(reader.get<uint8_t>());
    notification.inEmergencyMode = reader.get<uint8_t>() != 0;
    notification.isCachedLocation = reader.get<uint8_t>() != 0;
    std::shared_ptr<IGnssVisibilityControlCallback> callback;
    {
      std::lock_guard lock(mMutex);
      callback = mCallback;
    }
    if (reader.ok() && callback)
      callback->nfwNotifyCb(notification);
  });
  GnssBackend::get().setHandler(0x7e04, [this](const std::vector<uint8_t> &) {
    std::shared_ptr<IGnssVisibilityControlCallback> callback;
    {
      std::lock_guard lock(mMutex);
      callback = mCallback;
    }
    bool emergency = false;
    if (callback && callback->isInEmergencySession(&emergency).isOk())
      GnssBackend::get().send(0x7f04, {static_cast<uint8_t>(emergency)});
  });
}
GnssVisibilityControl::~GnssVisibilityControl() {
  GnssBackend::get().setHandler(0x0117, {});
  GnssBackend::get().setHandler(0x7e04, {});
}
ndk::ScopedAStatus GnssVisibilityControl::setCallback(
    const std::shared_ptr<IGnssVisibilityControlCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  mCallback = callback;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssVisibilityControl::enableNfwLocationAccess(
    const std::vector<std::string> &packages) {
  protocol::Writer data;
  auto count = std::min<size_t>(8, packages.size());
  data.put<uint32_t>(count);
  for (size_t i = 0; i < count; ++i)
    data.string(packages[i], 64);
  {
    std::lock_guard lock(mMutex);
    mPackages = data.data();
  }
  return GnssBackend::get().send(0x7d04, data.data())
             ? ndk::ScopedAStatus::ok()
             : ndk::ScopedAStatus::fromServiceSpecificError(
                   IGnss::ERROR_GENERIC);
}
void GnssVisibilityControl::clearCallback() {
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
}
void GnssVisibilityControl::onConnected() {
  std::vector<uint8_t> packages;
  {
    std::lock_guard lock(mMutex);
    packages = mPackages;
  }
  if (!packages.empty())
    GnssBackend::get().send(0x7d04, packages);
}
} // namespace aidl::android::hardware::gnss::visibility_control
