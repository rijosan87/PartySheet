#pragma once

#include <RE/Skyrim.h>

#include "GearCatalog.h"

namespace FollowerCombatHold {

    void Install();

    void OnEquipChanged(RE::TESObjectREFR* ref);

    void Clear();

    inline constexpr std::uint32_t kRecordType = 'HOLD';
    inline constexpr std::uint32_t kRecordVersion = 1;

    void OnGameSave(SKSE::SerializationInterface* serde);

    void LoadRecord(SKSE::SerializationInterface* serde, std::uint32_t version, std::uint32_t length);

    void RememberGear(RE::Actor* actor, RE::TESBoundObject* obj, const RE::BGSEquipSlot* slot,
                      const GearCatalog::InstanceKey& key);

    void RememberSpell(RE::Actor* actor, RE::SpellItem* spell, bool leftHand);

    void Forget(RE::FormID actorID, const GearCatalog::InstanceKey& key);
}
