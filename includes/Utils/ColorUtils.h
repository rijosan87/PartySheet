#pragma once

// STATUS: Working
// Color packing and conversion helpers.

#include <imgui.h>
#include <cstdint>
#include <algorithm>

namespace ColorUtils {
    inline ImVec4 RgbToVec4(std::uint32_t hex, float alpha = 1.0f)
    {
        return ImVec4(
            static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
            static_cast<float>((hex >> 8)  & 0xFF) / 255.0f,
            static_cast<float>( hex        & 0xFF) / 255.0f,
            alpha
        );
    }

    inline ImVec4 ArgbToVec4(std::uint32_t hex)
    {
        return ImVec4(
            static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
            static_cast<float>((hex >> 8)  & 0xFF) / 255.0f,
            static_cast<float>( hex        & 0xFF) / 255.0f,
            static_cast<float>((hex >> 24) & 0xFF) / 255.0f
        );
    }

    inline std::uint32_t Vec4ToRgb(const ImVec4& c)
    {
        auto b = [](float v) -> std::uint32_t {
            return static_cast<std::uint32_t>((std::clamp)(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return (b(c.x) << 16) | (b(c.y) << 8) | b(c.z);
    }

    inline std::uint32_t Vec4ToArgb(const ImVec4& c)
    {
        auto b = [](float v) -> std::uint32_t {
            return static_cast<std::uint32_t>((std::clamp)(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return (b(c.w) << 24) | (b(c.x) << 16) | (b(c.y) << 8) | b(c.z);
    }
}
