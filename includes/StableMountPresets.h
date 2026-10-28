#pragma once

#include "Settings.h"

#include <array>
#include <cstddef>

namespace StableMountPresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 4> kPresets{ {
        { "1 - Default" },
        { "2 - Dark Knight" },
        { "3 - Ranger" },
        { "4 - Halo" }
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.smLayoutOrientation   = 1;
        s.smPortraitStyle       = 0;
        s.smHaloHealthShare     = 70;
        s.smHaloThickness       = 0;
        s.smHaloGap             = 4;
        s.smHaloOffset          = 2;
        s.smPortraitSize        = 72;
        s.smPortraitBorderRadius= 36;
        s.smPortraitBorderWidth = 2;
        s.smPortraitBorderColor = 0xFF1E1E1E;
        s.smBarWidth            = 80;
        s.smBarHeight           = 14;
        s.tsBarWidth            = 4;
        s.tsBarHeight           = 14;
        s.smBarSpacing          = 2;
        s.smBarStyleEnabled     = true;
        s.smBarStyleIndex       = 1;
        s.smMountSpacing        = 5;
        s.smShowHealth          = true;
        s.smShowStamina         = true;
        s.smShowCarryWeight     = true;
        s.smShowName            = true;
        s.smNameFirstWordOnly   = true;
        s.smNameFontScale       = 0.9f;
        s.smBarTextFontScale    = 0.8f;
        s.smHealthColor         = 0x901515;
        s.smStaminaColor        = 0x1F731F;
        s.smCarryColor          = 0xD8C37A;
        s.smNameColor           = 0xFFFFFF;
        s.smRibbonBgColor       = 0x000D0D1A;
        s.smRibbonCornerRadius  = 4.0f;
        s.smRibbonGap           = 4;
    }

    inline void ApplyDarkKnight(Settings& s)
    {
        s.smLayoutOrientation   = 0;
        s.smPortraitStyle       = 0;
        s.smHaloHealthShare     = 70;
        s.smHaloThickness       = 0;
        s.smHaloGap             = 4;
        s.smHaloOffset          = 2;
        s.smPortraitSize        = 80;
        s.smPortraitBorderRadius= 8;
        s.smPortraitBorderWidth = 3;
        s.smPortraitBorderColor = 0xFF3A0A0A;
        s.smBarWidth            = 90;
        s.smBarHeight           = 12;
        s.tsBarWidth            = 4;
        s.tsBarHeight           = 12;
        s.smBarSpacing          = 3;
        s.smBarStyleEnabled     = true;
        s.smBarStyleIndex       = 1;
        s.smMountSpacing        = 6;
        s.smShowHealth          = true;
        s.smShowStamina         = true;
        s.smShowCarryWeight     = true;
        s.smShowName            = true;
        s.smNameFirstWordOnly   = true;
        s.smNameFontScale       = 0.95f;
        s.smBarTextFontScale    = 0.8f;
        s.smHealthColor         = 0xB41A1A;
        s.smStaminaColor        = 0x5A1A8C;
        s.smCarryColor          = 0x787878;
        s.smNameColor           = 0xDC9664;
        s.smRibbonBgColor       = 0xD0050510;
        s.smRibbonCornerRadius  = 2.0f;
        s.smRibbonGap           = 3;
    }

    inline void ApplyRanger(Settings& s)
    {
        s.smLayoutOrientation   = 1;
        s.smPortraitStyle       = 1;
        s.smHaloHealthShare     = 70;
        s.smHaloThickness       = 0;
        s.smHaloGap             = 4;
        s.smHaloOffset          = 2;
        s.smPortraitSize        = 64;
        s.smPortraitBorderRadius= 32;
        s.smPortraitBorderWidth = 2;
        s.smPortraitBorderColor = 0xC8507840;
        s.smBarWidth            = 70;
        s.smBarHeight           = 10;
        s.tsBarWidth            = 3;
        s.tsBarHeight           = 24;
        s.smBarSpacing          = 2;
        s.smBarStyleEnabled     = true;
        s.smBarStyleIndex       = 1;
        s.smMountSpacing        = 4;
        s.smShowHealth          = true;
        s.smShowStamina         = true;
        s.smShowCarryWeight     = true;
        s.smShowName            = true;
        s.smNameFirstWordOnly   = true;
        s.smNameFontScale       = 0.85f;
        s.smBarTextFontScale    = 0.75f;
        s.smHealthColor         = 0x7A3020;
        s.smStaminaColor        = 0x3C7828;
        s.smCarryColor          = 0xA09050;
        s.smNameColor           = 0xC8B478;
        s.smRibbonBgColor       = 0xC0081408;
        s.smRibbonCornerRadius  = 6.0f;
        s.smRibbonGap           = 4;
    }

    inline void ApplyHalo(Settings& s)
    {
        ApplyDefault(s);
        s.smPortraitStyle = 2;
        s.smMountSpacing = 8;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s);    break;
        case 1:  ApplyDarkKnight(s); break;
        case 2:  ApplyRanger(s);     break;
        case 3:  ApplyHalo(s);       break;
        default: ApplyDefault(s);    break;
        }
    }
}
