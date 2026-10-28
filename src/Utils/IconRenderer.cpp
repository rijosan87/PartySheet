// STATUS: Working
// Helper for drawing customized icons.

#include "../../includes/Utils/IconRenderer.h"
#include "../../includes/ImGui/WidgetDrawHelpers.h"
#include "../../includes/Utils/HUDDraw.h"

#include <algorithm>

namespace IconRenderer {
    static std::uint32_t PackedToArgb(ImU32 col)
    {
        const std::uint32_t r = (col >> IM_COL32_R_SHIFT) & 0xFF;
        const std::uint32_t g = (col >> IM_COL32_G_SHIFT) & 0xFF;
        const std::uint32_t b = (col >> IM_COL32_B_SHIFT) & 0xFF;
        const std::uint32_t a = (col >> IM_COL32_A_SHIFT) & 0xFF;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    void Draw(ImDrawList* dl, ImVec2 screenPos, const Style& style)
    {
        float sz = style.size;
        if (style.chip) {
            const std::uint32_t bg = PackedToArgb(style.bgColor);
            const std::uint8_t a = (bg >> 24) & 0xFF, r = (bg >> 16) & 0xFF,
                               g = (bg >> 8) & 0xFF,  b =  bg        & 0xFF;
            auto lift = [](std::uint8_t c) { return static_cast<std::uint32_t>((std::min)(255.0f, c * 1.20f + 14.0f)); };
            auto sink = [](std::uint8_t c) { return static_cast<std::uint32_t>(c * 0.55f); };
            HUDDraw::ChipStyle cs;
            cs.rounding    = style.borderRadius;
            cs.topColor    = (static_cast<std::uint32_t>(a) << 24) | (lift(r) << 16) | (lift(g) << 8) | lift(b);
            cs.bottomColor = 0xFF000000u | (sink(r) << 16) | (sink(g) << 8) | sink(b);
            if (style.glowEnabled)
                cs.glowColor = (PackedToArgb(style.borderColor) & 0x00FFFFFFu) | 0xB4000000u;
            HUDDraw::DrawChip(dl, screenPos, ImVec2(screenPos.x + sz, screenPos.y + sz), cs);
        } else {
            WidgetDraw::DrawIconBadge(dl, screenPos.x, screenPos.y, sz,
                style.bgColor, style.glowEnabled,
                0, style.borderColor, style.borderRadius);
        }
        if (style.texture) {
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x, screenPos.y + style.bounceOffsetY));
            ImGui::Image(reinterpret_cast<ImTextureID>(style.texture),
                ImVec2(sz, sz), style.uv0, style.uv1);
        }
        if (style.borderWidth > 0) {
            dl->AddRect(
                screenPos,
                ImVec2(screenPos.x + sz, screenPos.y + sz),
                style.borderColor,
                style.borderRadius, 0,
                static_cast<float>(style.borderWidth));
        }
    }
}
