#include "../../includes/Utils/SpellCatalog.h"
#include "../../includes/Utils/SpellDamageResolver.h"

#include <algorithm>

#undef GetObject

namespace SpellCatalog {

    namespace {
        class SpellCollector : public RE::Actor::ForEachSpellVisitor {
        public:
            std::vector<RE::SpellItem*> spells;

            RE::BSContainer::ForEachResult Visit(RE::SpellItem* spell) override {
                if (spell) spells.push_back(spell);
                return RE::BSContainer::ForEachResult::kContinue;
            }
        };

        bool IsVanillaSchool(RE::ActorValue av) {
            using AV = RE::ActorValue;
            return av == AV::kAlteration || av == AV::kConjuration || av == AV::kDestruction ||
                   av == AV::kIllusion || av == AV::kRestoration;
        }

        bool AlreadyListed(const std::vector<SpellEntry>& list, const RE::TESForm* form) {
            return std::any_of(list.begin(), list.end(),
                [&](const SpellEntry& e) { return e.form == form; });
        }

        void AddSpell(std::vector<SpellEntry>& out, RE::SpellItem* spell, RE::Actor* actor) {
            using ST = RE::MagicSystem::SpellType;
            const ST type = spell->GetSpellType();
            const bool isPower = type == ST::kPower || type == ST::kLesserPower;
            if (type != ST::kSpell && !isPower) return;

            const char* name = spell->GetFullName();
            if (!name || !name[0]) return;
            if (AlreadyListed(out, spell)) return;

            SpellEntry e;
            e.form   = spell;
            e.name   = name;
            e.group  = isPower ? Group::Powers : Group::School;
            e.school = isPower ? RE::ActorValue::kNone : spell->GetAssociatedSkill();
            e.cost   = isPower ? 0.0f : spell->CalculateMagickaCost(actor);
            out.push_back(std::move(e));
        }

        void AddShout(std::vector<SpellEntry>& out, RE::TESShout* shout) {
            const char* name = shout->GetFullName();
            if (!name || !name[0]) return;
            if (AlreadyListed(out, shout)) return;

            SpellEntry e;
            e.form  = shout;
            e.name  = name;
            e.group = Group::Shouts;
            out.push_back(std::move(e));
        }
    }

    std::vector<SpellEntry> KnownSpells(RE::Actor* actor) {
        std::vector<SpellEntry> out;
        if (!actor) return out;

        SpellCollector collector;
        actor->VisitSpells(collector);
        for (auto* spell : collector.spells)
            AddSpell(out, spell, actor);

        if (auto* race = actor->GetRace(); race && race->actorEffects) {
            auto* list = race->actorEffects;
            for (std::uint32_t i = 0; list->spells && i < list->numSpells; ++i)
                if (list->spells[i]) AddSpell(out, list->spells[i], actor);
        }

        if (auto* base = actor->GetActorBase(); base && base->actorEffects) {
            auto* list = base->actorEffects;
            for (std::uint32_t i = 0; list->shouts && i < list->numShouts; ++i)
                if (list->shouts[i]) AddShout(out, list->shouts[i]);
        }
        return out;
    }

    SpellDamage DamageOf(RE::Actor* actor, const SpellEntry& entry) {
        SpellDamage out;
        if (!actor || entry.group != Group::School) return out;

        const auto sd = SpellDamageResolver::ResolveSpellDamage(actor, entry.form);
        const auto* p = sd.HasDamage() ? sd.Primary() : sd.Drain();
        if (!p) return out;
        out.value     = sd.HasDamage() ? sd.Headline() : p->Total(sd.isConcentration);
        out.perSecond = sd.isConcentration;
        out.type      = p->type;
        return out;
    }

    std::vector<SpellGroup> Grouped(RE::Actor* actor) {
        std::vector<SpellGroup> schools;
        SpellGroup powers{ Group::Powers };
        SpellGroup shouts{ Group::Shouts };

        for (auto& e : KnownSpells(actor)) {
            if (e.group == Group::Powers) { powers.entries.push_back(std::move(e)); continue; }
            if (e.group == Group::Shouts) { shouts.entries.push_back(std::move(e)); continue; }

            const RE::ActorValue school = IsVanillaSchool(e.school) ? e.school : RE::ActorValue::kNone;
            auto it = std::find_if(schools.begin(), schools.end(),
                [&](const SpellGroup& g) { return g.school == school; });
            if (it == schools.end()) {
                schools.push_back({ Group::School, school, {} });
                it = std::prev(schools.end());
            }
            it->entries.push_back(std::move(e));
        }

        std::stable_sort(schools.begin(), schools.end(),
            [](const SpellGroup& a, const SpellGroup& b) { return a.entries.size() > b.entries.size(); });

        if (!powers.entries.empty()) schools.push_back(std::move(powers));
        if (!shouts.entries.empty()) schools.push_back(std::move(shouts));
        return schools;
    }

    HandLock HandLockOf(const RE::TESForm* form) {
        auto* spell = form ? form->As<RE::SpellItem>() : nullptr;
        auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
        if (!spell || !dom) return HandLock::None;
        const auto* slot = spell->GetEquipSlot();
        if (!slot) return HandLock::None;
        if (slot == dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kRightHandEquip)) return HandLock::Right;
        if (slot == dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip)) return HandLock::Left;
        return HandLock::None;
    }

}
