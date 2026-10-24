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

#pragma once

#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>
#include <map>
#include <mutex>

namespace aidl::android::hardware::gnss {
struct GnssConfiguration : public BnGnssConfiguration {
  ndk::ScopedAStatus setSuplVersion(int version) override;
  ndk::ScopedAStatus setSuplMode(int mode) override;
  ndk::ScopedAStatus setLppProfile(int profile) override;
  ndk::ScopedAStatus setGlonassPositioningProtocol(int protocol) override;
  ndk::ScopedAStatus setEmergencySuplPdn(bool enable) override;
  ndk::ScopedAStatus setEsExtensionSec(int seconds) override;
  ndk::ScopedAStatus
  setBlocklist(const std::vector<BlocklistedSource> &blocklist) override;
  void onConnected();

private:
  ndk::ScopedAStatus update(uint16_t id, const std::vector<uint8_t> &data);
  std::mutex mMutex;
  std::map<uint16_t, std::vector<uint8_t>> mSettings;
};
} // namespace aidl::android::hardware::gnss
