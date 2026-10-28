#pragma once

// STATUS: Working
// Manages widget custom theme backgrounds.


#include <d3d11.h>
#include <string>
#include <vector>

namespace BackgroundManager {
    enum class Type {
        Partysheet,
        Partycard,
        Inspectcard,
        CharacterSheet,
        BarStyle,
        PortraitFrame,
        PlayerPortrait
    };

    struct BackgroundEntry {
        std::string filename;
        ID3D11ShaderResourceView* texture;
    };

    void Init();

    ID3D11ShaderResourceView* GetBackground(Type type, int index);

    ID3D11ShaderResourceView* GetBarFillTexture(int index);

    const std::vector<std::string>& GetBackgrounds(Type type);

    int GetBackgroundCount(Type type);

    const std::string& GetBackgroundName(Type type, int index);

    std::vector<BackgroundEntry> GetBackgroundEntries(Type type);
}
