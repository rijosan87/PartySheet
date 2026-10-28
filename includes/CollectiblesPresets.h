#pragma once

// STATUS: Working
// Presets for tracked collectible items.


#include "Settings.h"

#include <array>
#include <cstddef>

namespace CollectiblesPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 3> kPresets{ {
        { "1 - Default" },
        { "2 - Compact" },
        { "3 - Showcase" }
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.ccDirection       = 0;
        s.ccCardStyle       = 0;
        s.ccIconSize        = 54;
        s.ccLabelScale      = 0.9f;
        s.ccBarTextScale    = 0.9f;
        s.ccShowLabel       = false;
        s.ccBarWidth        = 72;
        s.ccBarHeight       = 14;
        s.ccBarColor        = 0xFF6E25;
        s.ccBarStyleEnabled = true;
        s.ccBarStyleIndex   = 5;
        s.ccRowSpacing      = -6;
        s.ccPaddingBottom   = 8;
        s.ccTextColor       = 0xFFFFFF;
        s.ccGoalMetColor    = 0x33B333;
        s.ccBgColor         = 0x000A0A0A;
        s.ccIconBgColor     = 0x7B000000;
    }

    inline void ApplyCompact(Settings& s)
    {
        s.ccDirection       = 0;
        s.ccCardStyle       = 0;
        s.ccIconSize        = 42;
        s.ccLabelScale      = 0.8f;
        s.ccBarTextScale    = 0.8f;
        s.ccShowLabel       = false;
        s.ccBarWidth        = 40;
        s.ccBarHeight       = 8;
        s.ccBarColor        = 0xB4782A;
        s.ccBarStyleEnabled = false;
        s.ccBarStyleIndex   = 5;
        s.ccRowSpacing      = -4;
        s.ccPaddingBottom   = 4;
        s.ccTextColor       = 0xD2D2D2;
        s.ccGoalMetColor    = 0x50C050;
        s.ccBgColor         = 0x00000000;
        s.ccIconBgColor     = 0x00000000;
    }

    inline void ApplyShowcase(Settings& s)
    {
        s.ccDirection       = 0;
        s.ccCardStyle       = 1;
        s.ccIconSize        = 60;
        s.ccLabelScale      = 1.0f;
        s.ccBarTextScale    = 0.8f;
        s.ccShowLabel       = false;
        s.ccBarWidth        = 60;
        s.ccBarHeight       = 16;
        s.ccBarColor        = 0xFFAA30;
        s.ccBarStyleEnabled = true;
        s.ccBarStyleIndex   = 5;
        s.ccRowSpacing      = -10;
        s.ccPaddingBottom   = 12;
        s.ccTextColor       = 0xF0E0B0;
        s.ccGoalMetColor    = 0x78E878;
        s.ccBgColor         = 0x00060610;
        s.ccIconBgColor     = 0xE1000000;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s);   break;
        case 1:  ApplyCompact(s);   break;
        case 2:  ApplyShowcase(s);  break;
        default: ApplyDefault(s);   break;
        }
    }
}
