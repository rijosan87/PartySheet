#pragma once

#include <RE/Skyrim.h>

#include <string>
#include <vector>

namespace SpellDamageResolver {

    struct MagicDamageEffect {
        RE::ActorValue target = RE::ActorValue::kHealth;
        RE::ActorValue resist = RE::ActorValue::kNone;
        float          magnitude = 0.0f;
        std::string    type;
        std::string    name;

        std::uint32_t  effectIndex = 0;
        RE::ActorValue school = RE::ActorValue::kNone;
        float          baseMagnitude = 0.0f;
        float          duration = 0.0f;
        float          perkMult = 1.0f;
        float          fortifyMult = 1.0f;
        bool           conditional = false;
        bool           powerAffectsMagnitude = false;
        bool           powerAffectsDuration = false;
        RE::ActorValue secondaryTarget = RE::ActorValue::kNone;
        float          secondaryWeight = 0.0f;

        float Total(bool isConcentration) const;
    };

    struct SpellDamage {
        bool           valid = false;
        bool           isConcentration = false;
        RE::ActorValue school = RE::ActorValue::kNone;
        std::vector<MagicDamageEffect> effects;

        bool  HasDamage() const;
        float Headline() const;
        const MagicDamageEffect* Primary() const;
        const MagicDamageEffect* Drain() const;
    };

    std::string ResistTypeName(RE::ActorValue resist);

    void CollectDamageEffects(const RE::MagicItem* item, std::vector<MagicDamageEffect>& out);

    SpellDamage ResolveSpellDamage(RE::Actor* actor, RE::TESForm* equippedForm);

}
