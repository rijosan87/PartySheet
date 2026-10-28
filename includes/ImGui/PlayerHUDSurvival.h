#pragma once

// STATUS: Working
// Player HUD Survival stats widget.


#include <imgui.h>
#include <optional>
#include <vector>
#include <string>

namespace RE { class SpellItem; }
struct ID3D11ShaderResourceView;

namespace PlayerHUDSurvival {
    void Init();

    bool IsAvailable();

    int GetColdStage();
    int GetHungerStage();
    int GetFatigueStage();
    int GetThirstStage();

    struct NeedStatus {
        std::string  needName;
        std::string  stageName;
        int          stage    = 0;
        int          maxStage = 5;
        ID3D11ShaderResourceView* icon = nullptr;
        const char*  key      = "";
    };

    std::vector<NeedStatus> GetActiveNeeds();

    bool IsNeedSpell(RE::SpellItem* spell);

    struct VitalityStatus {
        std::string name;
        std::string tierName;
        int  tier     = 4;
        int  maxTier  = 7;
        ID3D11ShaderResourceView* icon = nullptr;
    };

    std::optional<VitalityStatus> GetVitality();

    bool IsVitalitySpell(RE::SpellItem* spell);

    void RenderHUD();
    void RenderPreview();
    void ForceReposition();
}
