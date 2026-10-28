#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Settings.h"
#include "../../includes/SettingsIni.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/ImGui/WidgetPinnedItemsInternal.h"
#include "../../includes/PinnedItemsPresets.h"

#include <imgui.h>
#include <Windows.h>

namespace ImGuiRenderer::SettingsUI {

    void RenderPinnedSection(Settings* settings)
    {
        if (!settings) return;

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 28, 46, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 58, 84, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        bool ptOpen = ImGui::CollapsingHeader("Pinned Items");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (ptOpen) {
            ImGui::Indent(10);
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget##pt", &settings->enablePinnedWidget);
            ImGui::Checkbox("Use HUD Visibility Trigger##ptHVTrigger", &settings->hvTriggerPinnedItems);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When enabled, the widget fades in when a pinned item count changes,\nthen fades out after the HUD Visibility delay.");
            ImGui::Checkbox("Hide in Combat##pt", &settings->ptHideInCombat);
            ImGui::Spacing();

            ImGui::Text("Presets");
            ImGui::Separator();
            {
                static int presetIndex = 0;
                if (presetIndex < 0 || presetIndex >= (int)PinnedItemsPresets::kPresets.size())
                    presetIndex = 0;
                const char* presetNames[] = {
                    PinnedItemsPresets::kPresets[0].name,
                    PinnedItemsPresets::kPresets[1].name,
                    PinnedItemsPresets::kPresets[2].name
                };
                ImGui::SetNextItemWidth(300.0f);
                if (ImGui::Combo("##ptPreset", &presetIndex, presetNames, (int)PinnedItemsPresets::kPresets.size()))
                    PinnedItemsPresets::ApplyPreset(*settings, (std::size_t)presetIndex);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
            }
            ImGui::Spacing();

            if (!WidgetPinnedItems::g_PinnedItems.empty()) {
                ImGui::Text("Items to Track");
                ImGui::Separator();
                if (ImGui::BeginTable("PTItems", 2, ImGuiTableFlags_SizingStretchSame)) {
                    for (size_t i = 0; i < WidgetPinnedItems::g_PinnedItems.size(); ++i) {
                        auto& e = WidgetPinnedItems::g_PinnedItems[i];
                        int count = (i < WidgetPinnedItems::g_CachedCounts.size()) ? WidgetPinnedItems::g_CachedCounts[i] : 0;

                        ImGui::TableNextColumn();
                        std::string chkLabel = e.label + "##pt" + e.id;
                        if (ImGui::Checkbox(chkLabel.c_str(), &e.enabled)) {
                            std::wstring wkey(e.id.begin(), e.id.end());
                            wkey += L"_enabled";
                            SettingsIni::WritePresetStringOnce(
                                L"PinnedItems", wkey.c_str(), e.enabled ? L"1" : L"0");
                        }
                        ImGui::SameLine();
                        if (e.goal > 0)
                            ImGui::TextDisabled("%d / %d", count, e.goal);
                        else
                            ImGui::TextDisabled("%d", count);
                    }
                    ImGui::EndTable();
                }
                ImGui::Spacing();
            }

            ImGui::Text("Show / Hide");
            ImGui::Separator();

            if (ImGui::BeginTable("PTShowHide", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("Show Label##ptShowLabel", &settings->ptShowLabel);
                ImGui::TableSetColumnIndex(1);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("Icon Glow##ptIconGlowEnabled", &settings->ptIconGlowEnabled);

                ImGui::EndTable();
            }

            ImGui::Spacing();

            if (ImGui::BeginTable("PTColorsStyle", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Colors", ImGuiTableColumnFlags_WidthStretch, 0.3f);
                ImGui::TableSetupColumn("Style",  ImGuiTableColumnFlags_WidthStretch, 0.7f);

                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Style");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ptIconBgColor);
                    if (ImGui::ColorEdit4("Icon Bg##ptIconBgColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ptIconBgColor = ColorUtils::Vec4ToArgb(col);
                }
                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ptIconBorderColor);
                    if (ImGui::ColorEdit4("Icon Border##ptIconBorderColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ptIconBorderColor = ColorUtils::Vec4ToArgb(col);
                }
                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ptBadgeBgColor);
                    if (ImGui::ColorEdit4("Badge##ptBadgeBgColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ptBadgeBgColor = ColorUtils::Vec4ToArgb(col);
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->ptTextColor);
                    if (ImGui::ColorEdit3("Text##ptTextColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs))
                        settings->ptTextColor = ColorUtils::Vec4ToRgb(col);
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->ptGoalMetColor);
                    if (ImGui::ColorEdit3("Goal Met##ptGoalMetColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs))
                        settings->ptGoalMetColor = ColorUtils::Vec4ToRgb(col);
                }
                ImGui::TableSetColumnIndex(1);

                {
                    const char* dirItems[] = { "Column", "Row" };
                    ImGui::SetNextItemWidth(-1);
                    ImGui::Combo("Direction##ptDirection", &settings->ptDirection, dirItems, 2);
                }
                {
                    const char* styleItems[] = { "Inline", "Stacked" };
                    ImGui::SetNextItemWidth(-1);
                    if (settings->ptDirection == 1) ImGui::BeginDisabled();
                    ImGui::Combo("Card Style##ptCardStyle", &settings->ptCardStyle, styleItems, 2);
                    if (settings->ptDirection == 1) ImGui::EndDisabled();
                }
                {
                    const char* countStyles[] = { "Count", "Badge" };
                    ImGui::SetNextItemWidth(-1);
                    ImGui::Combo("Count Style##ptCountStyle", &settings->ptCountStyle, countStyles, 2);
                }
                ImGui::SliderInt("Icon Size##ptIconSize", &settings->ptIconSize, 48, 128);
                ImGui::SliderInt("Icon Border##ptIconBorderWidth", &settings->ptIconBorderWidth, 0, 6);
                ImGui::SliderInt("Border Radius##ptIconBorderRadius", &settings->ptIconBorderRadius, 0, 50);
                ImGui::SliderFloat("Icon Zoom##ptIconZoom", &settings->ptIconZoom, 1.0f, 4.0f, "%.2f");
                ImGui::SliderFloat("Icon Focus X##ptIconFocusU", &settings->ptIconFocusU, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Icon Focus Y##ptIconFocusV", &settings->ptIconFocusV, 0.0f, 1.0f, "%.2f");
                ImGui::SliderInt("Item Width##ptItemWidth", &settings->ptItemWidth, 20, 400);
                ImGui::SliderInt("Spacing##ptRowSpacing", &settings->ptRowSpacing, -20, 20);
                ImGui::SliderInt("Padding Bottom##ptPaddingBottom", &settings->ptPaddingBottom, -20, 40);
                ImGui::SliderFloat("Label Scale##ptLabelScale", &settings->ptLabelScale, 0.5f, 2.0f, "%.2f");
                ImGui::SliderFloat("Count Scale##ptCountScale", &settings->ptCountScale, 0.5f, 2.0f, "%.2f");
                ImGui::SliderInt("Badge Offset X##ptBadgeOffsetX", &settings->ptBadgeOffsetX, -50, 100);
                ImGui::SliderInt("Badge Offset Y##ptBadgeOffsetY", &settings->ptBadgeOffsetY, 0, 100);

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Text("Quick-Consume Hotkey Badge");
            ImGui::Separator();
            ImGui::Checkbox("Show Hotkey Badge##ptShowHotkeyBadge", &settings->ptShowHotkeyBadge);
            if (settings->ptShowHotkeyBadge) {
                ImGui::SliderFloat("Badge Scale##ptHotkeyBadgeScale", &settings->ptHotkeyBadgeScale, 0.3f, 1.5f, "%.2f");
                ImGui::SliderInt("Hotkey Badge X##ptHotkeyBadgeOffsetX", &settings->ptHotkeyBadgeOffsetX, -50, 100);
                ImGui::SliderInt("Hotkey Badge Y##ptHotkeyBadgeOffsetY", &settings->ptHotkeyBadgeOffsetY, -20, 100);
                {
                    ImVec4 col = ColorUtils::ArgbToVec4(settings->ptHotkeyBadgeBgColor);
                    if (ImGui::ColorEdit4("Hotkey Badge BG##ptHotkeyBadgeBgColor", reinterpret_cast<float*>(&col),
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                        settings->ptHotkeyBadgeBgColor = ColorUtils::Vec4ToArgb(col);
                }
                ImGui::TextDisabled("Only the Health/Magicka/Stamina potion cards.");
            }

            ImGui::Spacing();
            ImGui::Text("Position");
            ImGui::Separator();

            if (ImGui::BeginTable("PTPos", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("X Position");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderInt("##ptPosX", &settings->ptPosX, 0, 3840))
                    WidgetPinnedItems::g_ForcePinnedPos = true;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Y Position");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderInt("##ptPosY", &settings->ptPosY, 0, 2160))
                    WidgetPinnedItems::g_ForcePinnedPos = true;

                ImGui::EndTable();
            }
            ImGui::TextDisabled("Drag the widget in the preview to reposition it.");
            ImGui::Spacing();
            ImGui::Unindent(10);
        }
    }

}
