#include "../includes/HUDSymptoms.h"
#include "../includes/ImGui/PlayerHUDPenalties.h"
#include "../includes/EffectCategories.h"
#include "../includes/Utils/EffectAttribution.h"
#include "../includes/ImGui/PlayerHUDSurvival.h"
#include "../includes/LocalizationManager.h"
#include <array>
#include <unordered_map>
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace HUDSymptoms {

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

        std::string KindFor(RE::MagicItem* spell, bool isDisease)
        {
            auto* sp = spell ? spell->As<RE::SpellItem>() : nullptr;
            if (isDisease)
                return LocalizationManager::Get("Symptom.KindDisease");
            if (sp && (PlayerHUDSurvival::IsNeedSpell(sp) || PlayerHUDSurvival::IsVitalitySpell(sp)))
                return LocalizationManager::Get("Symptom.KindNeed");
            if (sp && sp->GetSpellType() == RE::MagicSystem::SpellType::kAbility)
                return LocalizationManager::Get("PartySheetV2.SpellTypeAbility");
            return LocalizationManager::Get("Symptom.KindEffect");
        }

        Cause MakeCause(RE::ActiveEffect* eff, RE::EffectSetting* mgef, const EffectAttribution::Record* rec)
        {
            Cause c;
            c.disease = eff->spell && eff->spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease;
            if (rec && rec->kind == EffectAttribution::OwnerKind::Equipment) c.name = EffectAttribution::OwnerName(*rec);
            else c.name = eff->spell ? eff->spell->GetFullName() : "";
            if (c.name.empty()) c.name = mgef->GetFullName();
            c.kind    = KindFor(eff->spell, c.disease);
            c.mgef    = mgef;
            c.spell   = eff->spell;
            c.effect  = eff;
            c.icon    = rec ? EffectAttribution::OwnerIcon(*rec) : nullptr;
            return c;
        }

        float TimeLeft(const RE::ActiveEffect* e)
        {
            if (!e || e->duration <= 0.0f) return 1e9f;
            return e->duration - e->elapsedSeconds;
        }

        void AddCause(Causes& list, const Cause& base, float amount, bool percent)
        {
            auto it = std::find_if(list.begin(), list.end(), [&](const Cause& x) {
                return x.name == base.name && x.percent == percent;
            });
            if (it != list.end()) {
                it->amount += amount;
                if (base.effect && base.effect != it->effect && base.mgef == it->mgef) {
                    ++it->count;
                    if (TimeLeft(base.effect) < TimeLeft(it->effect)) it->effect = base.effect;
                }
                return;
            }
            Cause c   = base;
            c.amount  = amount;
            c.percent = percent;
            list.push_back(std::move(c));
        }
    }

    namespace {
    void Fill(RE::Actor* player, Snapshot& s_snap, bool ccCap)
    {
        if (!player) return;

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

                const auto* rec   = EffectAttribution::Find(player, eff);
                const bool  marks = rec ? EffectAttribution::ShowsOnHud(rec->cls) : true;
                const bool  named = marks && amount >= 0.01f;
                const bool  barNamed = amount >= 0.01f;
                const Cause base  = barNamed ? MakeCause(eff, mgef, rec) : Cause{};

                const AV avs[2] = { mgef->data.primaryAV,
                                    arch == Arch::kDualValueModifier ? mgef->data.secondaryAV : AV::kNone };

                if (!isCut) {
                    for (AV av : avs) {
                        const HudTarget t = EffectCategories::GetHudTarget(av);
                        if (mag > 0.0f && IsMaxArchetype(arch))
                            if (float* b = buffFor(t)) *b += amount;
                        if (!barNamed || mag <= 0.0f) continue;
                        const bool pct = EffectCategories::IsPercentageMagnitude(av);
                        if (Bar* b = BarFor(s_snap, t)) {
                            if (IsRegen(t)) b->regenRaised = true;
                            else if (mgef->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kRecover))
                                AddCause(b->buffs, base, amount, pct);
                            continue;
                        }
                        if (!named) continue;
                        if (Mark* em = MarkFor(s_snap.equip, t))          AddCause(em->buffs, base, amount, pct);
                        else if (t == HudTarget::CarryWeight)             AddCause(s_snap.carryWeight.buffs, base, amount, pct);
                        else if (Mark* rm = ResistFor(s_snap.resist, t))  AddCause(rm->buffs, base, amount, pct);
                    }
                    continue;
                }

                for (AV av : avs) {
                    const HudTarget t   = EffectCategories::GetHudTarget(av);
                    const bool      pct = EffectCategories::IsPercentageMagnitude(av);
                    if (Mark* m = MarkFor(s_snap.equip, t)) {
                        if (marks) SetMark(*m, isDisease);
                        if (named) AddCause(m->cuts, base, amount, pct);
                        continue;
                    }
                    if (t == HudTarget::CarryWeight) {
                        if (marks) SetMark(s_snap.carryWeight, isDisease);
                        if (named) AddCause(s_snap.carryWeight.cuts, base, amount, pct);
                        continue;
                    }
                    if (Mark* m = ResistFor(s_snap.resist, t)) {
                        auto* sp = eff->spell ? eff->spell->As<RE::SpellItem>() : nullptr;
                        if (isDisease || PlayerHUDSurvival::IsNeedSpell(sp)) {
                            SetMark(*m, isDisease);
                            if (named) AddCause(m->cuts, base, amount, pct);
                        }
                        continue;
                    }
                    Bar* b = BarFor(s_snap, t);
                    if (!b) continue;
                    if (IsRegen(t)) {
                        if (b->regen == Regen::kNormal) b->regen = Regen::kSlowed;
                    } else if (IsMaxArchetype(arch)) {
                        (isDisease ? b->diseasePoints : b->needPoints) += amount;
                        if (barNamed) AddCause(b->cuts, base, amount, pct);
                    }
                }
                if (marks && EffectCategories::PerkLowersAttackDamage(mgef->data.perk)) {
                    SetMark(s_snap.equip.any, isDisease);
                    s_snap.equip.any.cuts.push_back(MakeCause(eff, mgef, rec));
                    static RE::FormID s_loggedPerk = 0;
                    if (s_loggedPerk != mgef->data.perk->GetFormID()) {
                        s_loggedPerk = mgef->data.perk->GetFormID();
                        logger::info("[Symptoms] perk route: MGEF {:08X} '{}' perk {:08X} -> weapon damage mark",
                            mgef->GetFormID(), mgef->GetFullName(), s_loggedPerk);
                    }
                }
            }
        }

        if (!ccCap) return;
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

        for (const auto& cut : EffectAttribution::NeedPoolCuts(player)) {
            const auto& rec = *cut.record;
            if (Bar* b = BarFor(s_snap, EffectCategories::GetHudTarget(cut.change.av)))
                AddCause(b->cuts, MakeCause(rec.effect, rec.mgef, &rec), std::fabs(cut.change.value), cut.change.percent);
        }
    }

    const Snapshot& Walk(RE::Actor* player)
    {
        static int      s_frame = -1;
        static Snapshot s_snap{};
        const int frame = ImGui::GetFrameCount();
        if (frame == s_frame) return s_snap;
        s_frame = frame;
        s_snap  = {};
        Fill(player, s_snap, true);
        return s_snap;
    }
    }

    const Snapshot& GetBars(RE::Actor* actor)
    {
        static const Snapshot kEmpty{};
        if (!actor) return kEmpty;
        if (actor->IsPlayerRef()) return Walk(actor);
        static int s_frame = -1;
        static std::unordered_map<RE::FormID, Snapshot> s_cache;
        const int frame = ImGui::GetFrameCount();
        if (frame != s_frame) {
            s_frame = frame;
            s_cache.clear();
        }
        auto [it, fresh] = s_cache.try_emplace(actor->GetFormID());
        if (fresh) Fill(actor, it->second, false);
        return it->second;
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
        struct SkillEntry { Causes cuts, buffs; };
        using SkillCauses = std::array<SkillEntry, kSkillCount>;

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
                const auto* rec = EffectAttribution::Find(actor, eff);
                if (rec && !EffectAttribution::ShowsOnHud(rec->cls)) continue;
                const float mag   = eff->magnitude;
                const bool  isCut = mgef->IsDetrimental() || mag < 0.0f;
                const float amount = std::fabs(mag);
                if (amount < 0.01f) continue;
                const Arch  arch = mgef->GetArchetype();
                const Cause base = MakeCause(eff, mgef, rec);

                const AV avs[2] = { mgef->data.primaryAV,
                                    arch == Arch::kDualValueModifier ? mgef->data.secondaryAV : AV::kNone };
                for (AV av : avs) {
                    const AV skill = EffectCategories::GetSkillTarget(av);
                    if (skill == AV::kNone) continue;
                    SkillEntry& e = out[static_cast<int>(skill) - kSkillFirst];
                    AddCause(isCut ? e.cuts : e.buffs, base, amount, EffectCategories::IsPercentageMagnitude(av));
                }
            }
        }

        const SkillEntry* SkillFor(RE::Actor* actor, RE::ActorValue skill)
        {
            const int idx = static_cast<int>(skill) - kSkillFirst;
            if (!actor || idx < 0 || idx >= kSkillCount) return nullptr;

            static int s_frame = -1;
            static std::unordered_map<RE::FormID, SkillCauses> s_cache;
            const int frame = ImGui::GetFrameCount();
            if (frame != s_frame) {
                s_frame = frame;
                s_cache.clear();
            }
            auto [it, fresh] = s_cache.try_emplace(actor->GetFormID());
            if (fresh) WalkSkills(actor, it->second);
            return &it->second[idx];
        }
    }

    const Causes& GetSkillCauses(RE::Actor* actor, RE::ActorValue skill)
    {
        static const Causes kEmpty{};
        const SkillEntry* e = SkillFor(actor, skill);
        return e ? e->cuts : kEmpty;
    }

    const Causes& GetSkillBuffs(RE::Actor* actor, RE::ActorValue skill)
    {
        static const Causes kEmpty{};
        const SkillEntry* e = SkillFor(actor, skill);
        return e ? e->buffs : kEmpty;
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
