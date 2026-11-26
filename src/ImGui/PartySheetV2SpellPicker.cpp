#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/PartySheetV2Tooltip.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/SpellIconManager.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/SpellCatalog.h"
#include "../../includes/Utils/SpellTags.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/GearSwitcher.h"

#include <imgui.h>
#include <RE/Skyrim.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace PartySheetV2 {

    namespace {
        constexpr ImU32 kAccent    = IM_COL32(212, 175,  55, 255);
        constexpr ImU32 kTextMuted = IM_COL32(136, 136, 136, 255);
        constexpr ImU32 kDamage    = IM_COL32(232, 120, 100, 255);

        std::vector<SpellCatalog::SpellGroup> s_groups;
        RE::FormID                            s_groupsActor = 0;
        std::chrono::steady_clock::time_point s_groupsAt;
        int                                   s_tab = -1;
        std::uint32_t                         s_tagFilter = 0;

        double MsSince(std::chrono::steady_clock::time_point start) {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }

        void FillTags(std::vector<SpellCatalog::SpellGroup>& groups) {
            const auto start  = std::chrono::steady_clock::now();
            const auto before = SpellTags::CachedCount();
            int spells = 0;
            for (auto& g : groups)
                for (auto& e : g.entries) { e.tags = SpellTags::Of(e.form); ++spells; }
            if (const auto added = SpellTags::CachedCount() - before; added > 0 && Settings::GetSingleton()->debugEnableV2PerfLogging)
                logger::info("[V2Perf] tagged {} new spells of {} in {:.2f} ms", added, spells, MsSince(start));
        }

        const std::vector<SpellCatalog::SpellGroup>& GroupsFor(RE::Actor* actor) {
            const auto now = std::chrono::steady_clock::now();
            const bool newActor = actor->GetFormID() != s_groupsActor;
            if (newActor || now - s_groupsAt > std::chrono::seconds(1)) {
                if (newActor) { s_tab = -1; s_tagFilter = 0; }
                s_groups      = SpellCatalog::Grouped(actor);
                FillTags(s_groups);
                s_groupsActor = actor->GetFormID();
                s_groupsAt    = now;
            }
            return s_groups;
        }

        int TabHolding(const std::vector<SpellCatalog::SpellGroup>& groups, const RE::TESForm* held) {
            for (int g = 0; g < static_cast<int>(groups.size()); ++g)
                for (const auto& e : groups[g].entries)
                    if (e.form == held) return g;
            return 0;
        }

        const char* GroupIconName(const SpellCatalog::SpellGroup& g) {
            if (g.group == SpellCatalog::Group::Powers) return "power";
            if (g.group == SpellCatalog::Group::Shouts) return "shout";
            switch (g.school) {
                case RE::ActorValue::kAlteration:  return "alteration";
                case RE::ActorValue::kConjuration: return "conjuration";
                case RE::ActorValue::kDestruction: return "destruction";
                case RE::ActorValue::kIllusion:    return "illusion";
                case RE::ActorValue::kRestoration: return "restoration";
                default:                           return "magic";
            }
        }

        const std::string& GroupLabel(const SpellCatalog::SpellGroup& g) {
            static std::string alt, conj, dest, illu, rest, other, powers, shouts;
            if (alt.empty()) {
                alt    = LocalizationManager::Get("Skills.Alteration");
                conj   = LocalizationManager::Get("Skills.Conjuration");
                dest   = LocalizationManager::Get("Skills.Destruction");
                illu   = LocalizationManager::Get("Skills.Illusion");
                rest   = LocalizationManager::Get("Skills.Restoration");
                other  = LocalizationManager::Get("Common.Other");
                powers = LocalizationManager::Get("PartySheetV2.GroupPowers");
                shouts = LocalizationManager::Get("PartySheetV2.GroupShouts");
            }
            if (g.group == SpellCatalog::Group::Powers) return powers;
            if (g.group == SpellCatalog::Group::Shouts) return shouts;
            switch (g.school) {
                case RE::ActorValue::kAlteration:  return alt;
                case RE::ActorValue::kConjuration: return conj;
                case RE::ActorValue::kDestruction: return dest;
                case RE::ActorValue::kIllusion:    return illu;
                case RE::ActorValue::kRestoration: return rest;
                default:                           return other;
            }
        }

        ID3D11ShaderResourceView* EntryIcon(const SpellCatalog::SpellEntry& e, const SpellCatalog::SpellGroup& g) {
            if (e.group == SpellCatalog::Group::School)
                if (auto* spell = e.form->As<RE::SpellItem>())
                    if (auto* tex = SpellIconManager::GetSpellIcon(spell)) return tex;
            return TextureManager::GetPartyIcon(GroupIconName(g));
        }

        bool BelowCombatSkill(RE::Actor* actor, const RE::TESForm* form) {
            auto* spell = form->As<RE::SpellItem>();
            auto* effect = spell ? spell->GetCostliestEffectItem() : nullptr;
            auto* mgef = effect ? effect->baseEffect : nullptr;
            if (!mgef || mgef->GetMagickSkill() == RE::ActorValue::kNone) return false;
            const float skill = actor->AsActorValueOwner()->GetActorValue(mgef->GetMagickSkill());
            return skill < static_cast<float>(mgef->GetMinimumSkillLevel());
        }

        void DrawTabs(const std::vector<SpellCatalog::SpellGroup>& groups, float sc, float rightEdge) {
            const float tabH  = 36.0f * sc;
            const float gap   = 4.0f * sc;
            const float avail = rightEdge - ImGui::GetCursorScreenPos().x;
            const float tabW  = (avail - gap * static_cast<float>(groups.size() - 1)) / static_cast<float>(groups.size());

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, 0));
            for (int i = 0; i < static_cast<int>(groups.size()); ++i) {
                const bool active = i == s_tab;
                ImGui::PushID(i);
                if (ImGui::InvisibleButton("##spell_tab", ImVec2(tabW, tabH))) s_tab = i;
                ImGui::PopID();

                const ImVec2 bMin = ImGui::GetItemRectMin(), bMax = ImGui::GetItemRectMax();
                const bool   lit  = active || ImGui::IsItemHovered();
                HUDDraw::DrawControlTab(ImGui::GetWindowDrawList(), bMin, bMax, active, ImGui::IsItemHovered(), sc);
                if (auto* tex = TextureManager::GetPartyIcon(GroupIconName(groups[i]))) {
                    const float  side = tabH * 0.8f;
                    const ImVec2 c((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);
                    ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(tex),
                        ImVec2(c.x - side * 0.5f, c.y - side * 0.5f), ImVec2(c.x + side * 0.5f, c.y + side * 0.5f),
                        ImVec2(0, 0), ImVec2(1, 1), lit ? IM_COL32(240, 220, 160, 255) : IM_COL32(200, 200, 200, 170));
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s  %d", GroupLabel(groups[i]).c_str(), static_cast<int>(groups[i].entries.size()));
                if (i + 1 < static_cast<int>(groups.size())) ImGui::SameLine();
            }
            ImGui::PopStyleVar();
        }

        void DrawTagChips(std::uint32_t present, float sc, float rightEdge) {
            if (!present) return;
            const float gap  = 6.0f * sc;
            const float left = ImGui::GetCursorScreenPos().x;
            float x = left;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * sc, 2.0f * sc));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
            for (int i = 0; i < SpellTags::kTagCount; ++i) {
                const std::uint32_t bit = 1u << i;
                if (!(present & bit)) continue;
                const std::string& label = LocalizationManager::Get(SpellTags::LabelKey(i));
                const ImVec2 size(ImGui::CalcTextSize(label.c_str()).x + 20.0f * sc, ImGui::GetFrameHeight());
                if (x != left && x + size.x > rightEdge) x = left;
                else if (x != left) ImGui::SameLine();

                const bool on = (s_tagFilter & bit) != 0;
                ImGui::PushID(i);
                if (ImGui::InvisibleButton("##tag", size)) s_tagFilter ^= bit;
                ImGui::PopID();
                HUDDraw::DrawControlTag(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                        label.c_str(), on, ImGui::IsItemHovered(), sc);
                x += size.x + gap;
            }
            ImGui::PopStyleVar(2);
        }
    }

    void DrawSpellPickerBody(RE::Actor* actor, GearCatalog::Card hand, float sc, ImVec2 wp, ImVec2 ws) {
        const bool firstFrame = s_tab < 0;
        const auto frameStart = std::chrono::steady_clock::now();
        const auto& groups = GroupsFor(actor);
        const bool  leftHand = hand == GearCatalog::Card::LeftHand;
        const RE::TESForm* held      = actor->GetEquippedObject(leftHand);
        const RE::TESForm* otherHeld = actor->GetEquippedObject(!leftHand);
        const RE::TESForm* voiceHeld = actor->GetActorRuntimeData().selectedPower;

        if (groups.empty()) {
            ImGui::TextDisabled("%s", LocalizationManager::Get("Inventory.Empty").c_str());
            return;
        }
        if (s_tab < 0 || s_tab >= static_cast<int>(groups.size())) s_tab = TabHolding(groups, held);

        auto* settings = Settings::GetSingleton();
        ImGui::SetWindowFontScale(settings->psV2FontSmall * sc);
        DrawTabs(groups, sc, wp.x + ws.x - 16.0f * sc);
        ImGui::Dummy(ImVec2(1, 6.0f * sc));

        std::uint32_t present = 0;
        for (const auto& e : groups[s_tab].entries) present |= e.tags;
        const bool schoolTab = groups[s_tab].group == SpellCatalog::Group::School;
        const std::uint32_t filter = schoolTab ? s_tagFilter : (s_tagFilter & present);
        std::vector<int> shown;
        for (int i = 0; i < static_cast<int>(groups[s_tab].entries.size()); ++i)
            if ((groups[s_tab].entries[i].tags & filter) == filter) shown.push_back(i);
        DrawTagChips(present | filter, sc, wp.x + ws.x - 16.0f * sc);
        ImGui::Dummy(ImVec2(1, 4.0f * sc));
        if (shown.empty()) ImGui::TextDisabled("%s", LocalizationManager::Get("PartySheetV2.SpellTagNoMatch").c_str());

        const float scrollbarW = 14.0f;
        const float bodyW   = ws.x - 32.0f * sc - scrollbarW;
        const float gap     = 6.0f * sc;
        const int   columns = 3;
        const float tileW   = (bodyW - gap * (columns - 1)) / columns;
        const float tileH   = tileW;
        const float listH   = (std::max)(1.0f, ws.y - (ImGui::GetCursorScreenPos().y - wp.y) - 14.0f * sc);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(212/255.f,175/255.f,55/255.f,0.4f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(212/255.f,175/255.f,55/255.f,0.7f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,    10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
        ImGui::PushID(s_tab);
        ImGui::BeginChild("##spell_list", ImVec2(bodyW + scrollbarW, listH), false, ImGuiWindowFlags_NoNav);

        if (const float wheel = ImGuiRenderer::ConsumeScrollDelta(); wheel != 0.0f)
            ImGui::SetScrollY(ImGui::GetScrollY() - wheel * 40.0f * sc);

        ImGui::SetWindowFontScale(settings->psV2FontSmall * sc);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
        const auto& group    = groups[s_tab];
        const float pad      = 4.0f * sc;
        const float rounding = 4.0f * sc;
        const float lineH    = ImGui::GetTextLineHeight();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool forFollower = !actor->IsPlayerRef();
        const int  count = static_cast<int>(shown.size());
        const int  rows  = (count + columns - 1) / columns;

        ImGuiListClipper clipper;
        clipper.Begin(rows, tileH + gap);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                for (int col = 0; col < columns; ++col) {
                    const int i = row * columns + col;
                    if (i >= count) break;
                    if (col > 0) ImGui::SameLine();
                    const auto& e = group.entries[shown[i]];
                    ImGui::PushID(i);
                    const bool clicked = ImGui::InvisibleButton("##spell_tile", ImVec2(tileW, tileH));
                    ImGui::PopID();
                    const ImVec2 tp = ImGui::GetItemRectMin(), tq = ImGui::GetItemRectMax();
                    const bool hovered  = ImGui::IsItemHovered();
                    const bool isVoice  = e.group != SpellCatalog::Group::School;
                    const bool inHand   = isVoice ? e.form == voiceHeld : e.form == held;
                    const bool inOther  = !isVoice && e.form == otherHeld;
                    const bool lowSkill = forFollower && BelowCombatSkill(actor, e.form);
                    const auto lock     = SpellCatalog::HandLockOf(e.form);
                    const bool otherHandOnly = lock == (leftHand ? SpellCatalog::HandLock::Right : SpellCatalog::HandLock::Left);
                    if (clicked && !inHand) GearSwitcher::EquipSpell(actor, e.form->GetFormID(), hand);

                    if (hovered) {
                        std::vector<Tip::SumLine> notes;
                        if (lowSkill)
                            notes.push_back({ LocalizationManager::Get("PartySheetV2.SpellSkillTooLow"), {}, {}, kDamage });
                        if (lock != SpellCatalog::HandLock::None)
                            notes.push_back({ LocalizationManager::Get(lock == SpellCatalog::HandLock::Left ? "PartySheetV2.SpellLeftHandOnly"
                                                                                                             : "PartySheetV2.SpellRightHandOnly"),
                                              {}, {}, kTextMuted });
                        if (auto* spell = e.form->As<RE::SpellItem>()) {
                            static const std::string s_rightHand = LocalizationManager::Get("CharacterSheet.slot_right_hand");
                            static const std::string s_leftHand  = LocalizationManager::Get("CharacterSheet.slot_left_hand");
                            Tip::DrawSpellHover(isVoice ? std::string{} : (leftHand ? s_leftHand : s_rightHand), actor, spell, sc, notes);
                        } else {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(e.name.c_str());
                            ImGui::EndTooltip();
                        }
                    }

                    HUDDraw::DrawChip(dl, tp, tq, HUDDraw::PanelChipFromColor(HUDDraw::PackedToArgb(Tip::g_slotChipBase),
                        Tip::kTipChipDarken, hovered ? 1.0f : 0.0f, rounding));
                    if (inHand || inOther)
                        dl->AddRect(tp, tq, inHand ? kAccent : IM_COL32(212, 175, 55, 110), rounding, 0, inHand ? 1.5f : 1.0f);

                    const float w = tq.x - tp.x;
                    if (auto* tex = EntryIcon(e, group)) {
                        const float  px = std::round(w * 0.36f);
                        const ImVec2 ia(std::round(tp.x + (w - px) * 0.5f), std::round(tp.y + pad * 2.0f + lineH));
                        tex = TextureManager::GetIconAtPixelSize(tex, static_cast<int>(px));
                        dl->AddImage(reinterpret_cast<ImTextureID>(tex), ia, ImVec2(ia.x + px, ia.y + px), ImVec2(0, 0), ImVec2(1, 1),
                                     otherHandOnly ? IM_COL32(255, 255, 255, 90) : IM_COL32_WHITE);
                    }

                    char dmg[20] = "";
                    if (const auto d = SpellCatalog::DamageOf(actor, e); d.value > 0.0f)
                        std::snprintf(dmg, sizeof(dmg), d.perSecond ? "%d/s" : "%d", static_cast<int>(d.value + 0.5f));
                    if (dmg[0]) dl->AddText(ImVec2(tp.x + pad, tp.y + pad), kDamage, dmg);

                    char cost[16] = "";
                    if (e.cost > 0.0f) std::snprintf(cost, sizeof(cost), "%d", static_cast<int>(e.cost + 0.5f));
                    if (cost[0]) dl->AddText(ImVec2(tq.x - pad - ImGui::CalcTextSize(cost).x, tp.y + pad), kTextMuted, cost);

                    const ImU32 nameCol = inHand ? kAccent : lowSkill ? kDamage : IM_COL32(200, 200, 200, 255);
                    HUDDraw::DrawTileCaption(dl, tp, tq, pad, nameCol, e.name.c_str());
                }
            }
        }

        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (firstFrame && settings->debugEnableV2PerfLogging)
            logger::info("[V2Perf] Spells window opened for {}, first frame {:.2f} ms, {} spells", actor->GetName(),
                         MsSince(frameStart), KnownSpellCount(actor));
    }

    int KnownSpellCount(RE::Actor* actor) {
        int n = 0;
        for (const auto& g : GroupsFor(actor)) n += static_cast<int>(g.entries.size());
        return n;
    }

    void ResetSpellPicker() { s_tab = -1; s_tagFilter = 0; }

}
