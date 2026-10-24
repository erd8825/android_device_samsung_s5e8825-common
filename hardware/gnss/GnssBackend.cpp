/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "GnssBackend"

#include "GnssBackend.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <log/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace aidl::android::hardware::gnss {
namespace {

uint16_t read16(const uint8_t *data) {
  return data[0] | (uint16_t(data[1]) << 8);
}

uint32_t read32(const uint8_t *data) {
  return read16(data) | (uint32_t(read16(data + 2)) << 16);
}

void append16(std::vector<uint8_t> &data, uint16_t value) {
  data.push_back(value);
  data.push_back(value >> 8);
}

uint32_t checksum(const uint8_t *data, size_t size) {
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
  if (fd < 0)
    return -1;

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  constexpr char name[] = "\0GNSSND";
  memcpy(address.sun_path, name, sizeof(name) - 1);
  int result = connect(fd, reinterpret_cast<sockaddr *>(&address),
                       offsetof(sockaddr_un, sun_path) + sizeof(name) - 1);
  if (result < 0 && errno == EINPROGRESS) {
    pollfd pending{fd, POLLOUT, 0};
    if (poll(&pending, 1, 1000) > 0) {
      int error = 0;
      socklen_t size = sizeof(error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 &&
          error == 0)
        result = 0;
    }
  }
  if (result < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

} // namespace

GnssBackend &GnssBackend::get() {
  static GnssBackend backend;
  return backend;
}

GnssBackend::~GnssBackend() { stop(); }

bool GnssBackend::start() {
  std::lock_guard lock(mLifecycleMutex);
  if (mRunning.exchange(true))
    return true;
  mThread = std::thread(&GnssBackend::run, this);
  return true;
}

void GnssBackend::stop() {
  std::lock_guard lifecycleLock(mLifecycleMutex);
  mRunning = false;
  {
    std::lock_guard lock(mMutex);
    if (mSocket >= 0)
      shutdown(mSocket, SHUT_RDWR);
  }
  mCondition.notify_all();
  if (mThread.joinable())
    mThread.join();
}

void GnssBackend::setHandler(uint16_t message, Handler handler) {
  std::lock_guard lock(mMutex);
  if (handler) {
    mHandlers[message] = std::move(handler);
  } else {
    mHandlers.erase(message);
  }
}

void GnssBackend::setConnectedHandler(std::function<void()> handler) {
  std::function<void()> notify;
  {
    std::lock_guard lock(mMutex);
    mConnectedHandler = std::move(handler);
    if (mReady)
      notify = mConnectedHandler;
  }
  if (notify)
    notify();
}

bool GnssBackend::send(uint16_t message, const Payload &payload) {
  if (payload.size() > UINT16_MAX)
    return false;
  Payload frame{0xa0, 0xa3};
  frame.reserve(payload.size() + 14);
  append16(frame, payload.size());
  append16(frame, 0x8003);
  append16(frame, message);
  frame.insert(frame.end(), payload.begin(), payload.end());
  uint32_t crc = checksum(frame.data() + 2, frame.size() - 2);
  append16(frame, crc);
  append16(frame, crc >> 16);
  frame.insert(frame.end(), {0xb0, 0xb3});

  std::lock_guard lock(mMutex);
  if (mSocket < 0)
    return false;
  size_t offset = 0;
  while (offset < frame.size() && mRunning) {
    ssize_t sent = ::send(mSocket, frame.data() + offset, frame.size() - offset,
                          MSG_NOSIGNAL);
    if (sent > 0) {
      offset += sent;
      continue;
    }
    if (sent < 0 && errno == EINTR)
      continue;
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      pollfd pending{mSocket, POLLOUT, 0};
      if (poll(&pending, 1, 1000) > 0 && (pending.revents & POLLOUT))
        continue;
    }
    shutdown(mSocket, SHUT_RDWR);
    return false;
  }
  return offset == frame.size();
}

void GnssBackend::receive(Payload &buffer) {
  while (buffer.size() >= 2) {
    size_t offset = 0;
    while (offset + 1 < buffer.size() &&
           (buffer[offset] != 0xa0 || buffer[offset + 1] != 0xa3)) {
      ++offset;
    }
    buffer.erase(buffer.begin(), buffer.begin() + offset);
    if (buffer.size() < 8)
      return;
    const size_t payloadSize = read16(buffer.data() + 2);
    const uint16_t flags = read16(buffer.data() + 4);
    const bool hasCrc = flags & 0x8000;
    const size_t footer = payloadSize + 8 + (hasCrc ? 4 : 0);
    if (buffer.size() < footer + 2)
      return;
    if (buffer[footer] != 0xb0 || buffer[footer + 1] != 0xb3 ||
        (hasCrc && read32(buffer.data() + 8 + payloadSize) !=
                       checksum(buffer.data() + 2, payloadSize + 6))) {
      ALOGW("Discarding invalid BETP frame");
      buffer.erase(buffer.begin(), buffer.begin() + 2);
      continue;
    }

    const uint16_t message = read16(buffer.data() + 6);
    const uint16_t protocol = flags & 0xf;
    Payload payload(buffer.begin() + 8, buffer.begin() + 8 + payloadSize);
    buffer.erase(buffer.begin(), buffer.begin() + footer + 2);
    Handler handler;
    std::function<void()> connected;
    {
      std::lock_guard lock(mMutex);
      if (protocol == 0 && message == 0x0101 && payload.size() >= 12) {
        bool ready = payload[8] != 0;
        if (ready && !mReady)
          connected = mConnectedHandler;
        mReady = ready;
      } else if (protocol == 3) {
        auto entry = mHandlers.find(message);
        if (entry != mHandlers.end())
          handler = entry->second;
      }
    }
    if (connected)
      connected();
    if (handler)
      handler(payload);
  }
}

void GnssBackend::run() {
  while (mRunning) {
    int fd = connectGpsd();
    if (fd < 0) {
      std::unique_lock lock(mMutex);
      mCondition.wait_for(lock, std::chrono::seconds(1),
                          [this] { return !mRunning; });
      continue;
    }
    {
      std::lock_guard lock(mMutex);
      mSocket = fd;
      mReady = false;
    }
    ALOGI("Connected to gpsd");
    Payload buffer;
    uint8_t data[4096];
    while (mRunning) {
      pollfd pending{fd, POLLIN, 0};
      int result = poll(&pending, 1, 500);
      if (result < 0 && errno == EINTR)
        continue;
      if (result < 0 || (!(pending.revents & POLLIN) &&
                         (pending.revents & (POLLERR | POLLHUP | POLLNVAL))))
        break;
      if (!(pending.revents & POLLIN))
        continue;
      ssize_t size = recv(fd, data, sizeof(data), 0);
      if (size < 0 &&
          (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
        continue;
      if (size <= 0)
        break;
      buffer.insert(buffer.end(), data, data + size);
      receive(buffer);
    }
    {
      std::lock_guard lock(mMutex);
      mSocket = -1;
      mReady = false;
      close(fd);
    }
    ALOGI("Disconnected from gpsd");
  }
}

} // namespace aidl::android::hardware::gnss
