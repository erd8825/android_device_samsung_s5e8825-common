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
