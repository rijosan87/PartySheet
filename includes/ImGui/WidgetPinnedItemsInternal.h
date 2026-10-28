#pragma once

// STATUS: Working
// Helper code for pinned items widget.


#include <string>
#include <vector>
#include <RE/Skyrim.h>
#include <d3d11.h>

namespace WidgetPinnedItems {
    struct PinnedItemRef {
        std::string mod;
        RE::FormID  localFormID   = 0;
        RE::FormID  resolvedFormID = 0;
    };

    struct PinnedEntry {
        std::string id;
        std::string label;
        int         goal = 0;
        std::vector<PinnedItemRef> items;
        bool        enabled = true;
        ID3D11ShaderResourceView* icon = nullptr;
    };
    // Shared globals
    extern std::vector<PinnedEntry> g_PinnedItems;
    extern std::vector<int>         g_CachedCounts;
    extern bool                     g_ForcePinnedPos;
    extern bool                     g_Loaded;

    void RefreshPotionSlots();

    std::string MatchPotionSlotId(RE::AlchemyItem* alch);
}
