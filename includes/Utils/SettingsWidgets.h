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

    inline void DimSwatchIfDisabled()
    {
        const float a = ImGui::GetStyle().Alpha;
        if (a >= 0.999f) return;
        const ImVec2 mn = ImGui::GetItemRectMin();
        const float  sz = ImGui::GetFrameHeight();
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        bg.w = 1.0f - a;
        ImGui::GetWindowDrawList()->AddRectFilled(mn, ImVec2(mn.x + sz, mn.y + sz),
            ImGui::ColorConvertFloat4ToU32(bg), ImGui::GetStyle().FrameRounding);

} // namespace SettingsWidgets
    inline bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0)
    {
        const bool r = ImGui::ColorEdit4(label, col, flags);
        DimSwatchIfDisabled();
        return r;
    }
    inline bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0)
    {
        const bool r = ImGui::ColorEdit3(label, col, flags);
        DimSwatchIfDisabled();
        return r;
    }

}
