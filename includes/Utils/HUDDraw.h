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

    inline ChipStyle PanelChipFromColor(std::uint32_t baseArgb, float darken, float hover, float rounding)
    {
        const float mul = darken + (1.0f - darken) * 0.6f * hover;
        auto ch = [&](int shift) {
            const float v = static_cast<float>((baseArgb >> shift) & 0xFF) * mul;
            return static_cast<std::uint32_t>(v > 255.0f ? 255.0f : v) << shift;
        };
        const std::uint32_t col = 0xE6000000u | ch(16) | ch(8) | ch(0);
        ChipStyle cs;
        cs.rounding    = rounding;
        cs.topColor    = ChipTopFromArgb(col);
        cs.bottomColor = ChipBottomFromArgb(col);
        cs.innerBorder = 0;
        cs.outerBorder = 0;
        cs.grainAlpha  = 0;
        return cs;
    }

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

    enum StripStyle : int {
        StripStyle_Off = 0,
        StripStyle_Fade,
        StripStyle_Arrow,
        StripStyle_Rounded,
        StripStyle_Count
    };
    inline constexpr const char* kStripStyleNames[StripStyle_Count] = { "Off", "Fade", "Arrow", "Rounded" };
    void DrawStrip(ImDrawList* dl, const ImVec2& bbMin, const ImVec2& bbMax, int style,
                   std::uint32_t color, bool towardRight);

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

    enum class PennantDir : int { Down = 0, Up, Left, Right };
    struct PennantStyle {
        PennantDir    direction    = PennantDir::Down;
        float         ringRadius   = 20.0f;
        float         clothWidth   = 24.0f;
        float         clothLength  = 46.0f;
        float         borderWidth  = 1.5f;
        float         iconSize     = 15.0f;
        std::uint32_t borderColor  = 0xFFD9CCB0;
        std::uint32_t ringColor    = 0xE614110D;
        std::uint32_t clothColor   = 0xE614110D;
        std::uint32_t numberColor  = 0xFFEFE3C6;
        std::uint32_t iconColor    = 0xFFD9CCB0;
        bool          filled       = false;
        ImFont*       font         = nullptr;
        float         fontSize     = 0.0f;
        IconFx        iconFx{};
    };
    void DrawPennantBadge(ImDrawList* dl, const ImVec2& ringCenter, const char* value, ImTextureID icon,
                          const PennantStyle& style);

    float DrawTextStyledWrapped(ImDrawList* dl, const ImVec2& pos, float maxWidth,
                                ImU32 baseCol, const char* txt, const TextFx& fx,
                                bool centered = false);

    enum BadgeShape : int { BadgeShape_Rounded = 0, BadgeShape_CutCorner, BadgeShape_Hex, BadgeShape_Diamond };

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
        ImTextureID   textIcon  = 0;
        float         alpha     = 1.0f;
        int           shape     = BadgeShape_Rounded;
        bool          tintRimOnly = false;
        int           trend     = 0;
    };
    struct BadgeRect { ImVec2 min, max; };
    BadgeRect DrawBadge(ImDrawList* dl, const ImVec2& anchor, int corner, const char* txt,
                        const BadgeStyle& style);
    ImVec2 MeasureBadge(const char* txt, const BadgeStyle& style);

    enum BadgePreset : int {
        BadgePreset_Text = 0,
        BadgePreset_Classic,
        BadgePreset_Plain,
        BadgePreset_Glass,
        BadgePreset_Steel,
        BadgePreset_Gem,
        BadgePreset_Count
    };
    inline constexpr const char* kBadgePresetNames[BadgePreset_Count] = {
        "Text", "Classic", "Plain", "Glass", "Steel", "Gem"
    };
    BadgeStyle BadgePresetStyle(int preset);
    void DrawTrendRune(ImDrawList* dl, float x, float y, float height, bool up, float alpha = 1.0f);
    void TintBadge(BadgeStyle& style, std::uint32_t tintArgb);

    struct PipStyle {
        int   shape  = 0;
        float rounding = 2.0f;
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
        bool  single   = false;
        std::uint32_t litEdge   = 0;
        std::uint32_t emptyEdge = 0;
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

    inline float SnapPx(float v) { return static_cast<float>(static_cast<int>(v + (v < 0 ? -0.5f : 0.5f))); }
    inline float SnapWidth(float w) { const float s = SnapPx(w); return s < 1.0f ? 1.0f : s; }
    void DrawCrispRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float thickness = 1.0f);

    void DrawGlowRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col,
                      float thickness = 1.5f, int layers = 3,
                      float rounding = 0.0f, bool border = true, float strength = 1.0f);

    struct GlowDiscStyle {
        int   layers        = 3;
        float glowStep      = 2.0f;
        float ringThickness = 2.0f;
        ImU32 fillCol       = IM_COL32(20, 25, 45, 230);
    };
    void DrawRuleH(ImDrawList* dl, float x0, float x1, float y, ImU32 col, float thickness = 1.0f);
    void DrawRuleV(ImDrawList* dl, float x, float y0, float y1, ImU32 col, float thickness = 1.0f);
    enum class RuleFade { Center, Right };
    void DrawFadeRule(ImDrawList* dl, float x0, float x1, float y, ImU32 col,
                      RuleFade fade = RuleFade::Center, float thickness = 1.0f);

    void DrawGlowDisc(ImDrawList* dl, const ImVec2& center, float radius, ImU32 ringCol,
                      const GlowDiscStyle& style);

    float TickBarValue(std::uint64_t id, float target);

    struct ScopedTextNoSnap {
        ImDrawList* dl; bool on;
        ScopedTextNoSnap(ImDrawList* d, bool enable) : dl(d), on(enable) { if (on) dl->PushDrawFlag(ImDrawFlags_TextNoPixelSnap, true); }
        ~ScopedTextNoSnap() { if (on) dl->PopDrawFlag(); }
        ScopedTextNoSnap(const ScopedTextNoSnap&) = delete;
        ScopedTextNoSnap& operator=(const ScopedTextNoSnap&) = delete;
    };

    void DrawControlTab(ImDrawList* dl, const ImVec2& min, const ImVec2& max, bool selected, bool hovered, float sc);

    void DrawControlTag(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const char* label,
                        bool on, bool hovered, float sc);

    void DrawOrnateFrame(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float sc);

    void DrawTileCaption(ImDrawList* dl, const ImVec2& tileMin, const ImVec2& tileMax, float pad, ImU32 col, const char* text);
    inline float TileCaptionHeight() { return ImGui::GetTextLineHeight() * 2.0f; }

    inline int RotateBegin(ImDrawList* dl) { return dl->VtxBuffer.Size; }
    void RotateEnd(ImDrawList* dl, int startVtx, const ImVec2& center, float degrees);

    ImVec2 RotatedExtent(float w, float h, float degrees);

}
