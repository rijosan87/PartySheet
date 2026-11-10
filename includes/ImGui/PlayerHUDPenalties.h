#pragma once

// STATUS: Working
// Player HUD debuffs display.

#include <RE/Skyrim.h>
#include <imgui.h>

namespace PlayerHUDPenalties {
    struct SurvivalData {
        float health  = 0.0f;
        float magicka = 0.0f;
        float stamina = 0.0f;
    };
    SurvivalData GetSurvivalPenalties();

    struct TemporaryData {
        float health  = 0.0f;
        float magicka = 0.0f;
        float stamina = 0.0f;
    };
    TemporaryData GetTemporaryPenalties(RE::Actor* player);

    bool DrawBarSlice(ImDrawList* dl, float x, float y, float barW, float barH,
                      float u0, float u1, ImU32 tint, void* barFillTex);

}
