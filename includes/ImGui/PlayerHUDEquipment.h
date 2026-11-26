#pragma once

#include "../Utils/EffectAttribution.h"

// STATUS: Working
// Player HUD Equipment widget.

namespace PlayerHUDEquipment {
    void Init();
    bool Render();
    void RenderContent(bool allowDrag = false);
    void ForceReposition();
    void MarkSlotsDirty();
    bool ShowsAsBadge(const EffectAttribution::GroupKey& key);
    void NotifyShoutPressed();
}
