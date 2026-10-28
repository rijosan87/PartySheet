#pragma once

// STATUS: Working
// Tracks current player vitals and status.

#include <RE/Skyrim.h>
#include <string>
#include <vector>

namespace PlayerStatus {
// ============================================================================
// Structs for player status information
// ============================================================================

struct FactionStatus {
    std::string key;
    std::string displayName;
    bool isMember = false;
    bool isLeader = false;
    std::string leaderTitle;
};

struct ThaneStatus {
    std::string key;
    std::string displayName;
    bool isThane = false;
};

struct ChampionStatus {
    std::string key;
    std::string displayName;
    std::string artifact;
    bool isChampion = false;
    std::string leaderTitle;
};

struct PlayerStatusData {
    std::vector<FactionStatus> factions;
    std::vector<ThaneStatus> thaneHolds;
    std::vector<ChampionStatus> champions;
};

// ============================================================================
// API Functions
// ============================================================================

void Initialize();

PlayerStatusData GetPlayerStatus(RE::PlayerCharacter* player);

void InvalidateCache();

RE::FormID ResolveFormID(const std::string& modName, RE::FormID localId);

bool IsPlayerInFaction(RE::Actor* player, const std::string& mod, RE::FormID localId);

bool IsQuestStageComplete(const std::string& mod, RE::FormID localId, int stage, bool requireCompleted = true);

enum class TransformState { None, Werewolf, VampireLord, Werebear };
TransformState GetTransformState();
}
