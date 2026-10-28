#pragma once

// STATUS: Working
// Draw utilities for widgets.

#include <imgui.h>
#include <algorithm>

namespace WidgetDraw {
    inline void DrawIconBadge(ImDrawList* dl, float screenX, float screenY, float sz, ImU32 tintCol,
                              bool glowEnabled = true, int borderWidth = 0, ImU32 borderCol = IM_COL32(0,0,0,255),
                              float borderRadius = -1.0f)
    {
        int r = (tintCol >>  0) & 0xFF;
        int g = (tintCol >>  8) & 0xFF;
        int b = (tintCol >> 16) & 0xFF;
        int a = (tintCol >> 24) & 0xFF;
        ImVec2 iMin = ImVec2(screenX, screenY);
        ImVec2 iMax = ImVec2(screenX + sz, screenY + sz);
        float  radius = (borderRadius >= 0.0f) ? borderRadius : sz * 0.18f;
        dl->AddRectFilled(iMin, iMax, IM_COL32(r, g, b, a), radius);
        if (glowEnabled) {
            constexpr int kLayers = 3;
            for (int i = kLayers; i >= 1; --i) {
                float pad   = i * 2.5f;
                int   alpha = 25 + (kLayers - i) * 30;
                dl->AddRect(
                    ImVec2(iMin.x - pad, iMin.y - pad),
                    ImVec2(iMax.x + pad, iMax.y + pad),
                    IM_COL32(r, g, b, alpha), radius + pad * 0.4f, 0, 1.5f);
            }
            dl->AddRect(iMin, iMax, IM_COL32(r, g, b, 200), radius, 0, 1.5f);
        }
        if (borderWidth > 0) {
            dl->AddRect(iMin, iMax, borderCol, radius, 0, static_cast<float>(borderWidth));
        }
    }

    inline void DrawCountBadge(ImFont* headingFont, float fontSize,
                               float screenX, float screenY,
                               const char* text, ImU32 bgColor)
    {
        if (!headingFont || !text || !text[0]) return;
        ImVec2 textSz = headingFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        float  padBX  = std::max(5.0f, fontSize * 0.35f);
        float  padBY  = std::max(2.0f, fontSize * 0.18f);
        float  pillW  = textSz.x + padBX * 2.0f;
        float  pillH  = textSz.y + padBY * 2.0f;
        float  radius = pillH * 0.45f;
        ImVec2 pMin = ImVec2(screenX, screenY);
        ImVec2 pMax = ImVec2(screenX + pillW, screenY + pillH);
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        ImU32 bgAlpha = (bgColor >> 24) & 0xFF;
        fdl->AddRectFilled(ImVec2(pMin.x + 1, pMin.y + 1), ImVec2(pMax.x + 1, pMax.y + 1),
            IM_COL32(0, 0, 0, (ImU32)(120 * bgAlpha / 255)), radius);
        fdl->AddRectFilled(pMin, pMax, bgColor, radius);
        ImVec2 tPos = ImVec2(pMin.x + padBX, pMin.y + padBY);
        fdl->AddText(headingFont, fontSize, ImVec2(tPos.x + 1, tPos.y + 1), IM_COL32(0, 0, 0, 140), text);
        fdl->AddText(headingFont, fontSize, tPos, IM_COL32(255, 255, 255, 255), text);
    }

}
