#pragma once

// STATUS: Working
// ImGui fullscreen Party Sheet dashboard.


#include <RE/Skyrim.h>

namespace PartySheetV2 {
    void Toggle();

    bool IsVisible();

    void Render();

    void PrevSubject();
    void NextSubject();
}
