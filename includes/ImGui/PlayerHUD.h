#pragma once

// STATUS: Working
// ImGui player health, magicka, and resource bars.


namespace PlayerHUD {
    void Init();
    bool Render();
    void RenderContent(bool allowDrag = false);
    bool ShouldShow();
    void ForceReposition();
}
