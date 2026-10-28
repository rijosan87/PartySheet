#pragma once

// STATUS: Working
// Presets for pinned quick-use items.


#include "Settings.h"

#include <array>
#include <cstddef>

namespace PinnedItemsPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 3> kPresets{ {
        { "1 - Round Row" },
        { "2 - Row" },
        { "3 - Compact" }
    } };

    inline void ApplyRoundRow(Settings& s)
    {
        s.ptDirection        = 1;
        s.ptCardStyle        = 0;
        s.ptCountStyle       = 1;
        s.ptIconSize         = 50;
        s.ptIconBgColor      = 0x79000000;
        s.ptIconBorderWidth  = 0;
        s.ptIconBorderRadius = 50;
        s.ptIconBorderColor  = 0x7AE29F17;
        s.ptIconGlowEnabled  = false;
        s.ptBadgeBgColor     = 0xCF000000;
        s.ptBadgeOffsetX     = 24;
        s.ptBadgeOffsetY     = 50;
        s.ptLabelScale       = 0.9f;
        s.ptCountScale       = 0.6f;
        s.ptShowLabel        = false;
        s.ptItemWidth        = 20;
        s.ptRowSpacing       = -15;
        s.ptPaddingBottom    = 8;
        s.ptTextColor        = 0xFFFFFF;
        s.ptGoalMetColor     = 0x33B333;
        s.ptIconZoom         = 1.0f;
        s.ptIconFocusU       = 0.5f;
        s.ptIconFocusV       = 0.5f;
        s.ptShowHotkeyBadge  = false;
    }

    inline void ApplyRow(Settings& s)
    {
        s.ptDirection        = 1;
        s.ptCardStyle        = 0;
        s.ptCountStyle       = 1;
        s.ptIconSize         = 48;
        s.ptIconBgColor      = 0x73000000;
        s.ptIconBorderWidth  = 2;
        s.ptIconBorderRadius = 18;
        s.ptIconBorderColor  = 0x6BE0E0E0;
        s.ptIconGlowEnabled  = false;
        s.ptBadgeBgColor     = 0xBE006758;
        s.ptBadgeOffsetX     = 6;
        s.ptBadgeOffsetY     = 0;
        s.ptLabelScale       = 0.9f;
        s.ptCountScale       = 0.5f;
        s.ptShowLabel        = false;
        s.ptItemWidth        = 64;
        s.ptRowSpacing       = -15;
        s.ptPaddingBottom    = 8;
        s.ptTextColor        = 0xFFFFFF;
        s.ptGoalMetColor     = 0x33B333;
        s.ptIconZoom         = 1.0f;
        s.ptIconFocusU       = 0.5f;
        s.ptIconFocusV       = 0.5f;
        s.ptShowHotkeyBadge  = true;
        s.ptHotkeyBadgeBgColor = 0x9B6E0000;
        s.ptHotkeyBadgeOffsetX = 30;
        s.ptHotkeyBadgeOffsetY = 60;
    }

    inline void ApplyCompact(Settings& s)
    {
        s.ptDirection        = 0;
        s.ptCardStyle        = 0;
        s.ptCountStyle       = 1;
        s.ptIconSize         = 48;
        s.ptIconBgColor      = 0x73000000;
        s.ptIconBorderWidth  = 0;
        s.ptIconBorderRadius = 50;
        s.ptIconBorderColor  = 0x6BE0E0E0;
        s.ptIconGlowEnabled  = false;
        s.ptBadgeBgColor     = 0x21000000;
        s.ptBadgeOffsetX     = 35;
        s.ptBadgeOffsetY     = 40;
        s.ptLabelScale       = 0.7f;
        s.ptCountScale       = 0.55f;
        s.ptShowLabel        = false;
        s.ptItemWidth        = 64;
        s.ptRowSpacing       = -5;
        s.ptPaddingBottom    = 0;
        s.ptTextColor        = 0xFFFFFF;
        s.ptGoalMetColor     = 0x33B333;
        s.ptIconZoom         = 1.0f;
        s.ptIconFocusU       = 0.5f;
        s.ptIconFocusV       = 0.5f;
        s.ptShowHotkeyBadge  = false;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyRoundRow(s);  break;
        case 1:  ApplyRow(s);     break;
        case 2:  ApplyCompact(s); break;
        default: ApplyRow(s);     break;
        }
    }
}
