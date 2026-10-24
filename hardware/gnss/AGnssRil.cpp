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

#include "AGnssRil.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <algorithm>
namespace aidl::android::hardware::gnss {
namespace {
ndk::ScopedAStatus send(uint16_t id, const std::vector<uint8_t> &data) {
  return GnssBackend::get().send(id, data)
             ? ndk::ScopedAStatus::ok()
             : ndk::ScopedAStatus::fromServiceSpecificError(
                   IGnss::ERROR_GENERIC);
}
} // namespace
AGnssRil::AGnssRil() : mNetworkState(11, 0), mNetworkAvailable(3, 0) {
  GnssBackend::get().setHandler(0x690a, [this](const std::vector<uint8_t> &) {
    std::shared_ptr<IAGnssRilCallback> callback;
    {
      std::lock_guard lock(mMutex);
      callback = mCallback;
    }
    if (callback)
      callback->requestSetIdCb(1);
  });
  GnssBackend::get().setHandler(0x6a0a,
                                [this](const std::vector<uint8_t> &data) {
                                  std::shared_ptr<IAGnssRilCallback> callback;
                                  {
                                    std::lock_guard lock(mMutex);
                                    callback = mCallback;
                                  }
                                  if (!data.empty() && data[0] == 0 && callback)
                                    callback->requestRefLocCb();
                                });
}
AGnssRil::~AGnssRil() {
  GnssBackend::get().setHandler(0x690a, {});
  GnssBackend::get().setHandler(0x6a0a, {});
}
ndk::ScopedAStatus
AGnssRil::setCallback(const std::shared_ptr<IAGnssRilCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  mCallback = callback;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus AGnssRil::setRefLocation(const AGnssRefLocation &location) {
  const auto &cell = location.cellID;
  protocol::Writer data;
  data.put<uint16_t>(static_cast<uint16_t>(location.type));
  data.put<uint16_t>(cell.mcc);
  data.put<uint16_t>(cell.mnc);
  data.put<uint16_t>(0);
  data.put<uint16_t>(cell.lac);
  data.put<uint16_t>(cell.lac >> 16);
  data.put<uint32_t>(static_cast<uint64_t>(cell.cid) >> 32);
  data.put<uint8_t>(static_cast<uint32_t>(cell.cid));
  data.put<uint8_t>(static_cast<uint32_t>(cell.cid) >> 8);
  data.put<uint8_t>(static_cast<uint32_t>(cell.cid) >> 16);
  data.put<uint8_t>(static_cast<uint32_t>(cell.cid) >> 24);
  data.put<uint8_t>(cell.tac);
  data.put<uint8_t>(cell.tac >> 8);
  return send(0x6a03, data.data());
}
ndk::ScopedAStatus AGnssRil::setSetId(SetIdType type,
                                      const std::string &setid) {
  if (type != SetIdType::IMSI || setid.size() != 15)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  protocol::Writer data;
  data.put<uint16_t>(static_cast<uint16_t>(type));
  data.string(setid, 16);
  return send(0x6603, data.data());
}
ndk::ScopedAStatus
AGnssRil::updateNetworkState(const NetworkAttributes &attributes) {
  protocol::Writer state, available;
  {
    std::lock_guard lock(mMutex);
    const bool wifi = (attributes.capabilities & 1) != 0;
    (wifi ? mWifiConnected : mCellConnected) = attributes.isConnected;
    const bool connected = mWifiConnected || mCellConnected;
    const int type =
        attributes.isConnected ? wifi : !mCellConnected && mWifiConnected;
    state.put<int16_t>(connected);
    state.put<int16_t>(type);
    state.put<int16_t>((attributes.capabilities & 2) == 0);
    state.put<uint32_t>(0);
    state.string("", 1);
    available.put<int16_t>(connected);
    available.string("", 101);
    mNetworkState = state.data();
    mNetworkAvailable = available.data();
  }
  auto status = send(0x6803, state.data());
  if (!status.isOk())
    return status;
  return send(0x6903, available.data());
}
ndk::ScopedAStatus
AGnssRil::injectNiSuplMessageData(const std::vector<uint8_t> &message,
                                  int slotIndex) {
  if (message.empty() || slotIndex < 0)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  protocol::Writer data;
  auto size = std::min<size_t>(message.size(), 1535);
  data.put<uint32_t>(size);
  data.bytes(message.data(), size);
  return send(0x6703, data.data());
}
void AGnssRil::clearCallback() {
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
}
void AGnssRil::onConnected() {
  std::vector<uint8_t> state, available;
  {
    std::lock_guard lock(mMutex);
    state = mNetworkState;
    available = mNetworkAvailable;
  }
  if (!state.empty())
    GnssBackend::get().send(0x6803, state);
  if (!available.empty())
    GnssBackend::get().send(0x6903, available);
}
} // namespace aidl::android::hardware::gnss
