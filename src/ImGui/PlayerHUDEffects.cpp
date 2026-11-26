#include "../../includes/ImGui/PlayerHUDEffects.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/ImGui/PlayerHUD.h"
#include "../../includes/ImGui/PlayerHUDEquipment.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Utils/EffectIconRenderer.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/HUDFlow.h"
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

    static std::function<bool(const EffectAttribution::GroupKey&)> BadgedOnEquipment() {
        const auto* s = Settings::GetSingleton();
        if (!s->peShowEffectBadges || !s->peEffectBadgeHideInList) return {};
        return [](const EffectAttribution::GroupKey& key) {
            return PlayerHUDEquipment::ShowsAsBadge(key) || EffectCategories::ChangesEquipment(key.mgef);
        };
    }

    static std::function<bool(const EffectAttribution::GroupKey&)> ShownElsewhere() {
        const auto* s = Settings::GetSingleton();
        std::vector<EffectAttribution::GroupKey> onBars;
        if (s->phEffectBadgeHideInList) onBars = PlayerHUD::BarBadgeGroups();
        return [onBars, onEquipment = BadgedOnEquipment()](const EffectAttribution::GroupKey& key) {
            return std::find(onBars.begin(), onBars.end(), key) != onBars.end() || (onEquipment && onEquipment(key));
        };
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
            settings->phEffectMinDuration, settings->phEffectMaxDuration, settings->hudHideManagedSummonEffects,
            ShownElsewhere());
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

        HUDFlow::Options flow;
        flow.corner    = static_cast<HUDFlow::Corner>(std::clamp(settings->phEffectAnchor, 0, 3));
        flow.axis      = vertical ? HUDFlow::Axis::Vertical : HUDFlow::Axis::Horizontal;
        flow.wrapCount = wrapCount;
        flow.gapX      = hSpacing;
        flow.gapY      = vSpacing;
        const int nEff = static_cast<int>(effects.size());
        const HUDFlow::Result grid = HUDFlow::ArrangeGrid((std::max)(nEff, 1), ImVec2(totalIconW, iconSize), flow);
        const float gridW = grid.size.x;
        const float gridH = grid.size.y;

        float extraH = settings->phShowEffectTimeText
                       ? ImGui::GetFont()->LegacySize * settings->phEffectTimeTextScale + 8.0f : 0.0f;

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
            const ImVec2 contentTopLeft = HUDFlow::TopLeftFromAnchor(
                ImVec2(static_cast<float>(settings->phEffectPosX), static_cast<float>(settings->phEffectPosY)),
                ImVec2(gridW, gridH + extraH), flow.corner);
            targetPos = ImVec2(contentTopLeft.x - borderBleed, contentTopLeft.y - borderBleed);
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
            const ImVec2& slot = grid.offsets[iconIndex];
            ImVec2 iconPos(iconOrigin.x + slot.x, iconOrigin.y + slot.y);

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
                if (effect.duration <= 0.0f)
                    snprintf(line, sizeof(line), "%s", effect.name.c_str());
                else if (effect.magnitude >= 1.0f)
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
                const ImVec2 anchor = HUDFlow::AnchorFromTopLeft(
                    ImVec2(pos.x + borderBleed, pos.y + borderBleed),
                    ImVec2(size.x - 2.0f * borderBleed, size.y - 2.0f * borderBleed), flow.corner);
                settings->phEffectPosX = static_cast<int>(std::lround(anchor.x));
                settings->phEffectPosY = static_cast<int>(std::lround(anchor.y));
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
