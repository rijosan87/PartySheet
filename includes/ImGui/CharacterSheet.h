#pragma once

// STATUS: Working
// ImGui character stats sheet renderer.


#include <imgui.h>
#include <vector>
#include <string>
#include <RE/Skyrim.h>
#include "../ActorStats.h"

struct EquipSlot {
    std::string slotName;
    std::string itemName;
    std::string iconName;
    float baseValue;
    float skillBonus;
    float fortifyBonus;
    float perkBonus;
    float enchantDamage;
    std::string enchantType;
    std::string skillIconName;
    float weaponSpeed;
    float stagger;
    bool is2H;
    bool hasItem;
    RE::TESForm* itemForm;
    RE::SpellItem* spellItem;
};

struct PlayerCombatStats {
    float armorRating;
    float weaponDamage;
    float baseWeaponDamage;
    float weaponSpeed;
    float weaponReach;
    float unarmedDamage;
    float speedMult;
    float damageResistPct;
    float inventoryWeight;
    float carryWeightMax;
    int gold;
    float healthRegen;
    float magickaRegen;
    float staminaRegen;
    float shoutRecovery;
    int dragonSouls;
    int perkPoints;
    ActorSkills skills;
};

struct PlayerResistances {
    float fire;
    float frost;
    float shock;
    float poison;
    float magic;
    float disease;
};

class CharacterSheet
{
public:
    static CharacterSheet* GetSingleton()
    {
        static CharacterSheet singleton;
        return &singleton;
    }
    void Render();
    void DrawUI();
    void Toggle();
    void Open();
    void Close();
    bool IsVisible() const { return _visible; }
    std::vector<EquipSlot> GetPlayerEquipment();
    PlayerCombatStats GetPlayerCombatStats();
    PlayerResistances GetPlayerResistances();
private:
    CharacterSheet() = default;
    ~CharacterSheet() = default;
    CharacterSheet(const CharacterSheet&) = delete;
    CharacterSheet& operator=(const CharacterSheet&) = delete;
    bool _visible = false;
    float _savedZoomOffset = 0.0f;
    float _savedPosOffsetX = 0.0f;
    float _savedPosOffsetZ = 0.0f;
    bool _wasThirdPerson = true;
    void RenderLeftPanel(float x, float y, float width, float height);
    void RenderRightPanel(float x, float y, float width, float height);
    void RenderBottomBar(float x, float y, float width, float height);
    void SaveCameraState();
    void RestoreCameraState();
    void SetupPlayerView();
};
