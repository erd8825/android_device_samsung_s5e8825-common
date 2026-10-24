/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssConfiguration.h"
#include "GnssBackend.h"
#include "GnssProtocol.h"
#include <aidl/android/hardware/gnss/IGnss.h>
#include <algorithm>

namespace aidl::android::hardware::gnss {
ndk::ScopedAStatus GnssConfiguration::update(uint16_t id,
                                             const std::vector<uint8_t> &data) {
  {
    std::lock_guard lock(mMutex);
    mSettings[id] = data;
  }
  return GnssBackend::get().send(id, data)
             ? ndk::ScopedAStatus::ok()
             : ndk::ScopedAStatus::fromServiceSpecificError(
                   IGnss::ERROR_GENERIC);
}
void GnssConfiguration::onConnected() {
  std::map<uint16_t, std::vector<uint8_t>> settings;
  {
    std::lock_guard lock(mMutex);
    settings = mSettings;
  }
  for (const auto &[id, data] : settings)
    GnssBackend::get().send(id, data);
}
ndk::ScopedAStatus GnssConfiguration::setSuplVersion(int version) {
  return update(0x7504, {static_cast<uint8_t>(version >> 16),
                         static_cast<uint8_t>(version >> 8)});
}
ndk::ScopedAStatus GnssConfiguration::setSuplMode(int mode) {
  return update(0x7604, {static_cast<uint8_t>((mode & 2) != 0),
                         static_cast<uint8_t>((mode & 1) != 0)});
}
ndk::ScopedAStatus GnssConfiguration::setLppProfile(int profile) {
  return update(0x7804, {static_cast<uint8_t>((profile & 1) != 0),
                         static_cast<uint8_t>((profile & 2) != 0)});
}
ndk::ScopedAStatus
GnssConfiguration::setGlonassPositioningProtocol(int protocol) {
  return update(0x7904, {static_cast<uint8_t>((protocol & 1) != 0),
                         static_cast<uint8_t>((protocol & 2) != 0),
                         static_cast<uint8_t>((protocol & 4) != 0)});
}
ndk::ScopedAStatus GnssConfiguration::setEmergencySuplPdn(bool enable) {
  return update(0x7a04, {static_cast<uint8_t>(enable)});
}
ndk::ScopedAStatus GnssConfiguration::setEsExtensionSec(int) {
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus GnssConfiguration::setBlocklist(
    const std::vector<BlocklistedSource> &blocklist) {
  protocol::Writer data;
  const auto count = std::min<size_t>(64, blocklist.size());
  data.put<uint32_t>(count);
  for (size_t i = 0; i < count; ++i) {
    const auto &source = blocklist[i];
    uint8_t constellation = 7;
    uint8_t svid = source.svid;
    switch (source.constellation) {
    case GnssConstellationType::GPS:
      constellation = 0;
      break;
    case GnssConstellationType::SBAS:
      constellation = 4;
      break;
    case GnssConstellationType::GLONASS:
      constellation = 1;
      if (svid)
        svid += source.svid >= 1 && source.svid <= 24 ? 64 : -23;
      break;
    case GnssConstellationType::QZSS:
      constellation = 5;
      break;
    case GnssConstellationType::BEIDOU:
      constellation = 3;
      if (svid)
        svid += 138;
      break;
    case GnssConstellationType::GALILEO:
      constellation = 2;
      if (svid)
        svid -= 54;
      break;
    default:
      break;
    }
    data.put<uint8_t>(constellation);
    data.put<uint8_t>(svid);
  }
  return update(0x7304, data.data());
}
} // namespace aidl::android::hardware::gnss
