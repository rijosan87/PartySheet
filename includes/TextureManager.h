#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>

namespace TextureManager {
    void Init(ID3D11Device* device);
    
    void Shutdown();
    
    ID3D11ShaderResourceView* GetTexture(const std::string& path);
    
    ID3D11ShaderResourceView* GetPortrait(RE::Actor* actor);
    
    ID3D11ShaderResourceView* GetEffectIcon(const std::string& iconId);
    
    ID3D11ShaderResourceView* GetPartyIcon(const std::string& iconName);

    ID3D11ShaderResourceView* GetFrameTexture(const std::string& name);
    
    ID3D11ShaderResourceView* GetItemIcon(RE::TESForm* item, const std::string& slotType);

    // Shared by GetItemIcon P5 and the PNGII label map so both classify the same way.
    bool IsWarhammer(RE::TESObjectWEAP* weapon);
    // Name contains "bolt" OR kNonBolt unset. Never IsBolt(): its flag logic is inverted.
    bool IsBoltAmmo(RE::TESAmmo* ammo);

    
    void PreloadPortraits();
    void PreloadEffectIcons();
    
    struct CacheStats {
        int    totalCached;
        int    successfulLoads;
        int    failedLoads;
        size_t totalVramBytes;
    };
    CacheStats GetCacheStats();
}
