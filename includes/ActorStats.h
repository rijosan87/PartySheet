#pragma once

// STATUS: Working
// Vitals and skill stats provider.

#include <RE/Skyrim.h>
#include <vector>

// ============================================================================
// RichStat: Breakdown-aware stat for tooltips and detailed UI
// ============================================================================
struct RichStat {
    float current = 0.0f;
    float base = 0.0f;
    float perm = 0.0f;
    float temp = 0.0f;
    float GetMax() const { return base + perm + temp; }
    float GetPercent() const {
        float max = GetMax();
        return max > 0 ? std::clamp(current / max, 0.0f, 1.0f) : 0.0f;
    }
    bool HasModifiers() const { return perm != 0.0f || temp != 0.0f; }
    operator float() const { return current; }
};

// ============================================================================
// SkillInfo: Individual skill data (value + enum for identification)
// ============================================================================
struct SkillInfo {
    float value = 0.0f;
    RE::ActorValue actorValue = RE::ActorValue::kNone;
};

// ============================================================================
// ActorSkills: All 18 Skyrim skills organized by category
// ============================================================================
struct ActorSkills {
    SkillInfo oneHanded;
    SkillInfo twoHanded;
    SkillInfo block;
    SkillInfo lightArmor;
    SkillInfo heavyArmor;
    SkillInfo smithing;
    SkillInfo destruction;
    SkillInfo restoration;
    SkillInfo conjuration;
    SkillInfo alteration;
    SkillInfo illusion;
    SkillInfo enchanting;
    SkillInfo sneak;
    SkillInfo lockpicking;
    SkillInfo pickpocket;
    SkillInfo archery;
    SkillInfo alchemy;
    SkillInfo speech;
    float GetSkillValue(RE::ActorValue av) const;
};

// ============================================================================
// ActorStats: Comprehensive actor statistics
// ============================================================================
struct ActorStats {
    RichStat health;
    RichStat magicka;
    RichStat stamina;
    float healthRegen = 100.0f;
    float magickaRegen = 100.0f;
    float staminaRegen = 100.0f;
    float shoutRecovery = 100.0f;
    float armorRating = 0.0f;
    float weaponDamage = 0.0f;
    float baseWeaponDamage = 0.0f;
    float weaponSpeed = 1.0f;
    float weaponReach = 1.0f;
    float unarmedDamage = 0.0f;
    float arrowDamage = 0.0f;
    int arrowCount = 0;
    std::string arrowName = "";
    float damageResistPct = 0.0f;
    float resistFire = 0.0f;
    float resistFrost = 0.0f;
    float resistShock = 0.0f;
    float resistMagic = 0.0f;
    float resistPoison = 0.0f;
    float resistDisease = 0.0f;
    float speedMult = 100.0f;
    float carryWeight = 0.0f;
    float carryWeightMax = 300.0f;
    int gold = 0;
    int dragonSouls = 0;
    int perkPoints = 0;
    int level = 1;
    ActorSkills skills;
    float HealthPercent() const { return health.GetPercent(); }
    float MagickaPercent() const { return magicka.GetPercent(); }
    float StaminaPercent() const { return stamina.GetPercent(); }
    float healthMax() const { return health.GetMax(); }
    float magickaMax() const { return magicka.GetMax(); }
    float staminaMax() const { return stamina.GetMax(); }
};

// ============================================================================
// ActorResistances: Lightweight resistance-only struct
// ============================================================================
struct ActorResistances {
    float fire = 0.0f;
    float frost = 0.0f;
    float shock = 0.0f;
    float magic = 0.0f;
    float poison = 0.0f;
    float disease = 0.0f;
};

// ============================================================================
// ActiveModifier: Represents a non-zero modifier on an actor
// ============================================================================
struct ActiveModifier {
    RE::ActorValue actorValue;
    const char* displayName;
    float permValue = 0.0f;
    float tempValue = 0.0f;
    float GetTotal() const { return permValue + tempValue; }
    bool IsPositive() const { return GetTotal() > 0.0f; }
};

// ============================================================================
// API Functions
// ============================================================================

ActorStats GetActorStats(RE::Actor* actor);

ActorSkills GetActorSkills(RE::Actor* actor);

ActorResistances GetActorResistances(RE::Actor* actor);

std::vector<ActiveModifier> GetActiveModifiers(RE::Actor* actor);

const char* GetActorValueName(RE::ActorValue av);
