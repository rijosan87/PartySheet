#pragma once

// STATUS: Working
// Helper code for collectibles widget.


#include <string>
#include <vector>
#include <RE/Skyrim.h>
#include <d3d11.h>

namespace WidgetCollectibles {
    struct CollectibleItemRef {
        std::string mod;
        RE::FormID  localFormID   = 0;
        RE::FormID  resolvedFormID = 0;
    };

    struct CollectibleEntry {
        std::string id;
        std::string label;
        int         goal = 0;
        std::vector<CollectibleItemRef> items;
        std::string questMod;
        RE::FormID  questLocalID    = 0;
        RE::FormID  questResolvedID = 0;
        bool        enabled         = true;
        ID3D11ShaderResourceView* icon = nullptr;
    };

    extern std::vector<CollectibleEntry> g_Collectibles;
    extern std::vector<int>              g_CachedCounts;
    extern bool                          g_ForceCollectiblesPos;
    extern bool                          g_Loaded;

    bool IsQuestComplete(const CollectibleEntry& e);
}
