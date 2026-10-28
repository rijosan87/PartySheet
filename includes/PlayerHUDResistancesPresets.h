#pragma once

#include "Settings.h"

#include <array>
#include <cstddef>

namespace PlayerHUDResistancesPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 5> kPresets{ {
        { "1 - Default" },
        { "2 - Compact" },
        { "3 - Vivid"   },
        { "4 - Minimal" },
        { "5 - Halo"    },
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.prLayout         = 1;
        s.prIconSize       = 22;
        s.prFontScale      = 0.80f;
        s.prSpacing        = 15;
        s.prRibbonColor    = 0x00000000;
        s.prBgTintEnabled  = true;
        s.prBgTintOpacity  = 90;
        s.prCornerRadius   = 5;
        s.prBgStyle              = 1;
        s.prChipTopColor         = 0xEB2E2C32;
        s.prChipBottomColor      = 0xFF0F0E11;
        s.prChipShadow           = true;
        s.prChipHighlight        = true;
        s.prChipAccent           = true;
        s.prChipInnerBorderColor = 0x37968A74;
        s.prChipOuterBorderColor = 0xEB060608;
        s.prChipNeutralText      = true;
        s.prChipGrain            = true;
        s.prChipGrainOpacity     = 40;
        s.prIconDuotone          = true;
        s.prIconShadow           = false;
        s.prIconOutline          = false;
        s.prIconOutlineWidth     = 1;
        s.prIconOutlineColor     = 0xC80A080C;
        s.prIconGlow             = true;
        s.prIconGloss            = false;
        s.prTextShadow = true;
        s.prTextOutline = false;
        s.prTextOutlineOpacity = 160;
        s.prTextDuotone = true;

        s.prArmorColor     = 0xFFB4B4B4;
        s.prDmgResistColor = 0xFFCCC8C0;
        s.prFireColor      = 0xFFCC4400;
        s.prFrostColor     = 0xFF4488CC;
        s.prShockColor     = 0xFFCCCC00;
        s.prMagicColor     = 0xFF8844CC;
        s.prPoisonColor    = 0xFF44AA44;
        s.prDiseaseColor   = 0xFFA0A0A0;
        s.prSpeedColor     = 0xFF7FD4A0;
    }

    inline void ApplyCompact(Settings& s)
    {
        s.prLayout         = 0;
        s.prIconSize       = 22;
        s.prFontScale      = 0.70f;
        s.prSpacing        = 6;
        s.prRibbonColor    = 0x00000000;
        s.prBgTintEnabled  = true;
        s.prBgTintOpacity  = 0;
        s.prCornerRadius   = 0;
        s.prBgStyle        = 0;
        s.prIconDuotone    = true;
        s.prIconShadow     = true;
        s.prIconOutline    = false;
        s.prIconOutlineWidth    = 1;
        s.prIconOutlineColor    = 0xC80A080C;
        s.prIconGlow       = false;
        s.prIconGloss      = false;
        s.prTextShadow = true;
        s.prTextOutline = false;
        s.prTextOutlineOpacity = 160;
        s.prTextDuotone = true;

        s.prArmorColor     = 0xFFB4B4B4;
        s.prDmgResistColor = 0xFFCCC8C0;
        s.prFireColor      = 0xFFCC4400;
        s.prFrostColor     = 0xFF4488CC;
        s.prShockColor     = 0xFFCCCC00;
        s.prMagicColor     = 0xFF8844CC;
        s.prPoisonColor    = 0xFF44AA44;
        s.prDiseaseColor   = 0xFFA0A0A0;
        s.prSpeedColor     = 0xFF7FD4A0;
    }

    inline void ApplyVivid(Settings& s)
    {
        s.prLayout         = 1;
        s.prIconSize       = 22;
        s.prFontScale      = 0.85f;
        s.prSpacing        = 16;
        s.prRibbonColor    = 0x00000000;
        s.prBgTintEnabled  = true;
        s.prBgTintOpacity  = 220;
        s.prCornerRadius   = 8;
        s.prBgStyle        = 0;
        s.prIconDuotone    = true;
        s.prIconShadow     = false;
        s.prIconOutline    = true;
        s.prIconOutlineWidth    = 1;
        s.prIconOutlineColor    = 0xC80A080C;
        s.prIconGlow       = false;
        s.prIconGloss      = false;
        s.prTextShadow = true;
        s.prTextOutline = false;
        s.prTextOutlineOpacity = 160;
        s.prTextDuotone = true;

        s.prArmorColor     = 0xFFE0E0E0;
        s.prDmgResistColor = 0xFFFFEECC;
        s.prFireColor      = 0xFFFF6622;
        s.prFrostColor     = 0xFF55AAFF;
        s.prShockColor     = 0xFFFFEE22;
        s.prMagicColor     = 0xFFBB66FF;
        s.prPoisonColor    = 0xFF66DD44;
        s.prDiseaseColor   = 0xFFCCBB88;
        s.prSpeedColor     = 0xFF77FFAA;
    }

    inline void ApplyMinimal(Settings& s)
    {
        s.prLayout         = 1;
        s.prIconSize       = 24;
        s.prFontScale      = 0.75f;
        s.prSpacing        = 10;
        s.prRibbonColor    = 0x00000000;
        s.prBgTintEnabled  = true;
        s.prBgTintOpacity  = 0;
        s.prCornerRadius   = 0;
        s.prBgStyle        = 0;
        s.prIconDuotone    = true;
        s.prIconShadow     = true;
        s.prIconOutline    = false;
        s.prIconOutlineWidth    = 1;
        s.prIconOutlineColor    = 0xC80A080C;
        s.prIconGlow       = false;
        s.prIconGloss      = false;
        s.prTextShadow = true;
        s.prTextOutline = false;
        s.prTextOutlineOpacity = 160;
        s.prTextDuotone = true;

        s.prArmorColor     = 0xDDFFFFFF;
        s.prDmgResistColor = 0xDDFFFFFF;
        s.prFireColor      = 0xDDFFFFFF;
        s.prFrostColor     = 0xDDFFFFFF;
        s.prShockColor     = 0xDDFFFFFF;
        s.prMagicColor     = 0xDDFFFFFF;
        s.prPoisonColor    = 0xDDFFFFFF;
        s.prDiseaseColor   = 0xDDFFFFFF;
        s.prSpeedColor     = 0xDDFFFFFF;
    }

    inline void ApplyHalo(Settings& s)
    {
        ApplyDefault(s);
        s.prLayout = 2; // Radial
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s); break;
        case 1:  ApplyCompact(s); break;
        case 2:  ApplyVivid(s);   break;
        case 3:  ApplyMinimal(s); break;
        case 4:  ApplyHalo(s);    break;
        default: ApplyDefault(s); break;
        }
    }
}
