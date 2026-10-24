/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
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
