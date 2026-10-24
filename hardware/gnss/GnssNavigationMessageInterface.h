/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/BnGnssNavigationMessageInterface.h>
#include <mutex>
namespace aidl::android::hardware::gnss {
struct GnssMeasurementInterface;
struct GnssNavigationMessageInterface
    : public BnGnssNavigationMessageInterface {
  explicit GnssNavigationMessageInterface(
      const std::shared_ptr<GnssMeasurementInterface> &measurement);
  ~GnssNavigationMessageInterface();
  ndk::ScopedAStatus setCallback(
      const std::shared_ptr<IGnssNavigationMessageCallback> &callback) override;
  ndk::ScopedAStatus close() override;
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IGnssNavigationMessageCallback> mCallback;
  std::shared_ptr<GnssMeasurementInterface> mMeasurement;
};
} // namespace aidl::android::hardware::gnss
