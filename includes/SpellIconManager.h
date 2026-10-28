#pragma once

// STATUS: Working
// Loads and caches spell icons.


#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <RE/Skyrim.h>

namespace SpellIconManager {
    void Init(ID3D11Device* device);

    void LoadConfig();

    ID3D11ShaderResourceView* GetSpellIcon(RE::SpellItem* spell);

    std::string GetSpellSchool(RE::SpellItem* spell);
}
