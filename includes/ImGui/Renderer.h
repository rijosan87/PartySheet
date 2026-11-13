#pragma once

#include <imgui.h>
#include "../BackgroundManager.h"

namespace ImGuiRenderer {
    void Install();

    void Shutdown();

    bool IsVisible();

    void Toggle();

    void AddScrollDelta(float delta);

    void FlushScrollDelta();

    float ConsumeScrollDelta();

    bool MsgHookFiredThisFrame();

    float GetHeadingFontSize();

    void RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type);

    void DrawSkyrimCursor();

    // Renders ImDrawData to the currently bound DirectX 11 render target,
    void RenderToBoundTarget(ImDrawData* drawData);

}
