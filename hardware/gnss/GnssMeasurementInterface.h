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

#include <aidl/android/hardware/gnss/BnGnssMeasurementCallback.h>
#include <aidl/android/hardware/gnss/BnGnssMeasurementInterface.h>
#include <memory>
#include <mutex>
#include <vector>

namespace aidl::android::hardware::gnss {
struct GnssMeasurementInterface : public BnGnssMeasurementInterface {
public:
  GnssMeasurementInterface();
  ~GnssMeasurementInterface();
  ndk::ScopedAStatus
  setCallback(const std::shared_ptr<IGnssMeasurementCallback> &callback,
              const bool enableFullTracking,
              const bool enableCorrVecOutputs) override;
  ndk::ScopedAStatus close() override;
  ndk::ScopedAStatus setCallbackWithOptions(
      const std::shared_ptr<IGnssMeasurementCallback> &callback,
      const Options &options) override;
  void setLocationInterval(int intervalMs);
  void setLocationEnabled(bool enabled);
  void setNavigationEnabled(bool enabled);
  void onConnected();
  std::shared_ptr<GnssData> getLastData() const;

private:
  void updateRequest(bool wasEnabled);
  void readHeader(const std::vector<uint8_t> &payload);
  void readMeasurement(const std::vector<uint8_t> &payload);
  void reportMeasurement(std::shared_ptr<GnssData> data);

  mutable std::mutex mMutex;
  std::shared_ptr<IGnssMeasurementCallback> mCallback;
  std::shared_ptr<GnssData> mPendingData;
  std::shared_ptr<GnssData> mLastData;
  int mIntervalMs = 1000;
  int mLocationIntervalMs = 1000;
  bool mLocationEnabled = false;
  bool mNavigationEnabled = false;
  bool mFullTracking = false;
  size_t mExpectedMeasurements = 0;
  uint64_t mReportedCount = 0;
  int mRfPathLossDb;
};

} // namespace aidl::android::hardware::gnss
