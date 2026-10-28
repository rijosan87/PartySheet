#pragma once

// STATUS: Working
// Detailed follower status pop-out.


#include <RE/Skyrim.h>

namespace PartySheetV2 {
    void OpenStatusModal();
    void CloseStatusModal();
    bool IsStatusModalOpen();

    void RenderStatusModal(RE::Actor* actor);
}
