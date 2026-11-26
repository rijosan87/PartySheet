#pragma once

#include <RE/Skyrim.h>

#include <chrono>
#include <string>
#include <vector>

#include "GearCatalog.h"

namespace GearSwitcher {

    bool Equip(RE::Actor* actor, const GearCatalog::InstanceKey& key, GearCatalog::Card card, int count = 1);

    bool Unequip(RE::Actor* actor, const GearCatalog::InstanceKey& key);

    bool Send(RE::Actor* from, RE::Actor* to, const GearCatalog::InstanceKey& key, int count = 1);

    bool EquipSpell(RE::Actor* actor, RE::FormID formID, GearCatalog::Card card);

    bool IsBusy();

    bool SentRecently(RE::FormID actorID);

    struct ActionMessage {
        std::string                           text;
        std::chrono::steady_clock::time_point at;
        RE::FormID                            receiver = 0;
    };

    std::vector<ActionMessage> RecentMessages();
}
