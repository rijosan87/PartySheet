#pragma once

// STATUS: Working
// Calculates font scaling for resolution adjustments.


// ============================================================================
// Font Scaling System
// ============================================================================

namespace FontScale {
    constexpr float BASE_FONT_SIZE = 32.0f;

    constexpr float Tiny   = 12.0f / BASE_FONT_SIZE;
    constexpr float Small  = 14.0f / BASE_FONT_SIZE;
    constexpr float Menu   = 16.0f / BASE_FONT_SIZE;
    constexpr float Normal = 18.0f / BASE_FONT_SIZE;
    constexpr float Medium = 22.0f / BASE_FONT_SIZE;
    constexpr float Large  = 26.0f / BASE_FONT_SIZE;
    constexpr float Header = 30.0f / BASE_FONT_SIZE;
    constexpr float Full   = 1.0f;

    constexpr float ForSize(float targetPixels) {
        return targetPixels / BASE_FONT_SIZE;
    }

    inline float GetScale(float targetPixels, float actualBaseFontSize) {
        return targetPixels / actualBaseFontSize;
    }
}
