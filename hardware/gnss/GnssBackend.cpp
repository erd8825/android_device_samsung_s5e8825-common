/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "GnssBackend"

#include "GnssBackend.h"

#include "GnssDiagnostics.h"
#include "GnssProtocol.h"

#include <fcntl.h>
#include <log/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace aidl::android::hardware::gnss {
namespace {

uint16_t read16(const uint8_t* data) {
    return data[0] | (uint16_t(data[1]) << 8);
}

uint32_t read32(const uint8_t* data) {
    return read16(data) | (uint32_t(read16(data + 2)) << 16);
}

void append16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(value);
    data.push_back(value >> 8);
}

uint32_t checksum(const uint8_t* data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
        }
    }
    return ~crc;
}

int connectGpsd() {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    constexpr char name[] = "\0GNSSND";
    memcpy(address.sun_path, name, sizeof(name) - 1);
    int result = connect(fd, reinterpret_cast<sockaddr*>(&address),
                         offsetof(sockaddr_un, sun_path) + sizeof(name) - 1);
    if (result < 0 && errno == EINPROGRESS) {
        pollfd pending{fd, POLLOUT, 0};
        if (poll(&pending, 1, 1000) > 0) {
            int error = 0;
            socklen_t size = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0) result = 0;
        }
    }
    if (result < 0) {
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void writeDeviceInfo() {
    const auto path = readConfigString("gnss_data_dir", "/data/vendor/gps") + "/chip.info";
    struct stat info;
    if (stat(path.c_str(), &info) == 0) return;
    FILE* output = std::fopen(path.c_str(), "w");
    if (!output) {
        if (errno != EEXIST) ALOGW("Cannot write %s: %s", path.c_str(), std::strerror(errno));
        return;
    }
    std::fputs("S.LSI,UNKOWN\nSPOTNAV_4.6.2_7_E26,f5a82f23911a65762014a4b237148dd38c9bbd23\n",
               output);
    std::fclose(output);
}

class ControlPort {
  public:
    ~ControlPort() { close(); }

    void close() {
        if (mLines[0] >= 0) write(mLines[0], "1", 1);
        for (int& fd : mLines) {
            if (fd >= 0) ::close(fd);
            fd = -1;
        }
    }

    bool open() {
        if (!readConfigPresent("gnss_device_control_port_gpio_present")) return true;
        constexpr const char* keys[]{"gnss_device_control_port_gpio_reset",
                                     "gnss_device_control_port_gpio_chip_wake",
                                     "gnss_device_control_port_gpio_host_wake"};
        for (size_t i = 0; i < mLines.size(); ++i) {
            const auto inverted = std::string(keys[i]) + "_F";
            const char* key = readConfigPresent(keys[i])                 ? keys[i]
                              : i && readConfigPresent(inverted.c_str()) ? inverted.c_str()
                                                                         : nullptr;
            if (!key) continue;
            const auto path = readConfigString(key, "");
            mLines[i] = ::open(path.c_str(), i == 2 ? O_RDONLY : O_WRONLY);
            if (mLines[i] < 0) {
                ALOGE("Cannot open GNSS control line %s: %s", path.c_str(), std::strerror(errno));
                return false;
            }
        }
        return true;
    }

  private:
    std::array<int, 3> mLines{-1, -1, -1};
};

}  // namespace

GnssBackend& GnssBackend::get() {
    static GnssBackend backend;
    return backend;
}

GnssBackend::~GnssBackend() {
    stop();
}

bool GnssBackend::start() {
    std::lock_guard lock(mLifecycleMutex);
    if (mRunning.exchange(true)) return true;
    writeDeviceInfo();
    GnssDiagnostics::get().start();
    mCallbackThread = std::thread(&GnssBackend::runCallbacks, this);
    mCommandThread = std::thread(&GnssBackend::runCommands, this);
    mThread = std::thread(&GnssBackend::run, this);
    return true;
}

void GnssBackend::stop() {
    std::lock_guard lifecycleLock(mLifecycleMutex);
    GnssDiagnostics::get().stop();
    mRunning = false;
    {
        std::lock_guard lock(mSocketMutex);
        if (mSocket >= 0) shutdown(mSocket, SHUT_RDWR);
    }
    {
        std::lock_guard lock(mMutex);
        mPendingFrames.clear();
        mPendingCallbacks.clear();
    }
    mCondition.notify_all();
    mCallbackCondition.notify_all();
    mCommandCondition.notify_all();
    if (mThread.joinable()) mThread.join();
    if (mCallbackThread.joinable()) mCallbackThread.join();
    if (mCommandThread.joinable()) mCommandThread.join();
}

void GnssBackend::setHandler(uint16_t message, Handler handler) {
    std::lock_guard lock(mMutex);
    if (handler) {
        mHandlers[message] = std::move(handler);
    } else {
        mHandlers.erase(message);
    }
}

void GnssBackend::setConnectedHandler(std::function<void(bool)> handler) {
    std::function<void(bool)> notify;
    {
        std::lock_guard lock(mMutex);
        mConnectedHandler = std::move(handler);
        if (mReady) notify = mConnectedHandler;
    }
    if (notify) notify(true);
}

void GnssBackend::setReadyHandler(std::function<void()> handler) {
    std::lock_guard lock(mMutex);
    mReadyHandler = std::move(handler);
}

bool GnssBackend::send(uint16_t message, const Payload& payload, uint8_t protocol) {
    if (payload.size() > UINT16_MAX) return false;
    Payload frame{0xa0, 0xa3};
    frame.reserve(payload.size() + 14);
    append16(frame, payload.size());
    append16(frame, 0x8000 | protocol);
    append16(frame, message);
    frame.insert(frame.end(), payload.begin(), payload.end());
    uint32_t crc = checksum(frame.data() + 2, frame.size() - 2);
    append16(frame, crc);
    append16(frame, crc >> 16);
    frame.insert(frame.end(), {0xb0, 0xb3});

    std::lock_guard lock(mMutex);
    if (!mRunning) return false;
    if (protocol != 1 && protocol != 5) GnssDiagnostics::get().onFrame(message, payload, protocol);
    if (message == 0xfd && mPendingFrames.size() >= 0x800) return true;
    mPendingFrames.push_back(std::move(frame));
    mCommandCondition.notify_one();
    return true;
}

bool GnssBackend::sendFrame(int socket, const Payload& frame) {
    const auto sent = ::send(socket, frame.data(), frame.size(), MSG_NOSIGNAL);
    if (sent == static_cast<ssize_t>(frame.size())) return true;
    shutdown(socket, SHUT_RDWR);
    return false;
}

void GnssBackend::runCommands() {
    std::unique_lock lock(mMutex);
    while (mRunning) {
        mCommandCondition.wait(lock, [this] {
            return !mRunning ||
                   (!mPendingFrames.empty() && (mReady || mPendingFrames.size() > 0x600));
        });
        if (!mRunning) return;
        for (int count = 0; count < 0x300 && !mPendingFrames.empty(); ++count) {
            auto frame = std::move(mPendingFrames.front());
            mPendingFrames.pop_front();
            const int socket = mSocket >= 0 ? fcntl(mSocket, F_DUPFD_CLOEXEC, 0) : -1;
            lock.unlock();
            bool sent = true;
            if (socket >= 0) {
                sent = sendFrame(socket, frame);
                close(socket);
            }
            lock.lock();
            if (!sent) {
                mReady = false;
                break;
            }
        }
        if (mPendingFrames.size() > 0xc00) {
            char reason[64];
            std::snprintf(reason, sizeof(reason), "lal_pending_tx.list.count=%zu",
                          mPendingFrames.size());
            GnssDiagnostics::get().assertFailure(reason, "lal_router.c", 0x352);
        }
    }
}

void GnssBackend::postCallback(std::function<void()> callback) {
    std::lock_guard lock(mMutex);
    if (!mRunning) return;
    mPendingCallbacks.push_back(std::move(callback));
    mCallbackCondition.notify_one();
}

void GnssBackend::receive(Payload& buffer) {
    while (buffer.size() >= 2) {
        size_t offset = 0;
        while (offset + 1 < buffer.size() &&
               (buffer[offset] != 0xa0 || buffer[offset + 1] != 0xa3)) {
            ++offset;
        }
        buffer.erase(buffer.begin(), buffer.begin() + offset);
        if (buffer.size() < 8) return;
        const size_t payloadSize = read16(buffer.data() + 2);
        const uint16_t flags = read16(buffer.data() + 4);
        const bool hasCrc = flags & 0x8000;
        const size_t footer = payloadSize + 8 + (hasCrc ? 4 : 0);
        if (buffer.size() < footer + 2) {
            bool recovered = false;
            for (size_t next = 2; next + 10 <= buffer.size(); ++next) {
                if (buffer[next] != 0xa0 || buffer[next + 1] != 0xa3) continue;
                const size_t length = read16(buffer.data() + next + 2);
                const bool crc = read16(buffer.data() + next + 4) & 0x8000;
                const size_t end = next + 8 + length + (crc ? 4 : 0);
                if (end + 2 > buffer.size()) continue;
                if (buffer[end] != 0xb0 || buffer[end + 1] != 0xb3) break;
                if (crc && read32(buffer.data() + next + 8 + length) !=
                                   checksum(buffer.data() + next + 2, length + 6))
                    break;
                buffer.erase(buffer.begin(), buffer.begin() + next);
                recovered = true;
                break;
            }
            if (recovered) continue;
            return;
        }
        if (buffer[footer] != 0xb0 || buffer[footer + 1] != 0xb3) {
            ALOGW("Discarding invalid BETP frame");
            buffer.erase(buffer.begin(), buffer.begin() + 2);
            continue;
        }
        if (hasCrc && read32(buffer.data() + 8 + payloadSize) !=
                              checksum(buffer.data() + 2, payloadSize + 6)) {
            ALOGW("Discarding BETP frame with invalid checksum");
            buffer.erase(buffer.begin(), buffer.begin() + footer + 2);
            continue;
        }

        const uint16_t message = read16(buffer.data() + 6);
        const uint16_t protocol = flags & 0xf;
        Payload payload(buffer.begin() + 8, buffer.begin() + 8 + payloadSize);
        buffer.erase(buffer.begin(), buffer.begin() + footer + 2);
        if (protocol != 0 || message != 0x0101)
            GnssDiagnostics::get().onFrame(message, payload, protocol);
        Handler handler;
        std::function<void(bool)> connected;
        std::function<void()> readyHandler;
        bool fresh = false;
        Payload operationalState;
        {
            std::lock_guard lock(mMutex);
            if (protocol == 0 && message == 0x0101 && payload.size() >= 12) {
                bool ready = payload[8] != 0;
                fresh = ready && !mReady;
                std::copy_n(payload.begin(), 8, mOperationalState.begin());
                mOperationalState[9] = payload[9];
                if (ready != mReady) {
                    mOperationalState[8] = ready;
                    operationalState = mOperationalState;
                }
                mReady = ready;
                if (fresh) {
                    readyHandler = mReadyHandler;
                    connected = mConnectedHandler;
                    mCommandCondition.notify_one();
                }
            } else if (protocol == 3) {
                auto entry = mHandlers.find(message);
                if (entry != mHandlers.end()) handler = entry->second;
            }
        }
        if (!operationalState.empty()) GnssDiagnostics::get().onFrame(0x0101, operationalState, 0);
        if (readyHandler) readyHandler();
        if (connected) connected(fresh);
        if (handler) handler(payload);
    }
}

void GnssBackend::runCallbacks() {
    while (true) {
        std::function<void()> callback;
        {
            std::unique_lock lock(mMutex);
            mCallbackCondition.wait(lock,
                                    [this] { return !mRunning || !mPendingCallbacks.empty(); });
            if (!mRunning) return;
            callback = std::move(mPendingCallbacks.front());
            mPendingCallbacks.pop_front();
        }
        if (callback) callback();
    }
}

void GnssBackend::run() {
    while (mRunning) {
        int fd = connectGpsd();
        ControlPort control;
        if (fd >= 0 && !control.open()) {
            close(fd);
            control.close();
            fd = -1;
        }
        if (fd < 0) {
            std::unique_lock lock(mMutex);
            mCondition.wait_for(lock, std::chrono::seconds(1), [this] { return !mRunning; });
            continue;
        }
        {
            std::lock_guard lock(mMutex);
            std::lock_guard socketLock(mSocketMutex);
            mSocket = fd;
            mReady = false;
        }
        ALOGI("Connected to gpsd");
        Payload buffer;
        uint8_t data[4096];
        while (mRunning) {
            pollfd pending{fd, POLLIN, 0};
            int result = poll(&pending, 1, 500);
            if (result < 0 && errno == EINTR) continue;
            if (result < 0 ||
                (!(pending.revents & POLLIN) && (pending.revents & (POLLERR | POLLHUP | POLLNVAL))))
                break;
            if (!(pending.revents & POLLIN)) continue;
            ssize_t size = recv(fd, data, sizeof(data), 0);
            if (size < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            if (size <= 0) break;
            buffer.insert(buffer.end(), data, data + size);
            receive(buffer);
        }
        Payload operationalState;
        {
            std::lock_guard lock(mMutex);
            std::lock_guard socketLock(mSocketMutex);
            shutdown(fd, SHUT_RDWR);
            mSocket = -1;
            if (mReady) {
                mOperationalState[8] = 0;
                operationalState = mOperationalState;
            }
            mReady = false;
            close(fd);
        }
        if (!operationalState.empty()) GnssDiagnostics::get().onFrame(0x0101, operationalState, 0);
        ALOGI("Disconnected from gpsd");
    }
}

}  // namespace aidl::android::hardware::gnss
