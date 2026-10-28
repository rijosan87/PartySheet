#pragma once

// STATUS: Working
// Presets for Character Sheet UI.


#include "Settings.h"

#include <array>
#include <cstddef>

namespace CharacterSheetPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 3> kPresets{ {
        { "1 - Default" },
        { "2 - Warm Gold" },
        { "3 - Minimal" }
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.csNameColor       = 0xE6DCB4;
        s.csNameFontScale   = 1.0f;
        s.csRaceColor       = 0x969696;
        s.csXpBarColor      = 0xCDB46D;
        s.csShoutBarColor   = 0x8586A5;
        s.csHealthColor     = 0x901515;
        s.csStaminaColor    = 0x1F731F;
        s.csMagickaColor    = 0x1E439F;
        s.csBarTextColor    = 0xDCDCDC;
        s.csBarTextScale    = 1.0f;
        s.csRegenColor      = 0xB4B4B4;
        s.csDiamondSize         = 48.0f;
        s.csDiamondBorderColor  = 0xC800C8FF;
        s.csDiamondFillColor    = 0xE614192D;
        s.csDiamondEmptyColor   = 0xB4505064;
        s.csGlowEnabled         = true;
        s.csGlowColor           = 0x00C8FF;
        s.csSlotLabelColor      = 0xDCC8A0;
        s.csSlotFontScale       = 0.85f;
        s.csShowBadges          = true;
        s.csBadgeTextScale      = 0.85f;
        s.csStatLabelColor      = 0xB4AFA0;
        s.csStatIconSize        = 24;
        s.csStatGlowEnabled     = true;
        s.csStatGlowColor       = 0xFF5050;
        s.csStatBgColor         = 0xE60F1428;
        s.csResistIconSize      = 28;
        s.csResistGlowEnabled   = true;
        s.csResistGlowColor     = 0x9664FF;
        s.csResistBgColor       = 0xE60F1428;
        s.csStatFontScale       = 1.0f;
        s.csSkillTextColor      = 0xB4AFA0;
        s.csSkillGlowEnabled    = true;
        s.csSkillGlowColor      = 0x3278FF;
        s.csSkillBgColor        = 0xE60F1428;
        s.csSkillFontScale      = 1.0f;
        s.csStatusGlowEnabled   = true;
        s.csStatusGlowColor     = 0xB4A064;
        s.csStatusBgColor       = 0xC8000000;
        s.csStatusFontScale     = 1.0f;
    }

    inline void ApplyWarmGold(Settings& s)
    {
        s.csNameColor       = 0xFFE8A0;
        s.csNameFontScale   = 1.0f;
        s.csRaceColor       = 0xB89860;
        s.csXpBarColor      = 0xD4960A;
        s.csShoutBarColor   = 0xA06428;
        s.csHealthColor     = 0xC03020;
        s.csStaminaColor    = 0x287828;
        s.csMagickaColor    = 0x4050B4;
        s.csBarTextColor    = 0xF0E0B0;
        s.csBarTextScale    = 1.0f;
        s.csRegenColor      = 0xC8B478;
        s.csDiamondSize         = 48.0f;
        s.csDiamondBorderColor  = 0xC8D4960A;
        s.csDiamondFillColor    = 0xE6201408;
        s.csDiamondEmptyColor   = 0xB4786050;
        s.csGlowEnabled         = true;
        s.csGlowColor           = 0xD4960A;
        s.csSlotLabelColor      = 0xF0D890;
        s.csSlotFontScale       = 0.85f;
        s.csShowBadges          = true;
        s.csBadgeTextScale      = 0.85f;
        s.csStatLabelColor      = 0xD4C090;
        s.csStatIconSize        = 24;
        s.csStatGlowEnabled     = true;
        s.csStatGlowColor       = 0xFF8040;
        s.csStatBgColor         = 0xE6180E04;
        s.csResistIconSize      = 28;
        s.csResistGlowEnabled   = true;
        s.csResistGlowColor     = 0xD4960A;
        s.csResistBgColor       = 0xE6180E04;
        s.csStatFontScale       = 1.0f;
        s.csSkillTextColor      = 0xD4C090;
        s.csSkillGlowEnabled    = true;
        s.csSkillGlowColor      = 0xD4960A;
        s.csSkillBgColor        = 0xE6180E04;
        s.csSkillFontScale      = 1.0f;
        s.csStatusGlowEnabled   = true;
        s.csStatusGlowColor     = 0xD4960A;
        s.csStatusBgColor       = 0xC8100800;
        s.csStatusFontScale     = 1.0f;
    }

    inline void ApplyMinimal(Settings& s)
    {
        s.csNameColor       = 0xE8E8E8;
        s.csNameFontScale   = 1.0f;
        s.csRaceColor       = 0x787878;
        s.csXpBarColor      = 0x8090A0;
        s.csShoutBarColor   = 0x707080;
        s.csHealthColor     = 0x883030;
        s.csStaminaColor    = 0x307040;
        s.csMagickaColor    = 0x304880;
        s.csBarTextColor    = 0xC8C8C8;
        s.csBarTextScale    = 1.0f;
        s.csRegenColor      = 0x909090;
        s.csDiamondSize         = 48.0f;
        s.csDiamondBorderColor  = 0xA0A8A8A8;
        s.csDiamondFillColor    = 0xE0101010;
        s.csDiamondEmptyColor   = 0x78404040;
        s.csGlowEnabled         = false;
        s.csGlowColor           = 0xA0A0A0;
        s.csSlotLabelColor      = 0xC0C0C0;
        s.csSlotFontScale       = 0.85f;
        s.csShowBadges          = true;
        s.csBadgeTextScale      = 0.85f;
        s.csStatLabelColor      = 0xA8A8A8;
        s.csStatIconSize        = 24;
        s.csStatGlowEnabled     = false;
        s.csStatGlowColor       = 0xC0C0C0;
        s.csStatBgColor         = 0xE0101010;
        s.csResistIconSize      = 28;
        s.csResistGlowEnabled   = false;
        s.csResistGlowColor     = 0xA0A0A0;
        s.csResistBgColor       = 0xE0101010;
        s.csStatFontScale       = 1.0f;
        s.csSkillTextColor      = 0xA8A8A8;
        s.csSkillGlowEnabled    = false;
        s.csSkillGlowColor      = 0xA0A0A0;
        s.csSkillBgColor        = 0xE0101010;
        s.csSkillFontScale      = 1.0f;
        s.csStatusGlowEnabled   = false;
        s.csStatusGlowColor     = 0xA0A0A0;
        s.csStatusBgColor       = 0x00080808;
        s.csStatusFontScale     = 1.0f;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s);   break;
        case 1:  ApplyWarmGold(s);  break;
        case 2:  ApplyMinimal(s);   break;
        default: ApplyDefault(s);   break;
        }
    }
}
