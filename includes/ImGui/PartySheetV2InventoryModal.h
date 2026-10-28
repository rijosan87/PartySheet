#pragma once

// STATUS: Working
// Party Sheet companion inventory viewer.


#include <RE/Skyrim.h>

namespace PartySheetV2 {
    void OpenInventoryModal();
    void CloseInventoryModal();
    bool IsInventoryModalOpen();
    void RenderInventoryModal(RE::Actor* actor);
}
