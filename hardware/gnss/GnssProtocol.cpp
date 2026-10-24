/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssProtocol.h"

#include <android-base/properties.h>
#include <log/log.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
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
    if (end == begin + size) {
        fail();
        return {};
    }
    std::string value(begin, end);
    mPosition += value.size() + 1;
    return value;
}

std::vector<uint8_t> Reader::bytes(size_t size) {
    if (size > remaining()) {
        fail();
        return {};
    }
    const auto begin = mData.begin() + mPosition;
    mPosition += size;
    return {begin, begin + size};
}

void Reader::skip(size_t size) {
    if (size > remaining())
        fail();
    else
        mPosition += size;
}

namespace {
using Config = std::map<std::string, std::vector<std::string>>;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n\v\f") - first + 1);
}

std::string configFile() {
    constexpr char base[] = "/vendor/etc/gnss/gps.cfg";
    std::string selected = base;
    auto select = [&](const char* suffix) {
        const std::string candidate = std::string("/vendor/etc/gnss/gps_") + suffix + ".cfg";
        selected = access(candidate.c_str(), F_OK) == 0 ? candidate : base;
    };
    if (::android::base::GetProperty("ro.csc.country_code", "") == "Canada" ||
        ::android::base::GetProperty("ro.csc.countryiso_code", "") == "CA")
        select("CA");
    if (::android::base::GetProperty("ro.csc.sales_code", "") == "KDI") select("KDI");
    const auto carrier = ::android::base::GetProperty("ro.csc.omcnw_code", "");
    for (const auto* cdma :
         {"VZW", "SPR", "USC", "BST", "VMU", "XAS", "CCT", "CHA", "TFV", "ACG", "LRA"}) {
        if (carrier == cdma) {
            select("CDMA");
            break;
        }
    }
    return selected;
}

const Config& config() {
    static const auto values = [] {
        Config result;
        FILE* input = std::fopen(configFile().c_str(), "r");
        if (!input) return result;
        char buffer[1024];
        while (std::fgets(buffer, sizeof(buffer), input)) {
            const auto line = trim(buffer);
            if (line.empty() || line.front() == '#') continue;
            const auto equal = line.find('=');
            if (equal == std::string::npos) continue;
            const auto key = trim(line.substr(0, equal));
            if (key.empty()) continue;
            auto value = trim(line.substr(equal + 1));
            if (value == "TRUE")
                value = "1";
            else if (value == "FALSE")
                value = "0";
            result[key].push_back(std::move(value));
        }
        std::fclose(input);
        return result;
    }();
    return values;
}

const std::string* numericValue(const char* key) {
    const auto& values = config();
    const auto entry = values.find(key);
    if (entry == values.end()) return nullptr;
    if (entry->second.size() != 1) {
        ALOGE("Duplicate GNSS configuration key: %s", key);
        return nullptr;
    }
    return &entry->second.front();
}
}  // namespace

std::string readConfigString(const char* key, const std::string& fallback) {
    const auto& values = config();
    const auto entry = values.find(key);
    if (entry == values.end()) return fallback;
    const auto& value = entry->second.front();
    if ((std::string(key) == "debug_dir" || std::string(key) == "gnss_data_dir") &&
        std::string("/data/vendor/").find(value) == std::string::npos)
        return fallback;
    return value;
}

int readConfigInt(const char* key, int fallback) {
    const auto* text = numericValue(key);
    int value;
    return text && std::sscanf(text->c_str(), "%i", &value) == 1 ? value : fallback;
}

bool readConfigBool(const char* key, bool fallback) {
    const auto* text = numericValue(key);
    if (!text) return fallback;
    int value;
    if (std::sscanf(text->c_str(), "%i", &value) != 1) return false;
    return value == 0 || value == 1 ? value != 0 : fallback;
}

std::string readChipType() {
    const auto configured = readConfigString("gnss_device_type", "");
    if (!configured.empty()) return configured.substr(0, 4);
    const auto hardware = ::android::base::GetProperty("ro.hardware", "");
    for (const auto& [soc, chip] :
         std::initializer_list<std::pair<const char*, const char*>>{{"exynos7570", "K142"},
                                                                    {"exynos7870", "K140"},
                                                                    {"exynos7885", "K148"},
                                                                    {"exynos7884", "K148"},
                                                                    {"exynos7904", "K148"},
                                                                    {"exynos7905", "K148"},
                                                                    {"exynos9610", "K102"},
                                                                    {"exynos9611", "K102"},
                                                                    {"exynos9110", "K103"},
                                                                    {"exynos3830", "K080"},
                                                                    {"exynos850", "K080"},
                                                                    {"exynos9630", "K081"},
                                                                    {"exynos880", "K081"},
                                                                    {"exynos980", "K081"},
                                                                    {"exynos9815", "K052"},
                                                                    {"s5e9815", "K052"},
                                                                    {"exynos5515", "K053"},
                                                                    {"s5e5515", "K053"},
                                                                    {"exynos2100", "K149"}}) {
        if (hardware.find(soc) != std::string::npos) return chip;
    }
    if (hardware.find("exynos2200") != std::string::npos ||
        hardware.find("s5e9925") != std::string::npos) {
        int version = 0;
        if (FILE* input = std::fopen("/sys/devices/system/chip-id/evt_ver", "r")) {
            char text[64]{};
            if (std::fgets(text, sizeof(text), input)) version = std::atoi(text + 3);
            std::fclose(input);
        }
        return version == 0 ? "K040" : version == 1 ? "K401" : "Kxxx";
    }
    if (hardware.find("exynosp150") != std::string::npos ||
        hardware.find("s5e8825") != std::string::npos)
        return "K050";
    return "Kxxx";
}

}  // namespace aidl::android::hardware::gnss::protocol
