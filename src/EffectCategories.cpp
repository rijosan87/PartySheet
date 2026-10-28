// STATUS: Working
// Categorizes magic effects and provides icons.

#include "../includes/EffectCategories.h"
#include <string>
#include <algorithm>

namespace EffectCategories {
    EffectType GetEffectType(RE::EffectArchetypes::ArchetypeID archetype) {
        using A = RE::EffectArchetypes::ArchetypeID;
        switch (archetype) {
            case A::kParalysis:
            case A::kInvisibility:
            case A::kLight:
            case A::kNightEye:
            case A::kDetectLife:
            case A::kTurnUndead:
            case A::kDemoralize:
            case A::kFrenzy:
            case A::kCalm:
            case A::kReanimate:
            case A::kSoulTrap:
            case A::kDisguise:
            case A::kEtherealize:
            case A::kBanish:
            case A::kDisarm:
            case A::kCommandSummoned:
            case A::kSlowTime:
            case A::kBoundWeapon:
            case A::kWerewolf:
            case A::kWerewolfFeed:
            case A::kVampireLord:
            case A::kCloak:
            case A::kRally:
            case A::kTelekinesis:
            case A::kEnhanceWeapon:
            case A::kSummonCreature:
            case A::kGuide:
                return EffectType::Standalone;
            case A::kValueModifier:
            case A::kPeakValueModifier:
            case A::kDualValueModifier:
            case A::kAbsorb:
                return EffectType::Modifier;
            default:
                return EffectType::Unknown;
        }
    }

    const char* GetArchetypeName(RE::EffectArchetypes::ArchetypeID archetype) {
        using A = RE::EffectArchetypes::ArchetypeID;
        switch (archetype) {
            case A::kNone: return "None";
            case A::kValueModifier: return "Value Modifier";
            case A::kScript: return "Script";
            case A::kDispel: return "Dispel";
            case A::kCureDisease: return "Cure Disease";
            case A::kAbsorb: return "Absorb";
            case A::kDualValueModifier: return "Dual Value Modifier";
            case A::kCalm: return "Calm";
            case A::kDemoralize: return "Demoralize";
            case A::kFrenzy: return "Frenzy";
            case A::kDisarm: return "Disarm";
            case A::kCommandSummoned: return "Command Summoned";
            case A::kInvisibility: return "Invisibility";
            case A::kLight: return "Light";
            case A::kDarkness: return "Darkness";
            case A::kNightEye: return "Night Eye";
            case A::kLock: return "Lock";
            case A::kOpen: return "Open";
            case A::kBoundWeapon: return "Bound Weapon";
            case A::kSummonCreature: return "Summon Creature";
            case A::kDetectLife: return "Detect Life";
            case A::kTelekinesis: return "Telekinesis";
            case A::kParalysis: return "Paralysis";
            case A::kReanimate: return "Reanimate";
            case A::kSoulTrap: return "Soul Trap";
            case A::kTurnUndead: return "Turn Undead";
            case A::kGuide: return "Guide";
            case A::kWerewolfFeed: return "Werewolf Feed";
            case A::kCureParalysis: return "Cure Paralysis";
            case A::kCureAddiction: return "Cure Addiction";
            case A::kCurePoison: return "Cure Poison";
            case A::kConcussion: return "Concussion";
            case A::kValueAndParts: return "Value and Parts";
            case A::kAccumulateMagnitude: return "Accumulate Magnitude";
            case A::kStagger: return "Stagger";
            case A::kPeakValueModifier: return "Peak Value Modifier";
            case A::kCloak: return "Cloak";
            case A::kWerewolf: return "Werewolf";
            case A::kSlowTime: return "Slow Time";
            case A::kRally: return "Rally";
            case A::kEnhanceWeapon: return "Enhance Weapon";
            case A::kSpawnHazard: return "Spawn Hazard";
            case A::kEtherealize: return "Etherealize";
            case A::kBanish: return "Banish";
            case A::kDisguise: return "Disguise";
            case A::kGrabActor: return "Grab Actor";
            case A::kVampireLord: return "Vampire Lord";
            default: return "Unknown";
        }
    }

    const char* GetActorValueName(RE::ActorValue av) {
        switch (av) {
            case RE::ActorValue::kHealth: return "Health";
            case RE::ActorValue::kMagicka: return "Magicka";
            case RE::ActorValue::kStamina: return "Stamina";
            case RE::ActorValue::kHealRate: return "Health Regen";
            case RE::ActorValue::kMagickaRate: return "Magicka Regen";
            case RE::ActorValue::kStaminaRate: return "Stamina Regen";
            case RE::ActorValue::kHealRateMult: return "Health Regen Mult";
            case RE::ActorValue::kMagickaRateMult: return "Magicka Regen Mult";
            case RE::ActorValue::kStaminaRateMult: return "Stamina Regen Mult";
            case RE::ActorValue::kSpeedMult: return "Speed";
            case RE::ActorValue::kCarryWeight: return "Carry Weight";
            case RE::ActorValue::kDamageResist: return "Armor";
            case RE::ActorValue::kResistMagic: return "Magic Resist";
            case RE::ActorValue::kPoisonResist: return "Poison Resist";
            case RE::ActorValue::kResistFire: return "Fire Resist";
            case RE::ActorValue::kResistShock: return "Shock Resist";
            case RE::ActorValue::kResistFrost: return "Frost Resist";
            case RE::ActorValue::kResistDisease: return "Disease Resist";
            case RE::ActorValue::kOneHanded:
            case RE::ActorValue::kOneHandedModifier: return "One-Handed";
            case RE::ActorValue::kTwoHanded:
            case RE::ActorValue::kTwoHandedModifier: return "Two-Handed";
            case RE::ActorValue::kArchery:
            case RE::ActorValue::kMarksmanModifier: return "Archery";
            case RE::ActorValue::kBlock:
            case RE::ActorValue::kBlockModifier: return "Block";
            case RE::ActorValue::kSmithing:
            case RE::ActorValue::kSmithingModifier: return "Smithing";
            case RE::ActorValue::kHeavyArmor:
            case RE::ActorValue::kHeavyArmorModifier: return "Heavy Armor";
            case RE::ActorValue::kLightArmor:
            case RE::ActorValue::kLightArmorModifier: return "Light Armor";
            case RE::ActorValue::kPickpocket:
            case RE::ActorValue::kPickpocketModifier: return "Pickpocket";
            case RE::ActorValue::kLockpicking:
            case RE::ActorValue::kLockpickingModifier: return "Lockpicking";
            case RE::ActorValue::kSneak:
            case RE::ActorValue::kSneakingModifier: return "Sneak";
            case RE::ActorValue::kAlchemy:
            case RE::ActorValue::kAlchemyModifier: return "Alchemy";
            case RE::ActorValue::kSpeech:
            case RE::ActorValue::kSpeechcraftModifier: return "Speech";
            case RE::ActorValue::kAlteration:
            case RE::ActorValue::kAlterationModifier: return "Alteration";
            case RE::ActorValue::kConjuration:
            case RE::ActorValue::kConjurationModifier: return "Conjuration";
            case RE::ActorValue::kDestruction:
            case RE::ActorValue::kDestructionModifier: return "Destruction";
            case RE::ActorValue::kIllusion:
            case RE::ActorValue::kIllusionModifier: return "Illusion";
            case RE::ActorValue::kRestoration:
            case RE::ActorValue::kRestorationModifier: return "Restoration";
            case RE::ActorValue::kEnchanting:
            case RE::ActorValue::kEnchantingModifier: return "Enchanting";
            case RE::ActorValue::kAttackDamageMult: return "Attack Damage";
            case RE::ActorValue::kMeleeDamage: return "Melee Damage";
            case RE::ActorValue::kUnarmedDamage: return "Unarmed Damage";
            case RE::ActorValue::kCriticalChance: return "Critical Chance";
            default: return "Unknown";
        }
    }

    bool IsNegativeArchetype(RE::EffectArchetypes::ArchetypeID archetype) {
        using A = RE::EffectArchetypes::ArchetypeID;
        switch (archetype) {
            case A::kParalysis:
            case A::kDemoralize:
            case A::kFrenzy:
            case A::kDisarm:
            case A::kSoulTrap:
            case A::kBanish:
            case A::kStagger:
            case A::kConcussion:
                return true;
            default:
                return false;
        }
    }

    const char* GetStandaloneIconId(RE::EffectArchetypes::ArchetypeID archetype) {
        using A = RE::EffectArchetypes::ArchetypeID;
        switch (archetype) {
            case A::kParalysis: return "paralysis";
            case A::kInvisibility: return "invisibility";
            case A::kLight: return "light";
            case A::kNightEye: return "nighteye";
            case A::kDetectLife: return "detectlife";
            case A::kTurnUndead: return "turnundead";
            case A::kDemoralize: return "fear";
            case A::kFrenzy: return "frenzy";
            case A::kCalm: return "calm";
            case A::kReanimate: return "reanimate";
            case A::kSoulTrap: return "soultrap";
            case A::kDisguise: return "disguise";
            case A::kEtherealize: return "ethereal";
            case A::kBanish: return "banish";
            case A::kDisarm: return "disarm";
            case A::kCommandSummoned: return "command";
            case A::kSlowTime: return "slowtime";
            case A::kBoundWeapon: return "boundweapon";
            case A::kWerewolf: return "werewolf";
            case A::kWerewolfFeed: return "werewolf";
            case A::kVampireLord: return "vampire";
            case A::kCloak: return "cloak";
            case A::kRally: return "rally";
            case A::kTelekinesis: return "telekinesis";
            case A::kEnhanceWeapon: return "enhanceweapon";
            case A::kSummonCreature: return "summon";
            case A::kGuide: return "clairvoyance";
            default: return "";
        }
    }

    const char* GetActorValueIconId(RE::ActorValue av) {
        switch (av) {
            case RE::ActorValue::kHealth: return "health";
            case RE::ActorValue::kMagicka: return "magicka";
            case RE::ActorValue::kStamina: return "stamina";
            case RE::ActorValue::kHealRate: return "health_regen";
            case RE::ActorValue::kMagickaRate: return "magicka_regen";
            case RE::ActorValue::kStaminaRate: return "stamina_regen";
            case RE::ActorValue::kSpeedMult: return "speed";
            case RE::ActorValue::kCarryWeight: return "carryweight";
            case RE::ActorValue::kDamageResist: return "armor";
            case RE::ActorValue::kResistMagic: return "magic_resist";
            case RE::ActorValue::kPoisonResist: return "poison_resist";
            case RE::ActorValue::kResistFire: return "fire_resist";
            case RE::ActorValue::kResistShock: return "shock_resist";
            case RE::ActorValue::kResistFrost: return "frost_resist";
            case RE::ActorValue::kResistDisease: return "disease_resist";
            case RE::ActorValue::kOneHanded: return "onehanded";
            case RE::ActorValue::kTwoHanded: return "twohanded";
            case RE::ActorValue::kArchery: return "archery";
            case RE::ActorValue::kBlock: return "block";
            case RE::ActorValue::kAlteration: return "alteration";
            case RE::ActorValue::kConjuration: return "conjuration";
            case RE::ActorValue::kDestruction: return "destruction";
            case RE::ActorValue::kIllusion: return "illusion";
            case RE::ActorValue::kRestoration: return "restoration";
            case RE::ActorValue::kEnchanting: return "enchanting";
            case RE::ActorValue::kSneak: return "sneak";
            case RE::ActorValue::kLightArmor: return "lightarmor";
            case RE::ActorValue::kPickpocket: return "pickpocket";
            case RE::ActorValue::kLockpicking: return "lockpicking";
            case RE::ActorValue::kAlchemy: return "alchemy";
            case RE::ActorValue::kSpeech: return "speech";
            case RE::ActorValue::kHeavyArmor: return "heavyarmor";
            case RE::ActorValue::kSmithing: return "smithing";
            case RE::ActorValue::kAttackDamageMult: return "damage";
            case RE::ActorValue::kMeleeDamage: return "damage";
            case RE::ActorValue::kUnarmedDamage: return "unarmed";
            case RE::ActorValue::kCriticalChance: return "critical";
            case RE::ActorValue::kHealRateMult: return "health_regen";
            case RE::ActorValue::kMagickaRateMult: return "magicka_regen";
            case RE::ActorValue::kStaminaRateMult: return "stamina_regen";
            case RE::ActorValue::kShoutRecoveryMult: return "shout";
            case RE::ActorValue::kWardPower: return "ward";
            case RE::ActorValue::kAbsorbChance: return "absorb";
            case RE::ActorValue::kBowSpeedBonus: return "bowspeed";
            case RE::ActorValue::kWeaponSpeedMult: return "weaponspeed";
            case RE::ActorValue::kLeftWeaponSpeedMultiply: return "weaponspeed";
            case RE::ActorValue::kReflectDamage: return "reflect";
            case RE::ActorValue::kWaterBreathing: return "waterbreathing";
            case RE::ActorValue::kWaterWalking: return "waterwalking";
            case RE::ActorValue::kMovementNoiseMult: return "noise";
            case RE::ActorValue::kOneHandedPowerModifier: return "onehanded";
            case RE::ActorValue::kTwoHandedPowerModifier: return "twohanded";
            case RE::ActorValue::kMarksmanPowerModifier: return "archery";
            case RE::ActorValue::kBlockPowerModifier: return "block";
            case RE::ActorValue::kSmithingPowerModifier: return "smithing";
            case RE::ActorValue::kHeavyArmorPowerModifier: return "heavyarmor";
            case RE::ActorValue::kLightArmorPowerModifier: return "lightarmor";
            case RE::ActorValue::kPickpocketPowerModifier: return "pickpocket";
            case RE::ActorValue::kLockpickingPowerModifier: return "lockpicking";
            case RE::ActorValue::kSneakingPowerModifier: return "sneak";
            case RE::ActorValue::kAlchemyPowerModifier: return "alchemy";
            case RE::ActorValue::kSpeechcraftPowerModifier: return "speech";
            case RE::ActorValue::kAlterationPowerModifier: return "alteration";
            case RE::ActorValue::kConjurationPowerModifier: return "conjuration";
            case RE::ActorValue::kDestructionPowerModifier: return "destruction";
            case RE::ActorValue::kIllusionPowerModifier: return "illusion";
            case RE::ActorValue::kRestorationPowerModifier: return "restoration";
            case RE::ActorValue::kEnchantingPowerModifier: return "enchanting";
            case RE::ActorValue::kParalysis: return "paralysis";
            case RE::ActorValue::kInvisibility: return "invisibility";
            case RE::ActorValue::kNightEye: return "nighteye";
            case RE::ActorValue::kOneHandedModifier: return "onehanded";
            case RE::ActorValue::kTwoHandedModifier: return "twohanded";
            case RE::ActorValue::kMarksmanModifier: return "archery";
            case RE::ActorValue::kBlockModifier: return "block";
            case RE::ActorValue::kSmithingModifier: return "smithing";
            case RE::ActorValue::kHeavyArmorModifier: return "heavyarmor";
            case RE::ActorValue::kLightArmorModifier: return "lightarmor";
            case RE::ActorValue::kPickpocketModifier: return "pickpocket";
            case RE::ActorValue::kLockpickingModifier: return "lockpicking";
            case RE::ActorValue::kSneakingModifier: return "sneak";
            case RE::ActorValue::kAlchemyModifier: return "alchemy";
            case RE::ActorValue::kSpeechcraftModifier: return "speech";
            case RE::ActorValue::kAlterationModifier: return "alteration";
            case RE::ActorValue::kConjurationModifier: return "conjuration";
            case RE::ActorValue::kDestructionModifier: return "destruction";
            case RE::ActorValue::kIllusionModifier: return "illusion";
            case RE::ActorValue::kRestorationModifier: return "restoration";
            case RE::ActorValue::kEnchantingModifier: return "enchanting";
            default: return "default";
        }
    }

    bool IsPercentageMagnitude(RE::ActorValue av) {
        switch (av) {
            case RE::ActorValue::kHealRateMult:
            case RE::ActorValue::kMagickaRateMult:
            case RE::ActorValue::kStaminaRateMult:
            case RE::ActorValue::kOneHanded:
            case RE::ActorValue::kTwoHanded:
            case RE::ActorValue::kArchery:
            case RE::ActorValue::kBlock:
            case RE::ActorValue::kSmithing:
            case RE::ActorValue::kHeavyArmor:
            case RE::ActorValue::kLightArmor:
            case RE::ActorValue::kPickpocket:
            case RE::ActorValue::kLockpicking:
            case RE::ActorValue::kSneak:
            case RE::ActorValue::kAlchemy:
            case RE::ActorValue::kSpeech:
            case RE::ActorValue::kAlteration:
            case RE::ActorValue::kConjuration:
            case RE::ActorValue::kDestruction:
            case RE::ActorValue::kIllusion:
            case RE::ActorValue::kRestoration:
            case RE::ActorValue::kEnchanting:
            case RE::ActorValue::kOneHandedPowerModifier:
            case RE::ActorValue::kTwoHandedPowerModifier:
            case RE::ActorValue::kMarksmanPowerModifier:
            case RE::ActorValue::kBlockPowerModifier:
            case RE::ActorValue::kSmithingPowerModifier:
            case RE::ActorValue::kHeavyArmorPowerModifier:
            case RE::ActorValue::kLightArmorPowerModifier:
            case RE::ActorValue::kPickpocketPowerModifier:
            case RE::ActorValue::kLockpickingPowerModifier:
            case RE::ActorValue::kSneakingPowerModifier:
            case RE::ActorValue::kAlchemyPowerModifier:
            case RE::ActorValue::kSpeechcraftPowerModifier:
            case RE::ActorValue::kAlterationPowerModifier:
            case RE::ActorValue::kConjurationPowerModifier:
            case RE::ActorValue::kDestructionPowerModifier:
            case RE::ActorValue::kIllusionPowerModifier:
            case RE::ActorValue::kRestorationPowerModifier:
            case RE::ActorValue::kEnchantingPowerModifier:
            case RE::ActorValue::kAlterationModifier:
            case RE::ActorValue::kConjurationModifier:
            case RE::ActorValue::kDestructionModifier:
            case RE::ActorValue::kIllusionModifier:
            case RE::ActorValue::kRestorationModifier:
            case RE::ActorValue::kEnchantingModifier:
            case RE::ActorValue::kOneHandedModifier:
            case RE::ActorValue::kTwoHandedModifier:
            case RE::ActorValue::kBlockModifier:
            case RE::ActorValue::kHeavyArmorModifier:
            case RE::ActorValue::kLightArmorModifier:
            case RE::ActorValue::kSmithingModifier:
            case RE::ActorValue::kSneakingModifier:
            case RE::ActorValue::kMarksmanModifier:
            case RE::ActorValue::kSpeechcraftModifier:
            case RE::ActorValue::kAlchemyModifier:
            case RE::ActorValue::kLockpickingModifier:
            case RE::ActorValue::kPickpocketModifier:
            case RE::ActorValue::kResistMagic:
            case RE::ActorValue::kPoisonResist:
            case RE::ActorValue::kResistFire:
            case RE::ActorValue::kResistShock:
            case RE::ActorValue::kResistFrost:
            case RE::ActorValue::kResistDisease:
            case RE::ActorValue::kSpeedMult:
            case RE::ActorValue::kAbsorbChance:
            case RE::ActorValue::kReflectDamage:
            case RE::ActorValue::kShoutRecoveryMult:
            case RE::ActorValue::kBowSpeedBonus:
            case RE::ActorValue::kWeaponSpeedMult:
            case RE::ActorValue::kLeftWeaponSpeedMultiply:
            case RE::ActorValue::kMovementNoiseMult:
                return true;
            default:
                return false;
        }
    }

    EffectSource GetEffectSource(RE::ActiveEffect* activeEffect) {
        if (!activeEffect) return EffectSource::Unknown;
        auto* spell = activeEffect->spell;
        if (!spell) return EffectSource::Unknown;
        using ST = RE::MagicSystem::SpellType;
        auto spellType = spell->GetSpellType();
        {
            const char* spellName = spell->GetFullName();
            if (spellName) {
                std::string lower(spellName);
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower.find("blessing") != std::string::npos ||
                    lower.find("of kynareth") != std::string::npos ||
                    lower.find("of talos") != std::string::npos ||
                    lower.find("of mara") != std::string::npos ||
                    lower.find("of julianos") != std::string::npos ||
                    lower.find("of dibella") != std::string::npos ||
                    lower.find("of arkay") != std::string::npos ||
                    lower.find("of zenithar") != std::string::npos ||
                    lower.find("of stendarr") != std::string::npos ||
                    lower.find("of akatosh") != std::string::npos)
                    return EffectSource::Blessing;
            }
        }
        if (spellType == ST::kVoicePower)
            return EffectSource::Shout;
        if (spellType == ST::kScroll)
            return EffectSource::Scroll;
        if (spellType == ST::kStaffEnchantment)
            return EffectSource::Staff;
        if (spellType == ST::kEnchantment)
            return EffectSource::Enchantment;
        if (spellType == ST::kPoison)
            return EffectSource::Potion;
        if (spellType == ST::kAbility)
            return EffectSource::Ability;
        if (spellType == ST::kWortCraft)
            return EffectSource::Ingredient;
        if (spellType == ST::kPotion) {
            auto* alch = spell->As<RE::AlchemyItem>();
            if (alch && alch->IsFood())
                return EffectSource::Food;
            return EffectSource::Potion;
        }
        if (spellType == ST::kSpell || spellType == ST::kLeveledSpell)
            return EffectSource::Spell;
        if (spellType == ST::kPower || spellType == ST::kLesserPower)
            return EffectSource::Power;
        return EffectSource::Unknown;
    }

    const char* GetFrostbiteIconId(const std::string& name) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("frostbite (head)")  != std::string::npos) return "frostbite_head";
        if (lower.find("frostbite (hands)") != std::string::npos) return "frostbite_hands";
        if (lower.find("frostbite (feet)")  != std::string::npos) return "frostbite_feet";
        if (lower.find("frostbite (body)")  != std::string::npos) return "frostbite_body";
        return nullptr;
    }

    const char* GetScriptEffectIconId(const std::string& name, RE::ActorValue school) {
        auto contains = [&](const char* sub) {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return lower.find(sub) != std::string::npos;
        };
        if (contains("arm injury"))       return "injury_arm";
        if (contains("front leg injury")) return "injury_leg";
        if (contains("chest injury"))    return "injury_chest";
        if (contains("head injury"))     return "injury_head";
        if (contains("leg injury"))      return "injury_leg";
        if (contains("hind leg injury")) return "injury_leg";
        if (contains("spine injury"))    return "injury_spine";
        if (contains("night eye"))       return "nighteye";
        if (contains("detect life"))     return "detectlife";
        if (contains("detect dead"))     return "detectlife";
        if (contains("clairvoyance"))    return "clairvoyance";
        if (contains("muffle"))          return "noise";
        if (contains("water breathing")) return "waterbreathing";
        if (contains("water walking"))   return "waterwalking";
        if (contains("invisib"))         return "invisibility";
        if (contains("ethereal"))        return "ethereal";
        if (contains("slow time"))       return "slowtime";
        if (contains("werewolf"))        return "werewolf";
        if (contains("vampire lord"))    return "vampire";
        switch (school) {
            case RE::ActorValue::kDestruction: return "destruction";
            case RE::ActorValue::kAlteration:  return "alteration";
            case RE::ActorValue::kConjuration: return "conjuration";
            case RE::ActorValue::kIllusion:    return "illusion";
            case RE::ActorValue::kRestoration: return "restoration";
            default: break;
        }
        return "default";
    }
}
