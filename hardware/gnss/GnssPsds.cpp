/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssPsds.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <android-base/file.h>
#include <android-base/unique_fd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
namespace aidl::android::hardware::gnss {
GnssPsds::GnssPsds() {
  GnssBackend::get().setHandler(0x010b, [this](const std::vector<uint8_t> &) {
    std::shared_ptr<IGnssPsdsCallback> callback;
    {
      std::lock_guard lock(mMutex);
      callback = mCallback;
    }
    if (callback)
      callback->downloadRequestCb(PsdsType::LONG_TERM);
  });
}
GnssPsds::~GnssPsds() { GnssBackend::get().setHandler(0x010b, {}); }
ndk::ScopedAStatus
GnssPsds::setCallback(const std::shared_ptr<IGnssPsdsCallback> &callback) {
  if (!callback)
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  std::lock_guard lock(mMutex);
  mCallback = callback;
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssPsds::injectPsdsData(PsdsType type,
                                            const std::vector<uint8_t> &data) {
  if (data.empty() || (type != PsdsType::LONG_TERM &&
                       type != PsdsType::NORMAL && type != PsdsType::REALTIME))
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  if (type != PsdsType::LONG_TERM)
    return ndk::ScopedAStatus::ok();
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
             : ndk::ScopedAStatus::fromServiceSpecificError(
                   IGnss::ERROR_GENERIC);
}
void GnssPsds::clearCallback() {
  std::lock_guard lock(mMutex);
  mCallback = nullptr;
}
} // namespace aidl::android::hardware::gnss
