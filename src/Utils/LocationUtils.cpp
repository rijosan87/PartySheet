// STATUS: Working
// Resolves location and cell display names.

#include "../../includes/Utils/LocationUtils.h"

namespace LocationUtils {
    namespace {
        struct KeywordRule {
            const char* editorID;
            LocationType type;
        };
        static const KeywordRule kRules[] = {
            { "LocTypeInn",              LocationType::SafeInterior },
            { "LocTypeDwelling",         LocationType::SafeInterior },
            { "LocTypePlayerHouse",      LocationType::SafeInterior },
            { "LocTypeStore",            LocationType::SafeInterior },
            { "LocTypeTemple",           LocationType::SafeInterior },
            { "LocTypeCastle",           LocationType::SafeInterior },
            { "LocTypeJail",             LocationType::SafeInterior },
            { "LocTypeDungeon",          LocationType::HostileInterior },
            { "LocTypeCave",             LocationType::HostileInterior },
            { "LocTypeDraugrCrypt",      LocationType::HostileInterior },
            { "LocTypeVampireLair",      LocationType::HostileInterior },
            { "LocTypeDragonPriestLair", LocationType::HostileInterior },
            { "LocTypeMilitaryFort",     LocationType::HostileInterior },
        };
        static RE::BGSKeyword* s_Keywords[std::size(kRules)] = {};
        static bool s_KeywordsInitialized = false;
        static void EnsureKeywordsInitialized() {
            if (s_KeywordsInitialized) return;
            s_KeywordsInitialized = true;
            for (std::size_t i = 0; i < std::size(kRules); ++i) {
                s_Keywords[i] = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(kRules[i].editorID);
            }
        }
        static LocationType ClassifyLocation(RE::BGSLocation* location) {
            if (!location) return LocationType::UnknownInterior;
            EnsureKeywordsInitialized();
            for (std::size_t i = 0; i < std::size(kRules); ++i) {
                if (s_Keywords[i] && location->HasKeyword(s_Keywords[i])) {
                    return kRules[i].type;
                }
            }
            return LocationType::UnknownInterior;
        }
    }

    LocationType GetPlayerLocationType() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return LocationType::Exterior;
        auto* cell = player->GetParentCell();
        if (!cell || !cell->IsInteriorCell()) return LocationType::Exterior;
        return ClassifyLocation(cell->GetLocation());
    }

    bool ShouldShowDirectionArrow() {
        LocationType loc = GetPlayerLocationType();
        return loc != LocationType::SafeInterior;
    }
}
