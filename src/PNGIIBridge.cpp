#include "../includes/PNGIIBridge.h"

#include "../includes/ImGui/PlayerHUDEquipment.h"
#include "../includes/ThirdParty/PNGII_API.h"

#include <chrono>
#include <unordered_map>
#include <Windows.h>

namespace PNGIIBridge
{
    namespace
    {
        const PNGII::API::InterfaceV2*              g_api = nullptr;
        PNGII::API::ResolveIconColorFunction         g_resolveColor = nullptr;
        std::unordered_map<RE::FormID, std::uint32_t> g_colors;
        std::mutex                                  g_lock;
        std::unordered_map<RE::FormID, std::string> g_paths;
        std::atomic_bool                            g_pending = false;
        std::atomic_bool                            g_sawNotReady = false;
    }

    void Init()
    {
        HMODULE dll = GetModuleHandleA("PNGIconInjector.dll");
        auto request = dll ? reinterpret_cast<PNGII::API::RequestAPIFunction>(GetProcAddress(dll, PNGII::API::kExportName)) : nullptr;
        if (auto* v3 = request ? static_cast<const PNGII::API::InterfaceV3*>(request(PNGII::API::kVersion3)) : nullptr;
            v3 && v3->version == PNGII::API::kVersion3 && v3->structSize >= sizeof(PNGII::API::InterfaceV3) && v3->resolveIcon) {
            g_api          = reinterpret_cast<const PNGII::API::InterfaceV2*>(v3);
            g_resolveColor = v3->resolveIconColor;
        } else if (auto* api = request ? static_cast<const PNGII::API::InterfaceV2*>(request(PNGII::API::kVersion2)) : nullptr;
                   api && api->version == PNGII::API::kVersion2 && api->structSize >= offsetof(PNGII::API::InterfaceV2, getIconLabel) && api->resolveIcon) {
            g_api = api;
        }
        logger::info("PNGII icon API: {}", g_resolveColor ? "v3 found, icon colors on" : g_api ? "v2 found, no icon colors" : "not available (pictograms use item art)");
    }

    void Clear()
    {
        std::scoped_lock l(g_lock);
        g_paths.clear();
        g_colors.clear();
    }

    bool Available() { return g_api != nullptr; }

    bool TryGetPath(RE::FormID a_id, std::string& a_path)
    {
        std::scoped_lock l(g_lock);
        const auto it = g_paths.find(a_id);
        if (it == g_paths.end()) return false;
        a_path = it->second;
        return true;
    }

    bool TryGetColor(RE::FormID a_id, std::uint32_t& a_rgb)
    {
        std::scoped_lock l(g_lock);
        const auto it = g_colors.find(a_id);
        if (it == g_colors.end()) return false;
        a_rgb = it->second;
        return true;
    }

    void RequestPaths(std::vector<RE::TESForm*> a_forms)
    {
        // 250 ms throttle only after a kNotReady pass: it stops the task -> dirty -> regather -> task
        static auto last = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (!g_api || a_forms.empty()) return;
        if (g_sawNotReady && now - last < std::chrono::milliseconds(250)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || g_pending.exchange(true)) return;
        last = now;
        std::vector<RE::FormID> ids;
        for (auto* f : a_forms) if (f) ids.push_back(f->GetFormID());
        tasks->AddUITask([ids = std::move(ids)]() {
            bool notReady = false;
            for (const auto id : ids) {
                // The form can be gone if a load happened between post and run.
                auto* form = RE::TESForm::LookupByID(id);
                if (!form) continue;
                char buf[260] = {};
                std::uint32_t need = 0;
                const auto r = g_api->resolveIcon(id, nullptr, -1, buf, sizeof(buf), &need);
                if (r == PNGII::API::Result::kSuccess || r == PNGII::API::Result::kNoMatch) {
                    std::scoped_lock l(g_lock);
                    g_paths[id] = r == PNGII::API::Result::kSuccess ? std::string(buf) : std::string();
                    std::uint32_t rgb = 0;
                    if (g_resolveColor && g_resolveColor(id, nullptr, -1, &rgb) == PNGII::API::Result::kSuccess) g_colors[id] = rgb;
                    else g_colors.erase(id);
                } else if (r == PNGII::API::Result::kBufferTooSmall) {
                    // 260 covers MAX_PATH; anything longer cannot be a loose Data file we can open anyway.
                    std::scoped_lock l(g_lock);
                    g_paths[id].clear();
                } else if (r == PNGII::API::Result::kNotReady) {
                    notReady = true;
                }
                // kNotReady / errors: left uncached, retried on a later gather.
            }
            g_sawNotReady = notReady;
            g_pending = false;
            PlayerHUDEquipment::MarkSlotsDirty();
        });
    }
}
