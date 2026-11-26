#pragma once

#include <string>
#include <vector>

// Optional consumer of PNG Icon Injector's PNGII_RequestAPI (v1). No link-time dependency:
namespace PNGIIBridge
{
    // kPostLoad: every DLL is loaded by then.
    void Init();
    // kPreLoadGame / kPostLoadGame / kNewGame: PNGII rebuilds its rules cache at load.
    void Clear();
    bool Available();
    // Render thread. Returns true and fills a_path (Data-relative, "" = no match) when cached.
    bool TryGetPath(RE::FormID a_id, std::string& a_path);
    bool TryGetColor(RE::FormID a_id, std::uint32_t& a_rgb);
    // Render thread. Queues one UI-thread resolve for forms not yet cached (throttled, one in flight).
    void RequestPaths(std::vector<RE::TESForm*> a_forms);
}
