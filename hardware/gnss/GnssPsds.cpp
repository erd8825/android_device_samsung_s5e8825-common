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

#include "GnssPsds.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <android-base/file.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
GnssPsds::GnssPsds() {
    GnssBackend::get().setHandler(0x010b, [this](const std::vector<uint8_t>&) {
        std::shared_ptr<IGnssPsdsCallback> callback;
        {
            std::lock_guard lock(mMutex);
            callback = mCallback;
        }
        if (callback) callback->downloadRequestCb(PsdsType::LONG_TERM);
    });
}

GnssPsds::~GnssPsds() {
    GnssBackend::get().setHandler(0x010b, {});
}

ndk::ScopedAStatus GnssPsds::setCallback(const std::shared_ptr<IGnssPsdsCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    std::lock_guard lock(mMutex);
    mCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssPsds::injectPsdsData(PsdsType type, const std::vector<uint8_t>& data) {
    if (data.empty() ||
        (type != PsdsType::LONG_TERM && type != PsdsType::NORMAL && type != PsdsType::REALTIME))
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    if (type != PsdsType::LONG_TERM) return ndk::ScopedAStatus::ok();
    std::lock_guard lock(mMutex);
    const auto directory =
            protocol::readConfigString("gnss_data_dir", "/data/vendor/gps") + "/sgee";
    const auto filename = directory + "/xtraee.bin";
    if (mkdir(directory.c_str(), 0770) != 0 && errno != EEXIST)
        return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_GENERIC);
    ::android::base::unique_fd fd(
            open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0660));
    if (fd < 0 || !::android::base::WriteFully(fd, data.data(), data.size()))
        return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_GENERIC);
    return GnssBackend::get().send(0x0106, {0})
                   ? ndk::ScopedAStatus::ok()
                   : ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_GENERIC);
}

void GnssPsds::clearCallback() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
}
}  // namespace aidl::android::hardware::gnss
