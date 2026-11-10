#pragma once

// STATUS: Working
// Fullscreen Party Sheet layout.

#include <RE/Skyrim.h>

namespace PartySheetV2 {
    void OpenEffectsModal();
    void CloseEffectsModal();
    bool IsEffectsModalOpen();

    void RenderEffectsModal(RE::Actor* actor);

    void DrawRaceTooltip(RE::Actor* actor, float scale);

    RE::SpellItem* GetStandingStone(RE::Actor* actor);

    void DrawPassiveTooltip(RE::Actor* actor, const std::string& title, float scale, bool stoneOnly);

    bool HasStatPassives(RE::Actor* actor, RE::ActorValue pool);
    void DrawStatTooltip(RE::Actor* actor, const std::string& title, RE::ActorValue pool, float scale);

    struct CurseInfo {
        enum class Kind { None, Vampire, Werewolf };
        Kind kind  = Kind::None;
        int  stage = 0;
    };
    CurseInfo GetCurse(RE::Actor* actor);
    void DrawCurseTooltip(RE::Actor* actor, const std::string& title, float scale);

    void OpenInventoryModal();
    void CloseInventoryModal();
    bool IsInventoryModalOpen();
    void RenderInventoryModal(RE::Actor* actor);
}
