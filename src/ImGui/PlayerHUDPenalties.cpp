#include "../../includes/ImGui/PlayerHUDPenalties.h"
#include <SKSE/SKSE.h>
#include <algorithm>

namespace PlayerHUDPenalties {

    SurvivalData GetSurvivalPenalties()
    {
        static RE::TESGlobal* s_enabled  = nullptr;
        static RE::TESGlobal* s_health   = nullptr;
        static RE::TESGlobal* s_magicka  = nullptr;
        static RE::TESGlobal* s_stamina  = nullptr;
        static bool           s_resolved = false;

        if (!s_resolved) {
            auto* dh = RE::TESDataHandler::GetSingleton();
            if (dh) {
                s_resolved = true;
                s_enabled  = dh->LookupForm<RE::TESGlobal>(0x314A,  "Update.esm");
                s_health   = dh->LookupForm<RE::TESGlobal>(0x2EDE,  "Update.esm");
                s_magicka  = dh->LookupForm<RE::TESGlobal>(0x2EE0,  "Update.esm");
                s_stamina  = dh->LookupForm<RE::TESGlobal>(0x2EDF,  "Update.esm");
                logger::info("[PlayerHUDPenalties] Survival globals: enabled={} hp={} mp={} sp={}",
                    s_enabled ? "OK" : "missing",
                    s_health  ? "OK" : "missing",
                    s_magicka ? "OK" : "missing",
                    s_stamina ? "OK" : "missing");
            }
        }

        SurvivalData out{};
        if (!s_enabled || s_enabled->value != 1.0f)
            return out;

        auto norm = [](RE::TESGlobal* g) {
            return g ? std::clamp(g->value / 100.0f, 0.0f, 0.95f) : 0.0f;
        };
        out.health  = norm(s_health);
        out.magicka = norm(s_magicka);
        out.stamina = norm(s_stamina);
        return out;
    }

    TemporaryData GetTemporaryPenalties(RE::Actor* player)
    {
        TemporaryData out{};
        if (!player) return out;

        auto read = [&](RE::ActorValue av) {
            float perm = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, av);
            float temp = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, av);
            float netPenalty = -(temp + perm);
            return netPenalty > 0.0f ? netPenalty : 0.0f;
        };

        out.health  = read(RE::ActorValue::kHealth);
        out.magicka = read(RE::ActorValue::kMagicka);
        out.stamina = read(RE::ActorValue::kStamina);
        return out;
    }

    bool DrawBarSlice(ImDrawList* dl, float x, float y, float barW, float barH,
                      float u0, float u1, ImU32 tint, void* barFillTex)
    {
        if (!barFillTex) return false;
        dl->AddImage(reinterpret_cast<ImTextureID>(barFillTex),
            ImVec2(x + barW * u0, y), ImVec2(x + barW * u1, y + barH),
            ImVec2(u0, 0.0f), ImVec2(u1, 1.0f), tint);
        return true;
    }

}
