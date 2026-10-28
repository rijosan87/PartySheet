#pragma once

// STATUS: Working
// Safe std::filesystem::path UTF-8 conversion.

#include <filesystem>
#include <string>

namespace PathUtils {
    inline std::string ToUtf8(const std::filesystem::path& p) {
        const auto u8 = p.u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }
}
