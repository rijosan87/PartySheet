// STATUS: Working
// Tracks current player vitals and status.

#include "../includes/PlayerStatus.h"
#include "../includes/LocalizationManager.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <Windows.h>

namespace PlayerStatus {
// ============================================================================
// Internal Data Structures
// ============================================================================

struct FactionDef {
    std::string mod;
    RE::FormID localFormId;
    std::string key;
};

struct ChampionDef {
    struct GlobalCondition {
        std::string editorId;
        float equals = 0.0f;
    };
    std::string mod;
    RE::FormID questId;
    std::vector<int> stagesFavor;
    std::vector<int> stagesOpposed;
    std::vector<GlobalCondition> favorConditions;
    std::vector<GlobalCondition> opposedConditions;
    std::string name;
    std::string key;
};

struct ThaneDef {
    std::string mod;
    RE::FormID questId;
    std::vector<int> stages;
    std::string hold;
    std::string key;
};

static std::vector<FactionDef> g_factionDefs;
static std::vector<ChampionDef> g_championDefs;
static std::vector<ThaneDef> g_thaneDefs;
static bool g_initialized = false;
using _GetFormFromEditorID = std::uintptr_t (*)(const char*);
static _GetFormFromEditorID g_po3GetFormFromEditorID = nullptr;
static bool g_po3LookupInitialized = false;

static RE::TESGlobal* ResolveGlobalByEditorID(const std::string& editorId) {
    if (!g_po3LookupInitialized) {
        g_po3LookupInitialized = true;
        HMODULE handle = GetModuleHandleA("po3_Tweaks.dll");
        if (!handle) {
            handle = GetModuleHandleA("po3_Tweaks");
        }
        if (handle) {
            g_po3GetFormFromEditorID = reinterpret_cast<_GetFormFromEditorID>(
                GetProcAddress(handle, "GetFormFromEditorID"));
            if (g_po3GetFormFromEditorID) {
                logger::info("PlayerStatus: Linked po3_Tweaks GetFormFromEditorID");
            } else {
                logger::warn("PlayerStatus: po3_Tweaks found, but GetFormFromEditorID export missing; using fallback lookup");
            }
        }
    }
    if (g_po3GetFormFromEditorID) {
        std::uintptr_t raw = g_po3GetFormFromEditorID(editorId.c_str());
        if (raw != 0) {
            if (raw <= 0xFFFFFFFFull) {
                return RE::TESForm::LookupByID<RE::TESGlobal>(static_cast<RE::FormID>(raw));
            }
            auto* form = reinterpret_cast<RE::TESForm*>(raw);
            if (form) {
                return form->As<RE::TESGlobal>();
            }
        }
    }
    return RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorId);
}

// ============================================================================
// FormID Resolution
// ============================================================================

RE::FormID ResolveFormID(const std::string& modName, RE::FormID localId) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return 0;
    auto* modInfo = dataHandler->LookupModByName(modName);
    if (!modInfo) return 0;
    if (modInfo->IsLight()) {
        return 0xFE000000 | (modInfo->GetSmallFileCompileIndex() << 12) | (localId & 0xFFF);
    } else {
        return (modInfo->GetCompileIndex() << 24) | (localId & 0xFFFFFF);
    }
}

// ============================================================================
// Quest Stage Check
// ============================================================================

bool IsQuestStageComplete(const std::string& mod, RE::FormID localId, int stage, bool requireCompleted) {
    if (mod.empty() || localId == 0) return false;
    RE::FormID fullId = ResolveFormID(mod, localId);
    if (fullId == 0) return false;
    auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(fullId);
    if (!quest) return false;
    bool completed = !requireCompleted || quest->IsCompleted();
    int currentStage = quest->GetCurrentStageID();
    return completed && currentStage == stage;
}

bool WasQuestStageExecuted(const std::string& mod, RE::FormID localId, int stage) {
    if (mod.empty() || localId == 0) return false;
    RE::FormID fullId = ResolveFormID(mod, localId);
    if (fullId == 0) return false;
    auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(fullId);
    if (!quest) return false;
    if (quest->executedStages) {
        for (auto it = quest->executedStages->begin(); it != quest->executedStages->end(); ++it) {
            if (it->data.index == stage && it->data.flags.underlying() == 1) return true;
        }
    }
    if (quest->waitingStages) {
        for (auto it = quest->waitingStages->begin(); it != quest->waitingStages->end(); ++it) {
            if ((*it)->data.index == stage && (*it)->data.flags.underlying() == 1) return true;
        }
    }
    return false;
}

bool IsPlayerInFaction(RE::Actor* player, const std::string& mod, RE::FormID localId) {
    RE::FormID fullId = ResolveFormID(mod, localId);
    if (fullId == 0) return false;
    auto* faction = RE::TESForm::LookupByID<RE::TESFaction>(fullId);
    return faction && player->IsInFaction(faction);
}

// ============================================================================
// Simple JSON Parsing Helpers
// ============================================================================

static std::string ExtractValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    size_t start = json.find_first_not_of(" \t\n\r", colonPos + 1);
    if (start == std::string::npos) return "";
    if (json[start] == '"') {
        size_t end = json.find('"', start + 1);
        if (end == std::string::npos) return "";
        return json.substr(start + 1, end - start - 1);
    } else if (json[start] == '[') {
        size_t end = json.find(']', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start + 1);
    } else {
        size_t end = json.find_first_of(",}\n\r", start);
        if (end == std::string::npos) return "";
        std::string val = json.substr(start, end - start);
        size_t first = val.find_first_not_of(" \t");
        size_t last = val.find_last_not_of(" \t");
        return (first == std::string::npos) ? "" : val.substr(first, last - first + 1);
    }
}

static RE::FormID ParseHexFormId(const std::string& s) {
    if (s.empty()) return 0;
    std::string clean = s;
    if (clean.size() > 2 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) {
        clean = clean.substr(2);
    }
    try {
        return static_cast<RE::FormID>(std::stoul(clean, nullptr, 16));
    } catch (...) {
        return 0;
    }
}

static std::vector<int> ParseIntArray(const std::string& arrayStr) {
    std::vector<int> result;
    std::string num;
    for (char c : arrayStr) {
        if (c >= '0' && c <= '9') {
            num += c;
        } else if (!num.empty()) {
            try {
                result.push_back(std::stoi(num));
            } catch (...) {}
            num.clear();
        }
    }
    if (!num.empty()) {
        try {
            result.push_back(std::stoi(num));
        } catch (...) {}
    }
    return result;
}

static std::vector<std::string> SplitObjects(const std::string& json) {
    std::vector<std::string> objects;
    int depth = 0;
    size_t start = 0;

    for (size_t i = 0; i < json.size(); i++) {
        if (json[i] == '{') {
            if (depth == 0) start = i;
            depth++;
        } else if (json[i] == '}') {
            depth--;
            if (depth == 0) {
                objects.push_back(json.substr(start, i - start + 1));
            }
        }
    }
    return objects;
}

static std::string ExtractArray(const std::string& json, const std::string& key);

static bool ParseFloatValue(const std::string& s, float& outValue) {
    if (s.empty()) return false;
    try {
        outValue = std::stof(s);
        return true;
    } catch (...) {
        return false;
    }
}

static std::vector<ChampionDef::GlobalCondition> ParseChampionConditions(const std::string& obj, const std::string& key) {
    std::vector<ChampionDef::GlobalCondition> conditions;
    std::string conditionsArray = ExtractArray(obj, key);
    if (conditionsArray.empty()) return conditions;

    for (const auto& condObj : SplitObjects(conditionsArray)) {
        ChampionDef::GlobalCondition cond;
        cond.editorId = ExtractValue(condObj, "editorId");
        float equalsValue = 0.0f;
        bool hasEquals = ParseFloatValue(ExtractValue(condObj, "equals"), equalsValue);
        if (!hasEquals || cond.editorId.empty()) {
            logger::warn("PlayerStatus: Invalid champion condition in '{}': editorId='{}' equals='{}'",
                         key, cond.editorId, ExtractValue(condObj, "equals"));
            continue;
        }
        cond.equals = equalsValue;
        conditions.push_back(cond);
    }

    return conditions;
}

static bool DoGlobalConditionsMatch(const std::vector<ChampionDef::GlobalCondition>& conditions) {
    if (conditions.empty()) return false;

    for (const auto& cond : conditions) {
        auto* global = ResolveGlobalByEditorID(cond.editorId);
        if (!global) {
            logger::warn("PlayerStatus: Could not resolve TESGlobal '{}'", cond.editorId);
            return false;
        }
        if (global->value != cond.equals) {
            return false;
        }
    }

    return true;
}

static std::string ExtractArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) return "";

    int depth = 1;
    size_t i = bracketStart + 1;
    while (i < json.size() && depth > 0) {
        if (json[i] == '[') depth++;
        else if (json[i] == ']') depth--;
        i++;
    }
    return json.substr(bracketStart, i - bracketStart);
}

// ============================================================================
// JSON Loading
// ============================================================================

static bool LoadFromJSON() {
    const char* jsonPath = "Data/SKSE/Plugins/PartySheet_Status.json";

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        logger::warn("PlayerStatus: Could not open {}", jsonPath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    g_factionDefs.clear();
    g_championDefs.clear();
    g_thaneDefs.clear();

    std::string factionsArray = ExtractArray(content, "factions");
    for (const auto& obj : SplitObjects(factionsArray)) {
        FactionDef def;
        def.mod = ExtractValue(obj, "mod");
        def.localFormId = ParseHexFormId(ExtractValue(obj, "formId"));
        def.key = ExtractValue(obj, "key");
        if (!def.mod.empty() && def.localFormId != 0) {
            g_factionDefs.push_back(def);
        }
    }

    std::string championsArray = ExtractArray(content, "champions");
    for (const auto& obj : SplitObjects(championsArray)) {
        ChampionDef def;
        def.mod = ExtractValue(obj, "mod");
        def.questId = ParseHexFormId(ExtractValue(obj, "questId"));
        std::string favorStagesStr = ExtractValue(obj, "favor");
        if (!favorStagesStr.empty()) {
            def.stagesFavor = ParseIntArray(favorStagesStr);
        } else {
            def.stagesFavor = ParseIntArray(ExtractValue(obj, "stages"));
        }
        std::string opposedStagesStr = ExtractValue(obj, "opposed");
        if (!opposedStagesStr.empty()) {
            def.stagesOpposed = ParseIntArray(opposedStagesStr);
        }
        def.favorConditions = ParseChampionConditions(obj, "favorConditions");
        def.opposedConditions = ParseChampionConditions(obj, "opposedConditions");
        def.name = ExtractValue(obj, "name");
        def.key = ExtractValue(obj, "key");
        if (!def.mod.empty() && def.questId != 0 &&
            (!def.stagesFavor.empty() || !def.stagesOpposed.empty() ||
             !def.favorConditions.empty() || !def.opposedConditions.empty())) {
            g_championDefs.push_back(def);
        }
    }

    std::string thaneArray = ExtractArray(content, "thane");
    for (const auto& obj : SplitObjects(thaneArray)) {
        ThaneDef def;
        def.mod = ExtractValue(obj, "mod");
        def.questId = ParseHexFormId(ExtractValue(obj, "questId"));
        std::string stagesStr = ExtractValue(obj, "stages");
        if (!stagesStr.empty()) {
            def.stages = ParseIntArray(stagesStr);
        } else {
            std::string stageStr = ExtractValue(obj, "stage");
            if (!stageStr.empty()) {
                def.stages.push_back(std::stoi(stageStr));
            }
        }
        def.hold = ExtractValue(obj, "hold");
        def.key = ExtractValue(obj, "key");
        if (!def.mod.empty() && def.questId != 0 && !def.stages.empty()) {
            g_thaneDefs.push_back(def);
        }
    }

    logger::info("PlayerStatus: Loaded {} factions, {} champions, {} thane from JSON",
                 g_factionDefs.size(), g_championDefs.size(), g_thaneDefs.size());

    return !g_factionDefs.empty();
}

// ============================================================================
// Initialization
// ============================================================================

void Initialize() {
    if (g_initialized) return;

    if (!LoadFromJSON()) {
        logger::warn("PlayerStatus: JSON load failed, no status tracking available");
    }

    g_initialized = true;
}

// ============================================================================
// Status Caching
// ============================================================================

static PlayerStatusData g_cachedStatus;
static bool g_statusCached = false;
static std::uint64_t g_cachedStateSignature = 0;

static void HashCombine(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}

static void HashCombineString(std::uint64_t& hash, const std::string& value) {
    for (unsigned char c : value) {
        HashCombine(hash, static_cast<std::uint64_t>(c));
    }
}

static std::uint64_t FloatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(float));
    return static_cast<std::uint64_t>(bits);
}

static std::uint64_t ComputeStateSignature(RE::PlayerCharacter* player) {
    std::uint64_t signature = 0xcbf29ce484222325ULL;

    for (const auto& def : g_factionDefs) {
        RE::FormID fullId = ResolveFormID(def.mod, def.localFormId);
        auto* faction = fullId ? RE::TESForm::LookupByID<RE::TESFaction>(fullId) : nullptr;
        bool isMember = faction && player->IsInFaction(faction);
        int rank = faction ? player->GetFactionRank(faction, false) : -2;
        HashCombine(signature, static_cast<std::uint64_t>(fullId));
        HashCombine(signature, static_cast<std::uint64_t>(isMember ? 1 : 0));
        HashCombine(signature, static_cast<std::uint64_t>(rank + 3));
    }

    for (const auto& def : g_championDefs) {
        RE::FormID fullId = ResolveFormID(def.mod, def.questId);
        auto* quest = fullId ? RE::TESForm::LookupByID<RE::TESQuest>(fullId) : nullptr;
        bool completed = quest ? quest->IsCompleted() : false;
        int currentStage = quest ? quest->GetCurrentStageID() : -1;
        HashCombine(signature, static_cast<std::uint64_t>(fullId));
        HashCombine(signature, static_cast<std::uint64_t>(completed ? 1 : 0));
        HashCombine(signature, static_cast<std::uint64_t>(currentStage + 1));
        for (const auto& cond : def.favorConditions) {
            auto* global = ResolveGlobalByEditorID(cond.editorId);
            HashCombineString(signature, cond.editorId);
            HashCombine(signature, FloatBits(global ? global->value : -999999.0f));
        }
        for (const auto& cond : def.opposedConditions) {
            auto* global = ResolveGlobalByEditorID(cond.editorId);
            HashCombineString(signature, cond.editorId);
            HashCombine(signature, FloatBits(global ? global->value : -999999.0f));
        }
    }

    for (const auto& def : g_thaneDefs) {
        RE::FormID fullId = ResolveFormID(def.mod, def.questId);
        HashCombine(signature, static_cast<std::uint64_t>(fullId));
        for (int stage : def.stages) {
            bool executed = WasQuestStageExecuted(def.mod, def.questId, stage);
            HashCombine(signature, static_cast<std::uint64_t>(stage + 1));
            HashCombine(signature, static_cast<std::uint64_t>(executed ? 1 : 0));
        }
    }

    return signature;
}

void InvalidateCache() {
    g_statusCached = false;
    g_cachedStateSignature = 0;
}

// ============================================================================
// Main API
// ============================================================================

PlayerStatusData GetPlayerStatus(RE::PlayerCharacter* player) {
    PlayerStatusData result;

    if (!player) return result;
    if (!g_initialized) Initialize();

    std::uint64_t currentSignature = ComputeStateSignature(player);
    if (g_statusCached && currentSignature == g_cachedStateSignature) {
        return g_cachedStatus;
    }

    // ========================================================================
    // Check Factions
    // ========================================================================
    for (const auto& def : g_factionDefs) {
        RE::FormID fullId = ResolveFormID(def.mod, def.localFormId);
        if (fullId == 0) continue;
        auto* faction = RE::TESForm::LookupByID<RE::TESFaction>(fullId);
        bool isMember = faction && player->IsInFaction(faction);
        const char* fName = faction ? faction->GetFullName() : nullptr;
        std::string dName = (fName && strlen(fName) > 0) ? fName : "";
        if (dName.empty() || dName == "Unknown" || def.key == "telvanni" || def.key == "dawnguard" || def.key == "volkihar") {
             if (def.key == "dawnguard") dName = LocalizationManager::Get("Status.Dawnguard");
             else if (def.key == "volkihar") dName = LocalizationManager::Get("Status.Volkihar");
             else if (def.key == "telvanni") dName = LocalizationManager::Get("Status.Telvanni");
             else dName = "Unknown";
        }
        if (!faction || !isMember) continue;
        FactionStatus status;
        status.isMember = true;
        status.displayName = dName;
        status.key = !def.key.empty() ? def.key : status.displayName;
        int rank = player->GetFactionRank(faction, false);
        if (rank >= 0) {
            int idx = 0;
            if (!faction->rankData.empty()) {
                for (auto* rankPtr : faction->rankData) {
                    if (idx == rank && rankPtr) {
                        const char* title = rankPtr->maleRankTitle.c_str();
                        if (title && strlen(title) > 0) {
                            status.leaderTitle = title;
                            status.isLeader = true;
                        }
                        break;
                    }
                    idx++;
                }
            }
        }
        if (def.key == "orc_friend") {
            status.leaderTitle = LocalizationManager::Get("Status.OrcKinTitle");
        }
        result.factions.push_back(status);
    }

    // ========================================================================
    // Check Champions (Daedric Quests)
    // ========================================================================
    for (const auto& def : g_championDefs) {
        RE::FormID fullId = ResolveFormID(def.mod, def.questId);
        auto* quest = fullId ? RE::TESForm::LookupByID<RE::TESQuest>(fullId) : nullptr;
        bool completed = quest ? quest->IsCompleted() : false;
        int currentStage = quest ? quest->GetCurrentStageID() : -1;
        if (DoGlobalConditionsMatch(def.opposedConditions)) {
            ChampionStatus status;
            status.displayName = def.name;
            status.leaderTitle = LocalizationManager::Get("opposed");
            status.key = !def.key.empty() ? def.key : def.name;
            status.isChampion = true;
            result.champions.push_back(status);
            goto NextChampion;
        }
        if (DoGlobalConditionsMatch(def.favorConditions)) {
            ChampionStatus status;
            status.displayName = def.name;
            status.leaderTitle = LocalizationManager::Get("champion");
            status.key = !def.key.empty() ? def.key : def.name;
            status.isChampion = true;
            result.champions.push_back(status);
            goto NextChampion;
        }
        for (int stage : def.stagesFavor) {
            if (IsQuestStageComplete(def.mod, def.questId, stage)) {
                ChampionStatus status;
                status.displayName = def.name;
                status.leaderTitle = LocalizationManager::Get("champion");
                status.key = !def.key.empty() ? def.key : def.name;
                status.isChampion = true;
                result.champions.push_back(status);
                goto NextChampion;
            }
        }
        for (int stage : def.stagesOpposed) {
            if (IsQuestStageComplete(def.mod, def.questId, stage)) {
                ChampionStatus status;
                status.displayName = def.name;
                status.leaderTitle = LocalizationManager::Get("opposed");
                status.key = !def.key.empty() ? def.key : def.name;
                status.isChampion = true;
                result.champions.push_back(status);
                goto NextChampion;
            }
        }
        NextChampion:;
    }

    // ========================================================================
    // Check Thane Status (via historic stage execution check)
    // ========================================================================
    for (const auto& def : g_thaneDefs) {
        RE::FormID fullId = ResolveFormID(def.mod, def.questId);
        auto* quest = fullId ? RE::TESForm::LookupByID<RE::TESQuest>(fullId) : nullptr;
        bool isThane = false;
        for (int stage : def.stages) {
            if (WasQuestStageExecuted(def.mod, def.questId, stage)) {
                isThane = true;
                break;
            }
        }
        bool alreadyHasHold = false;
        for (const auto& existing : result.thaneHolds) {
            if (existing.key == (!def.key.empty() ? def.key : def.hold)) {
                alreadyHasHold = true;
                break;
            }
        }
        if (alreadyHasHold) continue;
        if (isThane) {
            ThaneStatus status;
            status.displayName = LocalizationManager::Get("Status.ThanePrefix") + " " + def.hold;
            status.key = !def.key.empty() ? def.key : def.hold;
            status.isThane = true;
            result.thaneHolds.push_back(status);
        }
    }

    g_cachedStatus = result;
    g_statusCached = true;
    g_cachedStateSignature = currentSignature;

    logger::info("PlayerStatus: {} factions, {} champions, {} thane",
                 result.factions.size(), result.champions.size(), result.thaneHolds.size());

    return result;
}

TransformState GetTransformState() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return TransformState::None;
    auto* race = player->GetRace();
    if (!race) return TransformState::None;

    RE::FormID id = race->GetFormID();

    if (id == 0x000CDD84) return TransformState::Werewolf;

    static RE::TESRace* s_VampireLordRace = nullptr;
    if (!s_VampireLordRace)
        s_VampireLordRace = RE::TESForm::LookupByEditorID<RE::TESRace>("DLC1VampireBeastRace");
    if (s_VampireLordRace && race == s_VampireLordRace) return TransformState::VampireLord;

    static RE::TESRace* s_WerebearRace = nullptr;
    if (!s_WerebearRace)
        s_WerebearRace = RE::TESForm::LookupByEditorID<RE::TESRace>("DLC2WerebearBeastRace");
    if (s_WerebearRace && race == s_WerebearRace) return TransformState::Werebear;

    return TransformState::None;
}
}
