#include "../includes/SurvivalSymptoms.h"
#include "../includes/ImGui/PlayerHUDPenalties.h"
#include "../includes/EffectCategories.h"
#include "../includes/ImGui/PlayerHUDSurvival.h"
#include "../includes/LocalizationManager.h"
#include <array>
#include <unordered_map>
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace SurvivalSymptoms {

    namespace {
        using AV   = RE::ActorValue;
        using Arch = RE::EffectArchetypes::ArchetypeID;

        using EffectCategories::HudTarget;

        Bar* BarFor(Snapshot& s, HudTarget t)
        {
            switch (t) {
            case HudTarget::HealthMax:   case HudTarget::HealthRegen:  return &s.health;
            case HudTarget::MagickaMax:  case HudTarget::MagickaRegen: return &s.magicka;
            case HudTarget::StaminaMax:  case HudTarget::StaminaRegen: return &s.stamina;
            default:                                                  return nullptr;
            }
        }

        bool IsRegen(HudTarget t)
        {
            return t == HudTarget::HealthRegen || t == HudTarget::MagickaRegen || t == HudTarget::StaminaRegen;
        }

        Mark* MarkFor(Equip& e, HudTarget t)
        {
            switch (t) {
            case HudTarget::WeaponOneHanded: return &e.oneHanded;
            case HudTarget::WeaponTwoHanded: return &e.twoHanded;
            case HudTarget::WeaponRanged:    return &e.ranged;
            case HudTarget::WeaponMelee:     return &e.melee;
            case HudTarget::WeaponAny:       return &e.any;
            case HudTarget::Shield:          return &e.shield;
            case HudTarget::Shout:           return &e.shout;
            default:                         return nullptr;
            }
        }

        void SetMark(Mark& m, bool isDisease)
        {
            m.on = true;
            m.disease |= isDisease;
        }

        Mark* ResistFor(Snapshot::Resist& r, HudTarget t)
        {
            switch (t) {
            case HudTarget::Armor:         return &r.armor;
            case HudTarget::ResistFire:    return &r.fire;
            case HudTarget::ResistFrost:   return &r.frost;
            case HudTarget::ResistShock:   return &r.shock;
            case HudTarget::ResistMagic:   return &r.magic;
            case HudTarget::ResistPoison:  return &r.poison;
            case HudTarget::ResistDisease: return &r.disease;
            case HudTarget::Speed:         return &r.speed;
            default:                       return nullptr;
            }
        }

        bool IsMaxArchetype(Arch a)
        {
            return a == Arch::kValueModifier || a == Arch::kPeakValueModifier || a == Arch::kDualValueModifier;
        }
    }

    namespace {
    const Snapshot& Walk(RE::Actor* player)
    {
        static int      s_frame = -1;
        static Snapshot s_snap{};
        const int frame = ImGui::GetFrameCount();
        if (frame == s_frame) return s_snap;
        s_frame = frame;
        s_snap  = {};
        if (!player) return s_snap;

        float buffH = 0.0f, buffM = 0.0f, buffS = 0.0f;
        auto buffFor = [&](HudTarget t) -> float* {
            switch (t) {
            case HudTarget::HealthMax:  return &buffH;
            case HudTarget::MagickaMax: return &buffM;
            case HudTarget::StaminaMax: return &buffS;
            default:                    return nullptr;
            }
        };

        auto* mt   = player->AsMagicTarget();
        auto* list = mt ? mt->GetActiveEffectList() : nullptr;
        if (list) {
            for (auto* eff : *list) {
                if (!eff) continue;
                if (eff->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                    eff->flags.any(RE::ActiveEffect::Flag::kInactive))
                    continue;
                auto* mgef = eff->GetBaseObject();
                if (!mgef) continue;
                const float mag    = eff->magnitude;
                const bool  isCut  = mgef->IsDetrimental() || mag < 0.0f;
                const float amount = std::fabs(mag);
                const Arch  arch   = mgef->GetArchetype();
                const bool  isDisease = eff->spell &&
                    eff->spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease;

                if (!isCut) {
                    if (mag > 0.0f && IsMaxArchetype(arch)) {
                        const AV bavs[2] = { mgef->data.primaryAV,
                                             arch == Arch::kDualValueModifier ? mgef->data.secondaryAV : AV::kNone };
                        for (AV av : bavs)
                            if (float* b = buffFor(EffectCategories::GetHudTarget(av))) *b += amount;
                    }
                    continue;
                }

                const AV avs[2] = { mgef->data.primaryAV,
                                    arch == Arch::kDualValueModifier ? mgef->data.secondaryAV : AV::kNone };
                for (AV av : avs) {
                    const HudTarget t = EffectCategories::GetHudTarget(av);
                    if (Mark* m = MarkFor(s_snap.equip, t)) {
                        SetMark(*m, isDisease);
                        continue;
                    }
                    if (t == HudTarget::CarryWeight) {
                        SetMark(s_snap.carryWeight, isDisease);
                        continue;
                    }
                    if (Mark* m = ResistFor(s_snap.resist, t)) {
                        auto* sp = eff->spell ? eff->spell->As<RE::SpellItem>() : nullptr;
                        if (isDisease || PlayerHUDSurvival::IsNeedSpell(sp)) SetMark(*m, isDisease);
                        continue;
                    }
                    Bar* b = BarFor(s_snap, t);
                    if (!b) continue;
                    if (IsRegen(t)) {
                        if (b->regen == Regen::kNormal) b->regen = Regen::kSlowed;
                    } else if (IsMaxArchetype(arch)) {
                        (isDisease ? b->diseasePoints : b->needPoints) += amount;
                    }
                }
                if (EffectCategories::PerkLowersAttackDamage(mgef->data.perk)) {
                    SetMark(s_snap.equip.any, isDisease);
                    static RE::FormID s_loggedPerk = 0;
                    if (s_loggedPerk != mgef->data.perk->GetFormID()) {
                        s_loggedPerk = mgef->data.perk->GetFormID();
                        logger::info("[Symptoms] perk route: MGEF {:08X} '{}' perk {:08X} -> weapon damage mark",
                            mgef->GetFormID(), mgef->GetFullName(), s_loggedPerk);
                    }
                }
            }
        }

        const auto cc  = PlayerHUDPenalties::GetSurvivalPenalties();
        auto*      avo = player->AsActorValueOwner();
        auto cap = [&](float frac, RE::ActorValue av, float buff, const Bar& b) {
            if (frac <= 0.0f || !avo) return 0.0f;
            const float base   = avo->GetBaseActorValue(av);
            const float maxNow = base +
                player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, av) +
                player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, av);
            const float lost = (base + buff) - maxNow;
            return std::max(0.0f, lost - b.needPoints - b.diseasePoints);
        };
        s_snap.health.capPoints  = cap(cc.health,  RE::ActorValue::kHealth,  buffH, s_snap.health);
        s_snap.magicka.capPoints = cap(cc.magicka, RE::ActorValue::kMagicka, buffM, s_snap.magicka);
        s_snap.stamina.capPoints = cap(cc.stamina, RE::ActorValue::kStamina, buffS, s_snap.stamina);
        return s_snap;
    }
    }

    const Snapshot& Get(RE::Actor* player, float hpRegen, float mpRegen, float spRegen)
    {
        static int      s_frame = -1;
        static Snapshot s_snap{};
        const int frame = ImGui::GetFrameCount();
        if (frame == s_frame) return s_snap;
        s_frame = frame;
        s_snap  = Walk(player);

        auto stop = [](Bar& b, float r) {
            if (b.regen == Regen::kSlowed && r <= 0.001f) b.regen = Regen::kStopped;
        };
        stop(s_snap.health, hpRegen);
        stop(s_snap.magicka, mpRegen);
        stop(s_snap.stamina, spRegen);
        return s_snap;
    }

    const Equip& GetEquip(RE::Actor* player)
    {
        return Walk(player).equip;
    }

    const Mark& GetCarryWeight(RE::Actor* player)
    {
        return Walk(player).carryWeight;
    }

    const Snapshot::Resist& GetResist(RE::Actor* player)
    {
        return Walk(player).resist;
    }

    namespace {
        constexpr int kSkillFirst = static_cast<int>(RE::ActorValue::kOneHanded);
        constexpr int kSkillCount = static_cast<int>(RE::ActorValue::kEnchanting) - kSkillFirst + 1;
        using SkillCauses = std::array<Causes, kSkillCount>;

        void WalkSkills(RE::Actor* actor, SkillCauses& out)
        {
            auto* mt   = actor ? actor->AsMagicTarget() : nullptr;
            auto* list = mt ? mt->GetActiveEffectList() : nullptr;
            if (!list) return;
            for (auto* eff : *list) {
                if (!eff) continue;
                if (eff->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                    eff->flags.any(RE::ActiveEffect::Flag::kInactive))
                    continue;
                auto* mgef = eff->GetBaseObject();
                if (!mgef) continue;
                const float mag = eff->magnitude;
                if (!(mgef->IsDetrimental() || mag < 0.0f)) continue;
                const float amount = std::fabs(mag);
                if (amount < 0.01f) continue;
                const Arch arch = mgef->GetArchetype();
                const bool isDisease = eff->spell &&
                    eff->spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease;
                std::string name = eff->spell ? eff->spell->GetFullName() : "";
                if (name.empty()) name = mgef->GetFullName();
                std::string kind;
                {
                    auto* sp = eff->spell ? eff->spell->As<RE::SpellItem>() : nullptr;
                    if (isDisease)
                        kind = LocalizationManager::Get("Symptom.KindDisease");
                    else if (sp && (PlayerHUDSurvival::IsNeedSpell(sp) || PlayerHUDSurvival::IsVitalitySpell(sp)))
                        kind = LocalizationManager::Get("Symptom.KindNeed");
                    else if (sp && sp->GetSpellType() == RE::MagicSystem::SpellType::kAbility)
                        kind = LocalizationManager::Get("PartySheetV2.SpellTypeAbility");
                    else
                        kind = LocalizationManager::Get("Symptom.KindEffect");
                }

                const AV avs[2] = { mgef->data.primaryAV,
                                    arch == Arch::kDualValueModifier ? mgef->data.secondaryAV : AV::kNone };
                for (AV av : avs) {
                    const AV skill = EffectCategories::GetSkillTarget(av);
                    if (skill == AV::kNone) continue;
                    Causes& c = out[static_cast<int>(skill) - kSkillFirst];
                    const bool pct = EffectCategories::IsPercentageMagnitude(av);
                    auto it = std::find_if(c.begin(), c.end(), [&](const Cause& x) {
                        return x.name == name && x.percent == pct;
                    });
                    if (it != c.end()) it->amount += amount;
                    else c.push_back({ name, amount, pct, isDisease, kind });
                }
            }
        }
    }

    const Causes& GetSkillCauses(RE::Actor* actor, RE::ActorValue skill)
    {
        static const Causes kEmpty{};
        const int idx = static_cast<int>(skill) - kSkillFirst;
        if (!actor || idx < 0 || idx >= kSkillCount) return kEmpty;

        static int s_frame = -1;
        static std::unordered_map<RE::FormID, SkillCauses> s_cache;
        const int frame = ImGui::GetFrameCount();
        if (frame != s_frame) {
            s_frame = frame;
            s_cache.clear();
        }
        auto [it, fresh] = s_cache.try_emplace(actor->GetFormID());
        if (fresh) WalkSkills(actor, it->second);
        return it->second[idx];
    }

    void ShowCausesTooltip(const Causes& causes, const char* header)
    {
        if (causes.empty()) return;
        ImGui::BeginTooltip();
        if (header && header[0]) ImGui::TextUnformatted(header);
        ImGui::TextUnformatted(LocalizationManager::Get("Symptom.LoweredBy").c_str());
        const ImVec4 red = ImGui::ColorConvertU32ToFloat4(IM_COL32(0xE0, 0x5A, 0x4A, 0xFF));
        const ImVec4 muted = ImGui::ColorConvertU32ToFloat4(IM_COL32(136, 136, 136, 255));
        for (const auto& c : causes) {
            ImGui::TextUnformatted(c.name.c_str());
            if (!c.kind.empty()) { ImGui::SameLine(); ImGui::TextColored(muted, "%s", c.kind.c_str()); }
            ImGui::SameLine();
            ImGui::TextColored(red, c.percent ? "-%.0f%%" : "-%.0f", c.amount);
        }
        ImGui::EndTooltip();
    }

}
