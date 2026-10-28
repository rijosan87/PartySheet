#include "../../includes/Utils/BarRenderer.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace BarRenderer {

    static constexpr float kGhostHoldSeconds = 0.35f;
    static constexpr float kGhostDrainPerSec = 0.80f;

    struct GhostState {
        float value    = 0.0f;
        float hold     = 0.0f;
        float lastFrac = 1.0f;
        std::chrono::steady_clock::time_point last{};
        bool  seen     = false;
    };
    static std::unordered_map<std::uint64_t, GhostState> g_ghosts;

    float TickGhost(std::uint64_t id, float fraction)
    {
        const auto now = std::chrono::steady_clock::now();
        auto& g = g_ghosts[id];

        float dt;
        if (!g.seen) {
            dt = 0.0f;
            g.seen = true;
        } else {
            dt = std::chrono::duration<float>(now - g.last).count();
            dt = std::clamp(dt, 0.0f, 0.1f);
        }
        g.last = now;

        if (fraction < g.lastFrac - 0.0005f)
            g.hold = kGhostHoldSeconds;
        g.lastFrac = fraction;

        if (fraction >= g.value) {
            g.value = fraction;
            g.hold  = 0.0f;
        } else if (g.hold > 0.0f) {
            g.hold -= dt;
        } else {
            g.value = std::max(fraction, g.value - kGhostDrainPerSec * dt);
        }
        return g.value;
    }

    static inline ImU32 ApplyAlpha(ImU32 col, float a) {
        if (a >= 1.0f) return col;
        if (a <= 0.0f) return col & 0x00FFFFFFu;
        int origA = (col >> IM_COL32_A_SHIFT) & 0xFF;
        int newA  = static_cast<int>(origA * a);
        if (newA < 0) newA = 0; else if (newA > 255) newA = 255;
        return (col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(newA) << IM_COL32_A_SHIFT);
    }

    bool Draw(ImDrawList* dl, ImVec2 pos, const Style& style)
    {
        if (!style.bgTex || !style.fillTex)
            return false;

        const float fraction = std::clamp(style.fillFraction, 0.0f, 1.0f);
        const float uvM      = 0.001f;
        const float a        = std::clamp(style.alpha, 0.0f, 1.0f);
        if (a <= 0.0f) return true;

        ImVec2 bMin = pos;
        ImVec2 bMax = ImVec2(pos.x + style.width, pos.y + style.height);

        const ImU32 bgTint = ApplyAlpha(IM_COL32(255, 255, 255, 255), a);
        dl->AddImage(reinterpret_cast<ImTextureID>(style.bgTex),
            bMin, bMax,
            ImVec2(uvM, uvM), ImVec2(1.0f - uvM, 1.0f - uvM),
            bgTint);

        if (fraction > 0.0f) {
            const ImU32 fillTint = ApplyAlpha(style.fillColor, a);
            if (style.verticalFill) {
                float fillH   = style.height * fraction;
                ImVec2 fMin   = ImVec2(bMin.x, bMax.y - fillH);
                ImVec2 fMax   = bMax;
                float  uvYMin = 1.0f - fraction;
                dl->AddImage(reinterpret_cast<ImTextureID>(style.fillTex),
                    fMin, fMax,
                    ImVec2(0.0f, uvYMin), ImVec2(1.0f, 1.0f),
                    fillTint);
            } else {
                ImVec2 fMax = ImVec2(bMin.x + style.width * fraction, bMax.y);
                dl->AddImage(reinterpret_cast<ImTextureID>(style.fillTex),
                    bMin, fMax,
                    ImVec2(0.0f, 0.0f), ImVec2(fraction, 1.0f),
                    fillTint);
            }
        }

        const float ghost = std::clamp(style.ghostFraction, 0.0f, 1.0f);
        if (ghost > fraction + 0.001f) {
            const ImU32 ghostTint = ApplyAlpha(style.ghostColor, a);
            if (style.verticalFill) {
                ImVec2 gMin = ImVec2(bMin.x, bMax.y - style.height * ghost);
                ImVec2 gMax = ImVec2(bMax.x, bMax.y - style.height * fraction);
                dl->AddImage(reinterpret_cast<ImTextureID>(style.fillTex), gMin, gMax,
                    ImVec2(0.0f, 1.0f - ghost), ImVec2(1.0f, 1.0f - fraction),
                    ghostTint);
            } else {
                ImVec2 gMin = ImVec2(bMin.x + style.width * fraction, bMin.y);
                ImVec2 gMax = ImVec2(bMin.x + style.width * ghost, bMax.y);
                dl->AddImage(reinterpret_cast<ImTextureID>(style.fillTex), gMin, gMax,
                    ImVec2(fraction, 0.0f), ImVec2(ghost, 1.0f),
                    ghostTint);
            }
        }

        if (style.text && style.text[0]) {
            ImFont* font     = style.textFont ? style.textFont : ImGui::GetFont();
            float   fontSize = ImGui::GetFontSize() * style.textFontScale;
            ImVec2  textSz   = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, style.text);
            ImVec2  textPos  = ImVec2(
                bMin.x + (style.width  - textSz.x) * 0.5f,
                bMin.y + (style.height - textSz.y) * 0.5f);

            dl->AddText(font, fontSize,
                ImVec2(textPos.x + 1, textPos.y + 1),
                ApplyAlpha(IM_COL32(0, 0, 0, 160), a),
                style.text);
            dl->AddText(font, fontSize, textPos, ApplyAlpha(style.textColor, a), style.text);
        }

        return true;
    }

}
