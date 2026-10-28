#include "../includes/PNGIIBridge.h"

#include "../includes/ImGui/PlayerHUDEquipment.h"
#include "../includes/TextureManager.h"
#include "../includes/ThirdParty/PNGII_API.h"

#include <chrono>
#include <unordered_map>
#include <Windows.h>

namespace PNGIIBridge
{
    namespace
    {
        const PNGII::API::InterfaceV1*              g_api = nullptr;
        std::mutex                                  g_lock;
        std::unordered_map<RE::FormID, std::string> g_paths;
        std::atomic_bool                            g_pending = false;
        std::atomic_bool                            g_sawNotReady = false;

        // SkyUI label names so PNGII's label rules (00_SkyUI-Labels.json) match outside menus.
        const char* LabelFor(RE::TESForm* a_form)
        {
            if (auto* w = a_form->As<RE::TESObjectWEAP>()) {
                switch (w->GetWeaponType()) {
                case RE::WEAPON_TYPE::kOneHandSword:  return "weapon_sword";
                case RE::WEAPON_TYPE::kOneHandDagger: return "weapon_dagger";
                case RE::WEAPON_TYPE::kOneHandAxe:    return "weapon_waraxe";
                case RE::WEAPON_TYPE::kOneHandMace:   return "weapon_mace";
                case RE::WEAPON_TYPE::kTwoHandSword:  return "weapon_greatsword";
                case RE::WEAPON_TYPE::kTwoHandAxe:    return TextureManager::IsWarhammer(w) ? "weapon_hammer" : "weapon_battleaxe";
                case RE::WEAPON_TYPE::kBow:           return "weapon_bow";
                case RE::WEAPON_TYPE::kCrossbow:      return "weapon_crossbow";
                case RE::WEAPON_TYPE::kStaff:         return "weapon_staff";
                default:                              return "default_weapon";
                }
            }
            if (auto* a = a_form->As<RE::TESAmmo>()) return TextureManager::IsBoltAmmo(a) ? "weapon_bolt" : "weapon_arrow";
            if (a_form->As<RE::TESObjectLIGH>()) return "misc_torch";
            // costliest effect; Destruction splits by resistVariable; school-less -> default_power.
            if (a_form->As<RE::TESShout>()) return "default_shout";
            if (a_form->As<RE::ScrollItem>()) return "default_scroll";
            if (auto* sp = a_form->As<RE::SpellItem>()) {
                const auto* eff = sp->GetCostliestEffectItem();
                const auto* mgef = eff ? eff->baseEffect : nullptr;
                if (!mgef) return "default_power";
                switch (mgef->GetMagickSkill()) {
                case RE::ActorValue::kAlteration:  return "default_alteration";
                case RE::ActorValue::kConjuration: return "default_conjuration";
                case RE::ActorValue::kIllusion:    return "default_illusion";
                case RE::ActorValue::kRestoration: return "default_restoration";
                case RE::ActorValue::kDestruction:
                    switch (mgef->data.resistVariable) {
                    case RE::ActorValue::kResistFire:  return "magic_fire";
                    case RE::ActorValue::kResistFrost: return "magic_frost";
                    case RE::ActorValue::kResistShock: return "magic_shock";
                    default:                           return "default_destruction";
                    }
                default: return "default_power";
                }
            }
            if (auto* arm = a_form->As<RE::TESObjectARMO>(); arm && arm->IsShield()) {
                return arm->IsHeavyArmor() ? "armor_shield" : arm->IsLightArmor() ? "lightarmor_shield" : "clothing_shield";
            }
            return nullptr;
        }
    }

    void Init()
    {
        HMODULE dll = GetModuleHandleA("PNGIconInjector.dll");
        auto request = dll ? reinterpret_cast<PNGII::API::RequestAPIFunction>(GetProcAddress(dll, PNGII::API::kExportName)) : nullptr;
        auto* api = request ? static_cast<const PNGII::API::InterfaceV1*>(request(PNGII::API::kVersion1)) : nullptr;
        if (api && api->version == PNGII::API::kVersion1 && api->structSize >= sizeof(PNGII::API::InterfaceV1) && api->resolveIcon) {
            g_api = api;
        }
        logger::info("PNGII icon API: {}", g_api ? "v1 found" : "not available (pictograms use item art)");
    }

    void Clear()
    {
        std::scoped_lock l(g_lock);
        g_paths.clear();
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
                const auto r = g_api->resolveIcon(id, LabelFor(form), -1, buf, sizeof(buf), &need);
                if (r == PNGII::API::Result::kSuccess || r == PNGII::API::Result::kNoMatch) {
                    std::scoped_lock l(g_lock);
                    g_paths[id] = r == PNGII::API::Result::kSuccess ? std::string(buf) : std::string();
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
