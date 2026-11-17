#pragma once

#include <RE/Skyrim.h>

#include "EquipmentStatResolver.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace GearCatalog {

    enum class Card : std::uint8_t { RightHand, LeftHand, Head, Body, Feet, Hands, Amulet, Ring };
    constexpr std::size_t kCardCount = 8;

    enum class Category : std::uint8_t { Swords, Daggers, Axes, Maces, Bows, Staves, OffHand, Armor, Ammo, None };

    struct InstanceKey {
        RE::FormID  baseID = 0;
        float       health = 0.0f;
        RE::FormID  enchantID = 0;
        RE::FormID  poisonID = 0;
        std::string customName;
        bool        hasExtra = false;
        bool operator==(const InstanceKey&) const = default;
    };

    struct GearItem {
        InstanceKey  key;
        std::string  name;
        std::string  iconHint;
        Category     category = Category::None;
        int          count = 1;
        bool         equipped = false;
        bool         twoHanded = false;
        float        statValue = -1.0f;
        std::vector<EquipmentStatResolver::EnchantDamage> enchants;
    };

    struct CardList {
        std::vector<GearItem> items;
        std::uint32_t         categoryMask = 0;
        bool                  interactive = true;
    };

    enum class ItemTab : std::uint8_t { Weapon, Armor, Potion, Food, Ingredient, Book, Scroll, Misc };
    enum class ItemType : std::uint8_t { Weapon, Armor, Potion, Food, Ammo, Ingredient, SoulGem, Key, Book, Scroll, Misc };

    struct InventoryItem {
        InstanceKey  key;
        std::string  name;
        std::string  iconHint;
        ItemTab      tab = ItemTab::Misc;
        ItemType     type = ItemType::Misc;
        float        weight = 0.0f;
        int          value = 0;
        int          count = 1;
        float        statValue = -1.0f;
        bool         quest = false;
        bool         worn = false;
        bool         enchanted = false;
        bool         tempered = false;
    };

    struct ActorGear {
        std::array<CardList, kCardCount> cards{};
        std::vector<InventoryItem>       items;
        bool                             canEquip = false;
    };

    Category Classify(RE::TESForm* form);

    const char* IconHint(RE::TESForm* form);

    const char* CategoryIcon(Category category);

    std::shared_ptr<const ActorGear> Get(RE::Actor* actor);

    void Invalidate(RE::FormID actorID);

    void Clear();

    InstanceKey KeyOf(RE::TESBoundObject* obj, RE::ExtraDataList* xl);

    RE::ExtraDataList* ResolveExtraList(RE::Actor* actor, const InstanceKey& key, bool& outFound);
}
