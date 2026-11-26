#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../ImGui/PlayerHUDSurvival.h"

namespace SKSE { class SerializationInterface; }

namespace SurvivalState {

    struct Need {
        PlayerHUDSurvival::NeedStatus status;
        float hoursToNextStage = -1.0f;
        bool  rising = false;
    };

    struct Snapshot {
        bool available = false;
        std::vector<Need> needs;
        std::optional<PlayerHUDSurvival::VitalityStatus> vitality;
        std::string stackWarning;
        std::string regionWord;
        std::optional<PlayerHUDSurvival::AmbientCold> ambient;
        bool  night = false;
        bool  indoors = false;
        bool  raining = false, snowing = false;
        float hoursSinceSleep = -1.0f;
        bool  wellRested = false;
        float restedHoursLeft = -1.0f;
    };

    inline constexpr std::uint32_t kRecordType = 'SLEP';

    void  Install();
    void  Tick();
    const Snapshot& Get();
    void  Clear();

    void OnGameSave(SKSE::SerializationInterface* serde);
    void LoadRecord(SKSE::SerializationInterface* serde, std::uint32_t version, std::uint32_t length);
}
