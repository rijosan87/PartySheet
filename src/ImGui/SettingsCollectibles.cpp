#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Settings.h"
#include "../../includes/SettingsIni.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/ImGui/WidgetCollectiblesInternal.h"
#include "../../includes/CollectiblesPresets.h"

#include <imgui.h>
#include <Windows.h>

namespace ImGuiRenderer {
    void RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type);
}

namespace ImGuiRenderer::SettingsUI {

    void RenderCollectiblesSection(Settings* settings)
    {
        if (!settings) return;

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 28, 46, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 58, 84, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        bool ccOpen = ImGui::CollapsingHeader("Collectibles");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (ccOpen) {
            ImGui::Indent(10);
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget", &settings->enableCollectiblesWidget);
            ImGui::Checkbox("Use HUD Visibility Trigger##ccHVTrigger", &settings->hvTriggerCollectibles);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When enabled, the widget fades in when a collectible count changes,\nthen fades out after the HUD Visibility delay.");
            ImGui::Checkbox("Hide in Combat##cc", &settings->ccHideInCombat);
            ImGui::Spacing();

            ImGui::Text("Presets");
            ImGui::Separator();
            {
                static int presetIndex = 0;
                if (presetIndex < 0 || presetIndex >= (int)CollectiblesPresets::kPresets.size())
                    presetIndex = 0;
                const char* presetNames[] = {
                    CollectiblesPresets::kPresets[0].name,
                    CollectiblesPresets::kPresets[1].name,
                    CollectiblesPresets::kPresets[2].name
                };
                ImGui::SetNextItemWidth(300.0f);
                if (ImGui::Combo("##ccPreset", &presetIndex, presetNames, (int)CollectiblesPresets::kPresets.size()))
                    CollectiblesPresets::ApplyPreset(*settings, (std::size_t)presetIndex);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
            }
            ImGui::Spacing();

            if (!WidgetCollectibles::g_Collectibles.empty()) {
                ImGui::Text("Items to Track");
                ImGui::TextDisabled("Items with 0 count are hidden.");
                ImGui::Separator();
                if (ImGui::BeginTable("CCItems", 2, ImGuiTableFlags_SizingStretchSame)) {
                    for (size_t i = 0; i < WidgetCollectibles::g_Collectibles.size(); ++i) {
                        auto& e = WidgetCollectibles::g_Collectibles[i];
                        bool questDone = WidgetCollectibles::IsQuestComplete(e);
                        int count = (i < WidgetCollectibles::g_CachedCounts.size()) ? WidgetCollectibles::g_CachedCounts[i] : 0;

                        ImGui::TableNextColumn();
                        if (questDone) {
                            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 200, 100, 180));
                            ImGui::Text("%s  [Complete]", e.label.c_str());
                            ImGui::PopStyleColor();
                        } else {
                            std::string chkLabel = e.label + "##cc" + e.id;
                            if (ImGui::Checkbox(chkLabel.c_str(), &e.enabled)) {
                                std::wstring wkey(e.id.begin(), e.id.end());
                                wkey += L"_enabled";
                                SettingsIni::WritePresetStringOnce(
                                    L"Collectibles", wkey.c_str(), e.enabled ? L"1" : L"0");
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("%d / %d", count, e.goal);
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::Spacing();
            }

            ImGui::Text("Show / Hide");
            ImGui::Separator();

            if (ImGui::BeginTable("CCShowHide", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("Show Label##ccShowLabel", &settings->ccShowLabel);
                ImGui::TableSetColumnIndex(1);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("Custom Bar Style##ccBarStyleEnabled", &settings->ccBarStyleEnabled);
                if (settings->ccBarStyleEnabled) {
                    ImGui::SameLine();
                    RenderBackgroundPicker("CCBarStylePicker", settings->ccBarStyleIndex, BackgroundManager::Type::BarStyle);
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();

            if (ImGui::BeginTable("CCColorsStyle", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Colors", ImGuiTableColumnFlags_WidthStretch, 0.3f);
                ImGui::TableSetupColumn("Style",  ImGuiTableColumnFlags_WidthStretch, 0.7f);

                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Style");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ccIconBgColor);
                    if (ImGui::ColorEdit4("Icon Bg##ccIconBgColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ccIconBgColor = ColorUtils::Vec4ToArgb(col);
                }
                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ccIconBorderColor);
                    if (ImGui::ColorEdit4("Icon Border##ccIconBorderColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ccIconBorderColor = ColorUtils::Vec4ToArgb(col);
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->ccTextColor);
                    if (ImGui::ColorEdit3("Text##ccTextColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs))
                        settings->ccTextColor = ColorUtils::Vec4ToRgb(col);
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->ccGoalMetColor);
                    if (ImGui::ColorEdit3("Goal Met##ccGoalMetColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs))
                        settings->ccGoalMetColor = ColorUtils::Vec4ToRgb(col);
                }
                if (settings->ccBarStyleEnabled) {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->ccBarColor);
                    if (ImGui::ColorEdit3("Bar##ccBarColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs))
                        settings->ccBarColor = ColorUtils::Vec4ToRgb(col);
                }
                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ccBgColor);
                    if (ImGui::ColorEdit4("Background##ccBgColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ccBgColor = ColorUtils::Vec4ToArgb(col);
                }

                ImGui::TableSetColumnIndex(1);
                {
                    const char* dirItems[] = { "Column", "Row" };
                    ImGui::SetNextItemWidth(-1);
                    ImGui::Combo("Direction##ccDirection", &settings->ccDirection, dirItems, 2);
                }
                {
                    const char* styleItems[] = { "Inline", "Stacked" };
                    ImGui::SetNextItemWidth(-1);
                    ImGui::Combo("Card Style##ccCardStyle", &settings->ccCardStyle, styleItems, 2);
                }
                ImGui::SliderFloat("Label Scale##ccLabelScale", &settings->ccLabelScale, 0.5f, 2.0f, "%.2f");
                ImGui::SliderFloat("Bar Text Scale##ccBarTextScale", &settings->ccBarTextScale, 0.5f, 2.0f, "%.2f");
                ImGui::SliderInt("Icon Size##ccIconSize", &settings->ccIconSize, 48, 128);
                ImGui::SliderInt("Icon Border##ccIconBorderWidth", &settings->ccIconBorderWidth, 0, 6);
                ImGui::SliderInt("Border Radius##ccIconBorderRadius", &settings->ccIconBorderRadius, 0, 50);
                ImGui::Checkbox("Icon Glow##ccIconGlow", &settings->ccIconGlowEnabled);
                ImGui::SliderFloat("Icon Zoom##ccIconZoom",   &settings->ccIconZoom,   1.0f, 4.0f, "%.2f");
                ImGui::SliderFloat("Icon Focus X##ccIconFocusU", &settings->ccIconFocusU, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Icon Focus Y##ccIconFocusV", &settings->ccIconFocusV, 0.0f, 1.0f, "%.2f");
                ImGui::SliderInt("Bar Width##ccBarWidth", &settings->ccBarWidth, 40, 400);
                ImGui::SliderInt("Bar Height##ccBarHeight", &settings->ccBarHeight, 10, 48);
                ImGui::SliderInt("Spacing##ccRowSpacing", &settings->ccRowSpacing, -20, 20);
                ImGui::SliderInt("Padding Bottom##ccPaddingBottom", &settings->ccPaddingBottom, 0, 40);

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Text("Position");
            ImGui::Separator();

            if (ImGui::BeginTable("CCPos", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("X Position");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderInt("##ccPosX", &settings->ccPosX, 0, 3840))
                    WidgetCollectibles::g_ForceCollectiblesPos = true;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Y Position");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderInt("##ccPosY", &settings->ccPosY, 0, 2160))
                    WidgetCollectibles::g_ForceCollectiblesPos = true;

                ImGui::EndTable();
            }
            ImGui::TextDisabled("Drag the widget in the preview to reposition it.");
            ImGui::Spacing();
            ImGui::Unindent(10);
        }
    }

}
