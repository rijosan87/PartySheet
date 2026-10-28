#pragma once

#include "Settings.h"

#include <array>
#include <cstddef>

namespace PlayerHUDSurvivalPresets
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
        { "5 - Arc"     },
    } };

    inline void ResetPlacement(Settings& s)
    {
        s.phSurvivalInnerPaddingX = -1;
        s.phSurvivalInnerPaddingY = -1;
        s.phSurvivalIconOffsetX = 0;
        s.phSurvivalIconOffsetY = 0;
        s.phSurvivalLabelOffsetX = 0;
        s.phSurvivalLabelOffsetY = 0;
        s.phSurvivalStageOffsetX = 0;
        s.phSurvivalStageOffsetY = 0;
        s.phSurvivalIndicatorOffsetX = 0;
        s.phSurvivalIndicatorOffsetY = 0;
        s.phSurvivalPercentageOffsetX = 0;
        s.phSurvivalPercentageOffsetY = 0;
    }

    inline void ResetIndicatorDesign(Settings& s)
    {
        s.phSurvivalPipShape = 0;
        s.phSurvivalArcSweep = 100;
        s.phSurvivalArcRotation = 90;
        s.phSurvivalArcRadius = 24.0f;
        s.phSurvivalArcThickness = 2.0f;
        s.phSurvivalShowPercentage = false;
        s.phSurvivalPercentageScale = 0.80f;
        s.phSurvivalIndicatorShadow = true;
        s.phSurvivalIndicatorDuotone = true;
        s.phSurvivalIndicatorGlow = true;
        s.phSurvivalIndicatorHighlight = true;
    }

    inline void ApplyDefault(Settings& s)
    {
        ResetPlacement(s);
        ResetIndicatorDesign(s);
        s.phSurvivalShowBorder = true;
        s.phSurvivalBgTexture = true;
        s.phSurvivalBgDuotone = true;
        s.phSurvivalBorderWidth = 1.2f;
        s.phSurvivalBorderColor = 0xEB060608;
        s.phSurvivalStyle           = 0;
        s.phSurvivalDirection       = 0;
        s.phSurvivalIndicatorStyle  = 0;
        s.phSurvivalIconFill        = false;
        s.phSurvivalIconSize        = 36;
        s.phSurvivalCardWidth       = 80;
        s.phSurvivalCardSpacing     = 6;
        s.phSurvivalShowLabel       = true;
        s.phSurvivalShowStage       = true;
        s.phSurvivalLabelScale      = 0.75f;
        s.phSurvivalStageScale      = 0.80f;
        s.phSurvivalBgColor         = 0xB40A0C12;
        s.phSurvivalLabelColor      = 0xFF8C9BAF;
        s.phSurvivalColdColor       = 0xFF78C8FF;
        s.phSurvivalHungerColor     = 0xFFFF8C32;
        s.phSurvivalFatigueColor    = 0xFFB4A0D2;
        s.phSurvivalAlarmColor      = 0xFFDC2828;
    }

    inline void ApplyCompact(Settings& s)
    {
        ResetPlacement(s);
        ResetIndicatorDesign(s);
        s.phSurvivalShowBorder = true;
        s.phSurvivalBgTexture = true;
        s.phSurvivalBgDuotone = true;
        s.phSurvivalBorderWidth = 1.2f;
        s.phSurvivalBorderColor = 0xEB060608;
        s.phSurvivalStyle           = 1;
        s.phSurvivalDirection       = 0;
        s.phSurvivalIndicatorStyle  = 2;
        s.phSurvivalIconFill        = true;
        s.phSurvivalIconSize        = 24;
        s.phSurvivalCardWidth       = 60;
        s.phSurvivalCardSpacing     = 8;
        s.phSurvivalShowLabel       = true;
        s.phSurvivalShowStage       = true;
        s.phSurvivalLabelScale      = 0.75f;
        s.phSurvivalStageScale      = 0.80f;
        s.phSurvivalBgColor         = 0xA0080A10;
        s.phSurvivalLabelColor      = 0xFFAAAAAA;
        s.phSurvivalColdColor       = 0xFF78C8FF;
        s.phSurvivalHungerColor     = 0xFFFF8C32;
        s.phSurvivalFatigueColor    = 0xFFB4A0D2;
        s.phSurvivalAlarmColor      = 0xFFDC2828;
    }

    inline void ApplyVivid(Settings& s)
    {
        ResetPlacement(s);
        ResetIndicatorDesign(s);
        s.phSurvivalShowBorder = true;
        s.phSurvivalBgTexture = true;
        s.phSurvivalBgDuotone = true;
        s.phSurvivalBorderWidth = 1.2f;
        s.phSurvivalBorderColor = 0xEB060608;
        s.phSurvivalStyle           = 1;
        s.phSurvivalDirection       = 1;
        s.phSurvivalIndicatorStyle  = 2;
        s.phSurvivalIconFill        = true;
        s.phSurvivalIconSize        = 24;
        s.phSurvivalCardWidth       = 75;
        s.phSurvivalCardSpacing     = 8;
        s.phSurvivalShowLabel       = true;
        s.phSurvivalShowStage       = true;
        s.phSurvivalLabelScale      = 0.70f;
        s.phSurvivalStageScale      = 0.80f;
        s.phSurvivalBgColor         = 0xB7000C3E;
        s.phSurvivalLabelColor      = 0xFFCCDDEE;
        s.phSurvivalColdColor       = 0xFF55AAFF;
        s.phSurvivalHungerColor     = 0xFFFF7722;
        s.phSurvivalFatigueColor    = 0xFFCC88FF;
        s.phSurvivalAlarmColor      = 0xFFFF2A2A;
    }

    inline void ApplyMinimal(Settings& s)
    {
        ResetPlacement(s);
        ResetIndicatorDesign(s);
        s.phSurvivalShowBorder = false;
        s.phSurvivalBgTexture = true;
        s.phSurvivalBgDuotone = true;
        s.phSurvivalBorderWidth = 1.2f;
        s.phSurvivalBorderColor = 0xEB060608;
        s.phSurvivalStyle           = 0;
        s.phSurvivalDirection       = 0;
        s.phSurvivalIndicatorStyle  = 0;
        s.phSurvivalIconFill        = true;
        s.phSurvivalIconSize        = 36;
        s.phSurvivalCardWidth       = 70;
        s.phSurvivalCardSpacing     = 2;
        s.phSurvivalShowLabel       = false;
        s.phSurvivalShowStage       = false;
        s.phSurvivalLabelScale      = 0.75f;
        s.phSurvivalStageScale      = 0.80f;
        s.phSurvivalBgColor         = 0x00000000;
        s.phSurvivalLabelColor      = 0xFF8C9BAF;
        s.phSurvivalColdColor       = 0xFF78C8FF;
        s.phSurvivalHungerColor     = 0xFFFF8C32;
        s.phSurvivalFatigueColor    = 0xFFB4A0D2;
        s.phSurvivalAlarmColor      = 0xFFDC2828;
    }

    inline void ApplyArc(Settings& s)
    {
        ApplyMinimal(s);
        s.phSurvivalIndicatorStyle = 3;            // Arc
        s.phSurvivalIconFill = false;
        s.phSurvivalIndicatorDuotone = false;
    }

    inline void ApplyPreset(Settings& s, std::size_t index)
    {
        switch (index) {
        case 0:  ApplyDefault(s); break;
        case 1:  ApplyCompact(s); break;
        case 2:  ApplyVivid(s);   break;
        case 3:  ApplyMinimal(s); break;
        case 4:  ApplyArc(s);     break;
        default: ApplyDefault(s); break;
        }
    }
}
