/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace aidl::android::hardware::gnss {

class GnssSensors {
  public:
    ~GnssSensors();
    void init();
    void start();
    void stop();
    void disableConfig();

  private:
    void run();
    std::mutex mLifecycleMutex;
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mActive{false};
    std::atomic<uint64_t> mLastSendMs{0};
    std::atomic<uint64_t> mLastReportMs{0};
    bool mEnabled = false;
    bool mInitialized = false;
    bool mInitializing = false;
    std::function<void()> mWake;
    void* mLibrary = nullptr;
    std::thread mThread;
};

}  // namespace aidl::android::hardware::gnss
