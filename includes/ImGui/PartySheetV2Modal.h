#pragma once

// STATUS: Working
// Fullscreen Party Sheet layout.


#include <RE/Skyrim.h>

namespace PartySheetV2 {
    void OpenEffectsModal();
    void CloseEffectsModal();
    bool IsEffectsModalOpen();

    void RenderEffectsModal(RE::Actor* actor);

    void OpenInventoryModal();
    void CloseInventoryModal();
    bool IsInventoryModalOpen();
    void RenderInventoryModal(RE::Actor* actor);
}
