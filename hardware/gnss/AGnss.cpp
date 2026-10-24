/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AGnss.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
namespace aidl::android::hardware::gnss {
namespace {
ndk::ScopedAStatus send(uint16_t id, const std::vector<uint8_t> &data) {
  return GnssBackend::get().send(id, data)
             ? ndk::ScopedAStatus::ok()
             : ndk::ScopedAStatus::fromServiceSpecificError(
                   IGnss::ERROR_GENERIC);
}
} // namespace
AGnss::AGnss() {
  GnssBackend::get().setHandler(0x6b0a, [this](
                                            const std::vector<uint8_t> &data) {
    protocol::Reader reader(data);
    auto type = static_cast<IAGnssCallback::AGnssType>(reader.get<uint16_t>());
    auto status =
        static_cast<IAGnssCallback::AGnssStatusValue>(reader.get<uint16_t>());
    std::shared_ptr<IAGnssCallback> callback;
    {
      std::lock_guard lock(mMutex);
      callback = mCallback;
    }
    if (reader.ok() && callback)
      callback->agnssStatusCb(type, status);
  });
}
AGnss::~AGnss() { GnssBackend::get().setHandler(0x6b0a, {}); }
ndk::ScopedAStatus
AGnss::setCallback(const std::shared_ptr<IAGnssCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  mCallback = callback;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus AGnss::dataConnClosed() { return send(0x6601, {0}); }
ndk::ScopedAStatus AGnss::dataConnFailed() { return send(0x6701, {0}); }
ndk::ScopedAStatus AGnss::setServer(AGnssType type, const std::string &hostname,
                                    int port) {
  if (port <= 0 || port > 65535)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  protocol::Writer data;
  data.put<uint32_t>(static_cast<uint32_t>(type));
  data.string(hostname, 128);
  data.put<uint32_t>(port);
  data.put<uint8_t>(0);
  for (int i = 0; i < 3; ++i)
    data.put<uint32_t>(0);
  {
    std::lock_guard lock(mMutex);
    mServer = data.data();
  }
  return send(0x6801, data.data());
}
ndk::ScopedAStatus AGnss::dataConnOpen(int64_t handle, const std::string &apn,
                                       ApnIpType ipType) {
  protocol::Writer data;
  data.put<uint8_t>(2);
  data.put<uint32_t>(handle);
  data.put<uint32_t>(static_cast<uint64_t>(handle) >> 32);
  data.string(apn, 128);
  data.put<uint16_t>(static_cast<uint16_t>(ipType));
  return send(0x6901, data.data());
}
void AGnss::clearCallback() {
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
}
void AGnss::onConnected() {
  std::vector<uint8_t> server;
  {
    std::lock_guard lock(mMutex);
    server = mServer;
  }
  if (!server.empty())
    GnssBackend::get().send(0x6801, server);
}
} // namespace aidl::android::hardware::gnss
