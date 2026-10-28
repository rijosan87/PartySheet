#pragma once

// STATUS: Working
// Presets for Player HUD Active Effects widget.


#include "Settings.h"

#include <array>
#include <cstddef>

namespace PlayerHUDEffectsPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 5> kPresets{ {
        { "1 - Default"        },
        { "2 - Compact"        },
        { "3 - Floating Icons" },
        { "4 - Timeline"       },
        { "5 - Minimal"        },
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.phEffectColumnDirection   = 0;
        s.phEffectMaxIcons          = 16;
        s.phEffectMaxPerColumn      = 6;
        s.phEffectIconSize          = 48;
        s.phEffectVertSpacing       = 40;
        s.phEffectHorizSpacing      = 20;
        s.phShowEffectTimer         = true;
        s.phEffectTimerStyle        = 2;
        s.phEffectTimerBorderWidth  = 2;
        s.phEffectBuffColor         = 0xFF44FF44;
        s.phEffectDebuffColor       = 0xFFFF4444;
        s.phShowEffectMagnitude     = true;
        s.phEffectMagnitudeScale    = 0.8f;
        s.phShowEffectTimeText      = true;
        s.phEffectTimeTextScale     = 0.9f;
        s.phEffectTimeTextColor     = 0xFFFFFFFF;
        s.phEffectIconBgEnabled     = true;
        s.phEffectIconBgColor       = 0x80000000;
        s.phEffectIconGlowEnabled   = false;
        s.phEffectIconBorderColor   = 0xFFFFFFFF;
        s.phEffectIconBorderWidth   = 0;
        s.phEffectIconBorderRadius  = 50;
        s.phEffectIconBgPadding     = 3;
        s.phEffectIconZoom          = 1.0f;
        s.phEffectIconFocusU        = 0.5f;
        s.phEffectIconFocusV        = 0.5f;
        s.phEffectShowSourceBadge   = true;
        s.phEffectSourceBadgeScale  = 0.45f;
        s.phEffectSourceBadgeTint   = 0xFF3DCAC2;
        s.phEffectSourceBadgeOffsetY = 7;
    }

    inline void ApplyCompact(Settings& s)
    {
        s.phEffectColumnDirection   = 0;
        s.phEffectMaxIcons          = 12;
        s.phEffectMaxPerColumn      = 12;
        s.phEffectIconSize          = 32;
        s.phEffectVertSpacing       = 28;
        s.phEffectHorizSpacing      = 10;
        s.phShowEffectTimer         = true;
        s.phEffectTimerStyle        = 0;
        s.phEffectTimerBorderWidth  = 2;
        s.phEffectBuffColor         = 0xFF44FF44;
        s.phEffectDebuffColor       = 0xFFFF4444;
        s.phShowEffectMagnitude     = true;
        s.phEffectMagnitudeScale    = 0.7f;
        s.phShowEffectTimeText      = false;
        s.phEffectTimeTextScale     = 0.8f;
        s.phEffectTimeTextColor     = 0xFFCCCCCC;
        s.phEffectIconBgEnabled     = true;
        s.phEffectIconBgColor       = 0x99000000;
        s.phEffectIconGlowEnabled   = false;
        s.phEffectIconBorderColor   = 0x80FFFFFF;
        s.phEffectIconBorderWidth   = 1;
        s.phEffectIconBorderRadius  = 50;
        s.phEffectIconBgPadding     = 2;
        s.phEffectIconZoom          = 1.0f;
        s.phEffectIconFocusU        = 0.5f;
        s.phEffectIconFocusV        = 0.5f;
        s.phEffectShowSourceBadge   = false;
        s.phEffectSourceBadgeScale  = 0.35f;
        s.phEffectSourceBadgeTint   = 0xFF3DCAC2;
        s.phEffectSourceBadgeOffsetY = 5;
    }

    inline void ApplyFloatingIcons(Settings& s)
    {
        s.phEffectColumnDirection   = 0;
        s.phEffectMaxIcons          = 10;
        s.phEffectMaxPerColumn      = 5;
        s.phEffectIconSize          = 44;
        s.phEffectVertSpacing       = 16;
        s.phEffectHorizSpacing      = 14;
        s.phShowEffectTimer         = true;
        s.phEffectTimerStyle        = 2;
        s.phEffectTimerBorderWidth  = 2;
        s.phEffectBuffColor         = 0xFFFFD060;
        s.phEffectDebuffColor       = 0xFFFF5050;
        s.phShowEffectMagnitude     = true;
        s.phEffectMagnitudeScale    = 0.8f;
        s.phShowEffectTimeText      = false;
        s.phEffectTimeTextScale     = 0.9f;
        s.phEffectTimeTextColor     = 0xFFFFFFFF;
        s.phEffectIconBgEnabled     = true;
        s.phEffectIconBgColor       = 0x55100C00;
        s.phEffectIconGlowEnabled   = true;
        s.phEffectIconBorderColor   = 0xAAC8960A;
        s.phEffectIconBorderWidth   = 2;
        s.phEffectIconBorderRadius  = 12;
        s.phEffectIconBgPadding     = 4;
        s.phEffectIconZoom          = 1.1f;
        s.phEffectIconFocusU        = 0.5f;
        s.phEffectIconFocusV        = 0.5f;
        s.phEffectShowSourceBadge   = false;
        s.phEffectSourceBadgeScale  = 0.45f;
        s.phEffectSourceBadgeTint   = 0xFFFFD060;
        s.phEffectSourceBadgeOffsetY = 7;
    }

    inline void ApplyTimeline(Settings& s)
    {
        s.phEffectColumnDirection   = 0;
        s.phEffectMaxIcons          = 12;
        s.phEffectMaxPerColumn      = 6;
        s.phEffectIconSize          = 44;
        s.phEffectVertSpacing       = 38;
        s.phEffectHorizSpacing      = 18;
        s.phShowEffectTimer         = true;
        s.phEffectTimerStyle        = 1;
        s.phEffectTimerBorderWidth  = 3;
        s.phEffectBuffColor         = 0xFF60CFFF;
        s.phEffectDebuffColor       = 0xFFFF6060;
        s.phShowEffectMagnitude     = true;
        s.phEffectMagnitudeScale    = 0.75f;
        s.phShowEffectTimeText      = true;
        s.phEffectTimeTextScale     = 0.85f;
        s.phEffectTimeTextColor     = 0xFFDDDDDD;
        s.phEffectIconBgEnabled     = true;
        s.phEffectIconBgColor       = 0xA0101018;
        s.phEffectIconGlowEnabled   = false;
        s.phEffectIconBorderColor   = 0x60FFFFFF;
        s.phEffectIconBorderWidth   = 1;
        s.phEffectIconBorderRadius  = 8;
        s.phEffectIconBgPadding     = 4;
        s.phEffectIconZoom          = 1.0f;
        s.phEffectIconFocusU        = 0.5f;
        s.phEffectIconFocusV        = 0.5f;
        s.phEffectShowSourceBadge   = true;
        s.phEffectSourceBadgeScale  = 0.40f;
        s.phEffectSourceBadgeTint   = 0xFFFFD080;
        s.phEffectSourceBadgeOffsetY = 6;
    }

    inline void ApplyMinimal(Settings& s)
    {
        s.phEffectColumnDirection   = 1;
        s.phEffectMaxIcons          = 12;
        s.phEffectMaxPerColumn      = 12;
        s.phEffectIconSize          = 28;
        s.phEffectVertSpacing       = 6;
        s.phEffectHorizSpacing      = 6;
        s.phShowEffectTimer         = true;
        s.phEffectTimerStyle        = 2;
        s.phEffectTimerBorderWidth  = 2;
        s.phEffectBuffColor         = 0xFF44FF44;
        s.phEffectDebuffColor       = 0xFFFF4444;
        s.phShowEffectMagnitude     = false;
        s.phEffectMagnitudeScale    = 0.7f;
        s.phShowEffectTimeText      = false;
        s.phEffectTimeTextScale     = 0.8f;
        s.phEffectTimeTextColor     = 0xFFCCCCCC;
        s.phEffectIconBgEnabled     = true;
        s.phEffectIconBgColor       = 0x99000000;
        s.phEffectIconGlowEnabled   = false;
        s.phEffectIconBorderColor   = 0x80FFFFFF;
        s.phEffectIconBorderWidth   = 1;
        s.phEffectIconBorderRadius  = 50;
        s.phEffectIconBgPadding     = 2;
        s.phEffectIconZoom          = 1.0f;
        s.phEffectIconFocusU        = 0.5f;
        s.phEffectIconFocusV        = 0.5f;
        s.phEffectShowSourceBadge   = false;
        s.phEffectSourceBadgeScale  = 0.35f;
        s.phEffectSourceBadgeTint   = 0xFF3DCAC2;
        s.phEffectSourceBadgeOffsetY = 5;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s);      break;
        case 1:  ApplyCompact(s);      break;
        case 2:  ApplyFloatingIcons(s); break;
        case 3:  ApplyTimeline(s);     break;
        case 4:  ApplyMinimal(s);      break;
        default: ApplyDefault(s);      break;
        }
    }
}
