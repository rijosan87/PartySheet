#include "../includes/ActorStats.h"
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESBoundObject.h>
static void PopulateRichStat(RichStat& stat, RE::Actor* actor, RE::ActorValueOwner* avo, RE::ActorValue av) {
    if (!actor || !avo) return;
    
    stat.base = avo->GetBaseActorValue(av);
    stat.perm = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, av);
    stat.temp = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, av);
    
    stat.current = std::max(0.0f, avo->GetActorValue(av));

    if (av == RE::ActorValue::kHealth &&
        (actor->IsDead() || (actor->AsActorState()->IsBleedingOut() && actor->IsEssential())))
        stat.current = 0.0f;
    
}

ActorStats GetActorStats(RE::Actor* actor) {
    ActorStats stats;
    
    if (!actor) return stats;
    
    auto avo = actor->AsActorValueOwner();
    if (!avo) return stats;
    
    PopulateRichStat(stats.health, actor, avo, RE::ActorValue::kHealth);
    PopulateRichStat(stats.magicka, actor, avo, RE::ActorValue::kMagicka);
    PopulateRichStat(stats.stamina, actor, avo, RE::ActorValue::kStamina);
    
    float healthBaseRate = avo->GetActorValue(RE::ActorValue::kHealRate);
    float magickaBaseRate = avo->GetActorValue(RE::ActorValue::kMagickaRate);
    float staminaBaseRate = avo->GetActorValue(RE::ActorValue::kStaminaRate);
    
    float healthMult = avo->GetActorValue(RE::ActorValue::kHealRateMult);
    float magickaMult = avo->GetActorValue(RE::ActorValue::kMagickaRateMult);
    float staminaMult = avo->GetActorValue(RE::ActorValue::kStaminaRateMult);
    
    stats.healthRegen = healthBaseRate * (healthMult / 100.0f);
    stats.magickaRegen = magickaBaseRate * (magickaMult / 100.0f);
    stats.staminaRegen = staminaBaseRate * (staminaMult / 100.0f);
    stats.shoutRecovery = avo->GetActorValue(RE::ActorValue::kShoutRecoveryMult);
    
    stats.armorRating = avo->GetActorValue(RE::ActorValue::kDamageResist);
    stats.unarmedDamage = avo->GetActorValue(RE::ActorValue::kUnarmedDamage);
    stats.speedMult = avo->GetActorValue(RE::ActorValue::kSpeedMult);
    
    RE::TESObjectWEAP* weapon = nullptr;
    auto rightHand = actor->GetEquippedObject(false);
    auto leftHand = actor->GetEquippedObject(true);
    
    if (rightHand) {
        if (auto w = rightHand->As<RE::TESObjectWEAP>()) {
            if (w->GetAttackDamage() > 0) {
                weapon = w;
            }
        }
    }
    if (!weapon && leftHand) {
        weapon = leftHand->As<RE::TESObjectWEAP>();
    }

    if (weapon) {
        float baseDamage = static_cast<float>(weapon->GetAttackDamage());
            stats.baseWeaponDamage = baseDamage;
            
            float baseSpeed = weapon->GetSpeed();
            float speedMult = avo->GetActorValue(RE::ActorValue::kWeaponSpeedMult);
            if (speedMult <= 0.0f) speedMult = 1.0f;
            stats.weaponSpeed = baseSpeed * speedMult;
            
            stats.weaponReach = weapon->GetReach();
            
            float skillLevel = 0.0f;
            float fortifyMod = 0.0f;
            float arrowDamage = 0.0f;
            
            auto weaponType = weapon->GetWeaponType();
            switch (weaponType) {
                case RE::WEAPON_TYPE::kOneHandSword:
                case RE::WEAPON_TYPE::kOneHandDagger:
                case RE::WEAPON_TYPE::kOneHandAxe:
                case RE::WEAPON_TYPE::kOneHandMace:
                    skillLevel = avo->GetActorValue(RE::ActorValue::kOneHanded);
                    fortifyMod = avo->GetActorValue(RE::ActorValue::kOneHandedModifier);
                    break;
                case RE::WEAPON_TYPE::kTwoHandSword:
                case RE::WEAPON_TYPE::kTwoHandAxe:
                    skillLevel = avo->GetActorValue(RE::ActorValue::kTwoHanded);
                    fortifyMod = avo->GetActorValue(RE::ActorValue::kTwoHandedModifier);
                    break;
                case RE::WEAPON_TYPE::kBow:
                case RE::WEAPON_TYPE::kCrossbow:
                    skillLevel = avo->GetActorValue(RE::ActorValue::kArchery);
                    fortifyMod = avo->GetActorValue(RE::ActorValue::kMarksmanModifier);
                    
                    if (auto ammo = actor->GetCurrentAmmo()) {
                        arrowDamage = ammo->GetRuntimeData().data.damage;
                        stats.arrowDamage = arrowDamage;
                        stats.arrowName = ammo->GetName() ? ammo->GetName() : "Arrows";
                        
                        if (actor->IsPlayerRef()) {
                            if (auto player = actor->As<RE::PlayerCharacter>()) {
                                stats.arrowCount = player->GetItemCount(ammo);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
            
            float combinedBase = baseDamage + arrowDamage;
            stats.baseWeaponDamage = combinedBase;
            
            if (actor->IsPlayerRef()) {
                if (auto player = actor->As<RE::PlayerCharacter>()) {
                    auto* entryData = player->GetEquippedEntryData(false);
                    if (entryData) {
                        stats.weaponDamage = player->GetDamage(entryData);
                    } else {
                        float skillMultiplier = 1.0f + (skillLevel / 200.0f);
                        float fortifyMultiplier = 1.0f + (fortifyMod / 100.0f);
                        stats.weaponDamage = combinedBase * skillMultiplier * fortifyMultiplier;
                    }
                }
            } else {
                float skillMultiplier = 1.0f + (skillLevel / 200.0f);
                float fortifyMultiplier = 1.0f + (fortifyMod / 100.0f);
                stats.weaponDamage = combinedBase * skillMultiplier * fortifyMultiplier;

                if (auto* enchantable = weapon->As<RE::TESEnchantableForm>()) {
                    if (auto* enchant = enchantable->formEnchanting) {
                        for (auto* effect : enchant->effects) {
                            if (!effect || !effect->baseEffect) continue;
                            const float mag = effect->effectItem.magnitude;
                            if (mag <= 0.0f) continue;
                            const auto primaryAV  = effect->baseEffect->data.primaryAV;
                            const auto resistType = effect->baseEffect->data.resistVariable;
                            if (primaryAV != RE::ActorValue::kHealth) continue;
                            if (resistType == RE::ActorValue::kResistFire   ||
                                resistType == RE::ActorValue::kResistFrost  ||
                                resistType == RE::ActorValue::kResistShock  ||
                                resistType == RE::ActorValue::kPoisonResist) {
                                stats.weaponDamage += mag;
                                break;
                            }
                        }
                    }
                }
            }
    }
    
    stats.armorRating = avo->GetActorValue(RE::ActorValue::kDamageResist);
    
    // Physical Resistance %: standard damage reduction from armor rating (0.12% per point, capped at 80%).
    stats.damageResistPct = std::clamp(stats.armorRating * 0.12f, 0.0f, 80.0f);

    stats.resistFire = avo->GetActorValue(RE::ActorValue::kResistFire);
    stats.resistFrost = avo->GetActorValue(RE::ActorValue::kResistFrost);
    stats.resistShock = avo->GetActorValue(RE::ActorValue::kResistShock);
    stats.resistMagic = avo->GetActorValue(RE::ActorValue::kResistMagic);
    stats.resistPoison = avo->GetActorValue(RE::ActorValue::kPoisonResist);
    stats.resistDisease = avo->GetActorValue(RE::ActorValue::kResistDisease);
    
    stats.carryWeightMax = avo->GetActorValue(RE::ActorValue::kCarryWeight);
    
    stats.level = actor->GetLevel();
    
    stats.skills.oneHanded = { avo->GetActorValue(RE::ActorValue::kOneHanded), RE::ActorValue::kOneHanded };
    stats.skills.twoHanded = { avo->GetActorValue(RE::ActorValue::kTwoHanded), RE::ActorValue::kTwoHanded };
    stats.skills.archery = { avo->GetActorValue(RE::ActorValue::kArchery), RE::ActorValue::kArchery };
    stats.skills.block = { avo->GetActorValue(RE::ActorValue::kBlock), RE::ActorValue::kBlock };
    stats.skills.heavyArmor = { avo->GetActorValue(RE::ActorValue::kHeavyArmor), RE::ActorValue::kHeavyArmor };
    stats.skills.smithing = { avo->GetActorValue(RE::ActorValue::kSmithing), RE::ActorValue::kSmithing };
    
    stats.skills.destruction = { avo->GetActorValue(RE::ActorValue::kDestruction), RE::ActorValue::kDestruction };
    stats.skills.restoration = { avo->GetActorValue(RE::ActorValue::kRestoration), RE::ActorValue::kRestoration };
    stats.skills.conjuration = { avo->GetActorValue(RE::ActorValue::kConjuration), RE::ActorValue::kConjuration };
    stats.skills.alteration = { avo->GetActorValue(RE::ActorValue::kAlteration), RE::ActorValue::kAlteration };
    stats.skills.illusion = { avo->GetActorValue(RE::ActorValue::kIllusion), RE::ActorValue::kIllusion };
    stats.skills.enchanting = { avo->GetActorValue(RE::ActorValue::kEnchanting), RE::ActorValue::kEnchanting };
    
    stats.skills.sneak = { avo->GetActorValue(RE::ActorValue::kSneak), RE::ActorValue::kSneak };
    stats.skills.lockpicking = { avo->GetActorValue(RE::ActorValue::kLockpicking), RE::ActorValue::kLockpicking };
    stats.skills.pickpocket = { avo->GetActorValue(RE::ActorValue::kPickpocket), RE::ActorValue::kPickpocket };
    stats.skills.lightArmor = { avo->GetActorValue(RE::ActorValue::kLightArmor), RE::ActorValue::kLightArmor };
    stats.skills.alchemy = { avo->GetActorValue(RE::ActorValue::kAlchemy), RE::ActorValue::kAlchemy };
    stats.skills.speech = { avo->GetActorValue(RE::ActorValue::kSpeech), RE::ActorValue::kSpeech };
    
    if (actor->IsPlayerRef()) {
        auto player = actor->As<RE::PlayerCharacter>();
        if (player) {
             stats.carryWeight = player->GetWeightInContainer();
             
             auto goldForm = RE::TESForm::LookupByID(0x0000000F);
             if (goldForm) {
                  if (auto goldObj = goldForm->As<RE::TESBoundObject>()) {
                      stats.gold = player->GetItemCount(goldObj);
                  }
             }
             
             stats.dragonSouls = static_cast<int>(avo->GetActorValue(static_cast<RE::ActorValue>(133)));
             
             stats.perkPoints = static_cast<int>(player->GetGameStatsData().perkCount);
        }
    }
    
    return stats;
}

ActorResistances GetActorResistances(RE::Actor* actor) {
    ActorResistances resists;
    
    if (!actor) return resists;
    
    auto avo = actor->AsActorValueOwner();
    if (!avo) return resists;
    
    resists.fire = avo->GetActorValue(RE::ActorValue::kResistFire);
    resists.frost = avo->GetActorValue(RE::ActorValue::kResistFrost);
    resists.shock = avo->GetActorValue(RE::ActorValue::kResistShock);
    resists.magic = avo->GetActorValue(RE::ActorValue::kResistMagic);
    resists.poison = avo->GetActorValue(RE::ActorValue::kPoisonResist);
    resists.disease = avo->GetActorValue(RE::ActorValue::kResistDisease);
    
    return resists;
}

// ============================================================================
// GetActorSkills: Lightweight skill-only fetch
// ============================================================================
ActorSkills GetActorSkills(RE::Actor* actor) {
    ActorSkills skills;
    
    if (!actor) return skills;
    
    auto avo = actor->AsActorValueOwner();
    if (!avo) return skills;
    
    skills.oneHanded = { avo->GetActorValue(RE::ActorValue::kOneHanded), RE::ActorValue::kOneHanded };
    skills.twoHanded = { avo->GetActorValue(RE::ActorValue::kTwoHanded), RE::ActorValue::kTwoHanded };
    skills.block = { avo->GetActorValue(RE::ActorValue::kBlock), RE::ActorValue::kBlock };
    skills.lightArmor = { avo->GetActorValue(RE::ActorValue::kLightArmor), RE::ActorValue::kLightArmor };  
    skills.heavyArmor = { avo->GetActorValue(RE::ActorValue::kHeavyArmor), RE::ActorValue::kHeavyArmor };
    skills.smithing = { avo->GetActorValue(RE::ActorValue::kSmithing), RE::ActorValue::kSmithing };
    
    skills.destruction = { avo->GetActorValue(RE::ActorValue::kDestruction), RE::ActorValue::kDestruction };
    skills.restoration = { avo->GetActorValue(RE::ActorValue::kRestoration), RE::ActorValue::kRestoration };
    skills.conjuration = { avo->GetActorValue(RE::ActorValue::kConjuration), RE::ActorValue::kConjuration };
    skills.alteration = { avo->GetActorValue(RE::ActorValue::kAlteration), RE::ActorValue::kAlteration };
    skills.illusion = { avo->GetActorValue(RE::ActorValue::kIllusion), RE::ActorValue::kIllusion };
    skills.enchanting = { avo->GetActorValue(RE::ActorValue::kEnchanting), RE::ActorValue::kEnchanting };
    
    skills.sneak = { avo->GetActorValue(RE::ActorValue::kSneak), RE::ActorValue::kSneak };
    skills.lockpicking = { avo->GetActorValue(RE::ActorValue::kLockpicking), RE::ActorValue::kLockpicking };
    skills.pickpocket = { avo->GetActorValue(RE::ActorValue::kPickpocket), RE::ActorValue::kPickpocket };
    skills.archery = { avo->GetActorValue(RE::ActorValue::kArchery), RE::ActorValue::kArchery };
    skills.alchemy = { avo->GetActorValue(RE::ActorValue::kAlchemy), RE::ActorValue::kAlchemy };
    skills.speech = { avo->GetActorValue(RE::ActorValue::kSpeech), RE::ActorValue::kSpeech };
    
    return skills;
}

// ============================================================================
// ActorSkills::GetSkillValue - Helper to get skill by ActorValue enum
// ============================================================================
float ActorSkills::GetSkillValue(RE::ActorValue av) const {
    switch (av) {
        case RE::ActorValue::kOneHanded: return oneHanded.value;
        case RE::ActorValue::kTwoHanded: return twoHanded.value;
        case RE::ActorValue::kBlock: return block.value;
        case RE::ActorValue::kLightArmor: return lightArmor.value;
        case RE::ActorValue::kHeavyArmor: return heavyArmor.value;
        case RE::ActorValue::kSmithing: return smithing.value;
        case RE::ActorValue::kDestruction: return destruction.value;
        case RE::ActorValue::kRestoration: return restoration.value;
        case RE::ActorValue::kConjuration: return conjuration.value;
        case RE::ActorValue::kAlteration: return alteration.value;
        case RE::ActorValue::kIllusion: return illusion.value;
        case RE::ActorValue::kEnchanting: return enchanting.value;
        case RE::ActorValue::kSneak: return sneak.value;
        case RE::ActorValue::kLockpicking: return lockpicking.value;
        case RE::ActorValue::kPickpocket: return pickpocket.value;
        case RE::ActorValue::kArchery: return archery.value;
        case RE::ActorValue::kAlchemy: return alchemy.value;
        case RE::ActorValue::kSpeech: return speech.value;
        default: return 0.0f;
    }
}

// ============================================================================
// Actor Value Name Lookup
// ============================================================================
const char* GetActorValueName(RE::ActorValue av) {
    switch (av) {
        case RE::ActorValue::kAggression: return "Aggression";
        case RE::ActorValue::kConfidence: return "Confidence";
        case RE::ActorValue::kEnergy: return "Energy";
        case RE::ActorValue::kMorality: return "Morality";
        case RE::ActorValue::kMood: return "Mood";
        case RE::ActorValue::kAssistance: return "Assistance";
        
        case RE::ActorValue::kOneHanded: return "One-Handed";
        case RE::ActorValue::kTwoHanded: return "Two-Handed";
        case RE::ActorValue::kArchery: return "Archery";
        case RE::ActorValue::kBlock: return "Block";
        case RE::ActorValue::kSmithing: return "Smithing";
        case RE::ActorValue::kHeavyArmor: return "Heavy Armor";
        case RE::ActorValue::kLightArmor: return "Light Armor";
        case RE::ActorValue::kPickpocket: return "Pickpocket";
        case RE::ActorValue::kLockpicking: return "Lockpicking";
        case RE::ActorValue::kSneak: return "Sneak";
        case RE::ActorValue::kAlchemy: return "Alchemy";
        case RE::ActorValue::kSpeech: return "Speech";
        case RE::ActorValue::kAlteration: return "Alteration";
        case RE::ActorValue::kConjuration: return "Conjuration";
        case RE::ActorValue::kDestruction: return "Destruction";
        case RE::ActorValue::kIllusion: return "Illusion";
        case RE::ActorValue::kRestoration: return "Restoration";
        case RE::ActorValue::kEnchanting: return "Enchanting";
        
        case RE::ActorValue::kHealth: return "Health";
        case RE::ActorValue::kMagicka: return "Magicka";
        case RE::ActorValue::kStamina: return "Stamina";
        case RE::ActorValue::kHealRate: return "Health Rate";
        case RE::ActorValue::kMagickaRate: return "Magicka Rate";
        case RE::ActorValue::kStaminaRate: return "Stamina Rate";
        
        case RE::ActorValue::kSpeedMult: return "Speed";
        case RE::ActorValue::kInventoryWeight: return "Carry Weight";
        case RE::ActorValue::kCarryWeight: return "Max Carry";
        case RE::ActorValue::kCriticalChance: return "Critical Chance";
        case RE::ActorValue::kMeleeDamage: return "Melee Damage";
        case RE::ActorValue::kUnarmedDamage: return "Unarmed Damage";
        case RE::ActorValue::kMass: return "Mass";
        case RE::ActorValue::kVoicePoints: return "Voice Points";
        case RE::ActorValue::kVoiceRate: return "Voice Rate";
        
        case RE::ActorValue::kDamageResist: return "Armor Rating";
        case RE::ActorValue::kPoisonResist: return "Poison Resist";
        case RE::ActorValue::kResistFire: return "Fire Resist";
        case RE::ActorValue::kResistShock: return "Shock Resist";
        case RE::ActorValue::kResistFrost: return "Frost Resist";
        case RE::ActorValue::kResistMagic: return "Magic Resist";
        case RE::ActorValue::kResistDisease: return "Disease Resist";
        
        case RE::ActorValue::kParalysis: return "Paralysis";
        case RE::ActorValue::kInvisibility: return "Invisibility";
        case RE::ActorValue::kNightEye: return "Night Eye";
        case RE::ActorValue::kDetectLifeRange: return "Detect Life";
        case RE::ActorValue::kWaterBreathing: return "Water Breathing";
        case RE::ActorValue::kWaterWalking: return "Water Walking";
        
        case RE::ActorValue::kBowSpeedBonus: return "Bow Speed";
        case RE::ActorValue::kAbsorbChance: return "Spell Absorb";
        case RE::ActorValue::kWeaponSpeedMult: return "Weapon Speed";
        case RE::ActorValue::kShoutRecoveryMult: return "Shout Recovery";
        case RE::ActorValue::kBowStaggerBonus: return "Bow Stagger";
        case RE::ActorValue::kMovementNoiseMult: return "Movement Noise";
        
        case RE::ActorValue::kOneHandedModifier: return "Fortify One-Handed";
        case RE::ActorValue::kTwoHandedModifier: return "Fortify Two-Handed";
        case RE::ActorValue::kMarksmanModifier: return "Fortify Archery";
        case RE::ActorValue::kBlockModifier: return "Fortify Block";
        case RE::ActorValue::kSmithingModifier: return "Fortify Smithing";
        case RE::ActorValue::kHeavyArmorModifier: return "Fortify Heavy Armor";
        case RE::ActorValue::kLightArmorModifier: return "Fortify Light Armor";
        case RE::ActorValue::kPickpocketModifier: return "Fortify Pickpocket";
        case RE::ActorValue::kLockpickingModifier: return "Fortify Lockpicking";
        case RE::ActorValue::kSneakingModifier: return "Fortify Sneak";
        case RE::ActorValue::kAlchemyModifier: return "Fortify Alchemy";
        case RE::ActorValue::kSpeechcraftModifier: return "Fortify Speech";
        case RE::ActorValue::kAlterationModifier: return "Fortify Alteration";
        case RE::ActorValue::kConjurationModifier: return "Fortify Conjuration";
        case RE::ActorValue::kDestructionModifier: return "Fortify Destruction";
        case RE::ActorValue::kIllusionModifier: return "Fortify Illusion";
        case RE::ActorValue::kRestorationModifier: return "Fortify Restoration";
        case RE::ActorValue::kEnchantingModifier: return "Fortify Enchanting";
        
        case RE::ActorValue::kOneHandedPowerModifier: return "One-Handed Power";
        case RE::ActorValue::kTwoHandedPowerModifier: return "Two-Handed Power";
        case RE::ActorValue::kMarksmanPowerModifier: return "Archery Power";
        case RE::ActorValue::kBlockPowerModifier: return "Block Power";
        case RE::ActorValue::kSmithingPowerModifier: return "Smithing Power";
        case RE::ActorValue::kHeavyArmorPowerModifier: return "Heavy Armor Rating";
        case RE::ActorValue::kLightArmorPowerModifier: return "Light Armor Rating";
        case RE::ActorValue::kSneakingPowerModifier: return "Sneak Power";
        case RE::ActorValue::kAlchemyPowerModifier: return "Alchemy Power";
        case RE::ActorValue::kAlterationPowerModifier: return "Alteration Power";
        case RE::ActorValue::kConjurationPowerModifier: return "Conjuration Power";
        case RE::ActorValue::kDestructionPowerModifier: return "Destruction Power";
        case RE::ActorValue::kIllusionPowerModifier: return "Illusion Power";
        case RE::ActorValue::kRestorationPowerModifier: return "Restoration Power";
        case RE::ActorValue::kEnchantingPowerModifier: return "Enchanting Power";
        
        case RE::ActorValue::kHealRateMult: return "Health Regen";
        case RE::ActorValue::kMagickaRateMult: return "Magicka Regen";
        case RE::ActorValue::kStaminaRateMult: return "Stamina Regen";
        
        case RE::ActorValue::kDragonSouls: return "Dragon Souls";
        case RE::ActorValue::kAttackDamageMult: return "Attack Damage";
        case RE::ActorValue::kCombatHealthRegenMultiply: return "Combat Health Regen";
        case RE::ActorValue::kReflectDamage: return "Reflect Damage";
        case RE::ActorValue::kWerewolfPerks: return "Werewolf Perks";
        case RE::ActorValue::kVampirePerks: return "Vampire Perks";
        
        default: return nullptr;
    }
}

// ============================================================================
// Dynamic Active Modifier Discovery
// ============================================================================
std::vector<ActiveModifier> GetActiveModifiers(RE::Actor* actor) {
    std::vector<ActiveModifier> results;
    
    if (!actor) return results;
    
    for (int i = 0; i < static_cast<int>(RE::ActorValue::kTotal); i++) {
        auto av = static_cast<RE::ActorValue>(i);
        
        float perm = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, av);
        float temp = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, av);
        
        if (perm == 0.0f && temp == 0.0f) continue;
        
        const char* name = GetActorValueName(av);
        if (!name) continue;
        
        ActiveModifier mod;
        mod.actorValue = av;
        mod.displayName = name;
        mod.permValue = perm;
        mod.tempValue = temp;
        results.push_back(mod);
    }
    
    return results;
}
