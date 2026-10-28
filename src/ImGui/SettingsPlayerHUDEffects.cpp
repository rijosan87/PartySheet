// STATUS: Working
// MCM menu for Player HUD Active Effects configuration.

#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Settings.h"
#include "../../includes/ImGui/PlayerHUDEffects.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/PlayerHUDEffectsPresets.h"

#include <imgui.h>

namespace ImGuiRenderer::SettingsUI {
    void RenderPlayerHUDEffectsSection(Settings* settings, const ImVec2& displaySize) {
        if (!settings) return;
        ImGui::Indent(10);
        ImGui::Spacing();
        ImGui::Checkbox("Enable Widget##phEffEnable", &settings->enablePlayerEffectWidget);
        ImGui::Checkbox("Use HUD Visibility Trigger##phEffHVTrigger", &settings->hvTriggerPlayerEffects);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("When enabled, the widget fades in when a new effect is applied,\nthen fades out after the HUD Visibility delay.");
        if (settings->enablePlayerEffectWidget) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Presets");
            ImGui::Spacing();
            {
                static int presetIndex = 0;
                static constexpr const char* presetNames[] = {
                    "1 - Default", "2 - Compact", "3 - Floating Icons", "4 - Timeline", "5 - Minimal"
                };
                ImGui::SetNextItemWidth(300.0f);
                if (ImGui::Combo("##phEffPreset", &presetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
                    PlayerHUDEffectsPresets::ApplyPreset(*settings, static_cast<std::size_t>(presetIndex));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Layout / Behavior");
            ImGui::Spacing();
            if (ImGui::SliderInt("Pos X##phEffPosX", &settings->phEffectPosX, 0, static_cast<int>(displaySize.x)))
                PlayerHUDEffects::ForceReposition();
            if (ImGui::SliderInt("Pos Y##phEffPosY", &settings->phEffectPosY, 0, static_cast<int>(displaySize.y)))
                PlayerHUDEffects::ForceReposition();
            const char* layoutItems[] = { "Vertical", "Horizontal" };
            if (ImGui::Combo("Layout##phEffDir", &settings->phEffectColumnDirection, layoutItems, 2))
                PlayerHUDEffects::ForceReposition();
            const char* anchorItems[] = { "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" };
            if (ImGui::Combo("Anchor##phEffAnchor", &settings->phEffectAnchor, anchorItems, 4))
                PlayerHUDEffects::ForceReposition();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Which corner of the icon area is pinned at Pos X/Y.\nIcons grow away from this corner, so it never shifts\nas effects appear, expire, or wrap.");
            ImGui::SliderInt("Max Icons##phEffMax",       &settings->phEffectMaxIcons,      1, 32);
            ImGui::SliderInt("Wrap After##phEffPerCol",   &settings->phEffectMaxPerColumn,  1, 16);
            ImGui::SliderInt("Min Duration (s)##phEffMinDur", &settings->phEffectMinDuration, 0, 60);
            ImGui::SliderInt("Max Duration (m)##phEffMaxDur", &settings->phEffectMaxDuration, 0, 600);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Items (Show / Hide)");
            ImGui::Spacing();
            ImGui::Checkbox("Show Timer##phEffTimer",             &settings->phShowEffectTimer);       ImGui::SameLine();
            ImGui::Checkbox("Show Magnitude##phEffMag",           &settings->phShowEffectMagnitude);   ImGui::SameLine();
            ImGui::Checkbox("Show Time Text##phEffTimeText",      &settings->phShowEffectTimeText);
            ImGui::Checkbox("Show Source Badge##phEffSrcBadge",   &settings->phEffectShowSourceBadge); ImGui::SameLine();
            ImGui::Checkbox("Show Effect Text##phEffText",        &settings->phShowEffectText);
            ImGui::Checkbox("Show Icon BG##phEffBgEn",            &settings->phEffectIconBgEnabled);
            if (settings->phEffectIconBgEnabled) {
                ImGui::SameLine();
                ImGui::Checkbox("Glow##phEffGlow", &settings->phEffectIconGlowEnabled);
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Appearance");
            ImGui::Spacing();
            if (ImGui::BeginTable("##PHEAppearanceTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("##PHEColorCol", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableSetupColumn("##PHESizeCol",  ImGuiTableColumnFlags_WidthStretch, 0.6f);
                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                {
                    static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 c;
                    if (settings->phShowEffectTimer) {
                        c = ColorUtils::ArgbToVec4(settings->phEffectBuffColor);
                        if (ImGui::ColorEdit4("Buff Timer##phEffBuffCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->phEffectBuffColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phEffectDebuffColor);
                        if (ImGui::ColorEdit4("Debuff Timer##phEffDebuffCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->phEffectDebuffColor = ColorUtils::Vec4ToArgb(c);
                    }
                    if (settings->phShowEffectTimeText) {
                        c = ColorUtils::ArgbToVec4(settings->phEffectTimeTextColor);
                        if (ImGui::ColorEdit4("Time Text##phEffTimeCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->phEffectTimeTextColor = ColorUtils::Vec4ToArgb(c);
                    }
                    if (settings->phEffectIconBgEnabled) {
                        c = ColorUtils::ArgbToVec4(settings->phEffectIconBgColor);
                        if (ImGui::ColorEdit4("Icon BG##phEffBgCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->phEffectIconBgColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phEffectIconBorderColor);
                        if (ImGui::ColorEdit4("Icon Border##phEffBdrCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->phEffectIconBorderColor = ColorUtils::Vec4ToArgb(c);
                    }
                    if (settings->phEffectShowSourceBadge) {
                        c = ColorUtils::ArgbToVec4(settings->phEffectSourceBadgeTint);
                        if (ImGui::ColorEdit4("Source Tint##phSrcTint", reinterpret_cast<float*>(&c), kArgb))
                            settings->phEffectSourceBadgeTint = ColorUtils::Vec4ToArgb(c);
                    }
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("Icons");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phEffIcon",  &settings->phEffectIconSize,     16, 128, "Icon Size %d");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phEffVSpc",  &settings->phEffectVertSpacing,  0, 64,  "Vert Spacing %d");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phEffHSpc",  &settings->phEffectHorizSpacing, 0, 64,  "Horiz Spacing %d");
                if (settings->phShowEffectTimer) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Timer");
                    static const char* timerStyles[] = { "Arc", "Vertical Bar", "Background Fill" };
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::Combo("##phEffTStyle", &settings->phEffectTimerStyle, timerStyles, 3);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phEffTBW", &settings->phEffectTimerBorderWidth, 1, 6, "Timer Thickness %d");
                }
                if (settings->phEffectIconBgEnabled) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Icon BG");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phEffBdrW",  &settings->phEffectIconBorderWidth,  0, 6,  "Border Width %d");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phEffBdrR",  &settings->phEffectIconBorderRadius, 0, 50, "Border Radius %d");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phEffBgPad", &settings->phEffectIconBgPadding,    0, 16, "BG Padding %d");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float zoom = settings->phEffectIconZoom;
                        if (ImGui::SliderFloat("##phEffZoom", &zoom, 1.0f, 4.0f, "Icon Zoom %.2f"))
                            settings->phEffectIconZoom = zoom;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float fu = settings->phEffectIconFocusU;
                        if (ImGui::SliderFloat("##phEffFU", &fu, 0.0f, 1.0f, "Focus U %.2f"))
                            settings->phEffectIconFocusU = fu;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float fv = settings->phEffectIconFocusV;
                        if (ImGui::SliderFloat("##phEffFV", &fv, 0.0f, 1.0f, "Focus V %.2f"))
                            settings->phEffectIconFocusV = fv;
                    }
                }
                ImGui::Spacing();
                ImGui::TextDisabled("Icon Depth");
                ImGui::Checkbox("Duotone##phEffDepthDuo", &settings->phEffectDepthDuotone);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Shade each icon from a lit top to a darker bottom\ninstead of one flat color.");
                ImGui::SameLine();
                ImGui::Checkbox("Shadow##phEffDepthSh", &settings->phEffectDepthShadow);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Drop shadow + rim light behind each icon.");
                ImGui::Checkbox("Glow##phEffDepthGlow", &settings->phEffectDepthGlow);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Soft white bloom behind the icon, adding depth.");
                ImGui::SameLine();
                ImGui::Checkbox("Gloss##phEffDepthGloss", &settings->phEffectDepthGloss);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("White sheen across the icon's top half,\nfor a polished look.");
                ImGui::Checkbox("Outline##phEffDepthOl", &settings->phEffectDepthOutline);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Dark contour ring hugging the icon shape.");
                if (settings->phEffectDepthOutline) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phEffDepthOlW", &settings->phEffectDepthOutlineWidth, 1, 3, "Outline Width %d px");
                    static constexpr ImGuiColorEditFlags kOlArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 oc = ColorUtils::ArgbToVec4(settings->phEffectDepthOutlineColor);
                    if (ImGui::ColorEdit4("Outline Color##phEffDepthOlC", reinterpret_cast<float*>(&oc), kOlArgb))
                        settings->phEffectDepthOutlineColor = ColorUtils::Vec4ToArgb(oc);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Alpha channel = outline opacity.");
                }
                if (settings->phShowEffectMagnitude || settings->phShowEffectTimeText) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Font Scales");
                    if (settings->phShowEffectMagnitude) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        float magScale = settings->phEffectMagnitudeScale;
                        if (ImGui::SliderFloat("##phEffMagScale", &magScale, 0.4f, 1.5f, "Magnitude Scale %.2f"))
                            settings->phEffectMagnitudeScale = magScale;
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##phMagOffX", &settings->phEffectMagnitudeOffsetX, -80, 40, "Magnitude Offset X %d");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##phMagOffY", &settings->phEffectMagnitudeOffsetY, -80, 40, "Magnitude Offset Y %d");
                    }
                    if (settings->phShowEffectTimeText) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        float timeScale = settings->phEffectTimeTextScale;
                        if (ImGui::SliderFloat("##phEffTimeScale", &timeScale, 0.4f, 1.5f, "Time Text Scale %.2f"))
                            settings->phEffectTimeTextScale = timeScale;
                    }
                }
                if (settings->phEffectShowSourceBadge) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Source Badge");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        int scaleInt = static_cast<int>(settings->phEffectSourceBadgeScale * 100);
                        if (ImGui::SliderInt("##phSrcScale", &scaleInt, 10, 80, "Badge Size %d%%"))
                            settings->phEffectSourceBadgeScale = scaleInt / 100.0f;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSrcOffX", &settings->phEffectSourceBadgeOffsetX, -30, 60, "Badge Offset X %d");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSrcOffY", &settings->phEffectSourceBadgeOffsetY, -60, 30, "Badge Offset Y %d");
                }
                ImGui::EndTable();
            }
        }
        ImGui::Spacing();
        ImGui::Unindent(10);
    }
}
