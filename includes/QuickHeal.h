#pragma once

// STATUS: Working
// Automates quick healing item use.


#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include <cstdint>

namespace QuickHeal {
    enum class CastType : std::uint8_t {
        Instant         = 0,
        Concentration   = 1,
        AoE             = 2,
        Unknown         = 3
    };

    struct HealSpellEntry {
        std::string     id;
        std::string     label;
        std::string     mod;
        RE::FormID      localFormID    = 0;
        RE::FormID      resolvedFormID = 0;
        RE::SpellItem*  spell          = nullptr;
        CastType        castType       = CastType::Unknown;
        bool            enabled        = true;
    };

    void                              Init();
    void                              ReloadFromJson();
    const std::vector<HealSpellEntry>& GetAllSpells();
    const HealSpellEntry*             FindByMod(const std::string& mod, RE::FormID localFormID);
    bool                              PlayerKnowsSpell(const HealSpellEntry& e);

    RE::Actor* PickHealTarget();

    void TryCast();
}
