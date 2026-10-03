/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#include <string>

namespace vendor::samsung::hardware::thermal::V1_0 {

class ISehThermal : public android::RefBase {
  public:
    static android::sp<ISehThermal> getService(const std::string&, bool);
};

android::sp<ISehThermal> ISehThermal::getService(const std::string&, bool) {
    return nullptr;
}

}  // namespace vendor::samsung::hardware::thermal::V1_0
