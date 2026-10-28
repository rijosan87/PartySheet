#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <vector>

namespace FollowerHUD {
    void Init();
    
    bool Render();
    
    void RenderContent(bool allowDrag = false);
    
    void ResetFade();

    void ForceReposition();

    // Call right after the Grow Upward checkbox flips: converts saved Y between
    void OnGrowUpwardToggled(bool summonWindow = false);

    bool ShouldHideSummonEffect(RE::FormID ownerID, std::uintptr_t effectInstance, std::uint16_t uniqueID,
                                RE::FormID baseEffectID, RE::FormID spellID);
    
    bool ShouldShow();
    
    std::vector<RE::Actor*> GetAllies();
    
    // ====== Cache Management (Event-Based Scanning) ======
    void MarkSummonsDirty();
    void MarkQuestAlliesDirty();
    
    void MarkCacheDirty();
    
    void RemoveFromCache(RE::Actor* actor);
    
    void RefreshCache();
    
    std::vector<RE::Actor*> GetCachedAllies();
    
    bool IsActorValidForRender(RE::Actor* actor);
}
