#pragma once

#include "Settings.h"
#include "BackgroundManager.h"

#include <array>
#include <cstddef>

namespace PlayerHUDBarPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 7> kPresets{ {
        { "1 - Animated Portrait" },
        { "2 - Custom Bars" },
        { "3 - Scaled Bars" },
        { "4 - Level Emblem" },
        { "5 - No Portrait" },
        { "6 - Halo" },
        { "7 - Curved Status" },
    } };

    inline void ApplyAnimatedPortrait(Settings& s)
    {
        s.phShowPortrait          = true;
        s.phPortraitSize          = 96;
        s.phPortraitShape         = 1;
        // The UI "Portrait Style" dropdown is derived from the next 3 fields:
        s.phPortraitMode          = 0;
        s.phPortraitAnimEnabled   = true;
        s.phPortraitIndex         = 0;
        s.phPortraitFrameEnabled  = true;
        s.phPortraitFrameIndex    = 1;
        s.phPortraitFrameScale    = 1.4f;
        s.phPortraitBorderColor   = 0xFF1E1E1E;
        s.phPortraitBorderWidth   = 4;
        s.phPortraitBorderRadius  = 8.0f;
        s.phPortraitColumnSpacing = 20;

        s.phDynamicBars    = true;
        s.phPortraitAlwaysVisible = false;
        s.phDamageGhost    = true;
        s.phScaledBarWidth = false;

        s.phBarWidth         = 300;
        s.phBarHeight        = 16;
        s.phMagickaBarHeight = 16;
        s.phStaminaBarHeight = 16;
        s.phXPBarHeight      = 8;
        s.phXPBarWidth       = 0;

        s.phBarStyleEnabled = false;
        s.phBarStyleIndex   = 1;

        s.phBarVisualStyle  = 0;
        s.phBarGlow         = true;
        s.phBarTrack        = true;
        s.phBarDepthShading = true;
        s.phBarEdgeLight    = true;
        s.phBarTip          = true;
        s.phBarSmoothDrain  = true;
        s.phBarCornerRadius = 3.0f;

        s.phBarOuterBorderWidth = 0;
        s.phBarOuterBorderColor = 0xEB060608;
        s.phBarInnerBorderWidth = 0;
        s.phBarInnerBorderColor = 0x37968A74;

        s.phBarEndStyle     = 0;
        s.phBarEndStyleLeft = 0;

        s.phShowBarText   = true;
        s.phShowBarIcons  = true;
        s.phBarTextColor  = 0xAEFFFFFF;
        s.phBarPulseColor = 0x70FFFFFF;

        s.phShowXP       = true;
        s.phXPAsBar      = false;
        s.phHealthColor  = 0xFF901515;
        s.phMagickaColor = 0xFF1E439F;
        s.phStaminaColor = 0xFF1F731F;
        s.phXPColor      = 0xFFD4AF37;

        s.phBarSpacing     = 6;
        s.phBarTextOffsetY = 0;
        s.phShowRegenPip   = true;

        s.phHpOffsetX = 0;    s.phHpOffsetY = 0;
        s.phMpOffsetX = 0;    s.phMpOffsetY = 0;
        s.phSpOffsetX = 0;    s.phSpOffsetY = 0;
        s.phXPBarOffsetX = 0; s.phXPBarOffsetY = 0;
    }

    inline void ApplyCustomBarDesign(Settings& s)
    {
        s.phShowPortrait          = true;
        s.phPortraitSize          = 96;
        s.phPortraitShape         = 0;
        s.phPortraitMode          = 0;
        s.phPortraitAnimEnabled   = false;
        s.phPortraitIndex         = 1;
        s.phPortraitFrameEnabled  = true;
        s.phPortraitFrameIndex    = 2;
        s.phPortraitFrameScale    = 1.4f;
        s.phPortraitBorderColor   = 0xFF1E1E1E;
        s.phPortraitBorderWidth   = 4;
        s.phPortraitBorderRadius  = 0.25f;
        s.phPortraitColumnSpacing = 20;

        s.phDynamicBars    = false;
        s.phPortraitAlwaysVisible = true;
        s.phDamageGhost    = true;
        s.phScaledBarWidth = false;

        s.phBarWidth         = 300;
        s.phBarHeight        = 18;
        s.phMagickaBarHeight = 18;
        s.phStaminaBarHeight = 18;
        s.phXPBarHeight      = 8;
        s.phXPBarWidth       = 0;

        s.phBarStyleEnabled = true;
        s.phBarStyleIndex   = 16;

        s.phBarVisualStyle  = 0;
        s.phBarGlow         = true;
        s.phBarTrack        = true;
        s.phBarDepthShading = true;
        s.phBarEdgeLight    = true;
        s.phBarTip          = true;
        s.phBarSmoothDrain  = true;
        s.phBarCornerRadius = 3.0f;

        s.phBarOuterBorderWidth = 0;
        s.phBarOuterBorderColor = 0xEB060608;
        s.phBarInnerBorderWidth = 0;
        s.phBarInnerBorderColor = 0x37968A74;

        s.phBarEndStyle     = 0;
        s.phBarEndStyleLeft = 0;

        s.phShowBarText   = true;
        s.phShowBarIcons  = true;
        s.phBarTextColor  = 0xAEFFFFFF;
        s.phBarPulseColor = 0x70FFFFFF;

        s.phShowXP       = true;
        s.phXPAsBar      = false;
        s.phHealthColor  = 0xFF901515;
        s.phMagickaColor = 0xFF1E439F;
        s.phStaminaColor = 0xFF1F731F;
        s.phXPColor      = 0xFFD4AF37;

        s.phBarSpacing     = 2;
        s.phBarTextOffsetY = -5;
        s.phShowRegenPip   = true;

        s.phHpOffsetX = 0;    s.phHpOffsetY = 0;
        s.phMpOffsetX = 0;    s.phMpOffsetY = 0;
        s.phSpOffsetX = 0;    s.phSpOffsetY = 0;
        s.phXPBarOffsetX = 0; s.phXPBarOffsetY = 0;
    }

    inline void ApplyScaledBars(Settings& s)
    {
        s.phShowPortrait          = true;
        s.phPortraitSize          = 96;
        s.phPortraitShape         = 1;
        s.phPortraitMode          = 0;
        s.phPortraitAnimEnabled   = false;
        s.phPortraitIndex         = 0;
        s.phPortraitFrameEnabled  = true;
        s.phPortraitFrameIndex    = 1;
        s.phPortraitFrameScale    = 1.4f;
        s.phPortraitBorderColor   = 0xFF1E1E1E;
        s.phPortraitBorderWidth   = 4;
        s.phPortraitBorderRadius  = 0.25f;
        s.phPortraitColumnSpacing = 0;

        s.phDynamicBars    = true;
        s.phPortraitAlwaysVisible = false;
        s.phDamageGhost    = true;
        s.phScaledBarWidth = true;

        s.phBarWidth         = 300;
        s.phBarHeight        = 16;
        s.phMagickaBarHeight = 13;
        s.phStaminaBarHeight = 13;
        s.phXPBarHeight      = 8;
        s.phXPBarWidth       = 0;

        s.phBarStyleEnabled = false;
        s.phBarStyleIndex   = 1;

        s.phBarVisualStyle  = 0;
        s.phBarGlow         = false;
        s.phBarTrack        = true;
        s.phBarDepthShading = true;
        s.phBarEdgeLight    = true;
        s.phBarTip          = false;
        s.phBarSmoothDrain  = true;
        s.phBarCornerRadius = 2.0f;

        s.phBarOuterBorderWidth = 0;
        s.phBarOuterBorderColor = 0xEB060608;
        s.phBarInnerBorderWidth = 0;
        s.phBarInnerBorderColor = 0x37968A74;

        s.phBarEndStyle     = 0;
        s.phBarEndStyleLeft = 0;

        s.phShowBarText   = true;
        s.phShowBarIcons  = true;
        s.phBarTextColor  = 0xAEFFFFFF;
        s.phBarPulseColor = 0x70FFFFFF;

        s.phShowXP       = true;
        s.phXPAsBar      = false;
        s.phHealthColor  = 0xFF901515;
        s.phMagickaColor = 0xFF1E439F;
        s.phStaminaColor = 0xFF1F731F;
        s.phXPColor      = 0xFFD4AF37;

        s.phBarSpacing     = 0;
        s.phBarTextOffsetY = 0;
        s.phShowRegenPip   = true;

        s.phHpOffsetX = -5;    s.phHpOffsetY = 0;
        s.phMpOffsetX = -5;    s.phMpOffsetY = 0;
        s.phSpOffsetX = -5;    s.phSpOffsetY = 0;
        s.phXPBarOffsetX = 0; s.phXPBarOffsetY = 0;
    }

    inline void ApplyLevelEmblem(Settings& s)
    {
        s.phEmblemFillColor = 0xFF2E7D32;
        s.phShowPortrait          = true;
        s.phPortraitSize          = 96;
        s.phPortraitShape         = 2;
        s.phPortraitMode          = 1;
        s.phPortraitAnimEnabled   = false;
        s.phPortraitIndex         = 0;
        s.phPortraitFrameEnabled  = true;
        s.phPortraitFrameIndex    = 1;
        s.phPortraitFrameScale    = 1.4f;
        s.phPortraitBorderColor   = 0xFF1E1E1E;
        s.phPortraitBorderWidth   = 4;
        s.phPortraitBorderRadius  = 8.0f;
        s.phPortraitColumnSpacing = 0;

        s.phDynamicBars    = true;
        s.phPortraitAlwaysVisible = false;
        s.phDamageGhost    = true;
        s.phScaledBarWidth = false;

        s.phBarWidth         = 300;
        s.phBarHeight        = 18;
        s.phMagickaBarHeight = 18;
        s.phStaminaBarHeight = 18;
        s.phXPBarHeight      = 9;
        s.phXPBarWidth       = 0;

        s.phBarStyleEnabled = false;
        s.phBarStyleIndex   = 1;

        s.phBarVisualStyle  = 1;
        s.phBarGlow         = true;
        s.phBarTrack        = true;
        s.phBarDepthShading = true;
        s.phBarEdgeLight    = true;
        s.phBarTip          = true;
        s.phBarSmoothDrain  = true;
        s.phBarCornerRadius = 0.0f;

        s.phBarOuterBorderWidth = 0;
        s.phBarOuterBorderColor = 0xEB060608;
        s.phBarInnerBorderWidth = 0;
        s.phBarInnerBorderColor = 0x37968A74;

        s.phBarEndStyle     = 4;
        s.phBarEndStyleLeft = 0;

        s.phShowBarText   = true;
        s.phShowBarIcons  = true;
        s.phBarTextColor  = 0xAEFFFFFF;
        s.phBarPulseColor = 0x70FFFFFF;

        s.phShowXP       = true;
        s.phXPAsBar      = false;
        s.phHealthColor  = 0xFF901515;
        s.phMagickaColor = 0xFF1E439F;
        s.phStaminaColor = 0xFF1F731F;
        s.phXPColor      = 0xFFD4AF37;

        s.phBarSpacing     = 0;
        s.phBarTextOffsetY = 0;
        s.phShowRegenPip   = false;

        s.phHpOffsetX = -28;    s.phHpOffsetY = 0;
        s.phMpOffsetX = -28;    s.phMpOffsetY = 0;
        s.phSpOffsetX = -28;    s.phSpOffsetY = 0;
        s.phXPBarOffsetX = 0; s.phXPBarOffsetY = 0;
    }

    inline void ApplyNoPortrait(Settings& s)
    {
        ApplyAnimatedPortrait(s);
        s.phShowPortrait = false;
        s.phBarHeight = 4;
        s.phMagickaBarHeight = 4;
        s.phStaminaBarHeight = 4;
        s.phBarSpacing = 18;
        s.phBarTextFontScale = 0.80f;
        s.phBarTextOffsetY = 95;
        s.phRegenFontScale = 0.75f;
        s.phXPAnnotationOffsetY = -8;
    }

    inline void ApplyHalo(Settings& s)
    {
        ApplyAnimatedPortrait(s);
        s.phHaloEnable = true;
        s.phPortraitAlwaysVisible = true;
        s.phPortraitFrameEnabled = false;
        s.phPortraitBorderWidth = 0;
        s.phLevelBadgeOnPortrait = false;
        s.phShowXP = true;
        s.phXPAsBar = true;
        s.phShowName = false;
        s.phShowHealth = false;
        s.phShowMagicka = false;
        s.phShowStamina = false;
        s.phShowRegenPip = false;
        s.phShowFortify = false;
    }

    // Bar-only showcase: keep Halo, XP, and behavior preferences intact.
    inline void ApplyIndividualBarTextures(Settings& s)
    {
        const auto& filenames = BackgroundManager::GetBackgrounds(BackgroundManager::Type::BarStyle);
        auto FindStyle = [&](const char* filename) -> int {
            for (std::size_t i = 0; i < filenames.size(); ++i)
                if (filenames[i] == filename) return static_cast<int>(i + 1);
            return 0;
        };
        s.phShowHealth = true;
        s.phShowMagicka = true;
        s.phShowStamina = true;
        s.phShowPortrait = true;
        s.phHaloEnable = false;
        s.phShowFortify = false;
        s.phShowRegenPip = false;
        s.phLevelBadgeOnPortrait = false;
        s.phPortraitMode = 1;
        s.phPortraitShape = 1;
        s.phEmblemFillColor = 0xFF111A31;
        s.phPortraitBorderColor = 0xFF17536A;
        s.phPortraitOffsetX = 37;
        s.phPortraitOffsetY = 20;
        s.phPortraitSize = 66;
        s.phPortraitColumnSpacing = 20;
        s.phBarStyleEnabled = true;
        s.phShowXP = true;
        s.phXPAsBar = true;
        s.phXPBarWidth = 214;
        s.phXPBarHeight = 8;
        s.phXPBarOffsetX = 32;
        s.phXPBarOffsetY = -25;
        s.phScaledBarWidth = false;
        s.phBarWidth = 300;
        s.phHealthBarStyleIndex = FindStyle("BarStyle_17.png");
        s.phMagickaBarStyleIndex = FindStyle("BarStyle_18.png");
        s.phStaminaBarStyleIndex = FindStyle("BarStyle_19.png");
        s.phBarStyleIndex = s.phHealthBarStyleIndex > 0 ? s.phHealthBarStyleIndex : 1;
        s.phHealthBarWidth = 300;
        s.phMagickaBarWidth = 300;
        s.phStaminaBarWidth = 102;
        s.phBarHeight = 25;
        s.phMagickaBarHeight = 18;
        s.phStaminaBarHeight = 93;
        s.phHealthColor = 0xFF901515;
        s.phMagickaColor = 0xFF1E439F;
        s.phStaminaColor = 0xFF1F731F;
        s.phBarSpacing = 0;
        s.phHpOffsetX = 13;
        s.phHpOffsetY = 68;
        s.phMpOffsetX = 13;
        s.phMpOffsetY = 67;
        s.phSpOffsetX = -68;
        s.phSpOffsetY = 6;
        s.phDamageGhost = true;
        s.phShowBarText = false;
        s.phShowBarIcons  = false;
        s.phBarTextColor = 0xAEFFFFFF;
        s.phBarTextFontScale = 0.75f;
        s.phBarTextOffsetY = 0;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        s.phPortraitOffsetX = 0;
        s.phPortraitOffsetY = 0;
        s.phHpAnnotationOffsetX = 0;
        s.phHpAnnotationOffsetY = 0;
        s.phMpAnnotationOffsetX = 0;
        s.phMpAnnotationOffsetY = 0;
        s.phSpAnnotationOffsetX = 0;
        s.phSpAnnotationOffsetY = 0;
        s.phXPAnnotationOffsetX = 0;
        s.phXPAnnotationOffsetY = 0;
        if (index == 6) {
            ApplyIndividualBarTextures(s);
            return;
        }
        // Existing presets retain their original shared appearance.
        s.phHealthBarStyleIndex = 0;
        s.phMagickaBarStyleIndex = 0;
        s.phStaminaBarStyleIndex = 0;
        s.phHealthBarWidth = 0;
        s.phMagickaBarWidth = 0;
        s.phStaminaBarWidth = 0;
        s.phHaloEnable = false;
        s.phHaloHealthShare = 70;
        s.phHaloThickness = 0;
        s.phHaloGap = 4;
        s.phHaloOffset = 2;
        s.phShowHealth = true;
        s.phShowMagicka = true;
        s.phShowStamina = true;
        s.phLevelBadgeOnPortrait = true;
        s.phShowFortify = true;
        switch (index) {
        case 0:  ApplyAnimatedPortrait(s); break;
        case 1:  ApplyCustomBarDesign(s);  break;
        case 2:  ApplyScaledBars(s);       break;
        case 3:  ApplyLevelEmblem(s);      break;
        case 4:  ApplyNoPortrait(s);       break;
        case 5:  ApplyHalo(s);             break;
        default: ApplyAnimatedPortrait(s); break;
        }
    }
}
