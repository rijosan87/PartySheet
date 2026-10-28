#pragma once

#include <string>

namespace WidgetPinnedItems {
    void Init();
    void RenderPreview();
    void RenderHUD();
    void ForceReposition();

    // What hotkey press actually did. The caller decides
    enum class ConsumeResult {
        kNone,
        kDrankVanilla,
        kDrankAnimated,
        kDeclined,
        kBlocked,
    };

    // Consume the best owned potion in the entry
    ConsumeResult TryConsume(const std::string& entryId);
}
