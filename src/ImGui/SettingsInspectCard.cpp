#include "../../includes/ImGui/RendererSections.h"

#include "../../includes/Settings.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/Utils/ColorUtils.h"
#include <imgui.h>

namespace ImGuiRenderer {
    void RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type);
}

namespace ImGuiRenderer::SettingsUI {

    void RenderInspectCardSection(Settings* settings, const ImVec2& displaySize)
    {
        if (!settings) {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(26, 45, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(36, 60, 84, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(46, 75, 104, 255));
        bool icOpen = ImGui::CollapsingHeader("Inspect Card HUD");
        ImGui::PopStyleColor(3);
        if (!icOpen) return;

        ImGui::Indent(10);

        ImGui::Spacing();
        ImGui::TextDisabled("Layout");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-200);
        ImGui::SliderInt("X Position##ICPosX", &settings->inspectCardPosX, -1, static_cast<int>(displaySize.x), "%d");
        ImGui::SetNextItemWidth(-200);
        ImGui::SliderInt("Y Position##ICPosY", &settings->inspectCardPosY, -1, static_cast<int>(displaySize.y), "%d");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Background");
        ImGui::Spacing();

        ImGui::Checkbox("Custom BG##IC", &settings->icUseCustomBackground);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enable/disable custom background");
        if (settings->icUseCustomBackground) {
            ImGui::SameLine();
            RenderBackgroundPicker("ICPickerPopup", settings->icBackgroundIndex, BackgroundManager::Type::Inspectcard);
        } else {
            ImVec4 bgCol = ColorUtils::RgbToVec4(settings->inspectCardBgColor);
            if (ImGui::ColorEdit3("##ICBgColor", &bgCol.x, ImGuiColorEditFlags_NoInputs))
                settings->inspectCardBgColor = ColorUtils::Vec4ToRgb(bgCol);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            ImGui::SliderFloat("Opacity##IC", &settings->inspectCardBgOpacity, 0.0f, 1.0f, "Opacity %.2f");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Bars");
        ImGui::Spacing();

        if (ImGui::BeginTable("##ICBarsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("##BarsCol0", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("##BarsCol1", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableNextRow();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
            ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Styles");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                ImVec4 col = ColorUtils::RgbToVec4(settings->inspectCardHealthColor);
                if (ImGui::ColorEdit3("##ICHealth", (float*)&col, ImGuiColorEditFlags_NoInputs))
                    settings->inspectCardHealthColor = ColorUtils::Vec4ToRgb(col);
                ImGui::SameLine(); ImGui::TextUnformatted("Health");
            }
            {
                ImVec4 col = ColorUtils::RgbToVec4(settings->inspectCardStaminaColor);
                if (ImGui::ColorEdit3("##ICStamina", (float*)&col, ImGuiColorEditFlags_NoInputs))
                    settings->inspectCardStaminaColor = ColorUtils::Vec4ToRgb(col);
                ImGui::SameLine(); ImGui::TextUnformatted("Stamina");
            }
            {
                ImVec4 col = ColorUtils::RgbToVec4(settings->inspectCardMagickaColor);
                if (ImGui::ColorEdit3("##ICMagicka", (float*)&col, ImGuiColorEditFlags_NoInputs))
                    settings->inspectCardMagickaColor = ColorUtils::Vec4ToRgb(col);
                ImGui::SameLine(); ImGui::TextUnformatted("Magicka");
            }
            {
                ImVec4 col = ColorUtils::RgbToVec4(settings->inspectCardBarBorderColor);
                if (ImGui::ColorEdit3("##ICBorderColor", &col.x, ImGuiColorEditFlags_NoInputs))
                    settings->inspectCardBarBorderColor = ColorUtils::Vec4ToRgb(col);
                ImGui::SameLine(); ImGui::TextUnformatted("Border Color");
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ICBarWidth", &settings->inspectCardBarWidth, 150, 400, "Bar Width %d");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ICBarHeight", &settings->inspectCardBarHeight, 8, 30, "Bar Height %d");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ICBorderWidth", &settings->inspectCardBarBorderWidth, 0, 3, "Border Width %d");

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Text & Icons");
        ImGui::Spacing();

        if (ImGui::BeginTable("##ICTextTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("##TextCol0", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("##TextCol1", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableNextRow();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
            ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Styles");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                ImVec4 col = ColorUtils::RgbToVec4(settings->inspectCardNameColor);
                if (ImGui::ColorEdit3("##ICNameColor", &col.x, ImGuiColorEditFlags_NoInputs))
                    settings->inspectCardNameColor = ColorUtils::Vec4ToRgb(col);
                ImGui::SameLine(); ImGui::TextUnformatted("Name Color");
            }

            ImGui::TableSetColumnIndex(1);
            {
                int v = static_cast<int>(settings->inspectCardFontScale * 100);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::SliderInt("##ICNameScale", &v, 50, 150, "Name Scale %d%%"))
                    settings->inspectCardFontScale = static_cast<float>(v) / 100.0f;
            }
            {
                int v = static_cast<int>(settings->icBodyFontScale * 100);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::SliderInt("##ICBodyScale", &v, 50, 150, "Body Scale %d%%"))
                    settings->icBodyFontScale = static_cast<float>(v) / 100.0f;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Body text: level/race, relationship, bar text, armor rating, resistances");
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ICStatIconSize", &settings->inspectCardIconSize, 12, 32, "Stat Icon %dpx");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ICResistIconSize", &settings->inspectCardResistIconSize, 12, 32, "Resist Icon %dpx");

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Know Your Enemy 2");
        ImGui::Spacing();

        ImGui::Checkbox("Creature Type Badges##IC", &settings->icShowCreatureTypeBadges);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Type/trait pills (Animal, Undead, Skeleton...) at Alteration 15+.\nWorks without KYE2 using vanilla actor keywords.");
        ImGui::Checkbox("KYE2 Resistances##IC", &settings->icShowKYEResists);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Weak/Resistant breakdown for weapon & magic types at Alteration 35+.\nHidden if Know Your Enemy 2 is not installed.");

        ImGui::Unindent(10);
    }

}
