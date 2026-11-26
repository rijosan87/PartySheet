#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <string>

namespace WeatherDisplay {

    enum class Kind : std::uint8_t {
        kUnknown = 0,
        kInterior,
        kClear,
        kCloudy,
        kRain,
        kSnow
    };

    struct Display {
        Kind kind = Kind::kUnknown;
        RE::FormID identityFormID = 0;
        std::string label;
        const char* primaryIcon = nullptr;
        const char* fallbackIcon = nullptr;
    };

    Display Current(RE::PlayerCharacter* player, int maxChars);
}
