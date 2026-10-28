#pragma once

// STATUS: Working
// Player HUD Disease tracking widget.


#include <imgui.h>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

namespace PlayerHUDDisease {
    struct ActiveDisease {
        std::string name;
        int         stage;
        int         maxStage;
        ID3D11ShaderResourceView* icon;
        const char* key;
        std::string penalty;
    };

    void Init();

    std::vector<ActiveDisease> GetActiveDiseases();

    void RenderHUD();
    void RenderPreview();
    void ForceReposition();
}
