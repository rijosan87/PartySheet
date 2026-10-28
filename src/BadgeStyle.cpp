// STATUS: Working
// Visual style config for badges and pills.

#include "../includes/BadgeStyle.h"
#include <algorithm>

namespace BadgeStyle {
    EnchantCategory ClassifyEnchant(const std::string& enchantType) {
        if (enchantType.empty()) return EnchantCategory::None;
        if (enchantType == "fortify_health" ||
            enchantType == "fortify_magicka" ||
            enchantType == "fortify_stamina" ||
            enchantType == "fortify_carry") {
            return EnchantCategory::FortifyStats;
        }
        if (enchantType.find("skill_") == 0) {
            return EnchantCategory::SkillPoints;
        }
        if (enchantType == "fortify_skill_mod") {
            return EnchantCategory::Situational;
        }
        if (enchantType.find("fortify_") == 0) {
            return EnchantCategory::FortifySkill;
        }
        if (enchantType.find("regen_") == 0) {
            return EnchantCategory::Regen;
        }
        if (enchantType.find("resist_") == 0) {
            return EnchantCategory::Resist;
        }
        if (enchantType.find("fire") != std::string::npos) {
            return EnchantCategory::DamageFire;
        }
        if (enchantType.find("frost") != std::string::npos) {
            return EnchantCategory::DamageFrost;
        }
        if (enchantType.find("shock") != std::string::npos) {
            return EnchantCategory::DamageShock;
        }
        if (enchantType.find("poison") != std::string::npos) {
            return EnchantCategory::DamagePoison;
        }
        return EnchantCategory::Other;
    }

    BadgeColors GetEnchantColors(EnchantCategory category) {
        switch (category) {
            case EnchantCategory::SkillPoints:
                return Theme::SkillBlue();
            case EnchantCategory::FortifyStats:
            case EnchantCategory::FortifySkill:
            case EnchantCategory::Regen:
                return Theme::FortifyGreen();
            case EnchantCategory::Situational:
                return {IM_COL32(190, 165, 100, 200), IM_COL32(80, 65, 30, 220)};
            case EnchantCategory::Resist:
                return Theme::ResistPurple();
            case EnchantCategory::DamageFire:
                return Theme::FireOrange();
            case EnchantCategory::DamageFrost:
                return Theme::FrostCyan();
            case EnchantCategory::DamageShock:
                return Theme::ShockYellow();
            case EnchantCategory::DamagePoison:
                return Theme::PoisonGreen();
            case EnchantCategory::Other:
            default:
                return Theme::MagicPurple();
        }
    }

    BadgeColors GetEnchantColors(const std::string& enchantType) {
        return GetEnchantColors(ClassifyEnchant(enchantType));
    }

    ImU32 GetEnchantTextColor(EnchantCategory category) {
        switch (category) {
            case EnchantCategory::SkillPoints:
                return TextColor::SkillBlue();
            case EnchantCategory::FortifyStats:
            case EnchantCategory::FortifySkill:
            case EnchantCategory::Regen:
                return TextColor::FortifyGreen();
            case EnchantCategory::Situational:
                return IM_COL32(190, 165, 100, 255);
            case EnchantCategory::Resist:
                return TextColor::ResistPurple();
            case EnchantCategory::DamageFire:
                return TextColor::FireOrange();
            case EnchantCategory::DamageFrost:
                return TextColor::FrostCyan();
            case EnchantCategory::DamageShock:
                return TextColor::ShockYellow();
            case EnchantCategory::DamagePoison:
                return TextColor::PoisonGreen();
            case EnchantCategory::Other:
            default:
                return TextColor::ResistPurple();
        }
    }

    ImU32 GetEnchantTextColor(const std::string& enchantType) {
        return GetEnchantTextColor(ClassifyEnchant(enchantType));
    }

    SlotBadgeConfig GetSlotConfig(SlotType type) {
        switch (type) {
            case SlotType::Weapon:
                return {
                    true,
                    true,
                    true,
                    true,
                    true
                };
            case SlotType::Armor:
                return {
                    false,
                    false,
                    true,
                    true,
                    false
                };
            case SlotType::Shield:
                return {
                    false,
                    false,
                    true,
                    true,
                    false
                };
            case SlotType::Accessory:
                return {
                    false,
                    false,
                    false,
                    true,
                    false
                };
            case SlotType::Ammo:
                return {
                    false,
                    false,
                    true,
                    false,
                    true
                };
            default:
                return {false, false, false, false, false};
        }
    }

    SlotType GetSlotTypeFromName(const std::string& slotName) {
        std::string lower = slotName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "main" || lower == "off" || lower == "main hand" || lower == "off hand" ||
            lower == "right hand" || lower == "left hand") {
            return SlotType::Weapon;
        }
        if (lower == "head" || lower == "chest" || lower == "body" ||
            lower == "hands" || lower == "feet") {
            return SlotType::Armor;
        }
        if (lower == "shield") {
            return SlotType::Shield;
        }
        if (lower == "ring" || lower == "amulet" || lower == "necklace") {
            return SlotType::Accessory;
        }
        if (lower == "ammo" || lower == "arrows" || lower == "bolts") {
            return SlotType::Ammo;
        }
        return SlotType::Armor;
    }

    BadgeColors GetBaseValueColors(bool isWeapon) {
        return isWeapon ? Theme::DamageRed() : Theme::NeutralGrey();
    }

    BadgeColors GetBaseValueColors(SlotType type) {
        bool isRed = (type == SlotType::Weapon || type == SlotType::Ammo);
        return GetBaseValueColors(isRed);
    }
}
