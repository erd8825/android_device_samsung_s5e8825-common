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

#include "GnssNavigationMessageInterface.h"
#include "GnssBackend.h"
#include "GnssMeasurementInterface.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
GnssNavigationMessageInterface::GnssNavigationMessageInterface(
        const std::shared_ptr<GnssMeasurementInterface>& measurement)
    : mMeasurement(measurement) {
    GnssBackend::get().setHandler(0x730a, [this](const std::vector<uint8_t>& data) {
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
        if (length > 40) return;
        message.data = reader.bytes(length);
        if (id != 11 || message.messageId == 0 ||
            (message.submessageId == 0 && static_cast<int>(message.type) != 0x601))
            return;
        if (!reader.ok() || !mMeasurement->isRawSessionEnabled()) return;
        GnssBackend::get().postCallback([this, message = std::move(message)] {
            std::shared_ptr<IGnssNavigationMessageCallback> callback;
            {
                std::lock_guard lock(mMutex);
                callback = mCallback;
            }
            if (callback) callback->gnssNavigationMessageCb(message);
        });
    });
}

GnssNavigationMessageInterface::~GnssNavigationMessageInterface() {
    GnssBackend::get().setHandler(0x730a, {});
}

ndk::ScopedAStatus GnssNavigationMessageInterface::setCallback(
        const std::shared_ptr<IGnssNavigationMessageCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(-38);
    {
        std::lock_guard lock(mMutex);
        if (mCallback) return ndk::ScopedAStatus::ok();
        mCallback = callback;
    }
    mMeasurement->setNavigationEnabled(true);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssNavigationMessageInterface::close() {
    clearCallback();
    mMeasurement->setNavigationEnabled(false);
    return ndk::ScopedAStatus::ok();
}

void GnssNavigationMessageInterface::clearCallback() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
}

void GnssNavigationMessageInterface::onConnected() {
    bool enabled;
    {
        std::lock_guard lock(mMutex);
        enabled = mCallback != nullptr;
    }
    if (enabled) mMeasurement->setNavigationEnabled(true);
}
}  // namespace aidl::android::hardware::gnss
