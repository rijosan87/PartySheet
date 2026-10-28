// STATUS: Working
// Resolves equipment stats and damage values.

#include "../../includes/Utils/EquipmentStatResolver.h"

#include <cstdint>
#include <cmath>

namespace EquipmentStatResolver {
    HandWeaponStats ResolveHandWeaponStats(RE::PlayerCharacter* player, RE::TESForm* equippedForm, bool isLeftHand)
    {
        HandWeaponStats stats;
        if (!player || !equippedForm) {
            return stats;
        }
        auto* weapon = equippedForm->As<RE::TESObjectWEAP>();
        if (!weapon) {
            return stats;
        }
        stats.isWeapon    = true;
        stats.weaponSpeed = weapon->GetSpeed();
        const auto weaponType = weapon->GetWeaponType();
        switch (weaponType) {
        case RE::WEAPON_TYPE::kOneHandSword:
        case RE::WEAPON_TYPE::kOneHandDagger:
        case RE::WEAPON_TYPE::kOneHandAxe:
        case RE::WEAPON_TYPE::kOneHandMace:
            stats.skillIconName = "onehanded";
            break;
        case RE::WEAPON_TYPE::kTwoHandSword:
        case RE::WEAPON_TYPE::kTwoHandAxe:
            stats.skillIconName = "twohanded";
            stats.isTwoHanded   = true;
            break;
        case RE::WEAPON_TYPE::kBow:
        case RE::WEAPON_TYPE::kCrossbow:
            stats.skillIconName = "archery";
            stats.isTwoHanded   = true;
            stats.isRanged      = true;
            break;
        default:
            break;
        }
        const float baseDamage = static_cast<float>(weapon->GetAttackDamage());
        float       totalDamage = baseDamage;
        if (auto* avo = player->AsActorValueOwner()) {
            float skillLevel = 0.0f;
            switch (weaponType) {
            case RE::WEAPON_TYPE::kOneHandSword:
            case RE::WEAPON_TYPE::kOneHandDagger:
            case RE::WEAPON_TYPE::kOneHandAxe:
            case RE::WEAPON_TYPE::kOneHandMace:
                skillLevel = avo->GetActorValue(RE::ActorValue::kOneHanded);
                stats.fortifyBonus = avo->GetActorValue(RE::ActorValue::kOneHandedModifier);
                break;
            case RE::WEAPON_TYPE::kTwoHandSword:
            case RE::WEAPON_TYPE::kTwoHandAxe:
                skillLevel = avo->GetActorValue(RE::ActorValue::kTwoHanded);
                stats.fortifyBonus = avo->GetActorValue(RE::ActorValue::kTwoHandedModifier);
                break;
            case RE::WEAPON_TYPE::kBow:
            case RE::WEAPON_TYPE::kCrossbow:
                skillLevel = avo->GetActorValue(RE::ActorValue::kArchery);
                stats.fortifyBonus = avo->GetActorValue(RE::ActorValue::kMarksmanModifier);
                break;
            default:
                break;
            }
            if (auto* entryData = player->GetEquippedEntryData(isLeftHand)) {
                totalDamage = player->GetDamage(entryData);
            } else {
                const float skillMult   = 1.0f + (skillLevel / 200.0f);
                const float fortifyMult = 1.0f + (stats.fortifyBonus / 100.0f);
                totalDamage = baseDamage * skillMult * fortifyMult;
            }
            stats.skillBonus = baseDamage * (skillLevel / 200.0f);
        }
        stats.physicalDamage = std::round(totalDamage);
        try {
            if (auto* enchantable = weapon->As<RE::TESEnchantableForm>()) {
                if (auto* enchant = enchantable->formEnchanting) {
                    for (std::uint32_t i = 0; i < enchant->effects.size(); ++i) {
                        auto* effect = enchant->effects[i];
                        if (!effect || !effect->baseEffect) {
                            continue;
                        }
                        const auto resistType = effect->baseEffect->data.resistVariable;
                        const auto primaryAV  = effect->baseEffect->data.primaryAV;
                        const float magnitude = effect->effectItem.magnitude;
                        if (magnitude <= 0.0f || primaryAV != RE::ActorValue::kHealth) {
                            continue;
                        }
                        if (resistType == RE::ActorValue::kResistFire) {
                            stats.enchantDamage = magnitude;
                            stats.enchantType   = "fire_resist";
                            break;
                        }
                        if (resistType == RE::ActorValue::kResistFrost) {
                            stats.enchantDamage = magnitude;
                            stats.enchantType   = "frost_resist";
                            break;
                        }
                        if (resistType == RE::ActorValue::kResistShock) {
                            stats.enchantDamage = magnitude;
                            stats.enchantType   = "shock_resist";
                            break;
                        }
                        if (resistType == RE::ActorValue::kPoisonResist) {
                            stats.enchantDamage = magnitude;
                            stats.enchantType   = "poison_resist";
                            break;
                        }
                    }
                }
            }
        } catch (...) {
        }
        if (stats.isRanged) {
            if (auto* ammo = player->GetCurrentAmmo()) {
                stats.ammoDamage = ammo->GetRuntimeData().data.damage;
            }
        }
        float displayTotal = stats.physicalDamage;
        if (displayTotal < 0.0f && stats.enchantDamage > 0.0f) {
            displayTotal = 0.0f;
        }
        if (displayTotal >= 0.0f) {
            displayTotal += stats.ammoDamage;
            displayTotal += stats.enchantDamage;
            stats.totalDamage = std::round(displayTotal);
        }
        return stats;
    }
}
