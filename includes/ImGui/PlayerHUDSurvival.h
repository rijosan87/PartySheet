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
        float        fill     = -1.0f;
        float        nextFill = -1.0f;
    };

    std::vector<NeedStatus> GetActiveNeeds();

    struct AmbientCold {
        int   stage = -1;
        int   trend = 0;
        bool  hasTemperature = false;
        float temperature = 0.0f;
        bool  nearFire  = false;
        bool  sheltered = false;
        int   heatSize  = 0;
        int   shelterLevel = 0;
        float selfFill = -1.0f;
        float areaFill = -1.0f;
        float fireFill = 0.0f;
        float stageStart = -1.0f, stageEnd = -1.0f;
        int   selfStage = -1;
        bool  inFreezingWater = false;
        bool  coldHeld = false;
    };
    std::optional<AmbientCold> GetAmbientCold();

    std::string GetColdStageName(int stage);

    bool IsNeedSpell(RE::SpellItem* spell);

    struct VitalityStatus {
        std::string name;
        std::string tierName;
        int  tier     = 4;
        int  maxTier  = 7;
        ID3D11ShaderResourceView* icon = nullptr;
        int  trend    = 0;
        float fill    = -1.0f;
    };

    std::optional<VitalityStatus> GetVitality();

    bool IsVitalitySpell(RE::SpellItem* spell);

    ID3D11ShaderResourceView* GetNeedSpellIcon(RE::SpellItem* spell);

    std::string GetStackWarning();

    void RenderHUD();
    void RenderPreview();
    void ForceReposition();
}
