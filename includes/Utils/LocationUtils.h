#pragma once

// STATUS: Working
// Resolves location and cell display names.

#include <RE/Skyrim.h>

namespace LocationUtils {
    enum class LocationType {
        Exterior,
        HostileInterior,
        SafeInterior,
        UnknownInterior
    };

    LocationType GetPlayerLocationType();

    bool ShouldShowDirectionArrow();
}
