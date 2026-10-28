// STATUS: Working
// Parses player's currently worn armor and gear.

#include "../../includes/Utils/WornGearStatResolver.h"

#include <cstdio>

namespace WornGearStatResolver {
    namespace {
        struct SlotDef {
            RE::BIPED_MODEL::BipedObjectSlot slot;
        };
        struct SkillBitDef {
            RE::ActorValue baseAV;
            RE::ActorValue modifierAV;
            std::uint32_t  bit;
            const char*    iconName;
        };
        constexpr SlotDef kSlots[7] = {
            { RE::BIPED_MODEL::BipedObjectSlot::kHead },
            { RE::BIPED_MODEL::BipedObjectSlot::kBody },
            { RE::BIPED_MODEL::BipedObjectSlot::kFeet },
            { RE::BIPED_MODEL::BipedObjectSlot::kHands },
            { RE::BIPED_MODEL::BipedObjectSlot::kAmulet },
            { RE::BIPED_MODEL::BipedObjectSlot::kRing },
            { RE::BIPED_MODEL::BipedObjectSlot::kShield },
        };
        constexpr int kSlotDisplayPriority[7] = {
            1,
            0,
            3,
            2,
            4,
            5,
            6,
        };
        constexpr SkillBitDef kSkillBits[] = {
            { RE::ActorValue::kOneHanded,   RE::ActorValue::kOneHandedModifier,   1u << 0,  "onehanded"   },
            { RE::ActorValue::kTwoHanded,   RE::ActorValue::kTwoHandedModifier,   1u << 1,  "twohanded"   },
            { RE::ActorValue::kBlock,       RE::ActorValue::kBlockModifier,       1u << 2,  "block"       },
            { RE::ActorValue::kHeavyArmor,  RE::ActorValue::kHeavyArmorModifier,  1u << 3,  "heavyarmor"  },
            { RE::ActorValue::kLightArmor,  RE::ActorValue::kLightArmorModifier,  1u << 4,  "lightarmor"  },
            { RE::ActorValue::kSmithing,    RE::ActorValue::kSmithingModifier,    1u << 5,  "smithing"    },
            { RE::ActorValue::kSneak,       RE::ActorValue::kSneakingModifier,    1u << 6,  "sneak"       },
            { RE::ActorValue::kArchery,     RE::ActorValue::kMarksmanModifier,    1u << 7,  "archery"     },
            { RE::ActorValue::kSpeech,      RE::ActorValue::kSpeechcraftModifier, 1u << 8,  "speech"      },
            { RE::ActorValue::kAlchemy,     RE::ActorValue::kAlchemyModifier,     1u << 9,  "alchemy"     },
            { RE::ActorValue::kLockpicking, RE::ActorValue::kLockpickingModifier, 1u << 10, "lockpicking" },
            { RE::ActorValue::kPickpocket,  RE::ActorValue::kPickpocketModifier,  1u << 11, "pickpocket"  },
            { RE::ActorValue::kConjuration, RE::ActorValue::kConjurationModifier, 1u << 12, "conjuration" },
            { RE::ActorValue::kIllusion,    RE::ActorValue::kIllusionModifier,    1u << 13, "illusion"    },
            { RE::ActorValue::kAlteration,  RE::ActorValue::kAlterationModifier,  1u << 14, "alteration"  },
            { RE::ActorValue::kDestruction, RE::ActorValue::kDestructionModifier, 1u << 15, "destruction" },
            { RE::ActorValue::kRestoration, RE::ActorValue::kRestorationModifier, 1u << 16, "restoration" },
            { RE::ActorValue::kEnchanting,  RE::ActorValue::kEnchantingModifier,  1u << 17, "enchanting"  },
        };
        RE::TESObjectARMO* GetWornArmorForSlot(RE::Actor* actor, RE::BIPED_MODEL::BipedObjectSlot slot)
        {
            if (!actor) {
                return nullptr;
            }
            RE::TESObjectARMO* armor = actor->GetWornArmor(slot);
            if (!armor && slot == RE::BIPED_MODEL::BipedObjectSlot::kHead) {
                armor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHair);
                if (!armor) {
                    armor = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kCirclet);
                }
            }
            return armor;
        }
        void InsertEnchantBadge(WornGearSlotData& slotData, int priority, const char* iconName, const char* enchantType, const char* format, float magnitude)
        {
            for (std::size_t i = 0; i < slotData.enchantBadgeCount; ++i) {
                if (slotData.enchantBadges[i].enchantType == enchantType) {
                    return;
                }
            }
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), format, magnitude);
            struct PendingBadge {
                int         priority;
                EnchantBadge badge;
            };
            std::array<PendingBadge, 3> pending{};
            std::size_t pendingCount = 0;
            for (std::size_t i = 0; i < slotData.enchantBadgeCount; ++i) {
                pending[pendingCount++] = { slotData.enchantBadges[i].priority, slotData.enchantBadges[i] };
            }
            pending[pendingCount++] = {
                priority,
                {
                    priority,
                    buffer,
                    iconName,
                    enchantType
                }
            };
            for (std::size_t i = 1; i < pendingCount; ++i) {
                auto current = pending[i];
                std::size_t j = i;
                while (j > 0 && current.priority < pending[j - 1].priority) {
                    pending[j] = pending[j - 1];
                    --j;
                }
                pending[j] = current;
            }
            slotData.enchantBadgeCount = (std::min)(pendingCount, slotData.enchantBadges.size());
            for (std::size_t i = 0; i < slotData.enchantBadgeCount; ++i) {
                slotData.enchantBadges[i] = pending[i].badge;
            }
        }
    }

    WornGearData Resolve(RE::Actor* actor)
    {
        WornGearData data;
        if (!actor) {
            return data;
        }
        for (std::size_t i = 0; i < data.slots.size(); ++i) {
            auto& slotData = data.slots[i];
            slotData.slot = kSlots[i].slot;
            slotData.armor = GetWornArmorForSlot(actor, slotData.slot);
            slotData.armorValue = slotData.armor ? static_cast<float>(slotData.armor->GetArmorRating()) : -1.0f;
            auto* armor = slotData.armor;
            if (!armor) {
                continue;
            }
            auto* enchantable = armor->As<RE::TESEnchantableForm>();
            if (!enchantable || !enchantable->formEnchanting) {
                continue;
            }
            for (auto* effect : enchantable->formEnchanting->effects) {
                if (!effect || !effect->baseEffect) {
                    continue;
                }
                const auto av = effect->baseEffect->data.primaryAV;
                const float magnitude = effect->effectItem.magnitude;
                if (magnitude > 0.0f) {
                    if (av == RE::ActorValue::kResistFire) {
                        data.resistHighlightMask |= kResistFire;
                        InsertEnchantBadge(slotData, 2, "fire_resist", "resist_fire", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kResistFrost) {
                        data.resistHighlightMask |= kResistFrost;
                        InsertEnchantBadge(slotData, 2, "frost_resist", "resist_frost", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kResistShock) {
                        data.resistHighlightMask |= kResistShock;
                        InsertEnchantBadge(slotData, 2, "shock_resist", "resist_shock", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kPoisonResist) {
                        data.resistHighlightMask |= kResistPoison;
                        InsertEnchantBadge(slotData, 2, "poison_resist", "resist_poison", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kResistMagic) {
                        data.resistHighlightMask |= kResistMagic;
                        InsertEnchantBadge(slotData, 2, "magic_resist", "resist_magic", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kHealth) {
                        InsertEnchantBadge(slotData, 1, "health", "fortify_health", "+%.0f", magnitude);
                    } else if (av == RE::ActorValue::kMagicka) {
                        InsertEnchantBadge(slotData, 1, "magicka", "fortify_magicka", "+%.0f", magnitude);
                    } else if (av == RE::ActorValue::kStamina) {
                        InsertEnchantBadge(slotData, 1, "stamina", "fortify_stamina", "+%.0f", magnitude);
                    } else if (av == RE::ActorValue::kCarryWeight) {
                        InsertEnchantBadge(slotData, 1, "weight", "fortify_carry", "+%.0f", magnitude);
                    } else if (av == RE::ActorValue::kHealRate) {
                        InsertEnchantBadge(slotData, 3, "health", "regen_health", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kMagickaRate) {
                        InsertEnchantBadge(slotData, 3, "magicka", "regen_magicka", "%.0f%%", magnitude);
                    } else if (av == RE::ActorValue::kStaminaRate) {
                        InsertEnchantBadge(slotData, 3, "stamina", "regen_stamina", "%.0f%%", magnitude);
                    }
                }
                for (const auto& skill : kSkillBits) {
                    const bool isBaseSkill = av == skill.baseAV;
                    const bool isModifier = av == skill.modifierAV;
                    if (!isBaseSkill && !isModifier) {
                        continue;
                    }
                    if (isModifier) {
                        data.situationalHighlightMask |= skill.bit;
                    } else {
                        data.skillHighlightMask |= skill.bit;
                    }
                    if (magnitude > 0.0f) {
                        if (isModifier) {
                            InsertEnchantBadge(slotData, 0, skill.iconName, "fortify_skill_mod", "%.0f%%", magnitude);
                        } else {
                            InsertEnchantBadge(slotData, 0, skill.iconName, "skill_points", "+%.0f", magnitude);
                        }
                    }
                    break;
                }
            }
        }
        for (std::size_t i = 0; i < data.slots.size(); ++i) {
            auto* armorI = data.slots[i].armor;
            if (!armorI) {
                continue;
            }
            for (std::size_t j = i + 1; j < data.slots.size(); ++j) {
                auto* armorJ = data.slots[j].armor;
                if (!armorJ || armorI != armorJ) {
                    continue;
                }
                const std::size_t keepIndex = (kSlotDisplayPriority[i] <= kSlotDisplayPriority[j]) ? i : j;
                const std::size_t clearIndex = (keepIndex == i) ? j : i;
                data.slots[clearIndex] = {};
            }
        }
        return data;
    }
}
