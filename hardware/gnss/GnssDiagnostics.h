/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace aidl::android::hardware::gnss {

class GnssDiagnostics {
  public:
    static GnssDiagnostics& get();
    void start();
    void stop();
    void onFrame(uint16_t message, const std::vector<uint8_t>& payload, uint8_t protocol);

  private:
    ~GnssDiagnostics();
    void run();

    std::atomic<bool> mRunning{false};
    std::mutex mLifecycleMutex;
    std::mutex mMutex;
    std::thread mThread;
    int mWake = -1;
    bool mConnected = false;
    std::deque<std::vector<uint8_t>> mFrames;
};

}  // namespace aidl::android::hardware::gnss
