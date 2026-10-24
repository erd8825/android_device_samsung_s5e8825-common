/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssNavigationMessageInterface.h"
#include "GnssBackend.h"
#include "GnssMeasurementInterface.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
GnssNavigationMessageInterface::GnssNavigationMessageInterface(
    const std::shared_ptr<GnssMeasurementInterface> &measurement)
    : mMeasurement(measurement) {
  GnssBackend::get().setHandler(0x730a, [this](
                                            const std::vector<uint8_t> &data) {
    protocol::Reader reader(data);
    auto id = reader.get<uint8_t>();
    reader.get<uint8_t>();
    reader.get<uint8_t>();
    reader.get<uint8_t>();
    IGnssNavigationMessageCallback::GnssNavigationMessage message;
    message.svid = reader.get<uint8_t>();
    message.type = static_cast<decltype(message.type)>(reader.get<uint16_t>());
    message.status = reader.get<uint16_t>();
    message.messageId = reader.get<int16_t>();
    message.submessageId = reader.get<uint16_t>();
    auto length = reader.get<uint8_t>();
    if (length > 40)
      return;
    message.data = reader.bytes(length);
    if (id != 11 || message.messageId == 0 ||
        (message.submessageId == 0 && static_cast<int>(message.type) != 0x601))
      return;
    std::shared_ptr<IGnssNavigationMessageCallback> callback;
    {
      std::lock_guard lock(mMutex);
      callback = mCallback;
    }
    if (reader.ok() && callback)
      callback->gnssNavigationMessageCb(message);
  });
}
GnssNavigationMessageInterface::~GnssNavigationMessageInterface() {
  GnssBackend::get().setHandler(0x730a, {});
}
ndk::ScopedAStatus GnssNavigationMessageInterface::setCallback(
    const std::shared_ptr<IGnssNavigationMessageCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  {
    std::lock_guard lock(mMutex);
    mCallback = callback;
  }
  mMeasurement->setNavigationEnabled(true);
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssNavigationMessageInterface::close() {
  {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
  }
  mMeasurement->setNavigationEnabled(false);
  return ndk::ScopedAStatus::ok();
}
void GnssNavigationMessageInterface::onConnected() {
  bool enabled;
  {
    std::lock_guard lock(mMutex);
    enabled = mCallback != nullptr;
  }
  if (enabled)
    mMeasurement->setNavigationEnabled(true);
}
} // namespace aidl::android::hardware::gnss
