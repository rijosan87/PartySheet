#include "../../includes/ImGui/PlayerHUDEffects.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Utils/EffectIconRenderer.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/PlayerStatus.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>

#include <cmath>

namespace PlayerHUDEffects {

    static bool g_ForcePos       = false;
    static int  g_PrevEffectCount = 0;

    void ForceReposition() { g_ForcePos = true; }

    // ── Master scale helpers ────────────────────────────────────────────────
    static float MasterScale()
    {
        const auto* settings = Settings::GetSingleton();
        return settings
            ? std::clamp(static_cast<float>(settings->phEffectScale) / 100.0f, 0.5f, 2.0f)
            : 1.0f;
    }

    static float ScalePx(float value)
    {
        return value * MasterScale();
    }

    static int ScalePxInt0(int value)
    {
        if (value <= 0) return 0;
        return (std::max)(1, static_cast<int>(std::lround(ScalePx(static_cast<float>(value)))));
    }

    static int ScalePxSigned(int value)
    {
        return static_cast<int>(std::lround(ScalePx(static_cast<float>(value))));
    }

    static void RenderWindow(bool allowDrag) {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerEffectWidget) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;
        if (MenuUtils::ShouldBlockOverlay()) return;

        auto transformState = PlayerStatus::GetTransformState();
        if (transformState != PlayerStatus::TransformState::None) return;

        auto effects = EffectIconRenderer::GetActiveEffectsInfo(
            player, settings->phEffectMaxIcons,
            settings->phEffectMinDuration, settings->phEffectMaxDuration, settings->hudHideManagedSummonEffects);
        if (effects.empty() && !allowDrag) return;

        int effectCount = static_cast<int>(effects.size());
        if (effectCount > g_PrevEffectCount)
            HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEffects);
        g_PrevEffectCount = effectCount;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));

        // ── Shared metrics ──
        float iconSize         = ScalePx(static_cast<float>(settings->phEffectIconSize));
        float timerBorderWidth = ScalePx(static_cast<float>(settings->phEffectTimerBorderWidth));
        float totalIconW       = iconSize + timerBorderWidth * 2.0f;
        float hSpacing         = ScalePx(static_cast<float>(settings->phEffectHorizSpacing));
        float vSpacing         = ScalePx(static_cast<float>(settings->phEffectVertSpacing));
        int   wrapCount        = settings->phEffectMaxPerColumn;
        bool  vertical         = settings->phEffectColumnDirection == 0;

        float glowBleed   = settings->phEffectIconBgEnabled
                            ? (settings->phEffectIconGlowEnabled ? ScalePx(8.0f) : ScalePx(4.0f)) : 0.0f;
        float arcBleed    = settings->phShowEffectTimer
                            ? (timerBorderWidth * 1.6f + ScalePx(3.0f)) : 0.0f;
        float vbarBleed   = (settings->phShowEffectTimer && settings->phEffectTimerStyle == 1)
                            ? ((std::max)(timerBorderWidth + 2.0f, 5.0f) + 10.0f) : 0.0f;
        float borderBleed = (std::max)({ glowBleed, arcBleed, vbarBleed }) + ScalePx(2.0f);

        int nEff       = static_cast<int>(effects.size());
        int activeCols = nEff > 0 ? ((nEff - 1) / wrapCount + 1) : 1;
        int primCount  = nEff > 0 ? (std::min)(nEff, wrapCount) : 1;

        int colsMax  = vertical ? activeCols : primCount;
        int rowsMax  = vertical ? primCount  : activeCols;
        float gridW  = colsMax * totalIconW + (colsMax - 1) * hSpacing;
        float gridH  = rowsMax * iconSize   + (rowsMax - 1) * vSpacing;

        bool anchorLeft = settings->phEffectAnchor == 0 || settings->phEffectAnchor == 2;
        bool anchorTop  = settings->phEffectAnchor <= 1;

        float extraH = settings->phShowEffectTimeText
                       ? ImGui::GetFont()->FontSize * settings->phEffectTimeTextScale + 8.0f : 0.0f;

        {
            static float s_LastW = -1.0f, s_LastH = -1.0f;
            if (gridW != s_LastW || gridH + extraH != s_LastH) {
                s_LastW = gridW;
                s_LastH = gridH + extraH;
                g_ForcePos = true;
            }
        }
        ImVec2 targetPos;
        const bool forcedThisFrame = g_ForcePos;
        {
            float winX = anchorLeft ? static_cast<float>(settings->phEffectPosX) - borderBleed
                                    : static_cast<float>(settings->phEffectPosX) - gridW - borderBleed;
            float winY = anchorTop  ? static_cast<float>(settings->phEffectPosY) - borderBleed
                                    : static_cast<float>(settings->phEffectPosY) - gridH - extraH - borderBleed;

            targetPos = ImVec2(winX, winY);
            if (!allowDrag || g_ForcePos) {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
                g_ForcePos = false;
            } else {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
            }
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar      |
                                 ImGuiWindowFlags_NoResize         |
                                 ImGuiWindowFlags_NoScrollbar      |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoBackground;
        if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

        ImGui::Begin("##playerHUDeffects", nullptr, flags);

        EffectIconRenderer::DrawOptions opt;
        opt.iconSize        = iconSize;
        opt.borderThickness = timerBorderWidth;
        opt.showTimer       = settings->phShowEffectTimer;
        opt.timerStyle      = settings->phEffectTimerStyle;
        opt.showMagnitude   = settings->phShowEffectMagnitude;
        opt.magnitudeScale  = settings->phEffectMagnitudeScale;
        opt.bgPadding       = ScalePx(static_cast<float>(settings->phEffectIconBgPadding));
        opt.zoom            = settings->phEffectIconZoom;
        opt.focusU          = settings->phEffectIconFocusU;
        opt.focusV          = settings->phEffectIconFocusV;
        opt.showTimeText    = settings->phShowEffectTimeText;
        opt.timeTextScale   = settings->phEffectTimeTextScale;
        opt.showSourceBadge    = settings->phEffectShowSourceBadge;
        opt.sourceBadgeScale   = settings->phEffectSourceBadgeScale;
        opt.sourceBadgeTint    = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phEffectSourceBadgeTint));
        opt.sourceBadgeOffsetX = ScalePxSigned(settings->phEffectSourceBadgeOffsetX);
        opt.sourceBadgeOffsetY = ScalePxSigned(settings->phEffectSourceBadgeOffsetY);
        opt.magnitudeOffsetX   = ScalePxSigned(settings->phEffectMagnitudeOffsetX);
        opt.magnitudeOffsetY   = ScalePxSigned(settings->phEffectMagnitudeOffsetY);
        opt.timeTextColor   = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phEffectTimeTextColor));
        opt.buffColor       = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phEffectBuffColor));
        opt.debuffColor     = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phEffectDebuffColor));
        opt.bgFillRadius    = opt.iconSize * settings->phEffectIconBorderRadius * 0.01f;
        opt.iconFx.duotone      = settings->phEffectDepthDuotone;
        opt.iconFx.shadow       = settings->phEffectDepthShadow;
        opt.iconFx.outline      = settings->phEffectDepthOutline;
        opt.iconFx.glow         = settings->phEffectDepthGlow;
        opt.iconFx.gloss        = settings->phEffectDepthGloss;
        opt.iconFx.outlineWidth = ScalePxInt0(settings->phEffectDepthOutlineWidth);
        opt.iconFx.outlineColor = settings->phEffectDepthOutlineColor;

        ImDrawList* dl     = ImGui::GetForegroundDrawList();
        ImVec2      winPos = ImGui::GetCursorScreenPos();
        ImVec2 iconOrigin  = ImVec2(winPos.x + borderBleed, winPos.y + borderBleed);

        int iconIndex = 0;
        for (const auto& effect : effects) {
            int primary   = iconIndex % wrapCount;
            int secondary = iconIndex / wrapCount;

            int col = vertical ? secondary : primary;
            int row = vertical ? primary   : secondary;
            if (!anchorLeft) col = colsMax - 1 - col;
            if (!anchorTop)  row = rowsMax - 1 - row;

            ImVec2 iconPos(
                iconOrigin.x + col * (totalIconW + hSpacing),
                iconOrigin.y + row * (opt.iconSize + vSpacing));

            if (settings->phEffectIconBgEnabled) {
                const std::uint32_t bg = settings->phEffectIconBgColor;
                const std::uint8_t  bR = (bg >> 16) & 0xFF, bG = (bg >> 8) & 0xFF, bB = bg & 0xFF;
                auto lift = [](std::uint8_t c) { return static_cast<std::uint32_t>((std::min)(255.0f, c * 1.18f + 14.0f)); };
                auto deep = [](std::uint8_t c) { return static_cast<std::uint32_t>(c * 0.50f); };
                float radius = opt.iconSize * settings->phEffectIconBorderRadius * 0.01f;

                HUDDraw::ChipStyle chip;
                chip.rounding    = radius;
                chip.topColor    = (bg & 0xFF000000u) | (lift(bR) << 16) | (lift(bG) << 8) | lift(bB);
                chip.bottomColor = 0xFF000000u | (deep(bR) << 16) | (deep(bG) << 8) | deep(bB);
                chip.glowColor   = settings->phEffectIconGlowEnabled
                                   ? (0x78000000u | (static_cast<std::uint32_t>(bR) << 16)
                                                  | (static_cast<std::uint32_t>(bG) << 8) | bB) : 0;
                chip.grainAlpha  = 0;
                HUDDraw::DrawChip(dl, iconPos,
                                  ImVec2(iconPos.x + opt.iconSize, iconPos.y + opt.iconSize), chip);

                if (settings->phEffectIconBorderWidth > 0) {
                    dl->AddRect(iconPos,
                        ImVec2(iconPos.x + opt.iconSize, iconPos.y + opt.iconSize),
                        ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phEffectIconBorderColor)),
                        radius, 0, (std::max)(1.0f, ScalePx(static_cast<float>(settings->phEffectIconBorderWidth))));
                }
            }

            if (EffectIconRenderer::DrawEffectIcon(effect, iconPos, dl, opt))
                ++iconIndex;
        }

        float totalW = gridW + 2.0f * borderBleed;
        float totalH = gridH + 2.0f * borderBleed + extraH;
        if (totalW < opt.iconSize) totalW = opt.iconSize;
        if (totalH < opt.iconSize) totalH = opt.iconSize;
        ImGui::Dummy(ImVec2(totalW, totalH));

        if (settings->phShowEffectText) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
            for (const auto& effect : effects) {
                char line[128];
                int sec = static_cast<int>(effect.timeRemaining);
                if (effect.magnitude >= 1.0f)
                    snprintf(line, sizeof(line), "%s +%.0f (%ds)", effect.name.c_str(), effect.magnitude, sec);
                else
                    snprintf(line, sizeof(line), "%s (%ds)", effect.name.c_str(), sec);
                ImGui::SetCursorPosX(0);
                ImGui::Text("%s", line);
            }
            ImGui::PopStyleVar();
        }

        if (allowDrag && !forcedThisFrame) {
            ImVec2 pos  = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            const float dx = pos.x - targetPos.x;
            const float dy = pos.y - targetPos.y;
            if (std::fabs(dx) > 1.5f || std::fabs(dy) > 1.5f) {
                settings->phEffectPosX = static_cast<int>(std::lround(
                    anchorLeft ? pos.x + borderBleed : pos.x + size.x - borderBleed));
                settings->phEffectPosY = static_cast<int>(std::lround(
                    anchorTop  ? pos.y + borderBleed : pos.y + size.y - borderBleed));
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    void RenderHUD() {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerEffectWidget) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) return;
        if (!HUDVisibility::IsVisible(HUDVisibility::WidgetID::PlayerEffects)) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerEffects));
        RenderWindow(false);
        ImGui::PopStyleVar();
        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerEffects));
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

    void RenderPreview() { RenderWindow(true); }

}
