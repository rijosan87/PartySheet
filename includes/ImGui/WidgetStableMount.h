#pragma once

#include <imgui.h>

class Settings;

namespace WidgetStableMount {
    void Init();
    void RenderSettingsPanel(Settings* settings, const ImVec2& displaySize);
    void RenderPreview();
    void RenderHUD();
    void ForceReposition();

    bool IsImmersiveHorsesActive();
}
