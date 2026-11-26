#pragma once

#include "SurvivalState.h"

#include <string>
#include <vector>

namespace EnvironmentChips {

    enum class Mood { Plain, Warm, Losing, Cold, Shelter, Good };

    struct Chip {
        const char* key = "";
        const char* icon = nullptr;
        const char* fallbackIcon = nullptr;
        std::string title, description;
        Mood mood = Mood::Plain;
        Mood descriptionMood = Mood::Plain;
        float thermo = -1.0f;
    };

    std::vector<Chip> Build(const SurvivalState::Snapshot& snapshot, int maxChars);
}
