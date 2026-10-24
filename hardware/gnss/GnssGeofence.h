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

#pragma once
#include <aidl/android/hardware/gnss/BnGnssGeofence.h>
#include <array>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssGeofence : public BnGnssGeofence {
    GnssGeofence();
    ~GnssGeofence();
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssGeofenceCallback>& callback) override;
    ndk::ScopedAStatus addGeofence(int id, double latitude, double longitude, double radius,
                                   int lastTransition, int transitions, int responsiveness,
                                   int unknownTimer) override;
    ndk::ScopedAStatus pauseGeofence(int id) override;
    ndk::ScopedAStatus resumeGeofence(int id, int transitions) override;
    ndk::ScopedAStatus removeGeofence(int id) override;
    void clearCallback();

  private:
    ndk::ScopedAStatus command(uint16_t command, int id, int transitions = 0);
    void response(uint16_t id, const std::vector<uint8_t>& data);
    std::mutex mMutex;
    std::shared_ptr<IGnssGeofenceCallback> mCallback;
    std::array<int, 512> mIds;
    int64_t mLastInitMillis = 0;
};
}  // namespace aidl::android::hardware::gnss
