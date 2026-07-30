/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Fence.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

// We would eliminate the non-conforming zero-length array, but we can't since
// this is effectively included from the Linux kernel
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#include <sync/sync.h>
#pragma clang diagnostic pop

#include <sys/types.h>
#include <unistd.h>
#include <utils/Trace.h>

using android::NO_ERROR;

namespace exynos5 {

Fence::Fence(int fenceFd) : mFenceFd(fenceFd) {}

Fence::Fence(unique_fd fenceFd) : mFenceFd(std::move(fenceFd)) {}

status_t Fence::wait(int timeout) {
    ATRACE_CALL();
    if (mFenceFd == -1) {
        return NO_ERROR;
    }
    int err = sync_wait(mFenceFd, timeout);

    close(mFenceFd.release());

    return err < 0 ? -errno : status_t(NO_ERROR);
}

}  // namespace exynos5
