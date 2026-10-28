#pragma once

// STATUS: Working
// Resolves player and target traits.

#include <string>
#include <vector>
#include "../ActorStats.h"

namespace TraitResolver {
constexpr float kMinSkillThreshold = 25.0f;

constexpr float kFlavorThreshold = 75.0f;

struct TraitEntry {
    std::string tierKey;
    std::string nameKey;
    std::string labelKey;
};

std::vector<TraitEntry> Resolve(const ActorSkills& skills, int maxTraits = 4);

std::vector<std::string> ResolveLocalized(const ActorSkills& skills, int maxTraits = 4);
}
