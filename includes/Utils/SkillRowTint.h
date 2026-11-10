#pragma once

#include "../BadgeStyle.h"
#include "../SurvivalSymptoms.h"
#include "WornGearStatResolver.h"
#include <imgui.h>

namespace SkillRowTint {

    struct Tint {
        bool                            on    = false;
        ImU32                           bg    = 0;
        ImU32                           name  = 0;
        ImU32                           value = 0;
        const SurvivalSymptoms::Causes* cuts  = nullptr;
    };

    inline Tint Get(RE::Actor* actor, RE::ActorValue skill, const WornGearStatResolver::WornGearData& gear)
    {
        Tint t;
        const auto& cuts = SurvivalSymptoms::GetSkillCauses(actor, skill);
        const std::uint32_t bit = WornGearStatResolver::SkillBit(skill);
        if (!cuts.empty()) {
            t = { true, IM_COL32(100, 38, 38, 200), IM_COL32(255, 120, 105, 255), IM_COL32(255, 140, 125, 255), &cuts };
        } else if (gear.skillHighlightMask & bit) {
            t = { true, BadgeStyle::Theme::SkillBlue().fill, IM_COL32(255, 255, 255, 255), IM_COL32(255, 255, 255, 255) };
        } else if (gear.situationalHighlightMask & bit) {
            t = { true, IM_COL32(80, 65, 30, 220), IM_COL32(190, 165, 100, 255), IM_COL32(190, 165, 100, 255) };
        }
        return t;
    }

    inline void HoverTooltip(const Tint& t, ImVec2 rMin, ImVec2 rMax)
    {
        if (t.cuts && !t.cuts->empty() && ImGui::IsMouseHoveringRect(rMin, rMax))
            SurvivalSymptoms::ShowCausesTooltip(*t.cuts);
    }

}
