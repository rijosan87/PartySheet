#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

namespace HUDSymptoms {

    enum class Regen : std::uint8_t { kNormal, kSlowed, kStopped };

    struct Cause {
        std::string name;
        float       amount  = 0.0f;
        bool        percent = false;
        bool        disease = false;
        std::string kind;
        RE::EffectSetting*        mgef  = nullptr;
        RE::MagicItem*            spell = nullptr;
        ID3D11ShaderResourceView* icon  = nullptr;
        RE::ActiveEffect*         effect = nullptr;
        int                       count  = 1;
    };
    using Causes = std::vector<Cause>;

    struct Bar {
        float capPoints     = 0.0f;
        float needPoints    = 0.0f;
        float diseasePoints = 0.0f;
        Regen regen         = Regen::kNormal;
        bool  regenRaised   = false;
        Causes cuts;
        Causes buffs;
        float Lost() const { return capPoints + needPoints + diseasePoints; }
    };

    struct Mark {
        bool   on      = false;
        bool   disease = false;
        Causes cuts;
        Causes buffs;
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

    const Causes& GetSkillCauses(RE::Actor* actor, RE::ActorValue skill);
    const Causes& GetSkillBuffs(RE::Actor* actor, RE::ActorValue skill);

    void ShowCausesTooltip(const Causes& causes, const char* header = nullptr);

}
