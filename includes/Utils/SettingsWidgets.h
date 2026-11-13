#pragma once
#include <imgui.h>
#include <cstring>

namespace SettingsWidgets {

    inline bool SegmentedPicker(const char* label, int* current, const char* const items[], int count,
                                const char* const tooltips[] = nullptr)
    {
        bool changed = false;
        const ImGuiStyle& style = ImGui::GetStyle();

        const char* end = std::strstr(label, "##");
        if (!end)
            end = label + std::strlen(label);
        if (end != label) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ImGui::TextUnformatted(label, end);
            ImGui::PopStyleColor();
        }

        ImGui::PushID(label);
        const ImVec4 selCol  = style.Colors[ImGuiCol_ButtonActive];
        const ImVec4 offCol  = style.Colors[ImGuiCol_FrameBg];
        const ImVec4 offHov  = style.Colors[ImGuiCol_FrameBgHovered];
        const float  gap     = 2.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
        const float rowStartX = ImGui::GetCursorPosX();
        const float rowRight  = ImGui::GetContentRegionAvail().x + rowStartX;

        for (int i = 0; i < count; ++i) {
            const ImVec2 size(ImGui::CalcTextSize(items[i]).x + style.FramePadding.x * 2.0f, 0.0f);
            if (i > 0) {
                ImGui::SameLine();
                if (ImGui::GetCursorPosX() + size.x > rowRight)
                    ImGui::NewLine();
            }

            const bool selected = (*current == i);
            ImGui::PushStyleColor(ImGuiCol_Button,        selected ? selCol : offCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? selCol : offHov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  selCol);
            ImGui::PushID(i);
            if (ImGui::Button(items[i], size)) {
                *current = i;
                changed  = true;
            }
            ImGui::PopID();
            ImGui::PopStyleColor(3);

            if (tooltips && tooltips[i] && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltips[i]);
        }
        ImGui::PopStyleVar();

        ImGui::PopID();
        return changed;
    }

} // namespace SettingsWidgets
