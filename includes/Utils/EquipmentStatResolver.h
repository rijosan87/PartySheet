#pragma once

// STATUS: Working
// Resolves equipment stats and damage values.

#include <RE/Skyrim.h>

#include "SpellDamageResolver.h"

#include <string>
#include <vector>

namespace EquipmentStatResolver {

    using EnchantDamage = SpellDamageResolver::MagicDamageEffect;

    struct WeaponDamage {
        bool           valid      = false;
        bool           isRanged   = false;
        bool           isTwoHanded = false;
        RE::ActorValue skill      = RE::ActorValue::kNone;
        float          skillLevel = 0.0f;

        float weaponBase = 0.0f;
        float ammoBase   = 0.0f;
        float skillMult   = 1.0f;
        float fortifyMult = 1.0f;
        float perkMult    = 1.0f;
        float settingMult = 1.0f;
        float otherMult   = 1.0f;
        bool  tempered    = false;

        float physical = 0.0f;
        std::vector<EnchantDamage> enchants;

        float speed   = 0.0f;
        float reach   = 0.0f;
        float stagger = 0.0f;

        float TotalDamage() const;

        const EnchantDamage* FirstElementEnchant() const;
    };

    float FormulaDamage(RE::Actor* actor, RE::TESObjectWEAP* weapon, float perkMult, float ammoBase);

    RE::EnchantmentItem* GetWeaponEnchantment(RE::Actor* actor, RE::TESForm* equippedForm, bool isLeftHand);

    WeaponDamage ResolveWeaponDamage(RE::Actor* actor, RE::TESForm* equippedForm, bool isLeftHand);
    struct HandWeaponStats {
        bool        isWeapon      = false;
        bool        isTwoHanded   = false;
        bool        isRanged      = false;
        float       physicalDamage = -1.0f;
        float       ammoDamage     = 0.0f;
        float       hitDamage      = -1.0f;
        float       skillBonus    = 0.0f;
        float       fortifyBonus  = 0.0f;
        float       enchantDamage = 0.0f;
        float       enchantTotal  = 0.0f;
        float       weaponSpeed   = 0.0f;
        std::string enchantType;
        std::string skillIconName = "none";
    };

    HandWeaponStats ResolveHandWeaponStats(RE::PlayerCharacter* player, RE::TESForm* equippedForm, bool isLeftHand);

    float ArmorValue(RE::Actor* actor, RE::TESObjectARMO* armor, RE::InventoryEntryData* entry);
}
