#pragma once

// STATUS: Working
// Helper for drawing customized icons.

#include <imgui.h>
#include <d3d11.h>

namespace IconRenderer {
    struct Style {
        ID3D11ShaderResourceView* texture = nullptr;
        float size = 48.0f;
        ImU32 bgColor = IM_COL32(0, 0, 0, 180);
        bool glowEnabled = false;
        bool chip = false;
        ImU32 borderColor  = IM_COL32(255, 255, 255, 255);
        int   borderWidth  = 0;
        float borderRadius = 0.0f;
        ImVec2 uv0 = ImVec2(0.0f, 0.0f);
        ImVec2 uv1 = ImVec2(1.0f, 1.0f);
        float bounceOffsetY = 0.0f;
    };

    inline void SetZoom(Style& style, float zoom, float focusU, float focusV)
    {
        float half = 0.5f / zoom;
        style.uv0 = ImVec2(focusU - half, focusV - half);
        style.uv1 = ImVec2(focusU + half, focusV + half);
    }

    void Draw(ImDrawList* dl, ImVec2 screenPos, const Style& style);
}
