#pragma once

// STATUS: Working
// Coordinates potion consumption animations.


#include <RE/Skyrim.h>

#include <string>
#include <string_view>

namespace PotionAnimationBridge {
    bool TryDrinkWithAnimation(RE::AlchemyItem* potion, std::string_view entryId);

    bool IsBusy();

    std::string ActiveEntryId();
}
