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
        { "Default" },
        { "Diamond" },
        { "Crest" },
        { "Ladder" },
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
        s.peNamePlacement   = 0;
        s.peNameSideWidth   = 120;
        s.peNameGap         = 4;
        s.peBadgeAnchor     = 0;
        s.peEffectBadgePlace = 0;
        s.peEffectBadgeSize  = 28;
        s.peEffectBadgeStyle = 0;
        s.peEffectBadgeMax   = 3;
        s.peNameMaxChars     = 20;
        s.peNameFontScale   = 0.85f;
        s.peBadgeFontScale  = 0.90f;
        s.peDurabilityScale = 0.4f;
        s.peBadgeBgColor    = 0xDB4B4B4B;
        s.peBadgeOffsetX    = -4;
        s.peBadgeOffsetY    = -4;
        s.peShowDamage      = true;
        s.peIconBgEnabled   = true;
        s.peIconBgShape     = 0;
        s.peIconBgColor     = 0xFF373737;
        s.peIconBorderColor = 0xFF3E3D32;
        s.peIconBorderWidth = 0;
        s.peIconBorderRadius = 15;
        s.peIconBgStyle     = 0;
        s.peIconBgFadeMode  = 0;
        s.peIconBgFadeAnchor = 25;
        s.peIconBgFadeLength = 75;
        s.peIconBgPadding   = 8;
        s.peUseIconBgTexture = false;
        s.peIconBgTexOpacity = 1.0f;
        s.peIconBgIndex      = 1;
        s.peShowAura        = true;
        s.peShowEffectBadges = true;
        s.peSlotScale0      = 1.0f;
        s.peSlotScale1      = 1.0f;
        s.peSlotScale2      = 1.0f;
        s.peSlotShape0      = -1;
        s.peSlotShape1      = -1;
        s.peSlotShape2      = -1;
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
        s.peStripStyle      = 0;
        s.peStripWidth      = 0;
        s.peStripTint       = 0xB4141414;
        s.peStackOverlap    = 25;
        s.peStackShiftX     = 0;
        s.peClusterAngle1   = 210;
        s.peClusterAngle2   = 150;
        s.peSatelliteNamesBelow = false;
        s.peCenterNameSide      = 1;
    }

    inline void ApplyDiamond(Settings& s)
    {
        ApplyDefault(s);
        s.peIconBgIndex      = 11;
        s.peUseIconBgTexture = true;
        s.peIconSize         = 94;
        s.peSlotSpacing      = 40;
        s.peIconBgShape      = 2;
        s.peIconBgColor      = 0xC80C0D10;
        s.peIconBorderColor  = 0xFFC9A66B;
        s.peIconBorderWidth  = 2;
        s.peIconBorderRadius = 0;
        s.peIconBgPadding    = 4;
        s.peIconBgTexOpacity = 1.0f;
        s.peShowEffectBadges = false;
        s.peIconZoom         = 1.1f;
        s.peBadgeOffsetX     = 0;
        s.peBadgeOffsetY     = -6;
        s.peBadgeBgColor     = 0xE0141210;
        s.peNameAlignment    = 1;
        s.peNameFontScale    = 0.78f;
    }

    inline void ApplyCrest(Settings& s)
    {
        ApplyDefault(s);
        s.peIconBgIndex      = 9;
        s.peUseIconBgTexture = true;
        s.peIconBgEnabled    = false;
        s.peIconBgShape      = 1;
        s.peIconBgColor      = 0xFF282828;
        s.peIconBorderColor  = 0xA07C735E;
        s.peIconBorderWidth  = 2;
        s.peIconBorderRadius = 0;
        s.peIconBgPadding    = 4;
        s.peIconBgTexOpacity = 1.0f;
        s.peIconZoom         = 1.0f;
        s.peWeaponRotation   = 0.0f;
        s.peBadgeOffsetX    = 0;
        s.peBadgeOffsetY    = 0;
        s.peBadgeBgColor     = 0xE0202024;
        s.peShowAura        = true;
        s.peLayout        = 3;
        s.peIconSize      = 72;
        s.peSlotSpacing   = 60;
        s.peSlotOrder0    = 0;
        s.peSlotOrder1    = 1;
        s.peSlotOrder2    = 2;
        s.peSlotScale0    = 1.4f;
        s.peSlotScale1    = 1.1f;
        s.peSlotScale2    = 1.1f;
        s.peClusterAngle1 = 210;
        s.peClusterAngle2 = 150;
        s.peSlotOffsetX1    = 0;
        s.peSlotOffsetY1  = -15;
        s.peSlotOffsetX2    = 0;
        s.peSlotOffsetY2    = 0;
        s.peNamePlacement = 2;
        s.peNameAlignment = 0;
        s.peBadgeAnchor   = 4;
        s.peNameFontScale = 0.9f;
        s.peNameGap       = 12;
        s.peNameSideWidth = 110;
        s.peNameMaxChars  = 30;
        s.peCenterNameSide = 0;
        s.peSlotShape0    = 2;
        s.peSlotShape1    = 2;
        s.peStripStyle    = 2;
        s.peStripWidth    = 150;
        s.peEffectBadgeMax   = 5;
        s.peEffectBadgePlace = 1;
        s.peEffectBadgeStyle = 5;
    }

    inline void ApplyLadder(Settings& s)
    {
        ApplyDefault(s);
        s.peIconBgIndex      = 11;
        s.peIconBgShape      = 1;
        s.peIconBgColor      = 0xC0000000;
        s.peIconBorderColor  = 0xA0D8D2C4;
        s.peIconBorderWidth  = 2;
        s.peIconBorderRadius = 0;
        s.peIconBgPadding    = 4;
        s.peIconBgTexOpacity = 0.8f;
        s.peIconZoom         = 1.15f;
        s.peBadgeOffsetX    = 0;
        s.peBadgeOffsetY    = 0;
        s.peBadgeBgColor     = 0xE0202024;
        s.peShowAura         = true;
        s.peLayout        = 5;
        s.peIconSize      = 64;
        s.peSlotSpacing  = 30;
        s.peStackOverlap  = 15;
        s.peStackShiftX   = 14;
        s.peSlotScale0    = 1.15f;
        s.peNamePlacement = 2;
        s.peNameAlignment = 2;
        s.peBadgeAnchor   = 5;
        s.peEffectBadgePlace = 1;
        s.peEffectBadgeSize  = 35;
        s.peEffectBadgeStyle = 3;
        s.peNameFontScale = 0.9f;
        s.peNameGap       = 10;
        s.peNameSideWidth = 130;
        s.peStripStyle    = 1;
        s.peStripWidth    = 160;
        s.peStripTint     = 0xC8101012;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s); break;
        case 1:  ApplyDiamond(s); break;
        case 2:  ApplyCrest(s); break;
        case 3:  ApplyLadder(s); break;
        default: ApplyDefault(s); break;
        }
    }
}
