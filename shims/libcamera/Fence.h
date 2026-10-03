/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/unique_fd.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>

using android::status_t;
using android::base::unique_fd;

namespace exynos5 {

class Fence : public android::LightRefBase<Fence> {
  public:
    explicit Fence(int fenceFd);
    explicit Fence(unique_fd fenceFd);

    status_t wait(int timeout);

  private:
    friend class LightRefBase<Fence>;
    virtual ~Fence() = default;

    android::base::unique_fd mFenceFd;
};

}  // namespace exynos5
