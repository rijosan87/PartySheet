#pragma once

#include "Settings.h"

#include <array>
#include <cstddef>

namespace PlayerHUDEquipmentPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 4> kPresets{ {
        { "1 - Default" },
        { "2 - Compact" },
        { "3 - Minimal" },
        { "4 - Weapon Focus" },
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.peLayout          = 0;
        s.peIconSize        = 80;
        s.peSlotSpacing     = 50;
        s.peShowNames       = true;
        s.peShowSlotBadge   = true;
        s.peShowAmmoCount   = true;
        s.peNameColor       = 0xFFFFFFFF;
        s.peNameBgColor     = 0x00000000;
        s.peNameAlignment   = 0;
        s.peNameFontScale   = 0.85f;
        s.peBadgeFontScale  = 0.75f;
        s.peBadgeBgColor    = 0xDB4B4B4B;
        s.peBadgeOffsetX    = -4;
        s.peBadgeOffsetY    = -4;
        s.peShowDamage      = true;
        s.peIconBgEnabled   = true;
        s.peIconBgShape     = 0;
        s.peIconBgColor     = 0x99000000;
        s.peIconBorderColor = 0xFF3E3D32;
        s.peIconBorderWidth = 0;
        s.peIconBorderRadius = 15;
        s.peIconBgStyle     = 0;
        s.peIconBgFadeMode  = 0;
        s.peIconBgFadeAnchor = 25;
        s.peIconBgFadeLength = 75;
        s.peIconBgPadding   = 8;
        s.peUseIconBgTexture = true;
        s.peIconBgTexOpacity = 0.7f;
        s.peShowAura        = true;
        s.peSlotScale0      = 1.0f;
        s.peSlotScale1      = 1.0f;
        s.peSlotScale2      = 1.0f;
        s.peSlotOffsetX1    = 0;
        s.peSlotOffsetY1    = 0;
        s.peSlotOffsetX2    = 0;
        s.peSlotOffsetY2    = 0;
        s.peIconZoom        = 1.25f;
        s.peIconFocusU      = 0.5f;
        s.peIconFocusV      = 0.5f;
        s.peWeaponRotation  = 0.45f;
        s.peGlyphScale      = 0.62f;
        s.peGlyphColor      = 0xFFE8E4DA;
        s.peGlyphTintByElement = true;
    }

    inline void ApplyCompact(Settings& s)
    {
        s.peLayout          = 0;
        s.peIconSize        = 62;
        s.peSlotSpacing     = 28;
        s.peShowNames       = true;
        s.peShowSlotBadge   = false;
        s.peShowAmmoCount   = true;
        s.peNameColor       = 0xFFFFFFFF;
        s.peNameBgColor     = 0x00000000;
        s.peNameAlignment   = 0;
        s.peNameFontScale   = 0.75f;
        s.peBadgeFontScale  = 0.65f;
        s.peBadgeBgColor    = 0xBB333333;
        s.peBadgeOffsetX    = 0;
        s.peBadgeOffsetY    = 0;
        s.peShowDamage      = true;
        s.peIconBgEnabled   = true;
        s.peIconBgShape     = 2;
        s.peIconBgColor     = 0xAA000000;
        s.peIconBorderColor = 0x80555544;
        s.peIconBorderWidth = 0;
        s.peIconBorderRadius = 10;
        s.peIconBgStyle     = 0;
        s.peIconBgFadeMode  = 0;
        s.peIconBgFadeAnchor = 25;
        s.peIconBgFadeLength = 75;
        s.peIconBgPadding   = 5;
        s.peUseIconBgTexture = true;
        s.peIconBgTexOpacity = 0.7f;
        s.peShowAura        = true;
        s.peSlotScale0      = 1.0f;
        s.peSlotScale1      = 1.0f;
        s.peSlotScale2      = 1.0f;
        s.peSlotOffsetX1    = 0;
        s.peSlotOffsetY1    = 0;
        s.peSlotOffsetX2    = 0;
        s.peSlotOffsetY2    = 0;
        s.peIconZoom        = 1.0f;
        s.peIconFocusU      = 0.5f;
        s.peIconFocusV      = 0.12f;
        s.peWeaponRotation  = 0.45f;
        s.peGlyphScale      = 0.62f;
        s.peGlyphColor      = 0xFFE8E4DA;
        s.peGlyphTintByElement = true;
    }

    inline void ApplyMinimal(Settings& s)
    {
        s.peLayout          = 0;
        s.peIconSize        = 64;
        s.peSlotSpacing     = 36;
        s.peShowNames       = false;
        s.peShowSlotBadge   = false;
        s.peShowAmmoCount   = false;
        s.peNameColor       = 0xDDFFFFFF;
        s.peNameBgColor     = 0x00000000;
        s.peNameAlignment   = 0;
        s.peNameFontScale   = 0.70f;
        s.peBadgeFontScale  = 0.70f;
        s.peBadgeBgColor    = 0x00000000;
        s.peBadgeOffsetX    = 0;
        s.peBadgeOffsetY    = 0;
        s.peShowDamage      = true;
        s.peIconBgEnabled   = false;
        s.peIconBgShape     = 0;
        s.peIconBgColor     = 0x00000000;
        s.peIconBorderColor = 0x00000000;
        s.peIconBorderWidth = 0;
        s.peIconBorderRadius = 0;
        s.peIconBgStyle     = 0;
        s.peIconBgFadeMode  = 0;
        s.peIconBgFadeAnchor = 25;
        s.peIconBgFadeLength = 75;
        s.peIconBgPadding   = 0;
        s.peUseIconBgTexture = false;
        s.peIconBgTexOpacity = 0.7f;
        s.peShowAura        = false;
        s.peSlotScale0      = 1.0f;
        s.peSlotScale1      = 1.0f;
        s.peSlotScale2      = 1.0f;
        s.peSlotOffsetX1    = 0;
        s.peSlotOffsetY1    = 0;
        s.peSlotOffsetX2    = 0;
        s.peSlotOffsetY2    = 0;
        s.peIconZoom        = 1.2f;
        s.peIconFocusU      = 0.5f;
        s.peIconFocusV      = 0.5f;
        s.peWeaponRotation  = 0.45f;
        s.peGlyphScale      = 0.62f;
        s.peGlyphColor      = 0xFFE8E4DA;
        s.peGlyphTintByElement = true;
    }

    inline void ApplyWeaponFocus(Settings& s)
    {
        ApplyDefault(s);
        s.peIconBgTexOpacity = 1.0f; 
        s.peSlotSpacing  = 30;
        s.peIconBorderWidth = 0;
        s.peIconBorderRadius = 0;
        s.peIconZoom   = 1.25f;
        s.peSlotScale0 = 1.35f;
        s.peSlotScale1 = 0.80f;
        s.peSlotScale2 = 0.80f;
        s.peWeaponRotation  = 0.20f;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s); break;
        case 1:  ApplyCompact(s); break;
        case 2:  ApplyMinimal(s); break;
        case 3:  ApplyWeaponFocus(s); break;
        default: ApplyDefault(s); break;
        }
    }
}
