#pragma once

// STATUS: Working
// Party Sheet companion inventory viewer.

#include <RE/Skyrim.h>
#include <imgui.h>

#include "../Utils/GearCatalog.h"

namespace PartySheetV2 {
    void OpenInventoryModal();
    void OpenInventoryModal(GearCatalog::Card card);
    void OpenSpellPicker(GearCatalog::Card hand);
    void OpenSpellWindow();
    bool IsSpellWindowOpen();
    void CloseInventoryModal();
    bool IsInventoryModalOpen();
    void RenderInventoryModal(RE::Actor* actor);

    void DrawInventoryItemTooltip(RE::Actor* actor, RE::TESForm* form, const char* name,
                                  RE::EnchantmentItem* enchant, const char* typeLabel, void* icon, float sc,
                                  RE::FormID poisonID = 0, int poisonDoses = 0, std::uint32_t iconTint = 0xFFFFFFFF, bool quest = false);

    void DrawPoisonMark(ImDrawList* dl, ImVec2 topRight, int doses, float sc);

    void DrawSpellPickerBody(RE::Actor* actor, GearCatalog::Card hand, float sc, ImVec2 wp, ImVec2 ws);
    void ResetSpellPicker();
    int  KnownSpellCount(RE::Actor* actor);
}
