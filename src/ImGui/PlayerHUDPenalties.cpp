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

    void DrawPenaltySegment(ImDrawList* dl, float x, float y, float barW, float barH,
                            float penaltyFrac, ImU32 tintColor, void* barFillTex,
                            float rightOffsetFrac)
    {
        if (penaltyFrac <= 0.0f) return;
        const float frac        = std::clamp(penaltyFrac,       0.0f, 1.0f);
        const float rightOffset = std::clamp(rightOffsetFrac,   0.0f, 1.0f);
        const float rightEdge  = 1.0f - rightOffset;
        const float segLeft    = std::max(0.0f, rightEdge - frac);
        ImVec2 pMin = ImVec2(x + barW * segLeft,      y);
        ImVec2 pMax = ImVec2(x + barW * rightEdge,    y + barH);
        if (pMax.x <= pMin.x) return;

        if (barFillTex) {
            dl->AddImage(reinterpret_cast<ImTextureID>(barFillTex), pMin, pMax,
                ImVec2(segLeft, 0.0f), ImVec2(rightEdge, 1.0f), tintColor);
        } else {
            dl->AddRectFilled(pMin, pMax, tintColor);
        }
    }

}
