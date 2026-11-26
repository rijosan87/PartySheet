#pragma once

#include <RE/Skyrim.h>

#include <string>
#include <vector>

namespace SpellCatalog {

    enum class Group { School, Powers, Shouts };

    struct SpellEntry {
        RE::TESForm*   form   = nullptr;
        std::string    name;
        Group          group  = Group::School;
        RE::ActorValue school = RE::ActorValue::kNone;
        float          cost   = 0.0f;
        std::uint32_t  tags   = 0;
    };

    struct SpellGroup {
        Group                   group  = Group::School;
        RE::ActorValue          school = RE::ActorValue::kNone;
        std::vector<SpellEntry> entries;
    };

    std::vector<SpellEntry> KnownSpells(RE::Actor* actor);
    std::vector<SpellGroup> Grouped(RE::Actor* actor);

    struct SpellDamage {
        float       value = 0.0f;
        bool        perSecond = false;
        std::string type;
    };
    SpellDamage DamageOf(RE::Actor* actor, const SpellEntry& entry);

    enum class HandLock { None, Right, Left };
    HandLock HandLockOf(const RE::TESForm* form);

}
