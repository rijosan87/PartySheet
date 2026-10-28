#pragma once

// STATUS: Working
// ImGui team overview UI.


#include <RE/Skyrim.h>

namespace PartySheet {
    void Init();

    void Toggle();

    void Show();

    bool IsVisible();

    void PrevPage();
    void NextPage();

    void Render();
}
