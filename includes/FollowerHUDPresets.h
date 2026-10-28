#pragma once

#include "Settings.h"

#include <array>
#include <cstddef>

namespace FollowerHUDPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 5> kPresets{ {
        { "1 - Default" },
        { "2 - Inline" },
        { "3 - Stacked" },
        { "4- Minimal" },
        { "5 - Ribbon Fill" }
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.showName = false;
        s.showLevel = false;
        s.showTextOnBars = true;
        s.levelBadgeOnPortrait = true;
        s.showStateBadge = true;
        s.hudStateBadgeScale = 0.28f;
        s.hudLevelBadgeOffsetX = 0;
        s.hudLevelBadgeOffsetY = 0;
        s.hudStateBadgeOffsetX = 0;
        s.hudStateBadgeOffsetY = 0;
        s.hudStateBadgeBgColor = 0xB4000000;
        s.hudStateBadgeIconTint = 0xFFFFFF;
        s.hudBadgeFx = true;
        s.hudLevelBadgeShowBorder = true;
        s.hudStateBadgeShowBorder = true;
        s.hudArrowShowBorder = true;
        s.hudArrowBgColor = 0xC07B1818;
        s.textShadowEnabled = false;
        s.nameFirstWordOnly = true;
        s.nameUppercase = false;
        s.levelUppercase = false;
        s.nameColor = 0xFFFFFF;
        s.levelColor = 0xAAAAAA;
        s.nameFontScale = 0.55f;
        s.textAlignment = 0;
        s.textShadowOffsetX = 1;
        s.textShadowOffsetY = 1;
        s.textShadowColor = 0x000000;
        s.textShadowOpacity = 0.7f;

        s.barStyleEnabled = true;
        s.compactBars = true;
        s.barGlowEnabled = false;
        s.showHealth = true;
        s.showMagicka = false;
        s.showStamina = false;
        s.barStyleIndex = 1;
        s.healthColor = 0x901515;
        s.magickaColor = 0x1E439F;
        s.staminaColor = 0x1F731F;
        s.barTextColor = 0xFFFFFF;
        s.healthGradientColor = 0x560D0D;
        s.magickaGradientColor = 0x122860;
        s.staminaGradientColor = 0x124512;
        s.barBorderColor = 0x000000;
        s.fhBarInnerBorderColor = 0x000000;
        s.hudBarWidth = 70;
        s.hudBarHeight = 13;
        s.barCornerRadius = 0.0f;
        s.barSpacing = 2;
        s.barVisualStyle = 0;
        s.hudBarSegmentPoints = 100;
        s.hudBarSegmentGap = 2;
        s.barEndStyle = 0;
        s.barEndStyleLeft = 0;
        s.barTextFontScale = 0.8f;
        s.compactBarRatio = 0.5f;
        s.barOuterBorderWidth = 0;
        s.barInnerBorderWidth = 0;
        s.barOuterBorderOpacity = 1.0f;
        s.barInnerBorderOpacity = 1.0f;

        s.hudHaloEnable = false;
        s.hudHaloHealthShare = 70;
        s.hudHaloThickness = 0;
        s.hudHaloGap = 4;
        s.hudHaloOffset = 2;

        s.showPortrait = true;
        s.portraitSize = 72;
        s.portraitSpacing = 4;
        s.portraitAlignment = 1;
        s.portraitHAlign = 1;
        s.portraitBottomMargin = 4;
        s.portraitCornerRadius = 48.0f;
        s.portraitShape = 0;
        s.contentVAlign = 1;
        s.fhPortraitOuterBorderWidth = 2;
        s.fhPortraitInnerBorderWidth = 0;
        s.fhPortraitOuterBorderColor = 0x1E1E23;
        s.fhPortraitInnerBorderColor = 0xB49650;
        s.hudPortraitFillsRibbon = false;
        s.hudPortraitShapeZoom = 1.0f;
        s.hudPortraitShapeFocusU = 0.5f;
        s.hudPortraitShapeFocusV = 0.5f;
        s.showDirectionArrow = true;
        s.showDistanceMeters = true;
        s.hudArrowOffsetX = -8;
        s.hudArrowOffsetY = 0;
        s.hudDistanceOffsetX = -22;
        s.hudDistanceOffsetY = -50;

        s.showActiveEffects = false;
        s.showEffectIcons = true;
        s.showEffectTimer = true;
        s.showEffectMagnitude = true;
        s.maxEffectsShown = 5;
        s.effectIconSize = 32;
        s.effectIconSpacing = 4;
        s.effectIconVertSpacing = 2;
        s.maxEffectIconsPerColumn = 2;
        s.effectMinDuration = 2;
        s.effectMaxDuration = 600;
        s.effectIconPlacement = 0;
        s.overlayAnchor = 0;
        s.overlayOffsetX = 0;
        s.overlayOffsetY = 0;
        s.effectTimerStyle = 2;
        s.effectTimerBorderWidth = 2;
        s.effectTextScale = 0.8f;
        s.effectMagnitudeScale = 0.7f;
        s.effectBuffColor = 0xFF44FF44;
        s.effectDebuffColor = 0xFFFF4444;

        s.ribbonBgColor = 0x0D0D1A;
        s.ribbonBgOpacity = 0.0f;
        s.ribbonCornerRadius = 6.0f;
        s.ribbonBorderWidth = 0;
        s.ribbonBorderColor = 0x4D4D66;
        s.ribbonPaddingTop = 8;
        s.ribbonPaddingRight = 8;
        s.ribbonPaddingBottom = 8;
        s.ribbonPaddingLeft = 8;
        s.ribbonMargin = -15;
        s.ribbonEffectClearance = 30;
    }

    inline void ApplyInline(Settings& s)
    {
        s.showName = true;
        s.showLevel = false;
        s.showTextOnBars = true;
        s.levelBadgeOnPortrait = true;
        s.showStateBadge = true;
        s.hudStateBadgeScale = 0.28f;
        s.hudLevelBadgeOffsetX = 0;
        s.hudLevelBadgeOffsetY = 0;
        s.hudStateBadgeOffsetX = 0;
        s.hudStateBadgeOffsetY = 0;
        s.hudStateBadgeBgColor = 0xB4000000;
        s.hudStateBadgeIconTint = 0xFFFFFF;
        s.hudBadgeFx = true;
        s.hudLevelBadgeShowBorder = true;
        s.hudStateBadgeShowBorder = true;
        s.hudArrowShowBorder = true;
        s.hudArrowBgColor = 0xC07B1818;
        s.textShadowEnabled = true;
        s.nameFirstWordOnly = true;
        s.nameUppercase = false;
        s.levelUppercase = false;
        s.nameColor = 0xFFFFFF;
        s.levelColor = 0xAAAAAA;
        s.nameFontScale = 0.65f;
        s.textAlignment = 0;
        s.textShadowOffsetX = 1;
        s.textShadowOffsetY = 1;
        s.textShadowColor = 0x000000;
        s.textShadowOpacity = 0.7f;

        s.barStyleEnabled = true;
        s.compactBars = true;
        s.barGlowEnabled = false;
        s.showHealth = true;
        s.showMagicka = true;
        s.showStamina = true;
        s.barStyleIndex = 1;
        s.healthColor = 0x901515;
        s.magickaColor = 0x1E439F;
        s.staminaColor = 0x1F731F;
        s.barTextColor = 0xFFFFFF;
        s.healthGradientColor = 0x560D0D;
        s.magickaGradientColor = 0x122860;
        s.staminaGradientColor = 0x124512;
        s.barBorderColor = 0x000000;
        s.fhBarInnerBorderColor = 0x000000;
        s.hudBarWidth = 125;
        s.hudBarHeight = 14;
        s.barCornerRadius = 0.0f;
        s.barSpacing = 2;
        s.barVisualStyle = 0;
        s.hudBarSegmentPoints = 100;
        s.hudBarSegmentGap = 3;
        s.barEndStyle = 0;
        s.barEndStyleLeft = 0;
        s.barTextFontScale = 0.8f;
        s.compactBarRatio = 0.5f;
        s.barOuterBorderWidth = 0;
        s.barInnerBorderWidth = 0;
        s.barOuterBorderOpacity = 1.0f;
        s.barInnerBorderOpacity = 1.0f;

        s.hudHaloEnable = false;
        s.hudHaloHealthShare = 70;
        s.hudHaloThickness = 0;
        s.hudHaloGap = 4;
        s.hudHaloOffset = 2;

        s.showPortrait = true;
        s.portraitSize = 62;
        s.portraitSpacing = 4;
        s.portraitAlignment = 0;
        s.portraitHAlign = 1;
        s.portraitBottomMargin = 4;
        s.portraitCornerRadius = 10.0f;
        s.portraitShape = 0;
        s.contentVAlign = 1;
        s.fhPortraitOuterBorderWidth = 2;
        s.fhPortraitInnerBorderWidth = 0;
        s.fhPortraitOuterBorderColor = 0x1E1E23;
        s.fhPortraitInnerBorderColor = 0xB49650;
        s.hudPortraitFillsRibbon = false;
        s.hudPortraitShapeZoom = 1.5f;
        s.hudPortraitShapeFocusU = 0.5f;
        s.hudPortraitShapeFocusV = 0.5f;
        s.showDirectionArrow = true;
        s.showDistanceMeters = true;
        s.hudArrowOffsetX = -8;
        s.hudArrowOffsetY = 0;
        s.hudDistanceOffsetX = -22;
        s.hudDistanceOffsetY = -45;

        s.showActiveEffects = false;
        s.showEffectIcons = true;
        s.showEffectTimer = true;
        s.showEffectMagnitude = true;
        s.maxEffectsShown = 4;
        s.effectIconSize = 28;
        s.effectIconSpacing = 4;
        s.effectIconVertSpacing = 8;
        s.maxEffectIconsPerColumn = 2;
        s.effectMinDuration = 2;
        s.effectMaxDuration = 0;
        s.effectIconPlacement = 0;
        s.overlayAnchor = 0;
        s.overlayOffsetX = 0;
        s.overlayOffsetY = 0;
        s.effectTimerStyle = 0;
        s.effectTimerBorderWidth = 2;
        s.effectTextScale = 0.8f;
        s.effectMagnitudeScale = 0.8f;
        s.effectBuffColor = 0xFF44FF44;
        s.effectDebuffColor = 0xFFFF4444;

        s.ribbonBgColor = 0x0D0D1A;
        s.ribbonBgOpacity = 0.0f;
        s.ribbonCornerRadius = 6.0f;
        s.ribbonBorderWidth = 0;
        s.ribbonBorderColor = 0x4D4D66;
        s.ribbonPaddingTop = 8;
        s.ribbonPaddingRight = 8;
        s.ribbonPaddingBottom = 8;
        s.ribbonPaddingLeft = 8;
        s.ribbonMargin = 8;
        s.ribbonEffectClearance = 64;
    }

    inline void ApplyStacked(Settings& s)
    {
        s.showName = true;
        s.showLevel = false;
        s.showTextOnBars = true;
        s.levelBadgeOnPortrait = true;
        s.showStateBadge = true;
        s.hudStateBadgeScale = 0.28f;
        s.hudLevelBadgeOffsetX = 0;
        s.hudLevelBadgeOffsetY = 0;
        s.hudStateBadgeOffsetX = 0;
        s.hudStateBadgeOffsetY = 0;
        s.hudStateBadgeBgColor = 0xB4000000;
        s.hudStateBadgeIconTint = 0xFFFFFF;
        s.hudBadgeFx = true;
        s.hudLevelBadgeShowBorder = true;
        s.hudStateBadgeShowBorder = true;
        s.hudArrowShowBorder = true;
        s.hudArrowBgColor = 0xC07B1818;
        s.textShadowEnabled = true;
        s.nameFirstWordOnly = true;
        s.nameUppercase = false;
        s.levelUppercase = false;
        s.nameColor = 0xFFFFFF;
        s.levelColor = 0xAAAAAA;
        s.nameFontScale = 0.65f;
        s.textAlignment = 1;
        s.textShadowOffsetX = 1;
        s.textShadowOffsetY = 1;
        s.textShadowColor = 0x000000;
        s.textShadowOpacity = 0.7f;

        s.barStyleEnabled = true;
        s.compactBars = true;
        s.barGlowEnabled = false;
        s.showHealth = true;
        s.showMagicka = true;
        s.showStamina = true;
        s.barStyleIndex = 2;
        s.healthColor = 0x901515;
        s.magickaColor = 0x1E439F;
        s.staminaColor = 0x1F731F;
        s.barTextColor = 0xFFFFFF;
        s.healthGradientColor = 0x560D0D;
        s.magickaGradientColor = 0x122860;
        s.staminaGradientColor = 0x124512;
        s.barBorderColor = 0x000000;
        s.fhBarInnerBorderColor = 0x000000;
        s.hudBarWidth = 80;
        s.hudBarHeight = 18;
        s.barCornerRadius = 0.0f;
        s.barSpacing = 0;
        s.barVisualStyle = 0;
        s.hudBarSegmentPoints = 100;
        s.hudBarSegmentGap = 3;
        s.barEndStyle = 0;
        s.barEndStyleLeft = 0;
        s.barTextFontScale = 0.78f;
        s.compactBarRatio = 0.5f;
        s.barOuterBorderWidth = 0;
        s.barInnerBorderWidth = 0;
        s.barOuterBorderOpacity = 1.0f;
        s.barInnerBorderOpacity = 1.0f;

        s.hudHaloEnable = false;
        s.hudHaloHealthShare = 70;
        s.hudHaloThickness = 0;
        s.hudHaloGap = 4;
        s.hudHaloOffset = 2;

        s.showPortrait = true;
        s.portraitSize = 64;
        s.portraitSpacing = 4;
        s.portraitAlignment = 1;
        s.portraitHAlign = 1;
        s.portraitBottomMargin = 4;
        s.portraitCornerRadius = 10.0f;
        s.portraitShape = 0;
        s.contentVAlign = 1;
        s.fhPortraitOuterBorderWidth = 2;
        s.fhPortraitInnerBorderWidth = 0;
        s.fhPortraitOuterBorderColor = 0x0A0A0A;
        s.fhPortraitInnerBorderColor = 0x000000;
        s.hudPortraitFillsRibbon = false;
        s.hudPortraitShapeZoom = 1.5f;
        s.hudPortraitShapeFocusU = 0.5f;
        s.hudPortraitShapeFocusV = 0.5f;
        s.showDirectionArrow = true;
        s.showDistanceMeters = true;
        s.hudArrowOffsetX = -8;
        s.hudArrowOffsetY = 0;
        s.hudDistanceOffsetX = -22;
        s.hudDistanceOffsetY = -45;

        s.showActiveEffects = false;
        s.showEffectIcons = true;
        s.showEffectTimer = true;
        s.showEffectMagnitude = true;
        s.maxEffectsShown = 4;
        s.effectIconSize = 28;
        s.effectIconSpacing = 3;
        s.effectIconVertSpacing = 4;
        s.maxEffectIconsPerColumn = 4;
        s.effectMinDuration = 2;
        s.effectMaxDuration = 0;
        s.effectIconPlacement = 0;
        s.overlayAnchor = 0;
        s.overlayOffsetX = 0;
        s.overlayOffsetY = 0;
        s.effectTimerStyle = 1;
        s.effectTimerBorderWidth = 2;
        s.effectTextScale = 0.80f;
        s.effectMagnitudeScale = 0.70f;
        s.effectBuffColor = 0xFF44FF44;
        s.effectDebuffColor = 0xFFFF4444;

        s.ribbonBgColor = 0x0D0D1A;
        s.ribbonBgOpacity = 0.0f;
        s.ribbonCornerRadius = 6.0f;
        s.ribbonBorderWidth = 0;
        s.ribbonBorderColor = 0x4D4D66;
        s.ribbonPaddingTop = 8;
        s.ribbonPaddingRight = 12;
        s.ribbonPaddingBottom = 8;
        s.ribbonPaddingLeft = 8;
        s.ribbonMargin = -6;
        s.ribbonEffectClearance = 64;
    }

    inline void ApplyMinimal(Settings& s)
    {
        s.showName = false;
        s.showLevel = false;
        s.showTextOnBars = true;
        s.levelBadgeOnPortrait = false;
        s.showStateBadge = false;
        s.hudStateBadgeScale = 0.28f;
        s.hudLevelBadgeOffsetX = 0;
        s.hudLevelBadgeOffsetY = 0;
        s.hudStateBadgeOffsetX = 0;
        s.hudStateBadgeOffsetY = 0;
        s.hudStateBadgeBgColor = 0xB4000000;
        s.hudStateBadgeIconTint = 0xFFFFFF;
        s.hudBadgeFx = true;
        s.hudLevelBadgeShowBorder = true;
        s.hudStateBadgeShowBorder = true;
        s.hudArrowShowBorder = true;
        s.hudArrowBgColor = 0xC07B1818;
        s.textShadowEnabled = true;
        s.nameFirstWordOnly = true;
        s.nameUppercase = false;
        s.levelUppercase = false;
        s.nameColor = 0xFFFFFF;
        s.levelColor = 0xAAAAAA;
        s.nameFontScale = 0.65f;
        s.textAlignment = 1;
        s.textShadowOffsetX = 1;
        s.textShadowOffsetY = 1;
        s.textShadowColor = 0x000000;
        s.textShadowOpacity = 0.7f;

        s.barStyleEnabled = true;
        s.compactBars = true;
        s.barGlowEnabled = false;
        s.showHealth = true;
        s.showMagicka = true;
        s.showStamina = true;
        s.barStyleIndex = 2;
        s.healthColor = 0x901515;
        s.magickaColor = 0x1E439F;
        s.staminaColor = 0x1F731F;
        s.barTextColor = 0xFFFFFF;
        s.healthGradientColor = 0x560D0D;
        s.magickaGradientColor = 0x122860;
        s.staminaGradientColor = 0x124512;
        s.barBorderColor = 0x000000;
        s.fhBarInnerBorderColor = 0x000000;
        s.hudBarWidth = 80;
        s.hudBarHeight = 18;
        s.barCornerRadius = 0.0f;
        s.barSpacing = 0;
        s.barVisualStyle = 0;
        s.hudBarSegmentPoints = 100;
        s.hudBarSegmentGap = 2;
        s.barEndStyle = 0;
        s.barEndStyleLeft = 0;
        s.barTextFontScale = 0.78f;
        s.compactBarRatio = 0.5f;
        s.barOuterBorderWidth = 0;
        s.barInnerBorderWidth = 0;
        s.barOuterBorderOpacity = 1.0f;
        s.barInnerBorderOpacity = 1.0f;

        s.hudHaloEnable = true;
        s.hudHaloHealthShare = 70;
        s.hudHaloThickness = 4;
        s.hudHaloGap = 4;
        s.hudHaloOffset = 1;

        s.showPortrait = true;
        s.portraitSize = 72;
        s.portraitSpacing = 4;
        s.portraitAlignment = 1;
        s.portraitHAlign = 1;
        s.portraitBottomMargin = 4;
        s.portraitCornerRadius = 10.0f;
        s.portraitShape = 2;
        s.contentVAlign = 1;
        s.fhPortraitOuterBorderWidth = 2;
        s.fhPortraitInnerBorderWidth = 0;
        s.fhPortraitOuterBorderColor = 0x0A0A0A;
        s.fhPortraitInnerBorderColor = 0x000000;
        s.hudPortraitFillsRibbon = false;
        s.hudPortraitShapeZoom = 1.5f;
        s.hudPortraitShapeFocusU = 0.5f;
        s.hudPortraitShapeFocusV = 0.5f;
        s.showDirectionArrow = true;
        s.showDistanceMeters = true;
        s.hudArrowOffsetX = -8;
        s.hudArrowOffsetY = 18;
        s.hudDistanceOffsetX = -30;
        s.hudDistanceOffsetY = -4;

        s.showActiveEffects = false;
        s.showEffectIcons = false;
        s.showEffectTimer = false;
        s.showEffectMagnitude = false;
        s.maxEffectsShown = 6;
        s.effectIconSize = 32;
        s.effectIconSpacing = 3;
        s.effectIconVertSpacing = 4;
        s.maxEffectIconsPerColumn = 2;
        s.effectMinDuration = 2;
        s.effectMaxDuration = 0;
        s.effectIconPlacement = 0;
        s.overlayAnchor = 0;
        s.overlayOffsetX = -20;
        s.overlayOffsetY = 0;
        s.effectTimerStyle = 1;
        s.effectTimerBorderWidth = 1;
        s.effectTextScale = 0.80f;
        s.effectMagnitudeScale = 0.70f;
        s.effectBuffColor = 0xFF44FF44;
        s.effectDebuffColor = 0xFFFF4444;

        s.ribbonBgColor = 0x0D0D1A;
        s.ribbonBgOpacity = 0.0f;
        s.ribbonCornerRadius = 6.0f;
        s.ribbonBorderWidth = 0;
        s.ribbonBorderColor = 0x4D4D66;
        s.ribbonPaddingTop = 12;
        s.ribbonPaddingRight = 12;
        s.ribbonPaddingBottom = 8;
        s.ribbonPaddingLeft = 8;
        s.ribbonMargin = -30;
        s.ribbonEffectClearance = 30;
    }

    inline void ApplyRibbonFill(Settings& s)
    {
        ApplyInline(s);
        s.showName = true;
        s.showLevel = false;
        s.levelBadgeOnPortrait = true;
        s.showStateBadge = true;
        s.hudStateBadgeScale = 0.28f;
        s.hudLevelBadgeOffsetX = 0;
        s.hudLevelBadgeOffsetY = 0;
        s.hudStateBadgeOffsetX = 0;
        s.hudStateBadgeOffsetY = 0;
        s.hudStateBadgeBgColor = 0xB4000000;
        s.hudStateBadgeIconTint = 0xFFFFFF;

        s.barStyleEnabled = true;
        s.hudPortraitFillsRibbon = true;
        s.barStyleIndex = 13;
        s.hudBarWidth = 80;
        s.hudHaloEnable = false;

        s.hudPortraitZoom = 1.5f;
        s.hudPortraitFocusU = 0.65f;
        s.hudPortraitFocusV = 0.5f;

        s.showDirectionArrow = true;
        s.showDistanceMeters = true;
        s.hudArrowOffsetX = 4;
        s.hudArrowOffsetY = 0;
        s.hudDistanceOffsetX = -20;
        s.hudDistanceOffsetY = -55;

        // Active Effects: copied from Inline so ribbon fill can tune them independently.
        s.showActiveEffects = false;
        s.showEffectIcons = true;
        s.showEffectTimer = true;
        s.showEffectMagnitude = false;
        s.maxEffectsShown = 6;
        s.effectIconSize = 28;
        s.effectIconSpacing = 4;
        s.effectIconVertSpacing = 4;
        s.maxEffectIconsPerColumn = 2;
        s.effectMinDuration = 2;
        s.effectMaxDuration = 0;
        s.effectIconPlacement = 0;
        s.overlayAnchor = 0;
        s.overlayOffsetX = 0;
        s.overlayOffsetY = 0;
        s.effectTimerStyle = 2;
        s.effectTimerBorderWidth = 2;
        s.effectTextScale = 0.8f;
        s.effectMagnitudeScale = 0.8f;
        s.effectBuffColor = 0xFF005300;
        s.effectDebuffColor = 0xFFFF4444;

        s.ribbonBgColor = 0x0D0D1A;
        s.ribbonBgOpacity = 0.0f;
        s.ribbonCornerRadius = 6.0f;
        s.ribbonBorderWidth = 0;
        s.ribbonBorderColor = 0x4D4D66;
        s.ribbonPaddingTop = 8;
        s.ribbonPaddingRight = 8;
        s.ribbonPaddingBottom = 8;
        s.ribbonPaddingLeft = 80;
        s.ribbonMargin = 4;
        s.ribbonEffectClearance = 30;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:
            ApplyDefault(s);
            break;
        case 1:
            ApplyInline(s);
            break;
        case 2:
            ApplyStacked(s);
            break;
        case 3:
            ApplyMinimal(s);
            break;
        case 4:
            ApplyRibbonFill(s);
            break;
        default:
            ApplyDefault(s);
            break;
        }
    }
}
