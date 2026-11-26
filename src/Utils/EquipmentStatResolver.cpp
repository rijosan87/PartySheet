// STATUS: Working
// Resolves equipment stats and damage values.

#include "../../includes/Utils/EquipmentStatResolver.h"
#include "../../includes/Utils/GearCatalog.h"
#include "../../includes/Utils/PerkModifiers.h"

#include <algorithm>
#include <cmath>

namespace EquipmentStatResolver {

    namespace {
        float ReadGameSetting(const char* name, float fallback) {
            auto* gs = RE::GameSettingCollection::GetSingleton();
            auto* s  = gs ? gs->GetSetting(name) : nullptr;
            return s ? s->GetFloat() : fallback;
        }

        struct SkillPair { RE::ActorValue skill, modifier; };

        SkillPair SkillFor(RE::WEAPON_TYPE type) {
            switch (type) {
        case RE::WEAPON_TYPE::kOneHandSword:
        case RE::WEAPON_TYPE::kOneHandDagger:
        case RE::WEAPON_TYPE::kOneHandAxe:
        case RE::WEAPON_TYPE::kOneHandMace:
                return { RE::ActorValue::kOneHanded, RE::ActorValue::kOneHandedModifier };
        case RE::WEAPON_TYPE::kTwoHandSword:
        case RE::WEAPON_TYPE::kTwoHandAxe:
                return { RE::ActorValue::kTwoHanded, RE::ActorValue::kTwoHandedModifier };
        case RE::WEAPON_TYPE::kBow:
        case RE::WEAPON_TYPE::kCrossbow:
                return { RE::ActorValue::kArchery, RE::ActorValue::kMarksmanModifier };
        default:
                return { RE::ActorValue::kNone, RE::ActorValue::kNone };
        }
        }
    }

    namespace {
        struct FormulaParts {
            RE::ActorValue skill       = RE::ActorValue::kNone;
            float skillLevel = 0.0f;
            float          fortifyMult = 1.0f;
            float          skillMult   = 1.0f;
            float          settingMult = 1.0f;
        };

        FormulaParts ReadFormulaParts(RE::Actor* actor, RE::TESObjectWEAP* weapon) {
            FormulaParts parts;
            auto* avo = actor ? actor->AsActorValueOwner() : nullptr;
            if (!avo || !weapon) return parts;

            const auto pair = SkillFor(weapon->GetWeaponType());
            parts.skill = pair.skill;
            if (pair.skill != RE::ActorValue::kNone) {
                parts.skillLevel  = avo->GetActorValue(pair.skill);
                parts.fortifyMult = 1.0f + avo->GetActorValue(pair.modifier) / 100.0f;
            }

            const bool isPlayer = actor->IsPlayerRef();
            static const float pcMin  = ReadGameSetting("fDamagePCSkillMin", 1.0f);
            static const float pcMax  = ReadGameSetting("fDamagePCSkillMax", 1.5f);
            static const float npcMin = ReadGameSetting("fDamageSkillMin", 1.0f);
            static const float npcMax = ReadGameSetting("fDamageSkillMax", 3.0f);
            static const float weaponMult = ReadGameSetting("fDamageWeaponMult", 1.0f);
            const float skill01 = std::clamp(parts.skillLevel, 0.0f, 100.0f) / 100.0f;
            parts.skillMult   = isPlayer ? pcMin + (pcMax - pcMin) * skill01 : npcMin + (npcMax - npcMin) * skill01;
            parts.settingMult = isPlayer ? 1.0f : weaponMult;
            return parts;
            }

        float ApplyFormula(const FormulaParts& parts, float weaponBase, float ammoBase, float perkMult) {
            return (weaponBase + ammoBase) * parts.skillMult * parts.fortifyMult * perkMult * parts.settingMult;
        }
                        }

    float FormulaDamage(RE::Actor* actor, RE::TESObjectWEAP* weapon, float perkMult, float ammoBase) {
        if (!actor || !weapon) return 0.0f;
        return ApplyFormula(ReadFormulaParts(actor, weapon), static_cast<float>(weapon->GetAttackDamage()), ammoBase, perkMult);
                        }

    float WeaponDamage::TotalDamage() const {
        float total = physical;
        for (const auto& e : enchants)
            if (e.target == RE::ActorValue::kHealth) total += e.magnitude;
        return total;
                        }

    const EnchantDamage* WeaponDamage::FirstElementEnchant() const {
        for (const auto& e : enchants) {
            if (e.target != RE::ActorValue::kHealth) continue;
            if (e.resist == RE::ActorValue::kResistFire || e.resist == RE::ActorValue::kResistFrost ||
                e.resist == RE::ActorValue::kResistShock || e.resist == RE::ActorValue::kPoisonResist)
                return &e;
                        }
        return nullptr;
                        }

    RE::EnchantmentItem* GetWeaponEnchantment(RE::Actor* actor, RE::TESForm* equippedForm, bool isLeftHand) {
        auto* weapon = equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr;
        if (!weapon) return nullptr;
        if (auto* entry = actor ? actor->GetEquippedEntryData(isLeftHand) : nullptr; entry && entry->object == weapon)
            if (auto* ench = entry->GetEnchantment()) return ench;
        return weapon->formEnchanting;
    }

    WeaponDamage ResolveWeaponDamage(RE::Actor* actor, RE::TESForm* equippedForm, bool isLeftHand) {
        WeaponDamage wd;
        auto* weapon = equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr;
        auto* avo    = actor ? actor->AsActorValueOwner() : nullptr;
        if (!weapon || !avo) return wd;

        wd.valid       = true;
        wd.isRanged    = weapon->IsBow() || weapon->IsCrossbow();
        wd.isTwoHanded = wd.isRanged || weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe();
        wd.speed       = weapon->GetSpeed();
        wd.reach       = weapon->GetReach();
        wd.stagger     = weapon->GetStagger();
        wd.weaponBase  = static_cast<float>(weapon->GetAttackDamage());
        if (wd.isRanged)
            if (auto* ammo = actor->GetCurrentAmmo())
                wd.ammoBase = ammo->GetRuntimeData().data.damage;

        const auto parts = ReadFormulaParts(actor, weapon);
        wd.skill       = parts.skill;
        wd.skillLevel  = parts.skillLevel;
        wd.fortifyMult = parts.fortifyMult;
        wd.skillMult   = parts.skillMult;
        wd.settingMult = parts.settingMult;

        const bool isPlayer = actor->IsPlayerRef();
        const auto est = PerkModifiers::GetHitEstimate(actor, weapon, isLeftHand);
        wd.perkMult = est.perkMult;

        if (isPlayer) {
            auto* player = actor->As<RE::PlayerCharacter>();
            const float known = wd.weaponBase * wd.skillMult * wd.fortifyMult * wd.perkMult;

            bool foundWorn = false;
            if (auto gear = GearCatalog::Get(actor)) {
                const auto card = isLeftHand ? GearCatalog::Card::LeftHand : GearCatalog::Card::RightHand;
                for (const auto& item : gear->cards[static_cast<std::size_t>(card)].items) {
                    if (!item.equipped || item.key.baseID != weapon->GetFormID() || item.statValue < 0.0f) continue;
                    wd.physical  = item.statValue;
                    wd.tempered  = item.key.health > 1.0f;
                    wd.otherMult = known > 0.0f ? (item.statValue - wd.ammoBase) / known : 1.0f;
                    foundWorn = true;
                            break;
                        }
                    }
            if (!foundWorn) wd.physical = known + wd.ammoBase;
        } else {
            wd.physical  = ApplyFormula(parts, wd.weaponBase, wd.ammoBase, wd.perkMult);
            wd.otherMult = 1.0f;
        }

        SpellDamageResolver::CollectDamageEffects(GetWeaponEnchantment(actor, weapon, isLeftHand), wd.enchants);
        return wd;
    }

    HandWeaponStats ResolveHandWeaponStats(RE::PlayerCharacter* player, RE::TESForm* equippedForm, bool isLeftHand) {
        HandWeaponStats stats;
        const auto wd = ResolveWeaponDamage(player, equippedForm, isLeftHand);
        if (!wd.valid) return stats;

        stats.isWeapon     = true;
        stats.isTwoHanded  = wd.isTwoHanded;
        stats.isRanged     = wd.isRanged;
        stats.weaponSpeed  = wd.speed;
        stats.fortifyBonus = (wd.fortifyMult - 1.0f) * 100.0f;
        stats.skillBonus   = wd.weaponBase * (wd.skillMult - 1.0f);
        stats.skillIconName = wd.skill == RE::ActorValue::kOneHanded ? "onehanded"
                            : wd.skill == RE::ActorValue::kTwoHanded ? "twohanded"
                            : wd.skill == RE::ActorValue::kArchery   ? "archery" : "none";
        stats.ammoDamage     = wd.ammoBase;
        stats.physicalDamage = std::round(wd.physical - wd.ammoBase);
        if (const auto* e = wd.FirstElementEnchant()) {
            stats.enchantDamage = e->magnitude;
            stats.enchantType   = e->type;
        }
        stats.enchantTotal = wd.TotalDamage() - wd.physical;
        stats.hitDamage = std::round(stats.physicalDamage + stats.ammoDamage);
        return stats;
    }

    float ArmorValue(RE::Actor* actor, RE::TESObjectARMO* armor, RE::InventoryEntryData* entry) {
        if (!armor) return 0.0f;
        if (!actor || !entry) return static_cast<float>(armor->GetArmorRating());
        if (armor->IsClothing()) return 0.0f;

        if (actor->IsPlayerRef())
            if (auto* player = RE::PlayerCharacter::GetSingleton()) return player->GetArmorValue(entry);

        auto* avo = actor->AsActorValueOwner();
        const auto  skill = armor->IsHeavyArmor() ? RE::ActorValue::kHeavyArmor : RE::ActorValue::kLightArmor;
        const float skillMult = avo->GetArmorRatingSkillMultiplier(avo->GetActorValue(skill));
        const float rated = RE::GetArmorFinalRating(entry, avo->GetActorValue(RE::ActorValue::kArmorPerks), skillMult);
        return PerkModifiers::ArmorRatingPerk(actor, armor, rated);
                }
            }
