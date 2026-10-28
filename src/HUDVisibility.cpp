// STATUS: Working
// Manages dynamic widget fade-out states.

#include "../includes/HUDVisibility.h"
#include "../includes/Settings.h"
#include "../includes/EffectCategories.h"
#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <cmath>
#include <algorithm>

namespace HUDVisibility {
    namespace {
        float s_timer[static_cast<int>(WidgetID::Count)] = {};
        float s_alpha[static_cast<int>(WidgetID::Count)] = {};
        bool  s_popSuppress[static_cast<int>(WidgetID::Count)] = {};
        constexpr float kFadeInSpeed  = 5.0f;
        float Eased(float a) { return a * a * (3.0f - 2.0f * a); }
        bool s_vanillaHideApplied = false;
        bool PlayerHasActiveDebuff(RE::Actor* actor) {
            if (!actor) return false;
            auto* magicTarget = actor->AsMagicTarget();
            if (!magicTarget) return false;
            auto* effectList = magicTarget->GetActiveEffectList();
            if (!effectList) return false;
            for (auto* effect : *effectList) {
                if (!effect) continue;
                if (effect->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                    effect->flags.any(RE::ActiveEffect::Flag::kInactive)) continue;
                auto* base = effect->GetBaseObject();
                if (!base) continue;
                if (base->IsHostile() || EffectCategories::IsNegativeArchetype(base->GetArchetype()))
                    return true;
            }
            return false;
        }
        bool IsTriggered(WidgetID id) {
            auto* s = Settings::GetSingleton();
            if (!s || !s->hvEnabled) return false;
            switch (id) {
                case WidgetID::PlayerEquip:     return s->hvTriggerPlayerEquip;
                case WidgetID::PlayerEffects:   return s->hvTriggerPlayerEffects;
                case WidgetID::PlayerResist:    return s->hvTriggerPlayerResist;
                case WidgetID::PlayerResources: return s->hvTriggerPlayerResources;
                case WidgetID::Collectibles:    return s->hvTriggerCollectibles;
                case WidgetID::PinnedItems:     return s->hvTriggerPinnedItems;
                case WidgetID::StableMount:     return s->hvTriggerStableMount;
                case WidgetID::PlayerLocation:  return s->hvTriggerPlayerLocation;
                default:                        return false;
            }
        }
    }

    void Update(float deltaTime) {
        auto* s = Settings::GetSingleton();
        if (s && s->hvHideVanillaBars && !s_vanillaHideApplied) {
            auto ui = RE::UI::GetSingleton();
            if (ui) {
                auto hud = ui->GetMenu(RE::HUDMenu::MENU_NAME);
                if (hud && hud->uiMovie) {
                    ApplyVanillaBarVisibility();
                    s_vanillaHideApplied = true;
                }
            }
        }
        if (!s || !s->hvEnabled) {
            for (int i = 0; i < static_cast<int>(WidgetID::Count); ++i)
                s_alpha[i] = 1.0f;
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        bool weaponDrawn = player && player->AsActorState() && player->AsActorState()->IsWeaponDrawn();
        if (player && (weaponDrawn || player->IsInCombat())) {
            if (IsTriggered(WidgetID::PlayerEquip))
                s_timer[static_cast<int>(WidgetID::PlayerEquip)] = s->hvFadeOutDelay;
            if (IsTriggered(WidgetID::PlayerResist) &&
                (!s->hvShowResistCombatOnly || player->IsInCombat()))
                s_timer[static_cast<int>(WidgetID::PlayerResist)] = s->hvFadeOutDelay;
            if (IsTriggered(WidgetID::PlayerEffects))
                s_timer[static_cast<int>(WidgetID::PlayerEffects)] = s->hvFadeOutDelay;
        }
        if (player && IsTriggered(WidgetID::PlayerEffects) && PlayerHasActiveDebuff(player))
            s_timer[static_cast<int>(WidgetID::PlayerEffects)] = s->hvFadeOutDelay;
        for (int i = 0; i < static_cast<int>(WidgetID::Count); ++i) {
            WidgetID id = static_cast<WidgetID>(i);
            if (!IsTriggered(id)) {
                s_alpha[i] = 1.0f;
                s_timer[i] = 0.0f;
                continue;
            }
            if (s_timer[i] > 0.0f) {
                s_timer[i] -= deltaTime;
                if (s_timer[i] < 0.0f) s_timer[i] = 0.0f;
            }
            if (s_timer[i] > 0.0f) {
                s_alpha[i] += deltaTime * kFadeInSpeed;
                if (s_alpha[i] > 1.0f) s_alpha[i] = 1.0f;
            } else {
                const float dur = (std::max)(0.1f, s->hvFadeDuration);
                s_alpha[i] -= deltaTime / dur;
                if (s_alpha[i] <= 0.0f) {
                    s_alpha[i] = 0.0f;
                    s_popSuppress[i] = false;
                }
            }
        }
    }

    void Trigger(WidgetID id) {
        auto* s = Settings::GetSingleton();
        if (!s) return;
        if (id == WidgetID::PlayerResist && s->hvShowResistCombatOnly) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && !player->IsInCombat()) return;
        }
        float delay = s->hvFadeOutDelay;
        if (id == WidgetID::PlayerLocation) delay += 4.0f;
        s_timer[static_cast<int>(id)] = delay;
    }

    void BeginFadeOut(WidgetID id) {
        s_timer[static_cast<int>(id)] = 0.0f;
        s_alpha[static_cast<int>(id)] = 1.0f;
        s_popSuppress[static_cast<int>(id)] = true;
    }

    bool ConsumeEntrancePopSuppress(WidgetID id) {
        const int i = static_cast<int>(id);
        const bool v = s_popSuppress[i];
        s_popSuppress[i] = false;
        return v;
    }

    bool IsVisible(WidgetID id) {
        auto* s = Settings::GetSingleton();
        if (!s || !s->hvEnabled) return true;
        if (!IsTriggered(id)) return true;
        int i = static_cast<int>(id);
        return s_timer[i] > 0.0f || s_alpha[i] > 0.0f;
    }

    float GetAlpha(WidgetID id) {
        auto* s = Settings::GetSingleton();
        if (!s || !s->hvEnabled) return 1.0f;
        if (!IsTriggered(id)) return 1.0f;
        return Eased(s_alpha[static_cast<int>(id)]);
    }

    void ApplyAlphaToDrawData(float alpha) {
        if (alpha >= 0.999f) return;
        if (alpha < 0.0f) alpha = 0.0f;
        ImDrawData* dd = ImGui::GetDrawData();
        if (!dd) return;
        for (int i = 0; i < dd->CmdListsCount; ++i) {
            ImDrawList* cl = dd->CmdLists[i];
            for (int v = 0; v < cl->VtxBuffer.Size; ++v) {
                ImU32& col = cl->VtxBuffer.Data[v].col;
                ImU32 a = (col >> IM_COL32_A_SHIFT) & 0xFF;
                a = static_cast<ImU32>(a * alpha);
                col = (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
            }
        }
    }

    void ApplyOffsetToDrawData(float dx, float dy) {
        if (dx == 0.0f && dy == 0.0f) return;
        ImDrawData* dd = ImGui::GetDrawData();
        if (!dd) return;
        for (int i = 0; i < dd->CmdListsCount; ++i) {
            ImDrawList* cl = dd->CmdLists[i];
            for (int v = 0; v < cl->VtxBuffer.Size; ++v) {
                cl->VtxBuffer.Data[v].pos.x += dx;
                cl->VtxBuffer.Data[v].pos.y += dy;
            }
            for (int c = 0; c < cl->CmdBuffer.Size; ++c) {
                ImVec4& r = cl->CmdBuffer.Data[c].ClipRect;
                r.x += dx; r.y += dy; r.z += dx; r.w += dy;
            }
        }
    }

    void ApplyVanillaBarVisibility() {
        auto* s = Settings::GetSingleton();
        if (!s) return;
        auto hud = RE::UI::GetSingleton()->GetMenu(RE::HUDMenu::MENU_NAME);
        if (!hud) return;
        auto& uiMovie = hud->uiMovie;
        if (!uiMovie) return;
        RE::GFxValue scale;
        scale.SetNumber(s->hvHideVanillaBars ? 0.0 : 100.0);
        const char* paths[] = {
            "_root.HUDMovieBaseInstance.Health",
            "_root.HUDMovieBaseInstance.Magica",
            "_root.HUDMovieBaseInstance.Stamina",
            "_root.HUDMovieBaseInstance.ArrowInfoInstance",
            "_root.HUDMovieBaseInstance.BottomLeftLockInstance",
            "_root.HUDMovieBaseInstance.BottomRightLockInstance"
        };
        for (const char* path : paths) {
            RE::GFxValue mc;
            if (uiMovie->GetVariable(&mc, path) && mc.IsDisplayObject()) {
                mc.SetMember("_xscale", scale);
                mc.SetMember("_yscale", scale);
            }
        }
    }
}
