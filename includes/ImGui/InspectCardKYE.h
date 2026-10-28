#pragma once

// STATUS: Working

// ============================================================
// Inspect Card - Know Your Enemy 2 (KYE2) integration
// ============================================================

#include <RE/Skyrim.h>
#include <string>
#include <vector>

namespace InspectCardKYE {
    bool IsKYE2Loaded();

    enum class BadgeCategory {
        Archetype,
        Trait,
        Intelligence,
        Armor,
        Special
    };

    struct Badge {
        std::string  text;
        BadgeCategory category;
    };

    std::vector<Badge> GetBadges(RE::Actor* actor);

    enum class ResistTier {
        Weak,
        Resistant,
        HighlyResistant
    };

    struct ResistEntry {
        std::string label;
        ResistTier  tier;
    };

    std::vector<ResistEntry> GetResists(RE::Actor* actor);
}
