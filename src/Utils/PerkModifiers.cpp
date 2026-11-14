#include "../../includes/Utils/PerkModifiers.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace PerkModifiers {

    namespace {
        using Clock = std::chrono::steady_clock;

        constexpr auto kRefreshInterval = std::chrono::milliseconds(1000);

        struct Entry {
            HitEstimate       est{};
            Clock::time_point requested{};
            bool              pending = false;
        };

        std::mutex                               g_lock;
        std::unordered_map<std::uint64_t, Entry> g_cache;

        std::uint64_t MakeKey(RE::FormID actorID, RE::FormID weaponID) {
            return (static_cast<std::uint64_t>(actorID) << 32) | weaponID;
        }

        void Refresh(RE::ActorHandle handle, RE::FormID weaponID, std::uint64_t key) {
            HitEstimate est{};
            auto  actorPtr = handle.get();
            auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(weaponID);
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (actorPtr && weapon && player && actorPtr->Is3DLoaded()) {
                RE::BGSEntryPoint::HandleEntryPoint(
                    RE::BGSEntryPoint::ENTRY_POINT::kModAttackDamage,
                    actorPtr.get(), weapon, static_cast<RE::Actor*>(player), &est.perkMult);

                auto* entry = actorPtr->GetEquippedEntryData(false);
                if (weapon->IsMelee() && entry && entry->object == weapon) {
                    if (auto* hit = RE::HitData::Create(actorPtr.get(), player, entry, false)) {
                        est.physicalDamage = hit->physicalDamage;
                        hit->attackData.reset();
                        RE::free(hit);
                    }
                }
            }

            std::lock_guard lk(g_lock);
            auto& e = g_cache[key];
            e.est = est;
            e.pending = false;
        }
    }

    HitEstimate GetHitEstimate(RE::Actor* actor, RE::TESObjectWEAP* weapon) {
        if (!actor || !weapon) return {};

        const auto key = MakeKey(actor->GetFormID(), weapon->GetFormID());
        const auto now = Clock::now();

        HitEstimate est{};
        bool        queue = false;
        {
            std::lock_guard lk(g_lock);
            auto& e = g_cache[key];
            est = e.est;
            if (!e.pending && now - e.requested >= kRefreshInterval) {
                e.pending = true;
                e.requested = now;
                queue = true;
            }
        }

        if (queue) {
            auto* tasks = SKSE::GetTaskInterface();
            if (tasks) {
                tasks->AddTask([handle = actor->GetHandle(), weaponID = weapon->GetFormID(), key]() {
                    Refresh(handle, weaponID, key);
                });
            } else {
                std::lock_guard lk(g_lock);
                g_cache[key].pending = false;
            }
        }
        return est;
    }
}
