#pragma once

// STATUS: Working
// Player HUD Equipment widget.


namespace PlayerHUDEquipment {
    void Init();
    bool Render();
    void RenderContent(bool allowDrag = false);
    void ForceReposition();
    void MarkSlotsDirty();
    void NotifyShoutPressed();
}
