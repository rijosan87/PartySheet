#pragma once

#include "Settings.h"

#include <array>
#include <cstddef>

namespace PlayerHUDDiseasePresets
{
    struct Preset
    {
        const char* name;
    };

    inline constexpr std::array<Preset, 4> kPresets{ {
        { "1 - Default" },
        { "2 - Compact" },
        { "3 - Minimal" },
        { "4 - Grouped" },
    } };

    inline void ApplyDefault(Settings& s)
    {
        s.phDiseaseGrouped        = false;
        s.phDiseaseShowMagnitude  = false;
        s.phDiseaseStyle          = 0;
        s.phDiseaseDirection      = 0;
        s.phDiseaseIconSize       = 36;
        s.phDiseaseCardWidth      = 65;
        s.phDiseaseCardSpacing    = 6;
        s.phDiseaseBgCornerRadius = 6;
        s.phDiseaseShowBorder     = true;
        s.phDiseaseShowLabel      = true;
        s.phDiseaseShowStage      = true;
        s.phDiseaseShowPips       = true;
        s.phDiseaseLabelScale     = 0.75f;
        s.phDiseaseStageScale     = 0.70f;
        s.phDiseaseColor          = 0xFFC8B45A;
        s.phDiseaseAlarmColor     = 0xFFDC2828;
        s.phDiseaseBgColor        = 0x00000000;
        s.phDiseaseIconBgColor    = 0xC80C0E12;
        s.phDiseaseLabelColor     = 0xFF8C9BAF;
        s.phDiseaseBgBorder       = false;
        s.phDiseaseBgBorderColor  = 0x66D2C8B4;
        s.phDiseaseBgTexture      = true;
        s.phDiseaseBgDuotone      = true;
        s.phDiseaseIconBorder     = true;
    }

    inline void ApplyCompact(Settings& s)
    {
        s.phDiseaseGrouped        = false;
        s.phDiseaseShowMagnitude  = false;
        s.phDiseaseStyle          = 1;
        s.phDiseaseDirection      = 0;
        s.phDiseaseIconSize       = 24;
        s.phDiseaseCardWidth      = 64;
        s.phDiseaseCardSpacing    = 8;
        s.phDiseaseBgCornerRadius = 4;
        s.phDiseaseShowBorder     = false;
        s.phDiseaseShowLabel      = true;
        s.phDiseaseShowStage      = true;
        s.phDiseaseShowPips       = true;
        s.phDiseaseLabelScale     = 0.70f;
        s.phDiseaseStageScale     = 0.65f;
        s.phDiseaseColor          = 0xFFC8B45A;
        s.phDiseaseAlarmColor     = 0xFFDC2828;
        s.phDiseaseBgColor        = 0xA0080A10;
        s.phDiseaseIconBgColor    = 0xC80C0E12;
        s.phDiseaseLabelColor     = 0xFFAAAAAA;
        s.phDiseaseBgBorder       = false;
        s.phDiseaseBgBorderColor  = 0x66D2C8B4;
        s.phDiseaseBgTexture      = true;
        s.phDiseaseBgDuotone      = true;
        s.phDiseaseIconBorder     = true;
    }

    inline void ApplyMinimal(Settings& s)
    {
        s.phDiseaseGrouped        = false;
        s.phDiseaseShowMagnitude  = false;
        s.phDiseaseStyle          = 0;
        s.phDiseaseDirection      = 0;
        s.phDiseaseIconSize       = 28;
        s.phDiseaseCardWidth      = 44;
        s.phDiseaseCardSpacing    = 0;
        s.phDiseaseBgCornerRadius = 6;
        s.phDiseaseShowBorder     = false;
        s.phDiseaseShowLabel      = false;
        s.phDiseaseShowStage      = false;
        s.phDiseaseShowPips       = false;
        s.phDiseaseLabelScale     = 0.75f;
        s.phDiseaseStageScale     = 0.70f;
        s.phDiseaseColor          = 0xFFC8B45A;
        s.phDiseaseAlarmColor     = 0xFFDC2828;
        s.phDiseaseBgColor        = 0x00000000;
        s.phDiseaseIconBgColor    = 0x00000000;
        s.phDiseaseLabelColor     = 0xFF8C9BAF;
        s.phDiseaseBgBorder       = false;
        s.phDiseaseBgBorderColor  = 0x66D2C8B4;
        s.phDiseaseBgTexture      = true;
        s.phDiseaseBgDuotone      = true;
        s.phDiseaseIconBorder     = true;
    }

    inline void ApplyGrouped(Settings& s)
    {
        ApplyDefault(s);
        s.phDiseaseGrouped        = true;
        s.phDiseaseShowMagnitude  = true;
        s.phDiseaseIdleCollapse   = false;
        s.phDiseaseLabelScale     = 0.90f;
        s.phDiseaseStageScale     = 0.85f;
        s.phDiseaseIconSize       = 34;
        s.phDiseaseCardWidth      = 200;
        s.phDiseaseBgColor        = 0xE8181B20;
        s.phDiseaseBgCornerRadius = 2;
        s.phDiseaseBgBorder       = true;
        s.phDiseaseIconBgColor    = 0x00000000;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s); break;
        case 1:  ApplyCompact(s); break;
        case 2:  ApplyMinimal(s); break;
        case 3:  ApplyGrouped(s); break;
        default: ApplyDefault(s); break;
        }
    }
}
