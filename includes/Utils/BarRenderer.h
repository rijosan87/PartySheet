#pragma once

// STATUS: Working
// Draws simple health/magicka progress bars.

#include <imgui.h>
#include <d3d11.h>
#include <cstdint>

namespace BarRenderer {
    struct Style {
        float width         = 120.0f;
        float height        = 10.0f;
        float fillFraction  = 1.0f;
        ID3D11ShaderResourceView* bgTex   = nullptr;
        ID3D11ShaderResourceView* fillTex = nullptr;
        ImU32 fillColor = IM_COL32(255, 255, 255, 255);
        const char* text          = nullptr;
        ImU32       textColor     = IM_COL32(255, 255, 255, 255);
        float       textFontScale = 1.0f;
        ImFont*     textFont      = nullptr;
        bool verticalFill = false;
        float alpha = 1.0f;
        float ghostFraction = 0.0f;
        ImU32 ghostColor    = IM_COL32(0, 0, 0, 150);
    };

    float TickGhost(std::uint64_t id, float fraction);

    bool Draw(ImDrawList* dl, ImVec2 screenPos, const Style& style);
}
