#pragma once

// STATUS: Working
// Presets for pinned quick-use items.

#include "Settings.h"
#include "Utils/HUDDraw.h"

#include <array>
#include <cstddef>

namespace PinnedItemsPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 3> kPresets{ {
        { "Default" },
        { "Minimal" },
        { "Quick List" }
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.ptDirection        = 1;
        s.ptLabelPlacement   = 0;
        s.ptCountStyle       = HUDDraw::BadgePreset_Classic;
        s.ptCountAnchor      = 0;
        s.ptBadgeTint        = 0;
        s.ptIconSize         = 48;
        s.ptIconBgColor      = 0x79000000;
        s.ptIconBorderWidth  = 0;
        s.ptIconBorderRadius = 30;
        s.ptIconBorderColor  = 0x6BE0E0E0;
        s.ptIconGlowEnabled  = false;
        s.ptBadgeOffsetX     = 42;
        s.ptBadgeOffsetY     = 0;
        s.ptLabelScale       = 0.9f;
        s.ptCountScale       = 0.55f;
        s.ptShowLabel        = false;
        s.ptItemWidth        = 60;
        s.ptRowSpacing       = -8;
        s.ptAnchor           = 0;
        s.ptTextColor        = 0xFFFFFF;
        s.ptGoalMetColor     = 0x33B333;
        s.ptIconZoom         = 1.0f;
        s.ptIconFocusU       = 0.5f;
        s.ptIconFocusV       = 0.5f;
        s.ptHotkeyBadgeStyle = HUDDraw::BadgePreset_Plain;
        s.ptHotkeyBadgeTint  = 0;
        s.ptHotkeyBadgeScale = 0.9f;
        s.ptStripStyle       = HUDDraw::StripStyle_Off;
        s.ptStripWidth       = 0;
        s.ptStripTint        = 0xB4141414;
        s.ptCountPosition    = 0;
        s.ptHotkeyPosition   = 0;
        s.ptHotkeyBadgeOffsetX = 18;
        s.ptHotkeyBadgeOffsetY = 45;
    }

    inline void ApplyMinimal(Settings& s)
    {
        ApplyDefault(s);
        s.ptCountStyle         = HUDDraw::BadgePreset_Glass;
        s.ptIconSize           = 54;
        s.ptIconBorderRadius   = 15;
        s.ptIconZoom           = 0.75f;
        s.ptBadgeOffsetX       = 12;
        s.ptBadgeOffsetY       = 0;
        s.ptHotkeyBadgeStyle   = HUDDraw::BadgePreset_Plain;
        s.ptHotkeyBadgeTint    = 0xE4E12D2D;
        s.ptHotkeyBadgeScale   = 0.80f;
        s.ptHotkeyBadgeOffsetX = 33;
        s.ptHotkeyBadgeOffsetY = 31;
    }

    inline void ApplyQuickList(Settings& s)
    {
        ApplyDefault(s);
        s.ptDirection          = 0;
        s.ptLabelPlacement     = 3;
        s.ptShowLabel          = true;
        s.ptCountStyle         = HUDDraw::BadgePreset_Plain;
        s.ptBadgeTint          = 0xE7444341;
        s.ptBadgeOffsetX       = 3;
        s.ptBadgeOffsetY       = 7;
        s.ptIconBgColor        = 0xFF0C0C0C;
        s.ptIconBorderWidth    = 2;
        s.ptIconBorderRadius = 50;
        s.ptIconBorderColor    = 0xFF2D2626;
        s.ptIconZoom           = 0.75f;
        s.ptHotkeyBadgeStyle   = HUDDraw::BadgePreset_Plain;
        s.ptHotkeyBadgeTint    = 0xE4E12D2D;
        s.ptHotkeyBadgeScale   = 0.8f;
        s.ptHotkeyBadgeOffsetX = 33;
        s.ptHotkeyBadgeOffsetY = 31;
        s.ptStripStyle         = HUDDraw::StripStyle_Fade;
        s.ptStripWidth         = 210;
        s.ptStripTint          = 0xB4141414;
        s.ptCountPosition      = 0;
        s.ptHotkeyPosition     = 1;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s);  break;
        case 1:  ApplyMinimal(s);  break;
        case 2:  ApplyQuickList(s); break;
        default: ApplyDefault(s);  break;
        }
    }
}
