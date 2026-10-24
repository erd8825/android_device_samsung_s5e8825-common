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
#include <sys/stat.h>
#include <cstdio>
#include "GnssBackend.h"
#include "GnssProtocol.h"
namespace aidl::android::hardware::gnss {
GnssPsds::GnssPsds() {
    GnssBackend::get().setHandler(0x010b, [this](const std::vector<uint8_t>&) {
        GnssBackend::get().postCallback([this] {
            std::shared_ptr<IGnssPsdsCallback> callback;
            {
                std::lock_guard lock(mMutex);
                callback = mCallback;
            }
            if (callback) callback->downloadRequestCb(PsdsType::LONG_TERM);
        });
    });
}

GnssPsds::~GnssPsds() {
    GnssBackend::get().setHandler(0x010b, {});
}

ndk::ScopedAStatus GnssPsds::setCallback(const std::shared_ptr<IGnssPsdsCallback>& callback) {
    if (!callback) return ndk::ScopedAStatus::fromExceptionCode(-38);
    std::lock_guard lock(mMutex);
    mCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssPsds::injectPsdsData(PsdsType type, const std::vector<uint8_t>& data) {
    if (data.empty() ||
        (type != PsdsType::LONG_TERM && type != PsdsType::NORMAL && type != PsdsType::REALTIME))
        return ndk::ScopedAStatus::fromExceptionCode(-22);
    if (type != PsdsType::LONG_TERM) return ndk::ScopedAStatus::ok();
    std::lock_guard lock(mMutex);
    char directory[128];
    char filename[128];
    std::snprintf(directory, sizeof(directory), "%s/sgee",
                  protocol::readConfigString("gnss_data_dir", "/data/vendor/gps").c_str());
    std::snprintf(filename, sizeof(filename), "%s/xtraee.bin", directory);
    const auto mask = umask(0);
    struct stat info;
    if (stat(directory, &info) == -1) {
        mkdir(directory, 0775);
        umask(mask);
    }
    std::remove(filename);
    bool written = false;
    if (FILE* output = std::fopen(filename, "wb")) {
        const auto length = static_cast<int32_t>(data.size());
        if (length >= 0)
            written = std::fwrite(data.data(), 1, length, output) == static_cast<size_t>(length);
        std::fclose(output);
    }
    if (written) {
        chmod(filename, 0x777);
        GnssBackend::get().send(0x0106, {0});
    }
    return ndk::ScopedAStatus::ok();
}

void GnssPsds::clearCallback() {
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
}
}  // namespace aidl::android::hardware::gnss
