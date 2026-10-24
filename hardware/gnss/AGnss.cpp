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

#include "AGnss.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include "AGnssRil.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
namespace {
ndk::ScopedAStatus send(uint16_t id, const std::vector<uint8_t>& data) {
    return GnssBackend::get().send(id, data)
                   ? ndk::ScopedAStatus::ok()
                   : ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_GENERIC);
}
}  // namespace

AGnss::AGnss(const std::shared_ptr<AGnssRil>& ril) : mRil(ril) {
    GnssBackend::get().setHandler(0x6b0a, [this](const std::vector<uint8_t>& data) {
        protocol::Reader reader(data);
        auto type = static_cast<IAGnssCallback::AGnssType>(static_cast<uint8_t>(reader.get<uint16_t>()));
        auto status = static_cast<IAGnssCallback::AGnssStatusValue>(static_cast<uint8_t>(reader.get<uint16_t>()));
        std::shared_ptr<IAGnssCallback> callback;
        {
            std::lock_guard lock(mMutex);
            callback = mCallback;
        }
        if (reader.ok() && callback) callback->agnssStatusCb(type, status);
    });
}

AGnss::~AGnss() {
    GnssBackend::get().setHandler(0x6b0a, {});
}

ndk::ScopedAStatus AGnss::setCallback(const std::shared_ptr<IAGnssCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    std::lock_guard lock(mMutex);
    mCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AGnss::dataConnClosed() {
    if (mRil) mRil->setDataConnectionOpen(false);
    return send(0x6601, {0});
}

ndk::ScopedAStatus AGnss::dataConnFailed() {
    if (mRil) mRil->setDataConnectionOpen(false);
    return send(0x6701, {0});
}

ndk::ScopedAStatus AGnss::setServer(AGnssType type, const std::string& hostname, int port) {
    if (port <= 0 || port > 65535)
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    protocol::Writer data;
    data.put<uint32_t>(static_cast<uint32_t>(type));
    data.string(hostname, 128);
    data.put<uint32_t>(port);
    data.put<uint8_t>(0);
    for (int i = 0; i < 3; ++i) data.put<uint32_t>(0);
    {
        std::lock_guard lock(mMutex);
        mServer = data.data();
    }
    return send(0x6801, data.data());
}

ndk::ScopedAStatus AGnss::dataConnOpen(int64_t handle, const std::string& apn, ApnIpType ipType) {
    if (mRil) mRil->setDataConnectionOpen(true);
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
    if (!server.empty()) GnssBackend::get().send(0x6801, server);
}
}  // namespace aidl::android::hardware::gnss
