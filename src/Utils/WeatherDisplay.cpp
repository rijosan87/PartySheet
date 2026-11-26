#include "../../includes/Utils/WeatherDisplay.h"
#include "../../includes/LocalizationManager.h"

#include <iterator>

namespace WeatherDisplay {

    namespace {

        struct InteriorIconRule {
            const char* keywordEditorID;
            const char* iconName;
        };

        RE::BGSKeyword* LookupKeyword(const char* editorID) {
            return editorID ? RE::TESForm::LookupByEditorID<RE::BGSKeyword>(editorID) : nullptr;
        }

        const char* GetInteriorIconName(RE::BGSLocation* location) {
            if (!location) {
                return "interior_generic";
            }

            static const InteriorIconRule rules[] = {
                { "LocTypeInn",               "interior_inn" },
                { "LocTypeCastle",            "interior_palace" },
                { "LocTypeJail",              "interior_jail" },
                { "LocTypeTemple",            "interior_temple" },
                { "LocTypeStore",             "interior_generic" },
                { "LocTypeCave",              "interior_cave" },
                { "LocTypeDungeon",           "interior_dungeon" },
                { "LocTypeDraugrCrypt",       "interior_dungeon" },
                { "LocTypeVampireLair",       "interior_dungeon" },
                { "LocTypeDragonPriestLair",  "interior_dungeon" },
                { "LocTypeMilitaryFort",      "interior_fort" },
                { "LocTypePlayerHouse",       "interior_home" },
                { "LocTypeDwelling",          "interior_home" }
            };

            static RE::BGSKeyword* cachedKeywords[std::size(rules)] = {};
            static bool initialized = false;
            if (!initialized) {
                initialized = true;
                for (std::size_t i = 0; i < std::size(rules); ++i) {
                    cachedKeywords[i] = LookupKeyword(rules[i].keywordEditorID);
                }
            }

            for (std::size_t i = 0; i < std::size(rules); ++i) {
                if (cachedKeywords[i] && location->HasKeyword(cachedKeywords[i])) {
                    return rules[i].iconName;
                }
            }

            return "interior_generic";
        }

        bool LocationIsInteriorLike(RE::BGSLocation* location) {
            static const char* const kEnclosedBuildingKeywords[] = {
                "LocTypeInn",
                "LocTypeStore",
                "LocTypeDwelling",
                "LocTypePlayerHouse",
                "LocTypeTemple",
                "LocTypeJail",
            };

            static RE::BGSKeyword* cached[std::size(kEnclosedBuildingKeywords)] = {};
            static bool initialized = false;
            if (!initialized) {
                initialized = true;
                for (std::size_t i = 0; i < std::size(kEnclosedBuildingKeywords); ++i) {
                    cached[i] = LookupKeyword(kEnclosedBuildingKeywords[i]);
                }
            }

            if (!location) {
                return false;
            }
            for (std::size_t i = 0; i < std::size(cached); ++i) {
                if (cached[i] && location->HasKeyword(cached[i])) {
                    return true;
                }
            }
            return false;
        }

        Display LocationDisplay(RE::TESObjectCELL* c, RE::BGSLocation* location, int maxChars) {
            std::string label;
            if (const char* cellName = c->GetFullName(); cellName && cellName[0] != '\0') {
                label = cellName;
            } else if (location) {
                if (const char* locationName = location->GetFullName(); locationName && locationName[0] != '\0') {
                    label = locationName;
                }
            }

            if (label.empty()) {
                label = LocalizationManager::Get("interior_fallback");
            }

            if (maxChars > 3 && label.size() > static_cast<std::size_t>(maxChars)) {
                label.resize(static_cast<std::size_t>(maxChars) - 3);
                label += "...";
            }

            RE::FormID identity = location ? location->GetFormID() : c->GetFormID();
            return { Kind::kInterior, identity, std::move(label), GetInteriorIconName(location), nullptr };
        }
    }

    Display Current(RE::PlayerCharacter* player, int maxChars) {
        if (!player) {
            return {};
        }

        auto* cell = player->GetParentCell();
        if (cell && cell->IsInteriorCell()) {
            return LocationDisplay(cell, cell->GetLocation(), maxChars);
        }

        if (cell) {
            auto* location = cell->GetLocation();
            if (LocationIsInteriorLike(location)) {
                return LocationDisplay(cell, location, maxChars);
            }
        }

        auto* sky = RE::Sky::GetSingleton();
        if (!sky) {
            return {};
        }

        auto* w = sky->currentWeather ? sky->currentWeather : sky->lastWeather;
        if (!w) {
            return {};
        }

        RE::FormID identity = w->GetFormID();
        using Flag = RE::TESWeather::WeatherDataFlag;
        auto flags = w->data.flags;

        if (flags.any(Flag::kSnow)) {
            return { Kind::kSnow, identity, LocalizationManager::Get("weather_snow"), "weather_snow", nullptr };
        }
        if (flags.any(Flag::kRainy)) {
            return { Kind::kRain, identity, LocalizationManager::Get("weather_rain"), "weather_rain", nullptr };
        }
        if (flags.any(Flag::kCloudy)) {
            return { Kind::kCloudy, identity, LocalizationManager::Get("weather_cloudy"), "weather_cloudy", "cloud" };
        }
        return { Kind::kClear, identity, LocalizationManager::Get("weather_clear"), "weather_clear", "sun" };
    }
}
