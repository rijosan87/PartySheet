#pragma once

// STATUS: Working
// Monitors framerate and system load.


#include <chrono>
#include <unordered_map>
#include "Settings.h"

namespace PerfStats {
    inline bool g_Enabled = false;

    struct Counters {
        int   imguiRenderPasses   = 0;
        int   summonRefreshes     = 0;
        int   stableMountResolves = 0;
        int   allyStatsQueries    = 0;
        int   allyCount           = 0;
        float frameMs             = 0.0f;
        float fps                 = 0.0f;
        float fhFrameMs           = 0.0f;
        float smFrameMs           = 0.0f;
        int   csEquipmentQueries  = 0;
        int   csStatsQueries      = 0;
        int   csResistQueries     = 0;
        int   csDrawCalls         = 0;
        int   csShadowDrawCalls   = 0;
        float csFrameMs           = 0.0f;
        int   psActorStatQueries  = 0;
        float psFrameMs           = 0.0f;
        float psV2FrameMs         = 0.0f;
        int   icStatQueries       = 0;
        float icFrameMs           = 0.0f;
        float phFrameMs           = 0.0f;
        float phEquipMs           = 0.0f;
        float phResistMs          = 0.0f;
        float phResourceMs        = 0.0f;
        float phEffectsMs         = 0.0f;
        float phSurvivalMs        = 0.0f;
        float ccFrameMs           = 0.0f;
        float ptFrameMs           = 0.0f;
    };

    inline Counters g_Current;
    inline Counters g_Last;

    inline std::chrono::steady_clock::time_point g_FrameStart;

    inline std::unordered_map<const char*, double> g_ZoneMicros;
    inline std::unordered_map<const char*, double> g_ZoneMicrosLast;

    inline void BeginFrame() {
        g_Enabled = Settings::GetSingleton()->debugEnablePerformanceStats;
        if (!g_Enabled) return;
        g_Last       = g_Current;
        g_Current    = {};
        g_ZoneMicrosLast = g_ZoneMicros;
        g_ZoneMicros.clear();
        g_FrameStart = std::chrono::steady_clock::now();
    }

    inline void EndFrame() {
        if (!g_Enabled) return;
        auto now = std::chrono::steady_clock::now();
        g_Current.frameMs = std::chrono::duration<float, std::milli>(now - g_FrameStart).count();
        g_Current.fps     = g_Current.frameMs > 0.0f ? 1000.0f / g_Current.frameMs : 0.0f;
    }

    inline std::chrono::steady_clock::time_point g_CSStart;
    inline void BeginCSFrame() { if (g_Enabled) g_CSStart = std::chrono::steady_clock::now(); }
    inline void EndCSFrame()   { if (g_Enabled) g_Current.csFrameMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_CSStart).count(); }

    inline std::chrono::steady_clock::time_point g_PSStart;
    inline void BeginPSFrame() { if (g_Enabled) g_PSStart = std::chrono::steady_clock::now(); }
    inline void EndPSFrame()   { if (g_Enabled) g_Current.psFrameMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PSStart).count(); }

    inline std::chrono::steady_clock::time_point g_ICStart;
    inline void BeginICFrame() { if (g_Enabled) g_ICStart = std::chrono::steady_clock::now(); }
    inline void EndICFrame()   { if (g_Enabled) g_Current.icFrameMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_ICStart).count(); }

    inline std::chrono::steady_clock::time_point g_PHStart;
    inline void BeginPHFrame()    { if (g_Enabled) g_PHStart = std::chrono::steady_clock::now(); }
    inline void EndPHFrame()      { if (g_Enabled) g_Current.phFrameMs    = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PHStart).count(); }

    inline std::chrono::steady_clock::time_point g_PHEquipStart;
    inline void BeginPHEquip()    { if (g_Enabled) g_PHEquipStart = std::chrono::steady_clock::now(); }
    inline void EndPHEquip()      { if (g_Enabled) g_Current.phEquipMs    = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PHEquipStart).count(); }

    inline std::chrono::steady_clock::time_point g_PHResistStart;
    inline void BeginPHResist()   { if (g_Enabled) g_PHResistStart = std::chrono::steady_clock::now(); }
    inline void EndPHResist()     { if (g_Enabled) g_Current.phResistMs   = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PHResistStart).count(); }

    inline std::chrono::steady_clock::time_point g_PHResourceStart;
    inline void BeginPHResource() { if (g_Enabled) g_PHResourceStart = std::chrono::steady_clock::now(); }
    inline void EndPHResource()   { if (g_Enabled) g_Current.phResourceMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PHResourceStart).count(); }

    inline std::chrono::steady_clock::time_point g_PHEffectsStart;
    inline void BeginPHEffects()  { if (g_Enabled) g_PHEffectsStart = std::chrono::steady_clock::now(); }
    inline void EndPHEffects()    { if (g_Enabled) g_Current.phEffectsMs  = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PHEffectsStart).count(); }

    inline std::chrono::steady_clock::time_point g_CCStart;
    inline void BeginCCFrame()    { if (g_Enabled) g_CCStart = std::chrono::steady_clock::now(); }
    inline void EndCCFrame()      { if (g_Enabled) g_Current.ccFrameMs    = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_CCStart).count(); }

    inline std::chrono::steady_clock::time_point g_PTStart;
    inline void BeginPTFrame()    { if (g_Enabled) g_PTStart = std::chrono::steady_clock::now(); }
    inline void EndPTFrame()      { if (g_Enabled) g_Current.ptFrameMs    = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PTStart).count(); }

    inline std::chrono::steady_clock::time_point g_FHStart;
    inline void BeginFHFrame()    { if (g_Enabled) g_FHStart = std::chrono::steady_clock::now(); }
    inline void EndFHFrame()      { if (g_Enabled) g_Current.fhFrameMs    = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_FHStart).count(); }

    inline std::chrono::steady_clock::time_point g_SMStart;
    inline void BeginSMFrame()    { if (g_Enabled) g_SMStart = std::chrono::steady_clock::now(); }
    inline void EndSMFrame()      { if (g_Enabled) g_Current.smFrameMs    = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_SMStart).count(); }

    inline std::chrono::steady_clock::time_point g_PSV2Start;
    inline void BeginPSV2Frame()  { if (g_Enabled) g_PSV2Start = std::chrono::steady_clock::now(); }
    inline void EndPSV2Frame()    { if (g_Enabled) g_Current.psV2FrameMs  = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PSV2Start).count(); }

    inline std::chrono::steady_clock::time_point g_PHSurvivalStart;
    inline void BeginPHSurvival() { if (g_Enabled) g_PHSurvivalStart = std::chrono::steady_clock::now(); }
    inline void EndPHSurvival()   { if (g_Enabled) g_Current.phSurvivalMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_PHSurvivalStart).count(); }

    struct PerfZone {
        const char* name;
        std::chrono::steady_clock::time_point start;
        explicit PerfZone(const char* n) : name(n) {
            if (g_Enabled) start = std::chrono::steady_clock::now();
        }
        ~PerfZone() {
            if (!g_Enabled) return;
            auto us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - start).count();
            g_ZoneMicros[name] += us;
        }
        PerfZone(const PerfZone&) = delete;
        PerfZone& operator=(const PerfZone&) = delete;
    };
}
