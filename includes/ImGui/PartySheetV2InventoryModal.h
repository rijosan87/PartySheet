#pragma once

// STATUS: Working
// Party Sheet companion inventory viewer.

#include <RE/Skyrim.h>

#include "../Utils/GearCatalog.h"

namespace PartySheetV2 {
    void OpenInventoryModal();
    void OpenInventoryModal(GearCatalog::Card card);
    void CloseInventoryModal();
    bool IsInventoryModalOpen();
    void RenderInventoryModal(RE::Actor* actor);

    void DrawInventoryItemTooltip(RE::Actor* actor, RE::TESForm* form, const char* name,
                                  RE::EnchantmentItem* enchant, const char* typeLabel, void* icon, float sc);
}
