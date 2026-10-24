/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
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
    void postCallback(std::function<void()> callback);
    void setHandler(uint16_t message, Handler handler);
    void setConnectedHandler(std::function<void(bool)> handler);
    void setReadyHandler(std::function<void()> handler);

  private:
    GnssBackend() = default;
    ~GnssBackend();
    void run();
    void runCommands();
    void runCallbacks();
    void receive(Payload& buffer);
    bool sendFrame(int socket, const Payload& frame);

    std::atomic<bool> mRunning{false};
    std::mutex mLifecycleMutex;
    std::mutex mMutex;
    std::mutex mSocketMutex;
    std::condition_variable mCondition;
    std::condition_variable mCallbackCondition;
    std::condition_variable mCommandCondition;
    std::thread mThread;
    std::thread mCallbackThread;
    std::thread mCommandThread;
    int mSocket = -1;
    bool mReady = false;
    Payload mOperationalState{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
    std::map<uint16_t, Handler> mHandlers;
    std::function<void(bool)> mConnectedHandler;
    std::function<void()> mReadyHandler;
    std::deque<Payload> mPendingFrames;
    std::deque<std::function<void()>> mPendingCallbacks;
};

}  // namespace aidl::android::hardware::gnss
