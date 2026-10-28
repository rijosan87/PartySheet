#pragma once

// STATUS: Working
// Visual style config for badges and pills.


#include <imgui.h>
#include <string>

namespace BadgeStyle {
    struct BadgeColors {
        ImU32 border;
        ImU32 fill;
    };

    namespace Theme {
        inline BadgeColors DamageRed() { return {IM_COL32(255, 80, 80, 200), IM_COL32(100, 30, 30, 220)}; }
        inline BadgeColors SkillBlue() { return {IM_COL32(120, 200, 255, 200), IM_COL32(30, 80, 140, 220)}; }
        inline BadgeColors FortifyGreen() { return {IM_COL32(80, 200, 80, 200), IM_COL32(30, 80, 30, 220)}; }
        inline BadgeColors ResistPurple() { return {IM_COL32(180, 100, 255, 200), IM_COL32(70, 40, 100, 220)}; }
        inline BadgeColors NeutralGrey() { return {IM_COL32(180, 180, 180, 200), IM_COL32(50, 50, 55, 220)}; }
        inline BadgeColors FireOrange() { return {IM_COL32(255, 120, 50, 200), IM_COL32(100, 50, 20, 220)}; }
        inline BadgeColors FrostCyan() { return {IM_COL32(80, 200, 255, 200), IM_COL32(30, 80, 100, 220)}; }
        inline BadgeColors ShockYellow() { return {IM_COL32(255, 230, 80, 200), IM_COL32(100, 90, 30, 220)}; }
        inline BadgeColors PoisonGreen() { return {IM_COL32(100, 220, 100, 200), IM_COL32(40, 90, 40, 220)}; }
        inline BadgeColors MagicPurple() { return {IM_COL32(200, 150, 255, 200), IM_COL32(80, 60, 100, 220)}; }
    }

    namespace TextColor {
        inline ImU32 DamageRed() { return IM_COL32(255, 100, 100, 255); }
        inline ImU32 SkillBlue() { return IM_COL32(140, 210, 255, 255); }
        inline ImU32 FortifyGreen() { return IM_COL32(100, 220, 100, 255); }
        inline ImU32 ResistPurple() { return IM_COL32(180, 130, 255, 255); }
        inline ImU32 FireOrange() { return IM_COL32(255, 120, 50, 255); }
        inline ImU32 FrostCyan() { return IM_COL32(80, 200, 255, 255); }
        inline ImU32 ShockYellow() { return IM_COL32(255, 230, 80, 255); }
        inline ImU32 PoisonGreen() { return IM_COL32(100, 220, 100, 255); }
        inline ImU32 Neutral() { return IM_COL32(200, 200, 200, 255); }
    }

    enum class EnchantCategory {
        None,
        SkillPoints,
        FortifyStats,
        FortifySkill,
        Situational,
        Resist,
        Regen,
        DamageFire,
        DamageFrost,
        DamageShock,
        DamagePoison,
        Other
    };

    EnchantCategory ClassifyEnchant(const std::string& enchantType);

    BadgeColors GetEnchantColors(EnchantCategory category);
    BadgeColors GetEnchantColors(const std::string& enchantType);

    ImU32 GetEnchantTextColor(EnchantCategory category);
    ImU32 GetEnchantTextColor(const std::string& enchantType);

    enum class SlotType {
        Weapon,
        Armor,
        Shield,
        Accessory,
        Ammo
    };

    struct SlotBadgeConfig {
        bool showSkillBonus;
        bool showFortifyBonus;
        bool showBaseValue;
        bool showEnchant;
        bool baseIsRed;
    };

    SlotBadgeConfig GetSlotConfig(SlotType type);

    SlotType GetSlotTypeFromName(const std::string& slotName);

    BadgeColors GetBaseValueColors(bool isWeapon);
    BadgeColors GetBaseValueColors(SlotType type);
}
