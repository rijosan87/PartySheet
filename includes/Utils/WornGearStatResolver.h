#pragma once

// STATUS: Working
// Parses player's currently worn armor and gear.

#include <RE/Skyrim.h>

#include <array>
#include <cstdint>
#include <string>

namespace WornGearStatResolver {
    inline constexpr std::uint8_t kResistFire   = 1 << 0;
    inline constexpr std::uint8_t kResistFrost  = 1 << 1;
    inline constexpr std::uint8_t kResistShock  = 1 << 2;
    inline constexpr std::uint8_t kResistPoison = 1 << 3;
    inline constexpr std::uint8_t kResistMagic  = 1 << 4;

    struct EnchantBadge {
        int         priority = 0;
        std::string text;
        std::string iconName;
        std::string enchantType;
    };

    struct WornGearSlotData {
        RE::BIPED_MODEL::BipedObjectSlot slot = RE::BIPED_MODEL::BipedObjectSlot::kNone;
        RE::TESObjectARMO*               armor = nullptr;
        float                            armorValue = -1.0f;
        std::array<EnchantBadge, 2>      enchantBadges{};
        std::size_t                      enchantBadgeCount = 0;
    };

    struct WornGearData {
        std::array<WornGearSlotData, 7> slots{};
        std::uint8_t                    resistHighlightMask = 0;
        std::uint32_t                   skillHighlightMask = 0;
        std::uint32_t                   situationalHighlightMask = 0;
    };

    WornGearData Resolve(RE::Actor* actor);
}
