#pragma once

// STATUS: Working
// Resolves equipment stats and damage values.


#include <RE/Skyrim.h>

#include <string>

namespace EquipmentStatResolver {
    struct HandWeaponStats {
        bool        isWeapon      = false;
        bool        isTwoHanded   = false;
        bool        isRanged      = false;
        float       physicalDamage = -1.0f;
        float       ammoDamage     = 0.0f;
        float       totalDamage    = -1.0f;
        float       skillBonus    = 0.0f;
        float       fortifyBonus  = 0.0f;
        float       enchantDamage = 0.0f;
        float       weaponSpeed   = 0.0f;
        std::string enchantType;
        std::string skillIconName = "none";
    };

    HandWeaponStats ResolveHandWeaponStats(RE::PlayerCharacter* player, RE::TESForm* equippedForm, bool isLeftHand);
}
