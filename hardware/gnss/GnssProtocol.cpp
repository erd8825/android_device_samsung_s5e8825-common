/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssProtocol.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <map>

namespace aidl::android::hardware::gnss::protocol {

Writer& Writer::string(const std::string& value, size_t maximum) {
    if (maximum == 0) return *this;
    const auto size = std::min(value.find('\0'), std::min(value.size(), maximum - 1));
    bytes(value.data(), size);
    return u8(0);
}

Writer& Writer::bytes(const void* data, size_t size) {
    if (size) {
        const auto* begin = static_cast<const uint8_t*>(data);
        mData.insert(mData.end(), begin, begin + size);
    }
    return *this;
}

std::string Reader::string(size_t maximum) {
    const auto size = std::min(maximum, remaining());
    const auto begin = mData.begin() + mPosition;
    const auto end = std::find(begin, begin + size, 0);
    if (end == begin + size) { fail(); return {}; }
    std::string value(begin, end);
    mPosition += value.size() + 1;
    return value;
}

std::vector<uint8_t> Reader::bytes(size_t size) {
    if (size > remaining()) { fail(); return {}; }
    const auto begin = mData.begin() + mPosition;
    mPosition += size;
    return {begin, begin + size};
}

void Reader::skip(size_t size) {
    if (size > remaining()) fail();
    else mPosition += size;
}

namespace {
const std::map<std::string, std::string>& config() {
    static const auto values = [] {
        std::map<std::string, std::string> result;
        std::ifstream input("/vendor/etc/gnss/gps.cfg");
        std::string line;
        while (std::getline(input, line)) {
            line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
            const auto equal = line.find('=');
            if (equal == std::string::npos) continue;
            auto trim = [](std::string value) {
                auto first = value.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return std::string();
                return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
            };
            auto key = trim(line.substr(0, equal));
            if (!key.empty()) result[key] = trim(line.substr(equal + 1));
        }
        return result;
    }();
    return values;
}
}  // namespace

std::string readConfigString(const char* key, const std::string& fallback) {
    const auto& values = config();
    const auto entry = values.find(key);
    return entry == values.end() ? fallback : entry->second;
}

int readConfigInt(const char* key, int fallback) {
    const auto text = readConfigString(key, "");
    if (text.empty()) return fallback;
    char* end;
    errno = 0;
    const long value = std::strtol(text.c_str(), &end, 0);
    if (errno || *end || value < INT_MIN || value > INT_MAX) return fallback;
    return value;
}

}  // namespace aidl::android::hardware::gnss::protocol
