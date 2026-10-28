#pragma once

// STATUS: Working
// ImGui card for target character details.


#include <RE/Skyrim.h>

namespace InspectCard {
    void Init();
    void Toggle();
    void Show(RE::Actor* actor);
    void Hide();
    bool IsVisible();
    void Render();
    void RenderContent();

    RE::Actor* GetTargetActor();
    void SetTargetActor(RE::Actor* actor);
}
