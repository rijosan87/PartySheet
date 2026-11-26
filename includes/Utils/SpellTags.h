#pragma once

#include <RE/Skyrim.h>

#include <cstdint>

namespace SpellTags {

    enum Tag : std::uint32_t {
        kFire          = 1u << 0,
        kFrost         = 1u << 1,
        kShock         = 1u << 2,
        kPoison        = 1u << 3,
        kDamage        = 1u << 4,
        kHeal          = 1u << 5,
        kBuff          = 1u << 6,
        kDebuff        = 1u << 7,
        kResist        = 1u << 8,
        kControl       = 1u << 9,
        kSummon        = 1u << 10,
        kBound         = 1u << 11,
        kStealth       = 1u << 12,
        kCloak         = 1u << 13,
        kRune          = 1u << 14,
        kAoE           = 1u << 15,
        kConcentration = 1u << 16,
    };
    inline constexpr int kTagCount = 17;

    std::uint32_t Of(const RE::TESForm* form);

    std::size_t CachedCount();

    const char* LabelKey(int i);
}
