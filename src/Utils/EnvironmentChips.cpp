#include "../../includes/Utils/EnvironmentChips.h"
#include "../../includes/Utils/WeatherDisplay.h"
#include "../../includes/LocalizationManager.h"

#include <RE/Skyrim.h>
#include <algorithm>

namespace EnvironmentChips {

    namespace {

        std::string Loc(const char* key, const char* english)
        {
            std::string text = LocalizationManager::Get(key);
            return (text.empty() || text == key) ? std::string(english) : text;
        }

        constexpr int kFullShelter = 2;

        Chip TemperatureChip(const SurvivalState::Snapshot& s)
        {
            const auto& a = s.ambient;
            Chip chip;
            chip.key = "temperature";
            chip.icon = nullptr;
            if (a && a->hasTemperature) {
                const float t = a->temperature;
                chip.thermo = std::clamp((t + 15.0f) / 25.0f, 0.0f, 1.0f);
                chip.title = t >= 5.0f  ? Loc("Condition.Mild", "Mild")
                           : t >= 0.0f  ? Loc("Condition.Cool", "Cool")
                           : t >= -9.0f ? Loc("Condition.Cold", "Cold")
                                        : Loc("Condition.Freezing", "Freezing");
                if (t <= 0.0f) chip.mood = Mood::Cold;
            } else if (a && a->stage >= 0) {
                chip.thermo = a->areaFill >= 0.0f ? 1.0f - a->areaFill
                                                  : 1.0f - std::clamp(a->stage / 5.0f, 0.0f, 1.0f);
                chip.title = a->stage <= 0 ? Loc("Condition.WarmArea", "Warm area")
                           : a->stage <= 2 ? Loc("Condition.CoolArea", "Cool area")
                                           : Loc("Condition.FreezingArea", "Freezing area");
                if (a->stage >= 3) chip.mood = Mood::Cold;
            } else {
                chip.title = s.regionWord;
            }
            return chip;
        }
    }

    std::vector<Chip> Build(const SurvivalState::Snapshot& s, int maxChars)
    {
        std::vector<Chip> chips;
        const auto& a = s.ambient;
        bool trendSaid = false;

        if (a && a->nearFire) {
            Chip warmth{ "warmth", "fire", nullptr, Loc("Condition.Warmth", "Warmth"),
                         Loc("Condition.NearAFire", "near a fire"), Mood::Warm, Mood::Good };
            if (a->trend > 0) {
                warmth.description = Loc("Condition.FireNotEnough", "not warming you");
                warmth.descriptionMood = Mood::Losing;
                trendSaid = true;
            }
            chips.push_back(std::move(warmth));
        }

        if (s.indoors) {
            chips.push_back({ "shelter", "interior_home", nullptr, Loc("Condition.Indoors", "Indoors"), "", Mood::Shelter });
        } else if (a && a->sheltered) {
            const bool full = a->shelterLevel >= kFullShelter;
            chips.push_back({ "shelter", "interior_home", nullptr,
                              full ? Loc("Condition.Sheltered", "Sheltered") : Loc("Condition.PartlySheltered", "Partly sheltered"),
                              full ? Loc("Condition.ShelterFull", "fully dry") : Loc("Condition.ShelterPartly", "dry"),
                              Mood::Shelter });
        }

        const auto wd = WeatherDisplay::Current(RE::PlayerCharacter::GetSingleton(), maxChars);
        if (wd.kind != WeatherDisplay::Kind::kUnknown) {
            const bool clearNight = wd.kind == WeatherDisplay::Kind::kClear && s.night;
            Chip weather{ "weather", clearNight ? "moon" : wd.primaryIcon, clearNight ? wd.primaryIcon : wd.fallbackIcon,
                          wd.label, s.night ? Loc("Condition.Night", "Night") : Loc("Condition.Day", "Day") };
            if (wd.kind == WeatherDisplay::Kind::kRain || wd.kind == WeatherDisplay::Kind::kSnow) weather.mood = Mood::Cold;
            chips.push_back(std::move(weather));
        }

        Chip temp = TemperatureChip(s);
        if (!temp.title.empty()) {
            if (!trendSaid && a && a->trend < 0) { temp.description = Loc("Condition.WarmingUp", "warming up"); temp.descriptionMood = Mood::Good; }
            if (!trendSaid && a && a->trend > 0) { temp.description = Loc("Condition.GettingColder", "getting colder"); temp.descriptionMood = Mood::Cold; }
            chips.push_back(std::move(temp));
        }
        return chips;
    }
}
