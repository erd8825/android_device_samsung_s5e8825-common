/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <aidl/android/hardware/gnss/measurement_corrections/BnMeasurementCorrectionsInterface.h>
#include <mutex>
namespace aidl::android::hardware::gnss::measurement_corrections {
struct MeasurementCorrectionsInterface
    : public BnMeasurementCorrectionsInterface {
  MeasurementCorrectionsInterface();
  ~MeasurementCorrectionsInterface();
  ndk::ScopedAStatus
  setCorrections(const MeasurementCorrections &corrections) override;
  ndk::ScopedAStatus
  setCallback(const std::shared_ptr<IMeasurementCorrectionsCallback> &callback)
      override;
  void clearCallback();
  void onConnected();

private:
  std::mutex mMutex;
  std::shared_ptr<IMeasurementCorrectionsCallback> mCallback;
};
} // namespace aidl::android::hardware::gnss::measurement_corrections
