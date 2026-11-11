#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <string>
#include <vector>

namespace SurvivalSymptoms {

    enum class Regen : std::uint8_t { kNormal, kSlowed, kStopped };

    struct Bar {
        float capPoints     = 0.0f;
        float needPoints    = 0.0f;
        float diseasePoints = 0.0f;
        Regen regen         = Regen::kNormal;
        float Lost() const { return capPoints + needPoints + diseasePoints; }
    };

    struct Mark {
        bool on      = false;
        bool disease = false;
    };
    struct Equip {
        Mark oneHanded, twoHanded, ranged, melee, any, shield, shout;
    };

    struct Snapshot {
        Bar   health, magicka, stamina;
        Equip equip;
        Mark  carryWeight;
        struct Resist { Mark armor, fire, frost, shock, magic, poison, disease, speed; } resist;
    };

    const Snapshot& Get(RE::Actor* player, float hpRegen, float mpRegen, float spRegen);

    const Snapshot& GetBars(RE::Actor* actor);

    const Equip& GetEquip(RE::Actor* player);

    const Mark& GetCarryWeight(RE::Actor* player);

    const Snapshot::Resist& GetResist(RE::Actor* player);

    struct Cause {
        std::string name;
        float       amount  = 0.0f;
        bool        percent = false;
        bool        disease = false;
        std::string kind;
    };
    using Causes = std::vector<Cause>;

    const Causes& GetSkillCauses(RE::Actor* actor, RE::ActorValue skill);

    void ShowCausesTooltip(const Causes& causes, const char* header = nullptr);

}
