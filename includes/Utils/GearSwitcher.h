#pragma once

#include <RE/Skyrim.h>

#include <chrono>
#include <string>
#include <vector>

#include "GearCatalog.h"

namespace GearSwitcher {

    void InstallCombatHold();

    void OnEquipChanged(RE::TESObjectREFR* ref);

    void ClearCombatHolds();

    bool Equip(RE::Actor* actor, const GearCatalog::InstanceKey& key, GearCatalog::Card card, int count = 1);

    bool Unequip(RE::Actor* actor, const GearCatalog::InstanceKey& key);

    bool Send(RE::Actor* from, RE::Actor* to, const GearCatalog::InstanceKey& key);

    bool IsBusy();

    struct ActionMessage {
        std::string                           text;
        std::chrono::steady_clock::time_point at;
    };

    std::vector<ActionMessage> RecentMessages();
}
