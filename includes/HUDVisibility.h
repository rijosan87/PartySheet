#pragma once

// STATUS: Working
// Manages dynamic widget fade-out states.

#include <chrono>

namespace HUDVisibility {
    enum class WidgetID : int {
        PlayerEquip     = 0,
        PlayerEffects   = 1,
        PlayerResist    = 2,
        PlayerResources = 3,
        Collectibles    = 4,
        PinnedItems     = 5,
        StableMount     = 6,
        PlayerLocation  = 7,
        Count           = 8
    };

    void Update(float deltaTime);
    void Trigger(WidgetID id);
    void BeginFadeOut(WidgetID id);
    bool ConsumeEntrancePopSuppress(WidgetID id);
    bool IsVisible(WidgetID id);
    float GetAlpha(WidgetID id);
    void ApplyAlphaToDrawData(float alpha);
    void ApplyOffsetToDrawData(float dx, float dy);
    void ApplyVanillaBarVisibility();
}
