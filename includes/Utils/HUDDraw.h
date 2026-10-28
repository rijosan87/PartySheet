#pragma once

#include <imgui.h>
#include <cstdint>

namespace HUDDraw {

    inline std::uint32_t PackedToArgb(unsigned int col)
    {
        const std::uint32_t r = (col >> IM_COL32_R_SHIFT) & 0xFF;
        const std::uint32_t g = (col >> IM_COL32_G_SHIFT) & 0xFF;
        const std::uint32_t b = (col >> IM_COL32_B_SHIFT) & 0xFF;
        const std::uint32_t a = (col >> IM_COL32_A_SHIFT) & 0xFF;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    inline std::uint32_t ChipTopFromArgb(std::uint32_t argb)
    {
        const std::uint32_t a = (argb >> 24) & 0xFF;
        const std::uint32_t r = (argb >> 16) & 0xFF;
        const std::uint32_t g = (argb >> 8) & 0xFF;
        const std::uint32_t b = argb & 0xFF;
        auto lift = [](std::uint32_t c) -> std::uint32_t {
            std::uint32_t v = static_cast<std::uint32_t>(c * 1.20f + 14.0f);
            return v > 255 ? 255 : v;
        };
        return (a << 24) | (lift(r) << 16) | (lift(g) << 8) | lift(b);
    }
    inline std::uint32_t ChipBottomFromArgb(std::uint32_t argb)
    {
        const std::uint32_t r = (argb >> 16) & 0xFF;
        const std::uint32_t g = (argb >> 8) & 0xFF;
        const std::uint32_t b = argb & 0xFF;
        auto dim = [](std::uint32_t c) -> std::uint32_t { return static_cast<std::uint32_t>(c * 0.55f); };
        return (0xFFu << 24) | (dim(r) << 16) | (dim(g) << 8) | dim(b);
    }

    struct ChipStyle {
        float         rounding    = 5.0f;
        std::uint32_t topColor    = 0xEB2E2C32;
        std::uint32_t bottomColor = 0xFF0F0E11;
        bool          shadow      = true;
        bool          highlight   = true;
        std::uint32_t accentColor = 0;
        std::uint32_t innerBorder = 0x37968A74;
        std::uint32_t outerBorder = 0xEB060608;
        std::uint32_t glowColor   = 0;
        int           grainAlpha  = 40;
    };
    void DrawChip(ImDrawList* dl, const ImVec2& bMin, const ImVec2& bMax, const ChipStyle& style);

    enum class FadeBgMode : int {
        FadeRight = 0,
        FadeLeft,
        FadeDown,
        FadeUp,
        CenterOut,
        EdgeOrigin,
        DiagonalTL,
        DiagonalBL,
    };
    struct FadeBgStyle {
        FadeBgMode    mode        = FadeBgMode::FadeRight;
        std::uint32_t color       = 0xB4141414;
        float         solidFrac   = 0.25f;
        float         fadeFrac    = 0.75f;
    };
    void DrawFadeBg(ImDrawList* dl, const ImVec2& bbMin, const ImVec2& bbMax, const FadeBgStyle& style);

    struct IconFx {
        bool          duotone      = true;
        bool          shadow       = true;
        bool          outline      = true;
        bool          glow         = true;
        bool          gloss        = true;
        int           outlineWidth = 2;
        std::uint32_t outlineColor = 0xC80A080C;
    };
    void DrawIconStyled(ImDrawList* dl, const ImVec2& pos, const ImVec2& size, ImTextureID tex,
                        std::uint32_t argb, const IconFx& fx,
                        const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1));
    inline void DrawIconStyled(ImDrawList* dl, const ImVec2& pos, float size, ImTextureID tex,
                               std::uint32_t argb, const IconFx& fx,
                               const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1))
    {
        DrawIconStyled(dl, pos, ImVec2(size, size), tex, argb, fx, uvMin, uvMax);
    }

    struct TextFx {
        bool shadow         = true;
        bool outline        = false;
        bool duotone        = true;
        int  outlineOpacity = 160;
        ImFont* font     = nullptr;
        float   fontSize = 0.0f;
    };
    void DrawTextStyled(ImDrawList* dl, const ImVec2& pos, ImU32 baseCol, const char* txt,
                        const TextFx& fx);

    float DrawTextStyledWrapped(ImDrawList* dl, const ImVec2& pos, float maxWidth,
                                ImU32 baseCol, const char* txt, const TextFx& fx,
                                bool centered = false);

    struct BadgeStyle {
        ImFont*       font      = nullptr;
        float         fontScale = 1.0f;
        float         padX = 4.0f, padY = 2.5f;
        float         rounding  = -1.0f;
        std::uint32_t bgTop     = 0xF03A342C;
        std::uint32_t bgBottom  = 0xFF12100F;
        std::uint32_t textColor = 0xFFE8DCC0;
        std::uint32_t innerBorder = 0x50968A74;
        std::uint32_t outerBorder = 0xC8060608;
        bool          shadow    = true;
        bool          highlight = true;
        bool          textDuotone = true;
        bool          textShadow  = true;
        ImTextureID   icon      = 0;
        std::uint32_t iconTint  = 0xFFFFFFFF;
        float         iconSize  = 0.0f;
        float         alpha     = 1.0f;
    };
    struct BadgeRect { ImVec2 min, max; };
    BadgeRect DrawBadge(ImDrawList* dl, const ImVec2& anchor, int corner, const char* txt,
                        const BadgeStyle& style);
    ImVec2 MeasureBadge(const char* txt, const BadgeStyle& style);

    struct PipStyle {
        int   shape  = 0;
        float radius = 4.0f;
        float gap    = 2.0f;
        std::uint32_t litColor   = 0xFFFFC850;
        std::uint32_t emptyColor = 0xC8323741;
        bool  duotone  = true;
        bool  glow     = true;
        bool  socket   = true;
        bool  specular = true;
        bool  contour  = true;  // dark slot edges; disable with socket for flat pips
        float alpha    = 1.0f;
    };
    float MeasurePipRow(int count, const PipStyle& style);
    void DrawPipRow(ImDrawList* dl, const ImVec2& pos, int lit, int count, const PipStyle& style);

    struct BarStyle {
        float width    = 120.0f;
        float height   = 12.0f;
        float fraction = 1.0f;
        float rounding = 3.0f;
        int leftEnd  = 0;
        int rightEnd = 0;

        bool          drawFill   = true;
        std::uint32_t fillColor  = 0xFFB83A3A;
        std::uint32_t fillColor2 = 0;
        bool          duotone    = true;
        bool          gloss      = false;
        bool          highlight  = true;
        bool          tip        = true;
        bool          edgeLight     = true;
        float         edgeLightSize = 36.0f;
        bool          glow       = false;
        int           segments   = 0;
        float         segmentGap = 0.0f;
        float         minSegmentPx = 12.0f;

        bool          drawTrack   = true;
        std::uint32_t trackTop    = 0xD208070A;
        std::uint32_t trackBottom = 0xD2181519;
        bool          shadow      = true;
        bool          trackDepth  = true;       // inset shadow and bottom lip, independent of track colors

        float         outerBorderWidth = 0.0f;
        std::uint32_t outerBorderColor = 0xEB060608;
        float         innerBorderWidth = 0.0f;
        std::uint32_t innerBorderColor = 0x37968A74;

        // Damage ghost: gray segment lingering past the fill (BarRenderer::TickGhost
        float         ghostFraction = 0.0f;
        std::uint32_t ghostColor    = 0x966E6A64;

        float alpha = 1.0f;
    };
    void DrawBarStyled(ImDrawList* dl, const ImVec2& pos, const BarStyle& style);

    struct TimerBarStyle {
        float         width     = 5.0f;
        float         height    = 48.0f;
        float         fraction  = 1.0f;
        float         rounding  = -1.0f;
                                                // radius, for full-slot fills (bg-fill timer)
        std::uint32_t fillColor = 0xFF44FF44;
        bool          duotone   = true;
        bool          tip       = true;
        bool          edgeLight = true;
        float         edgeLightSize = 18.0f;
        bool          glow      = false;
        bool          shadow    = true;
        bool          drawTrack = true;
        std::uint32_t trackTop    = 0xD208070A;
        std::uint32_t trackBottom = 0xD2181519;
        float         alpha     = 1.0f;
    };
    void DrawTimerBarV(ImDrawList* dl, const ImVec2& pos, const TimerBarStyle& style);

    struct TimerArcStyle {
        float         radius     = 20.0f;
        float         thickness  = 2.0f;
        float         fraction   = 1.0f;
        std::uint32_t fillColor  = 0xFF44FF44;
        bool          duotone    = true;
        bool          tip        = true;
        bool          edgeLight  = true;
        bool          shadow     = true;
        bool          drawTrack  = true;
        std::uint32_t trackColor = 0xD20A080C;
        float         alpha      = 1.0f;
        float         startAngle = -90.0f;      // degrees; timer callers retain the 12 o'clock start
        float         sweepAngle = 360.0f;      // degrees, track span
        bool          centeredFill = false;     // grow equally toward both ends of the track
        bool          roundCaps  = false;
        bool          glow       = false;
        bool          trackDepth = true;        // recessed contour edges
    };
    void DrawTimerArc(ImDrawList* dl, const ImVec2& center, const TimerArcStyle& style);

    struct RingSegment {
        float         fraction = 1.0f;
        float         share    = 0.0f;
        std::uint32_t color    = 0xFFFFFFFF;
    };
    struct ResourceRingStyle {
        float radius        = 40.0f;
        float thickness     = 4.0f;
        float startAngleDeg = -90.0f;
        float gapDeg         = 4.0f;
        std::uint32_t trackColor  = 0x1EFFFFFF;
        std::uint32_t shadowColor = 0x78000000;
        bool  hotTip = true;
        bool  gloss  = true;
        const RingSegment* segments = nullptr;
        int   segmentCount = 0;
    };
    void DrawResourceRing(ImDrawList* dl, const ImVec2& center, const ResourceRingStyle& style);

    struct PortraitFx {
        bool  topLight = true;
        bool  scrim    = true;
        bool  vignette = true;
        bool  rim      = true;
        bool          backlight      = false;
        std::uint32_t backlightColor = 0xFFC478;
        float strength = 1.0f;
    };
    void DrawPortraitFx(ImDrawList* dl, const ImVec2& pMin, const ImVec2& pMax,
                        int shape, float rounding, const PortraitFx& fx);

    void DrawPortraitBackHalo(ImDrawList* dl, const ImVec2& pMin, const ImVec2& pMax,
                              std::uint32_t rgb);

    float TickBarValue(std::uint64_t id, float target);

}
