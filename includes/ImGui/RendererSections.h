#pragma once

#include <imgui.h>

class Settings;

namespace ImGuiRenderer {
    enum class CSFocusRow { None = 0, Slots = 3 };
    inline CSFocusRow g_csFocusRow = CSFocusRow::None;
}

namespace ImGuiRenderer::SettingsUI {
    void RenderHotkeysSection(Settings* settings);
    void RenderPartySheetSection(Settings* settings);
    void RenderInspectCardSection(Settings* settings, const ImVec2& displaySize);
    void RenderFollowerHUDSection(Settings* settings, const ImVec2& displaySize);
    void RenderCharacterSheetSection(Settings* settings);
    void RenderCollectiblesSection(Settings* settings);
    void RenderPinnedSection(Settings* settings);
    void RenderPlayerHUDSection(Settings* settings, const ImVec2& displaySize);
    void RenderPlayerHUDEffectsSection(Settings* settings, const ImVec2& displaySize);
    void RenderHUDVisibilitySection(Settings* settings);
    void RenderScalingSection(Settings* settings);
}
