/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "GnssDiagnostics"

#include "GnssDiagnostics.h"

#include <arpa/inet.h>
#include <log/log.h>
#include <openssl/aes.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "GnssBackend.h"
#include "GnssProtocol.h"

namespace aidl::android::hardware::gnss {
namespace {

int createServer() {
    const auto port = static_cast<uint16_t>(
            std::atoi(readConfigString("encrypted_tcp_server_port", "7557").c_str()));
    const int fd = socket(port ? AF_INET : AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;
    const int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int result;
    if (port) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        result = bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    } else {
        const auto host = readConfigString("encrypted_tcp_server_host", "127.0.0.1");
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const auto length = std::min(host.size(), sizeof(address.sun_path));
        std::memcpy(address.sun_path, host.data(), length);
        if (address.sun_path[0] == '@') address.sun_path[0] = '\0';
        result = bind(fd, reinterpret_cast<sockaddr*>(&address),
                      offsetof(sockaddr_un, sun_path) + length);
    }
    if (result < 0 || listen(fd, 2) < 0) {
        const int error = errno;
        close(fd);
        errno = error;
        return -1;
    }
    return fd;
}

void logging(bool enabled) {
    if (readConfigInt("debug_enable", 0) == 0)
        GnssBackend::get().send(0x00e4, std::vector<uint8_t>(52, enabled), 2);
}

}  // namespace

GnssDiagnostics& GnssDiagnostics::get() {
    static GnssDiagnostics diagnostics;
    return diagnostics;
}

GnssDiagnostics::~GnssDiagnostics() {
    stop();
}

void GnssDiagnostics::start() {
    if (readConfigInt("encrypted_tcp_server_force", 0) != 1) return;
    std::lock_guard lifecycle(mLifecycleMutex);
    if (mRunning) return;
    mWake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (mWake < 0) {
        ALOGW("Cannot create diagnostics wake event: %s", std::strerror(errno));
        return;
    }
    mRunning = true;
    mThread = std::thread(&GnssDiagnostics::run, this);
}

void GnssDiagnostics::stop() {
    std::lock_guard lifecycle(mLifecycleMutex);
    if (!mRunning.exchange(false)) return;
    const uint64_t wake = 1;
    write(mWake, &wake, sizeof(wake));
    if (mThread.joinable()) mThread.join();
    close(mWake);
    mWake = -1;
}

void GnssDiagnostics::onFrame(uint16_t message, const std::vector<uint8_t>& payload,
                              uint8_t protocol) {
    if (!mRunning || protocol == 4 || protocol >= 6 || payload.size() > UINT16_MAX) return;
    std::lock_guard lock(mMutex);
    if (!mConnected) return;
    if (mFrames.size() >= 0xc00) {
        ALOGW("Diagnostics frame queue is full");
        return;
    }
    auto frame = Writer().u16(0xa3a0).u16(payload.size()).u16(0x8000 | protocol).u16(message);
    frame.bytes(payload.data(), payload.size());
    const auto checksum = crc32(0, frame.data().data() + 2, frame.data().size() - 2);
    frame.u32(checksum).u16(0xb3b0);
    mFrames.push_back(frame.data());
    const uint64_t wake = 1;
    write(mWake, &wake, sizeof(wake));
}

void GnssDiagnostics::run() {
    constexpr uint8_t key[] = "gY84giNWQR3MXS4t3sDjWxIQM9V27Hec";
    AES_KEY cipher;
    AES_set_encrypt_key(key, 256, &cipher);
    std::array<uint8_t, 512> pending{};
    size_t pendingSize = 0;
    std::array<uint8_t, 32> encryption{};
    size_t encryptionSize = 0;
    std::vector<uint8_t> output;
    size_t outputOffset = 0;
    int server = -1;
    int client = -1;
    bool retry = true;
    auto disconnect = [&] {
        {
            std::lock_guard lock(mMutex);
            mConnected = false;
            mFrames.clear();
        }
        if (client >= 0) {
            close(client);
            client = -1;
            logging(false);
        }
        output.clear();
        outputOffset = 0;
    };
    auto compress = [&](const std::vector<uint8_t>& frame) {
        size_t offset = 0;
        while (offset < frame.size()) {
            const auto size = std::min(frame.size() - offset, pending.size() - pendingSize);
            std::memcpy(pending.data() + pendingSize, frame.data() + offset, size);
            pendingSize += size;
            offset += size;
            if (pendingSize != pending.size()) continue;
            std::array<uint8_t, 1024> compressed{};
            uLongf compressedSize = compressed.size();
            const int result = compress2(compressed.data(), &compressedSize, pending.data(),
                                         pending.size(), 9);
            pendingSize = 0;
            if (result != Z_OK) {
                ALOGW("Cannot compress diagnostics data: %d", result);
                continue;
            }
            size_t position = 0;
            while (position < compressedSize) {
                const auto count =
                        std::min(compressedSize - position, encryption.size() - encryptionSize);
                std::memcpy(encryption.data() + encryptionSize, compressed.data() + position,
                            count);
                position += count;
                encryptionSize += count;
                if (encryptionSize != encryption.size()) continue;
                const auto begin = output.size();
                output.resize(begin + encryption.size());
                for (size_t block = 0; block < encryption.size(); block += AES_BLOCK_SIZE)
                    AES_encrypt(encryption.data() + block, output.data() + begin + block, &cipher);
                encryptionSize = 0;
            }
        }
    };
    while (mRunning) {
        if (server < 0 && retry) {
            server = createServer();
            if (server < 0) {
                retry = errno != EADDRINUSE;
                ALOGW("Cannot bind diagnostics server: %s", std::strerror(errno));
            }
        }
        pollfd descriptors[]{
                {mWake, POLLIN, 0},
                {server, POLLIN, 0},
                {client, static_cast<short>(POLLIN | (output.empty() ? 0 : POLLOUT)), 0}};
        const int result = poll(descriptors, std::size(descriptors), server < 0 ? 5000 : -1);
        if (result < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (descriptors[0].revents & POLLIN) {
            uint64_t wake;
            read(mWake, &wake, sizeof(wake));
        }
        if (!mRunning) break;
        if (descriptors[1].revents & POLLIN) {
            const int accepted = accept4(server, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
            if (accepted >= 0) {
                disconnect();
                client = accepted;
                {
                    std::lock_guard lock(mMutex);
                    mConnected = true;
                }
                logging(true);
                GnssBackend::get().send(0x12de, {0}, 2);
            }
        }
        if (descriptors[2].fd == client && client >= 0 && descriptors[2].revents) {
            if (descriptors[2].revents & POLLIN) {
                uint8_t discarded[1024];
                const auto count = recv(client, discarded, sizeof(discarded), 0);
                if (count == 0 || (count < 0 && errno != EINTR && errno != EAGAIN)) disconnect();
            }
            if (descriptors[2].revents & (POLLERR | POLLHUP | POLLNVAL)) disconnect();
        }
        if (client < 0) continue;
        std::deque<std::vector<uint8_t>> frames;
        {
            std::lock_guard lock(mMutex);
            frames.swap(mFrames);
        }
        for (const auto& frame : frames) {
            compress(frame);
            if (output.size() - outputOffset > 0x120000) {
                ALOGW("Diagnostics client is not consuming data");
                disconnect();
                break;
            }
        }
        if (client < 0) continue;
        while (outputOffset < output.size()) {
            const auto count = send(client, output.data() + outputOffset,
                                    output.size() - outputOffset, MSG_NOSIGNAL);
            if (count > 0)
                outputOffset += count;
            else if (count < 0 && errno == EINTR)
                continue;
            else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            else {
                disconnect();
                break;
            }
        }
        if (outputOffset == output.size()) {
            output.clear();
            outputOffset = 0;
        }
    }
    disconnect();
    if (server >= 0) close(server);
}

}  // namespace aidl::android::hardware::gnss
