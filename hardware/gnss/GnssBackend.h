/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace aidl::android::hardware::gnss {

class GnssBackend {
  public:
    using Payload = std::vector<uint8_t>;
    using Handler = std::function<void(const Payload&)>;

    static GnssBackend& get();
    bool start();
    void stop();
    bool send(uint16_t message, const Payload& payload, uint8_t protocol = 3);
    void setHandler(uint16_t message, Handler handler);
    void setConnectedHandler(std::function<void(bool)> handler);
    void setReadyHandler(std::function<void()> handler);

  private:
    GnssBackend() = default;
    ~GnssBackend();
    void run();
    void runCallbacks();
    void receive(Payload& buffer);
    bool sendFrameLocked(const Payload& frame);

    std::atomic<bool> mRunning{false};
    std::mutex mLifecycleMutex;
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::condition_variable mCallbackCondition;
    std::thread mThread;
    std::thread mCallbackThread;
    int mSocket = -1;
    bool mReady = false;
    std::map<uint16_t, Handler> mHandlers;
    std::function<void(bool)> mConnectedHandler;
    std::function<void()> mReadyHandler;
    std::deque<Payload> mPendingFrames;
    std::deque<std::pair<uint16_t, Payload>> mPendingCallbacks;
};

}  // namespace aidl::android::hardware::gnss
