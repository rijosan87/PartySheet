#pragma once

#include <RE/Skyrim.h>

#include <string>
#include <vector>

namespace PerkModifiers {

    struct HitEstimate {
        float perkMult = 1.0f;
        float physicalDamage = -1.0f;
    };

    float AttackPerkMult(RE::Actor* actor, RE::TESObjectWEAP* weapon);

    float ArmorRatingPerk(RE::Actor* actor, RE::TESObjectARMO* armor, float value);

    HitEstimate GetHitEstimate(RE::Actor* actor, RE::TESObjectWEAP* weapon, bool leftHand = false);

    struct SpellEstimate {
        bool               known = false;
        std::vector<float> magnitudes;
        std::vector<float> durations;
    };

    SpellEstimate GetSpellEstimate(RE::Actor* actor, RE::SpellItem* spell);

    struct SkillPerk {
        std::string name;
        std::string description;
        int         rank    = 1;
        int         maxRank = 1;
    };
    struct SkillPerks {
        bool                   known = false;
        std::vector<SkillPerk> perks;
    };

    SkillPerks GetSkillPerks(RE::Actor* actor, RE::ActorValue skill);

    SkillPerks GetOtherPerks(RE::Actor* actor);
}
