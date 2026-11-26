#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Settings.h"
#include "../../includes/SettingsIni.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/SettingsWidgets.h"
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
                const char* presetNames[PinnedItemsPresets::kPresets.size()];
                for (std::size_t i = 0; i < PinnedItemsPresets::kPresets.size(); ++i)
                    presetNames[i] = PinnedItemsPresets::kPresets[i].name;
                if (SettingsWidgets::SegmentedPicker("##ptPreset", &presetIndex, presetNames, (int)PinnedItemsPresets::kPresets.size()))
                    PinnedItemsPresets::ApplyPreset(*settings, (std::size_t)presetIndex);
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

            auto ColorArgb = [](const char* label, std::uint32_t* argb) {
                ImVec4 col = ColorUtils::ArgbToVec4(*argb);
                if (ImGui::ColorEdit4(label, reinterpret_cast<float*>(&col),
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
                    *argb = ColorUtils::Vec4ToArgb(col);
            };
            auto ColorRgb = [](const char* label, std::uint32_t* rgb) {
                ImVec4 col = ColorUtils::RgbToVec4(*rgb);
                if (ImGui::ColorEdit3(label, reinterpret_cast<float*>(&col), ImGuiColorEditFlags_NoInputs))
                    *rgb = ColorUtils::Vec4ToRgb(col);
            };

            if (ImGui::CollapsingHeader("Layout##ptSecLayout", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* anchorItems[] = { "Top Left", "Top Right", "Bottom Left", "Bottom Right" };
                SettingsWidgets::SegmentedPicker("Anchor##ptAnchor", &settings->ptAnchor, anchorItems, 4);
                const char* dirItems[] = { "Column", "Row" };
                SettingsWidgets::SegmentedPicker("Direction##ptDirection", &settings->ptDirection, dirItems, 2);
                const char* placeItems[] = { "Below", "Above", "Left", "Right" };
                SettingsWidgets::SegmentedPicker("Label Placement##ptLabelPlacement", &settings->ptLabelPlacement, placeItems, 4);
                ImGui::SliderInt("Item Width##ptItemWidth", &settings->ptItemWidth, 20, 400);
                ImGui::SliderInt("Spacing##ptRowSpacing", &settings->ptRowSpacing, -20, 20);
            }

            if (ImGui::CollapsingHeader("Icon##ptSecIcon")) {
                ImGui::SliderInt("Size##ptIconSize", &settings->ptIconSize, 48, 128);
                ImGui::SliderFloat("Zoom##ptIconZoom", &settings->ptIconZoom, 0.5f, 4.0f, "%.2f");
                ColorArgb("Background##ptIconBgColor", &settings->ptIconBgColor);
                ColorArgb("Border Colour##ptIconBorderColor", &settings->ptIconBorderColor);
                ImGui::SliderInt("Border Width##ptIconBorderWidth", &settings->ptIconBorderWidth, 0, 6);
                ImGui::SliderInt("Corner Radius##ptIconBorderRadius", &settings->ptIconBorderRadius, 0, 50);
                ImGui::Checkbox("Glow##ptIconGlowEnabled", &settings->ptIconGlowEnabled);
            }

            if (ImGui::CollapsingHeader("Label##ptSecLabel")) {
                ImGui::Checkbox("Show Label##ptShowLabel", &settings->ptShowLabel);
                ImGui::BeginDisabled(!settings->ptShowLabel);
                ImGui::SliderFloat("Label Scale##ptLabelScale", &settings->ptLabelScale, 0.5f, 2.0f, "%.2f");
                ImGui::EndDisabled();
                ColorRgb("Text Colour##ptTextColor", &settings->ptTextColor);
                ImGui::SetItemTooltip("Label and plain count text.");

                const bool sideLabel = settings->ptShowLabel && settings->ptLabelPlacement >= 2;
                ImGui::BeginDisabled(!sideLabel);
                SettingsWidgets::SegmentedPicker("Strip##ptStripStyle", &settings->ptStripStyle,
                                                 HUDDraw::kStripStyleNames, HUDDraw::StripStyle_Count);
                if (!sideLabel) ImGui::SetItemTooltip("Needs a label to the Left or Right of the icon.");
                if (settings->ptStripStyle > 0) {
                    ImGui::SliderInt("Strip Width##ptStripWidth", &settings->ptStripWidth, 0, 400);
                    ImGui::SetItemTooltip("0 fits the text. Set a width to make every row the same length.");
                    ColorArgb("Strip Colour##ptStripTint", &settings->ptStripTint);
            }
                ImGui::EndDisabled();
            }

            if (ImGui::CollapsingHeader("Count##ptSecCount")) {

                {
                    const char* posItems[] = { "On Icon", "Before Name", "Strip End" };
                    SettingsWidgets::SegmentedPicker("Position##ptCountPosition", &settings->ptCountPosition, posItems, 3);
                    ImGui::SetItemTooltip("Before Name and Strip End need Show Label, Strip End a side label.");
                }
                SettingsWidgets::SegmentedPicker("Style##ptCountStyle", &settings->ptCountStyle,
                                                 HUDDraw::kBadgePresetNames, HUDDraw::BadgePreset_Count);
                if (settings->ptCountStyle != HUDDraw::BadgePreset_Text) {
                    ColorArgb("Tint##ptBadgeTint", &settings->ptBadgeTint);
                    ImGui::SetItemTooltip("Alpha 0 keeps the style's own colours.");
                }
                {
                    const char* spotItems[] = { "Top Left", "Top Right", "Center", "Bottom Left", "Bottom Right" };
                    const int   spotValues[] = { 0, 2, 4, 6, 8 };
                    int pick = 0;
                    for (int i = 0; i < 5; ++i) if (spotValues[i] == settings->ptCountAnchor) pick = i;
                    if (SettingsWidgets::SegmentedPicker("Anchor##ptCountAnchor", &pick, spotItems, 5))
                        settings->ptCountAnchor = spotValues[pick];
                }
                ImGui::SliderFloat("Scale##ptCountScale", &settings->ptCountScale, 0.5f, 2.0f, "%.2f");
                ImGui::SliderInt("Offset X##ptBadgeOffsetX", &settings->ptBadgeOffsetX, -50, 100);
                ImGui::SliderInt("Offset Y##ptBadgeOffsetY", &settings->ptBadgeOffsetY, 0, 100);
                ColorRgb("Goal Met Colour##ptGoalMetColor", &settings->ptGoalMetColor);
            }

            if (ImGui::CollapsingHeader("Hotkey Badge##ptSecHotkey")) {
                ImGui::TextDisabled("Only the Health, Magicka and Stamina potion cards.");
                {
                    const char* posItems[] = { "On Icon", "Strip End" };
                    SettingsWidgets::SegmentedPicker("Position##ptHotkeyPosition", &settings->ptHotkeyPosition, posItems, 2);
                    ImGui::SetItemTooltip("Strip End shows the key as text at the end of a side label.");
                }
                const char* hotkeyStyles[HUDDraw::BadgePreset_Count + 1] = { "Off" };
                for (int i = 0; i < HUDDraw::BadgePreset_Count; ++i) hotkeyStyles[i + 1] = HUDDraw::kBadgePresetNames[i];
                int pick = settings->ptHotkeyBadgeStyle + 1;
                if (SettingsWidgets::SegmentedPicker("Style##ptHotkeyBadgeStyle", &pick, hotkeyStyles, HUDDraw::BadgePreset_Count + 1))
                    settings->ptHotkeyBadgeStyle = pick - 1;
                if (settings->ptHotkeyBadgeStyle >= 0) {
                    if (settings->ptHotkeyBadgeStyle != HUDDraw::BadgePreset_Text) {
                        ColorArgb("Tint##ptHotkeyBadgeTint", &settings->ptHotkeyBadgeTint);
                        ImGui::SetItemTooltip("Alpha 0 keeps the style's own colours.");
                }
                    ImGui::SliderFloat("Scale##ptHotkeyBadgeScale", &settings->ptHotkeyBadgeScale, 0.3f, 2.0f, "%.2f");
                    ImGui::SliderInt("Offset X##ptHotkeyBadgeOffsetX", &settings->ptHotkeyBadgeOffsetX, -50, 100);
                    ImGui::SliderInt("Offset Y##ptHotkeyBadgeOffsetY", &settings->ptHotkeyBadgeOffsetY, -20, 100);
                }
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
