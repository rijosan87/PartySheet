#include "../../includes/ImGui/RendererSections.h"

#include "../../includes/Settings.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/CharacterSheetPresets.h"
#include <imgui.h>

namespace ImGuiRenderer {
    void RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type);
}

namespace ImGuiRenderer::SettingsUI {

    static bool CSInnerHeader(const char* label)
    {
        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(38, 16, 16, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(55, 24, 24, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(72, 32, 32, 255));
        bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_None);
        ImGui::PopStyleColor(3);
        return open;
    }

    void RenderCharacterSheetSection(Settings* settings)
    {
        if (!settings) {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(58, 26, 26, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(78, 36, 36, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(98, 46, 46, 255));
        bool csOpen = ImGui::CollapsingHeader("Character Sheet");
        ImGui::PopStyleColor(3);
        if (csOpen) {
            ImGui::Indent(10);

            ImGui::Checkbox("Enable Character Sheet##CS", &settings->enableCharacterSheet);
            ImGui::Checkbox("Text Shadows##CS", &settings->csTextShadowEnabled);

            ImGui::Spacing();
            ImGui::Text("Presets");
            ImGui::Separator();
            {
                static int presetIndex = 0;
                if (presetIndex < 0 || presetIndex >= (int)CharacterSheetPresets::kPresets.size())
                    presetIndex = 0;
                const char* presetNames[] = {
                    CharacterSheetPresets::kPresets[0].name,
                    CharacterSheetPresets::kPresets[1].name,
                    CharacterSheetPresets::kPresets[2].name
                };
                ImGui::SetNextItemWidth(300.0f);
                if (ImGui::Combo("##csPreset", &presetIndex, presetNames, (int)CharacterSheetPresets::kPresets.size()))
                    CharacterSheetPresets::ApplyPreset(*settings, (std::size_t)presetIndex);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
            }
            ImGui::Spacing();
            ImGui::Separator();

            if (CSInnerHeader("Behavior##CSInner")) {
                ImGui::Indent(10);
                if (ImGui::BeginTable("##BottomTable", 2, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Hide in Magic/Inventory Menu");

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("Others");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Hide Name/Bars##CS", &settings->csHideNameOnMenus);
                    ImGui::Checkbox("Hide Equipments##CS", &settings->csHideEquipmentOnMenus);
                    ImGui::Checkbox("Hide Skills##CS", &settings->csHideSkillsOnMenus);
                    ImGui::Checkbox("Hide Status/Factions##CS", &settings->csHideStatusOnMenus);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("Camera Zoom##CS", &settings->csEnableCameraControl);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Automatically zoom the camera to show your character when the Character Sheet opens.");

                    ImGui::EndTable();

                    if (settings->csEnableCameraControl) {
                        ImGui::Spacing();
                        ImGui::SetNextItemWidth(260.0f);
                        ImGui::SliderFloat("Zoom (distance)##CSCam", &settings->csCameraZoom, 0.0f, 100.0f, "%.0f");
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("How far the camera sits from you when the Character Sheet opens.\nLow = tight closeup, High = pulled back.");
                    }
                }
                ImGui::Unindent(10);
            }

            // ---- Position & Layout ----
            if (CSInnerHeader("Position & Layout##CSInner")) {
                ImGui::Indent(10);
                ImGui::SliderFloat("X Offset##CS", &settings->csOffsetX, -50.0f, 50.0f, "%.0f%%");
                ImGui::SliderFloat("Y Offset##CS", &settings->csOffsetY, -50.0f, 50.0f, "%.0f%%");
                ImGui::SliderFloat("Padding##CS", &settings->csPadding, 0.0f, 100.0f, "%.0f px");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Expands background on left/right. Content stays fixed.");
                }
                ImGui::Unindent(10);
            }

            if (CSInnerHeader("Background##CSInner")) {
                ImGui::Indent(10);
                ImGui::Checkbox("Custom BG##CS", &settings->csUseCustomBackground);

                if (settings->csUseCustomBackground) {
                    ImGui::SameLine();
                    RenderBackgroundPicker("CSPickerPopup", settings->csBackgroundIndex, BackgroundManager::Type::CharacterSheet);
                } else {
                    ImVec4 bgCol = ColorUtils::RgbToVec4(settings->csBackgroundColor);
                    if (ImGui::ColorEdit3("##CSBgColor", (float*)&bgCol, ImGuiColorEditFlags_NoInputs)) {
                        settings->csBackgroundColor = ColorUtils::Vec4ToRgb(bgCol);
                    }
                    ImGui::SameLine();
                    ImGui::Text("Color");

                    ImGui::SameLine();
                    int bgOpacity = static_cast<int>(settings->csBackgroundOpacity * 100);
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::SliderInt("##CSBgOpacity", &bgOpacity, 0, 100, "%d%%")) {
                        settings->csBackgroundOpacity = static_cast<float>(bgOpacity) / 100.0f;
                    }
                    ImGui::SameLine();
                    ImGui::Text("Opacity");
                }
                ImGui::Unindent(10);
            }

            if (CSInnerHeader("Name & Stat Bars##CSInner")) {
                ImGui::Indent(10);

                if (ImGui::BeginTable("##NameStatBarsTable", 3, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Fill Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Text Colors");
                    ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("Scale");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csXpBarColor);
                        if (ImGui::ColorEdit3("##CSXpColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csXpBarColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("XP");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csShoutBarColor);
                        if (ImGui::ColorEdit3("##CSShoutColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csShoutBarColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Shout");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csHealthColor);
                        if (ImGui::ColorEdit3("##CSHealthColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csHealthColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Health");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csStaminaColor);
                        if (ImGui::ColorEdit3("##CSStaminaColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csStaminaColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Stamina");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csMagickaColor);
                        if (ImGui::ColorEdit3("##CSMagickaColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csMagickaColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Magicka");
                    }

                    ImGui::TableSetColumnIndex(1);
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csNameColor);
                        if (ImGui::ColorEdit3("##CSNameColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csNameColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Name");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csRaceColor);
                        if (ImGui::ColorEdit3("##CSRaceColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csRaceColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Race");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csBarTextColor);
                        if (ImGui::ColorEdit3("##CSBarTextColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csBarTextColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Bar Text");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csRegenColor);
                        if (ImGui::ColorEdit3("##CSRegenColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csRegenColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Regen");
                    }

                    ImGui::TableSetColumnIndex(2);
                    {
                        int nameScale = static_cast<int>(settings->csNameFontScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##CSNameScale", &nameScale, 50, 150, "Name %d%%"))
                            settings->csNameFontScale = static_cast<float>(nameScale) / 100.0f;
                    }
                    {
                        int barScale = static_cast<int>(settings->csBarTextScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##CSBarTextScale", &barScale, 50, 150, "Text %d%%"))
                            settings->csBarTextScale = static_cast<float>(barScale) / 100.0f;
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent(10);
            }

            if (CSInnerHeader("Equipment Slots##CSInner")) {
                ImGui::Indent(10);

                if (ImGui::BeginTable("##SlotsShowHideTable", 1, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show / Hide");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Glow##CS", &settings->csGlowEnabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Show Badges##CS", &settings->csShowBadges);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Show armor/damage/enchant stat badges on slots.");

                    ImGui::EndTable();
                }

                ImGui::Spacing();

                if (ImGui::BeginTable("##SlotsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##SlotsCol0", ImGuiTableColumnFlags_WidthStretch, 40.0f);
                    ImGui::TableSetupColumn("##SlotsCol1", ImGuiTableColumnFlags_WidthStretch, 60.0f);
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        ImVec4 col = ColorUtils::ArgbToVec4(settings->csDiamondBorderColor);
                        if (ImGui::ColorEdit4("Equipped##CS", (float*)&col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csDiamondBorderColor = ColorUtils::Vec4ToArgb(col);
                    }
                    {
                        ImVec4 fill = ColorUtils::ArgbToVec4(settings->csDiamondFillColor);
                        if (ImGui::ColorEdit4("Fill##CS", (float*)&fill, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csDiamondFillColor = ColorUtils::Vec4ToArgb(fill);
                    }
                    {
                        ImVec4 empty = ColorUtils::ArgbToVec4(settings->csDiamondEmptyColor);
                        if (ImGui::ColorEdit4("Empty##CS", (float*)&empty, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csDiamondEmptyColor = ColorUtils::Vec4ToArgb(empty);
                    }
                    {
                        ImVec4 labelCol = ColorUtils::RgbToVec4(settings->csSlotLabelColor);
                        if (ImGui::ColorEdit3("Label##CS", (float*)&labelCol, ImGuiColorEditFlags_NoInputs))
                            settings->csSlotLabelColor = ColorUtils::Vec4ToRgb(labelCol);
                    }
                    if (settings->csGlowEnabled) {
                        ImVec4 glowCol = ColorUtils::RgbToVec4(settings->csGlowColor);
                        if (ImGui::ColorEdit3("Glow##CSGlowColor", (float*)&glowCol, ImGuiColorEditFlags_NoInputs))
                            settings->csGlowColor = ColorUtils::Vec4ToRgb(glowCol);
                    }

                    ImGui::TableSetColumnIndex(1);
                    {
                        int fontScale = static_cast<int>(settings->csSlotFontScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##SlotFontScaleCS", &fontScale, 50, 150, "Label Scale %d%%"))
                            settings->csSlotFontScale = static_cast<float>(fontScale) / 100.0f;
                    }
                    {
                        int badgeScale = static_cast<int>(settings->csBadgeTextScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##BadgeTextScaleCS", &badgeScale, 50, 150, "Badge Scale %d%%"))
                            settings->csBadgeTextScale = static_cast<float>(badgeScale) / 100.0f;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderFloat("##DiamondSizeCS", &settings->csDiamondSize, 36.0f, 72.0f, "Diamond Size %.0f px");

                    ImGui::EndTable();
                }
                ImGui::Unindent(10);
            }

            if (CSInnerHeader("Stats & Resistances##CSInner")) {
                ImGui::Indent(10);

                if (ImGui::BeginTable("##StatsShowHideTable", 1, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show / Hide");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Stat Glow##CSStatGlow", &settings->csStatGlowEnabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Resist Glow##CSResistGlow", &settings->csResistGlowEnabled);

                    ImGui::EndTable();
                }

                ImGui::Spacing();

                if (ImGui::BeginTable("##StatsResistTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##StatsCol0", ImGuiTableColumnFlags_WidthStretch, 40.0f);
                    ImGui::TableSetupColumn("##StatsCol1", ImGuiTableColumnFlags_WidthStretch, 60.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        ImVec4 bgCol = ColorUtils::ArgbToVec4(settings->csStatBgColor);
                        if (ImGui::ColorEdit4("##CSStatBgColor", (float*)&bgCol, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csStatBgColor = ColorUtils::Vec4ToArgb(bgCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Stat BG");
                    }
                    {
                        ImVec4 bgCol = ColorUtils::ArgbToVec4(settings->csResistBgColor);
                        if (ImGui::ColorEdit4("##CSResistBgColor", (float*)&bgCol, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csResistBgColor = ColorUtils::Vec4ToArgb(bgCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Resist BG");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csStatLabelColor);
                        if (ImGui::ColorEdit3("Stat Label##CSStatLabel", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csStatLabelColor = ColorUtils::Vec4ToRgb(col);
                    }
                    if (settings->csStatGlowEnabled) {
                        ImVec4 glowCol = ColorUtils::RgbToVec4(settings->csStatGlowColor);
                        if (ImGui::ColorEdit3("##CSStatGlowColor", (float*)&glowCol, ImGuiColorEditFlags_NoInputs))
                            settings->csStatGlowColor = ColorUtils::Vec4ToRgb(glowCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Stat Glow");
                    }
                    if (settings->csResistGlowEnabled) {
                        ImVec4 glowCol = ColorUtils::RgbToVec4(settings->csResistGlowColor);
                        if (ImGui::ColorEdit3("##CSResistGlowColor", (float*)&glowCol, ImGuiColorEditFlags_NoInputs))
                            settings->csResistGlowColor = ColorUtils::Vec4ToRgb(glowCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Resist Glow");
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##StatIconSizeCS", &settings->csStatIconSize, 16, 36, "Stat Icon %dpx");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##ResistIconSizeCS", &settings->csResistIconSize, 16, 40, "Resist Icon %dpx");
                    {
                        int fontScale = static_cast<int>(settings->csStatFontScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##StatFontScaleCS", &fontScale, 50, 150, "Font Scale %d%%"))
                            settings->csStatFontScale = static_cast<float>(fontScale) / 100.0f;
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent(10);
            }

            if (CSInnerHeader("Skills##CSInner")) {
                ImGui::Indent(10);

                if (ImGui::BeginTable("##SkillsShowHide", 1, ImGuiTableFlags_None)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show / Hide");
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Skill Glow##CSSkillGlow", &settings->csSkillGlowEnabled);
                    ImGui::EndTable();
                }

                if (ImGui::BeginTable("##SkillsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##SkillsCol0", ImGuiTableColumnFlags_WidthStretch, 40.0f);
                    ImGui::TableSetupColumn("##SkillsCol1", ImGuiTableColumnFlags_WidthStretch, 60.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        ImVec4 bgCol = ColorUtils::ArgbToVec4(settings->csSkillBgColor);
                        if (ImGui::ColorEdit4("##CSSkillBgColor", (float*)&bgCol, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csSkillBgColor = ColorUtils::Vec4ToArgb(bgCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Background");
                    }
                    {
                        ImVec4 col = ColorUtils::RgbToVec4(settings->csSkillTextColor);
                        if (ImGui::ColorEdit3("##CSSkillTextColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                            settings->csSkillTextColor = ColorUtils::Vec4ToRgb(col);
                        ImGui::SameLine(); ImGui::TextUnformatted("Text Color");
                    }
                    if (settings->csSkillGlowEnabled) {
                        ImVec4 glowCol = ColorUtils::RgbToVec4(settings->csSkillGlowColor);
                        if (ImGui::ColorEdit3("##CSSkillGlowColor", (float*)&glowCol, ImGuiColorEditFlags_NoInputs))
                            settings->csSkillGlowColor = ColorUtils::Vec4ToRgb(glowCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Skill Glow");
                    }

                    ImGui::TableSetColumnIndex(1);
                    {
                        int fontScale = static_cast<int>(settings->csSkillFontScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##SkillFontScaleCS", &fontScale, 50, 150, "Font Scale %d%%"))
                            settings->csSkillFontScale = static_cast<float>(fontScale) / 100.0f;
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent(10);
            }

            if (CSInnerHeader("Status & Factions##CSInner")) {
                ImGui::Indent(10);

                if (ImGui::BeginTable("##StatusShowHide", 1, ImGuiTableFlags_None)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show / Hide");
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Status Glow##CSStatusGlow", &settings->csStatusGlowEnabled);
                    ImGui::EndTable();
                }

                if (ImGui::BeginTable("##StatusTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##StatusCol0", ImGuiTableColumnFlags_WidthStretch, 40.0f);
                    ImGui::TableSetupColumn("##StatusCol1", ImGuiTableColumnFlags_WidthStretch, 60.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        ImVec4 bgCol = ColorUtils::ArgbToVec4(settings->csStatusBgColor);
                        if (ImGui::ColorEdit4("##CSStatusBgColor", (float*)&bgCol, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                            settings->csStatusBgColor = ColorUtils::Vec4ToArgb(bgCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Background");
                    }
                    if (settings->csStatusGlowEnabled) {
                        ImVec4 glowCol = ColorUtils::RgbToVec4(settings->csStatusGlowColor);
                        if (ImGui::ColorEdit3("##CSStatusGlowColor", (float*)&glowCol, ImGuiColorEditFlags_NoInputs))
                            settings->csStatusGlowColor = ColorUtils::Vec4ToRgb(glowCol);
                        ImGui::SameLine(); ImGui::TextUnformatted("Status Glow");
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderFloat("##CarouselCS", &settings->csCarouselInterval, 1.0f, 10.0f, "Carousel %.1f sec");
                    {
                        int fontScale = static_cast<int>(settings->csStatusFontScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##StatusFontScaleCS", &fontScale, 50, 150, "Font Scale %d%%"))
                            settings->csStatusFontScale = static_cast<float>(fontScale) / 100.0f;
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent(10);
            }

            ImGui::Unindent(10);
        }
    }

}
