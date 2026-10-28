#include "../includes/ImGui/FollowerHUD.h"
#include "../includes/PerfStats.h"
#include "../includes/ImGui/Renderer.h"
#include "../includes/Settings.h"
#include "../includes/BackgroundManager.h"
#include "../includes/EffectCategories.h"
#include "../includes/TextureManager.h"
#include "../includes/Utils/EffectIconRenderer.h"
#include "../includes/Utils/HUDDraw.h"
#include "../includes/Utils/BarRenderer.h"
#include "../includes/Utils/ColorUtils.h"
#include "../includes/Utils/PathUtils.h"
#include "../includes/Utils/LocationUtils.h"
#include "../includes/LocalizationManager.h"
#include "../includes/ActorStats.h"
#include "../includes/MenuUtils.h"
#include "../includes/PlayerStatus.h"
#include "../includes/UIAnimation.h"
#include "../includes/QuickHeal.h"
#include "../includes/InputHandler.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
#include <filesystem>
#include <set>
#include <chrono>
#include <cmath>
#include <mutex>
#include <atomic>

static bool IsActorValid(RE::Actor* actor) {
    if (!actor) return false;
    
    RE::FormID formID = actor->GetFormID();
    
    if (formID == 0) return false;
    
    auto* lookedUp = RE::TESForm::LookupByID<RE::Actor>(formID);
    
    return (lookedUp != nullptr && lookedUp == actor);
}

namespace FollowerHUD {
    static float g_HudAlpha = 0.0f;

    static float MasterScale()
    {
        const auto* settings = Settings::GetSingleton();
        return settings ? std::clamp(static_cast<float>(settings->hudScale) / 100.0f, 0.5f, 2.0f) : 1.0f;
    }

    static float ScalePx(float value)
    {
        return value * MasterScale();
    }

    static int ScalePxInt(int value)
    {
        if (value == 0) return 0;
        return (std::max)(1, static_cast<int>(std::lround(ScalePx(static_cast<float>(value)))));
    }

    static std::recursive_mutex g_CacheMutex;

    // Each window owns its anchor dimensions and idle-stack timeline.
    struct HUDWindowState {
        bool forcePosition = true;
        // Previous-frame window size: AlwaysAutoResize means this frame's size isn't known
        float cachedHeight = 0.0f;
        float cachedWidth  = 0.0f;

        // Full ribbon-loop window height, refreshed ONLY on frames where the ribbon path
        float stackFullHeight = 0.0f;

        // ── Idle Portrait Stack state (render-thread only; RenderContent runs on the
        float stackT = 0.0f;
        std::chrono::steady_clock::time_point stackLastTick;
        bool  stackTickInit = false;

        bool  stackCalm = false;              // latched calm state from last frame
        std::chrono::steady_clock::time_point stackCalmSince; // when calm most recently began

        // Real screen-space top-left of each shown follower's portrait, indexed by global
        std::vector<ImVec2> stackPortraitPos;

    };
    static HUDWindowState g_FollowerWindow;
    static HUDWindowState g_SummonWindow;

    struct SummonLifetime {
        RE::FormID actorID = 0;
        RE::FormID ownerID = 0;
        RE::FormID baseEffectID = 0;
        RE::FormID spellID = 0;
        std::uint16_t uniqueID = 0;
        // Address token only; the render thread never dereferences the sampled effect.
        std::uintptr_t effectInstance = 0;
        size_t linkedActors = 0;
        float duration = 0.0f;
        float remaining = 0.0f;
        std::chrono::steady_clock::time_point lastDrawn{};
    };
    static std::mutex g_SummonLifetimeMutex;
    static std::vector<SummonLifetime> g_SummonLifetimes;
    static std::vector<RE::FormID> g_VisibleSummonIDs;
    static std::chrono::steady_clock::time_point g_SummonLifetimeSampleTime{};
    static std::atomic_bool g_SummonLifetimePending = false;

    static void QueueSummonLifetimes(const std::vector<RE::Actor*>& summons) {
        static auto lastRequest = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (now - lastRequest < std::chrono::milliseconds(100)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || g_SummonLifetimePending.exchange(true)) return;
        lastRequest = now;
        std::vector<RE::ActorHandle> handles;
        for (auto* actor : summons) handles.push_back(actor->GetHandle());
        tasks->AddTask([handles = std::move(handles)]() {
            std::vector<SummonLifetime> samples;
            size_t signature = handles.size();
            for (const auto& handle : handles) {
                auto actor = handle.get();
                if (!actor || actor->IsDead()) continue;
                SummonLifetime sample;
                sample.actorID = actor->GetFormID();
                auto owner = actor->GetCommandingActor();
                if (owner) {
                    sample.ownerID = owner->GetFormID();
                    auto* process = owner->GetActorRuntimeData().currentProcess;
                    if (process && process->middleHigh) {
                        // Exact actor/effect pairing avoids mixing timers from repeated casts.
                        for (const auto& command : process->middleHigh->commandedActors) {
                            if (command.commandedActor.get().get() != actor.get()) continue;
                            auto* effect = command.activeEffect;
                            if (effect && !effect->flags.any(RE::ActiveEffect::Flag::kDispelled,
                                                            RE::ActiveEffect::Flag::kInactive) &&
                                std::isfinite(effect->duration) && std::isfinite(effect->elapsedSeconds)) {
                                sample.duration = (std::max)(0.0f, effect->duration);
                                sample.remaining = std::clamp(effect->duration - effect->elapsedSeconds,
                                                              0.0f, sample.duration);
                                sample.uniqueID = effect->usUniqueID;
                                sample.effectInstance = reinterpret_cast<std::uintptr_t>(effect);
                                if (auto* base = effect->GetBaseObject()) sample.baseEffectID = base->GetFormID();
                                if (effect->spell) sample.spellID = effect->spell->GetFormID();
                                for (const auto& linked : process->middleHigh->commandedActors) {
                                    if (linked.activeEffect != effect) continue;
                                    auto linkedActor = linked.commandedActor.get();
                                    if (linkedActor && !linkedActor->IsDead()) ++sample.linkedActors;
                                }
                            }
                            break;
                        }
                    }
                }
                signature = signature * 31 + sample.actorID;
                signature = signature * 31 + sample.effectInstance;
                signature = signature * 31 + (sample.duration > 0.0f ? 1 : 0);
                samples.push_back(sample);
            }
            static size_t lastSignature = 0;
            if (signature != lastSignature) {
                for (const auto& sample : samples)
                    logger::debug("[SummonBorder] actor={:08X} owner={:08X} effect={:X} uniqueID={} base={:08X} spell={:08X} linked={} duration={} remaining={}",
                                  sample.actorID, sample.ownerID, sample.effectInstance, sample.uniqueID,
                                  sample.baseEffectID, sample.spellID, sample.linkedActors,
                                  sample.duration, sample.remaining);
                lastSignature = signature;
            }
            {
                std::lock_guard lock(g_SummonLifetimeMutex);
                // Preserve draw evidence only for the same actor and effect instance.
                for (auto& sample : samples) {
                    auto old = std::find_if(g_SummonLifetimes.begin(), g_SummonLifetimes.end(),
                        [&](const auto& previous) {
                            return previous.actorID == sample.actorID && previous.ownerID == sample.ownerID &&
                                   previous.effectInstance == sample.effectInstance &&
                                   previous.uniqueID == sample.uniqueID && previous.baseEffectID == sample.baseEffectID &&
                                   previous.spellID == sample.spellID;
                        });
                    if (old != g_SummonLifetimes.end()) sample.lastDrawn = old->lastDrawn;
                }
                g_SummonLifetimes = std::move(samples);
                g_SummonLifetimeSampleTime = std::chrono::steady_clock::now();
            }
            g_SummonLifetimePending = false;
        });
    }

    bool ShouldHideSummonEffect(RE::FormID ownerID, std::uintptr_t effectInstance, std::uint16_t uniqueID,
                                RE::FormID baseEffectID, RE::FormID spellID) {
        const auto* settings = Settings::GetSingleton();
        if (!settings->hudSeparateSummons || !settings->hudHideManagedSummonEffects ||
            !settings->showSummons || !settings->showPortrait || settings->followerHudMode == 0) return false;
        if (ownerID != 0x14 && !settings->showFollowerSummons) return false;
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(g_SummonLifetimeMutex);
        if (now - g_SummonLifetimeSampleTime > std::chrono::milliseconds(500)) return false;
        size_t represented = 0;
        size_t linkedActors = 0;
        for (const auto& sample : g_SummonLifetimes) {
            // Separate casts can share usUniqueID=0; group only actors linked to this exact effect.
            if (sample.ownerID != ownerID || sample.effectInstance != effectInstance || sample.uniqueID != uniqueID ||
                sample.baseEffectID != baseEffectID || sample.spellID != spellID) continue;
            if (sample.duration <= 0.0f || sample.remaining <= 0.0f ||
                now - sample.lastDrawn > std::chrono::milliseconds(250) ||
                std::find(g_VisibleSummonIDs.begin(), g_VisibleSummonIDs.end(), sample.actorID) == g_VisibleSummonIDs.end())
                return false;
            linkedActors = (std::max)(linkedActors, sample.linkedActors);
            ++represented;
        }
        // A multi-summon effect stays visible if the actor cap or filters hide any member.
        return represented > 0 && represented == linkedActors;
    }

    static void DrawSummonLifetimeBorder(ImDrawList* dl, ImVec2 min, ImVec2 max,
                                         int shape, float rounding, RE::Actor* actor,
                                         bool halo, float alpha = 1.0f) {
        if (!actor || !Settings::GetSingleton()->hudSeparateSummons) return;
        SummonLifetime lifetime;
        {
            std::lock_guard lock(g_SummonLifetimeMutex);
            if (std::chrono::steady_clock::now() - g_SummonLifetimeSampleTime > std::chrono::milliseconds(500)) return;
            auto it = std::find_if(g_SummonLifetimes.begin(), g_SummonLifetimes.end(),
                [&](const auto& sample) { return sample.actorID == actor->GetFormID(); });
            if (it == g_SummonLifetimes.end()) return;
            if (alpha >= 0.1f && it->duration > 0.0f && it->remaining > 0.0f)
                it->lastDrawn = std::chrono::steady_clock::now();
            lifetime = *it;
        }
        if (lifetime.duration <= 0.0f || lifetime.remaining <= 0.0f) return;
        const float fraction = lifetime.remaining / lifetime.duration;
        const float thickness = ScalePx(2.0f);
        const auto fill = lifetime.remaining <= 5.0f ? 0xFFFFA065u : 0xFF79D3E5u;
        const float minDim = (std::min)(max.x - min.x, max.y - min.y);
        dl->PushClipRectFullScreen();
        if (shape == 2 || halo) {
            HUDDraw::TimerArcStyle style;
            // Halo retains its separate inner countdown. Ordinary circles follow the
            style.radius = (std::max)(thickness, halo
                ? minDim * 0.5f - thickness * 0.5f - ScalePx(1.0f)
                    + ScalePx(Settings::GetSingleton()->hudHaloOffset)
                : minDim * 0.5f - 0.5f);
            style.thickness = thickness;
            style.fraction = fraction;
            style.fillColor = fill;
            style.trackColor = 0xA0182126u;
            style.alpha = alpha;
            HUDDraw::DrawTimerArc(dl, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), style);
        } else {
            // Match the visible outline: rounded rectangles use ImGui's half-pixel
            const float inset = shape == 3 ? 0.0f : 0.5f;
            min.x += inset; min.y += inset;
            max.x -= inset; max.y -= inset;
            std::vector<ImVec2> outline;
            if (shape == 3) {
                outline = {ImVec2((min.x + max.x) * 0.5f, min.y),
                           ImVec2(max.x, (min.y + max.y) * 0.5f),
                           ImVec2((min.x + max.x) * 0.5f, max.y),
                           ImVec2(min.x, (min.y + max.y) * 0.5f)};
            } else {
                const float radius = shape == 1 ? minDim * 0.2f : rounding;
                dl->PathRect(min, max, (std::max)(0.0f, radius));
                outline.assign(dl->_Path.begin(), dl->_Path.end());
                dl->PathClear();
            }
            auto distance = [](const ImVec2& a, const ImVec2& b) {
                return std::hypot(b.x - a.x, b.y - a.y);
            };
            float perimeter = 0.0f;
            for (size_t i = 0; i < outline.size(); ++i)
                perimeter += distance(outline[i], outline[(i + 1) % outline.size()]);
            const int opacity = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
            dl->AddPolyline(outline.data(), static_cast<int>(outline.size()), IM_COL32(24, 33, 38, opacity),
                            ImDrawFlags_Closed, thickness);
            float remainingLength = perimeter * fraction;
            if (!outline.empty()) dl->PathLineTo(outline.front());
            for (size_t i = 0; i < outline.size() && remainingLength > 0.0f; ++i) {
                const auto a = outline[i];
                const auto b = outline[(i + 1) % outline.size()];
                const float length = distance(a, b);
                if (length <= 0.0f) continue;
                const float t = (std::min)(1.0f, remainingLength / length);
                dl->PathLineTo(ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
                remainingLength -= length;
            }
            dl->PathStroke(IM_COL32((fill >> 16) & 0xFF, (fill >> 8) & 0xFF, fill & 0xFF, opacity),
                           0, thickness);
        }
        dl->PopClipRect();
    }
    static constexpr float kStackRestackHoldSeconds = 3.0f;
    static constexpr float kStackFadePhase = 0.4f;

    static std::vector<RE::Actor*> g_CachedSummons;
    static std::vector<RE::Actor*> g_CachedFollowers;
    static std::vector<RE::Actor*> g_CachedQuestAllies;
    
    // Dirty flags track what needs rescanning
    static bool g_SummonsDirty = true;
    static bool g_FollowersDirty = true;
    static bool g_QuestAlliesDirty = true;
    
    static std::chrono::steady_clock::time_point g_LastCacheRefresh;
    static std::chrono::steady_clock::time_point g_LastFollowerRefresh;
    
    static void LoadBlacklist();
    
    void Init() {
        g_HudAlpha = 0.0f;
    }
    
    void ResetFade() {
        g_HudAlpha = 0.0f;
    }

    void ForceReposition() {
        g_FollowerWindow.forcePosition = true;
        g_SummonWindow.forcePosition = true;
    }

    // Called by the Grow Upward checkbox AFTER the value flips. Converts the saved Y
    void OnGrowUpwardToggled(bool summonWindow) {
        auto* settings = Settings::GetSingleton();
        auto& state = summonWindow ? g_SummonWindow : g_FollowerWindow;
        auto& posY = summonWindow ? settings->hudSummonPosY : settings->hudPosY;
        const bool growUpward = summonWindow ? settings->hudSummonGrowUpward : settings->hudGrowUpward;
        const long height = std::lround(state.cachedHeight);
        posY += growUpward ? height : -height;
        state.forcePosition = true;
    }
    
    bool IsActorValidForRender(RE::Actor* actor) {
        return IsActorValid(actor);
    }
    
    static ImU32 hexToU32(uint32_t hex, uint8_t alpha = 255) {
        uint8_t r = (hex >> 16) & 0xFF;
        uint8_t g = (hex >> 8) & 0xFF;
        uint8_t b = hex & 0xFF;
        return IM_COL32(r, g, b, alpha);
    }
    
    static std::string toUppercase(const std::string& str) {
        std::string result = str;
        for (auto& c : result) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return result;
    }
    
    static void DrawTextWithShadow(ImDrawList* drawList, ImVec2 pos, ImU32 color, const char* text, bool useShadow = false) {
        auto settings = Settings::GetSingleton();
        if (useShadow && settings->textShadowEnabled) {
            ImU32 shadowColor = hexToU32(settings->textShadowColor, 
                                         static_cast<uint8_t>(settings->textShadowOpacity * 255));
            ImVec2 shadowPos = ImVec2(pos.x + ScalePx(settings->textShadowOffsetX),
                                       pos.y + ScalePx(settings->textShadowOffsetY));
            drawList->AddText(shadowPos, shadowColor, text);
        }
        drawList->AddText(pos, color, text);
    }
    
    static void DrawDangerOverlay(ImDrawList* drawList, ImVec2 pos, float size, ImU32 fillColor, ImU32 borderColor, bool drawBorder) {
        auto settings = Settings::GetSingleton();
        int shape = settings->portraitShape;
        float rounding = ScalePx(static_cast<float>(settings->portraitCornerRadius));
        
        float effectiveRounding = rounding;
        switch (shape) {
            case 1:
                effectiveRounding = size * 0.2f;
                break;
            case 2:
                effectiveRounding = size * 0.5f;
                break;
            case 3:
                {
                    float cx = pos.x + size * 0.5f;
                    float cy = pos.y + size * 0.5f;
                    ImVec2 dTop(cx, pos.y);
                    ImVec2 dRight(pos.x + size, cy);
                    ImVec2 dBottom(cx, pos.y + size);
                    ImVec2 dLeft(pos.x, cy);
                    
                    drawList->AddQuadFilled(dTop, dRight, dBottom, dLeft, fillColor);
                    
                    if (drawBorder) {
                        drawList->AddQuad(dTop, dRight, dBottom, dLeft, borderColor, 2.0f);
                    }
                    return;
                }
            default:
                effectiveRounding = rounding;
                break;
        }
        
        ImVec2 pMax = ImVec2(pos.x + size, pos.y + size);
        drawList->AddRectFilled(pos, pMax, fillColor, effectiveRounding);
        
        if (drawBorder) {
            drawList->AddRect(pos, pMax, borderColor, effectiveRounding, 0, 2.0f);
        }
    }
    
    enum class FollowerState { Following, Waiting, Combat, Mounted, Downed };

    static RE::ActorValue GetWaitingForPlayerAV() {
        static RE::ActorValue av = RE::ActorValue::kNone;
        if (av == RE::ActorValue::kNone) {
            if (auto* avl = RE::ActorValueList::GetSingleton())
                av = avl->LookupActorValueByName("WaitingforPlayer");
        }
        return av;
    }

    static FollowerState GetFollowerState(RE::Actor* actor) {
        if (actor->AsActorState()->IsBleedingOut()) return FollowerState::Downed;
        if (actor->IsInCombat())   return FollowerState::Combat;
        if (actor->IsOnMount())    return FollowerState::Mounted;

        RE::ActorValue waitAV = GetWaitingForPlayerAV();
        if (waitAV != RE::ActorValue::kNone) {
            float waiting = actor->AsActorValueOwner()->GetActorValue(waitAV);
            return (waiting >= 1.0f) ? FollowerState::Waiting : FollowerState::Following;
        }
        return FollowerState::Waiting;
    }

    static RE::Actor* g_QuickHealTarget = nullptr;

    // Badge style from an ARGB bg + ARGB text color: same recipe as PlayerHUD /
    static HUDDraw::BadgeStyle MakeBadgeStyle(std::uint32_t bgArgb, std::uint32_t textArgb, float fontScale, bool showBorder = true) {
        auto settings = Settings::GetSingleton();
        HUDDraw::BadgeStyle bs;
        bs.fontScale = fontScale;
        bs.padX = ScalePx(bs.padX);
        bs.padY = ScalePx(bs.padY);
        // Shared corner radius for every portrait badge (Level, State, QuickHeal). -1 =
        bs.rounding = ScalePx(static_cast<float>(settings->hudBadgeRadius));
        const std::uint8_t a = (bgArgb >> 24) & 0xFF, r = (bgArgb >> 16) & 0xFF,
                           g = (bgArgb >> 8) & 0xFF,  b =  bgArgb        & 0xFF;
        auto lift = [](std::uint8_t c) { return static_cast<std::uint32_t>((std::min)(255.0f, c * 1.20f + 14.0f)); };
        auto sink = [](std::uint8_t c) { return static_cast<std::uint32_t>(c * 0.55f); };
        bs.bgTop    = (static_cast<std::uint32_t>(a) << 24) | (lift(r) << 16) | (lift(g) << 8) | lift(b);
        bs.bgBottom = (static_cast<std::uint32_t>(a) << 24) | (sink(r) << 16) | (sink(g) << 8) | sink(b);
        bs.textColor = textArgb;

        // Badge FX controls drop shadow and top highlight across all portrait badges
        bs.shadow    = settings->hudBadgeFx;
        bs.highlight = settings->hudBadgeFx;

        if (!showBorder) {
            bs.innerBorder = 0;
            bs.outerBorder = 0;
        }
        return bs;
    }

    static void DrawPortraitOverlays(ImDrawList* dl, ImVec2 pPos, float portraitSize, float healthPercent, int level, RE::Actor* actor, float ribbonWidth = 0.0f,
                                     float magickaPercent = -1.0f, float staminaPercent = -1.0f) {
        auto settings = Settings::GetSingleton();

        if (settings->hudHaloEnable && magickaPercent >= 0.0f && staminaPercent >= 0.0f) {
            const float healthShare = settings->hudHaloHealthShare / 100.0f;
            const float otherShare  = (1.0f - healthShare) * 0.5f;
            float wHealth  = settings->showHealth  ? healthShare : 0.0f;
            float wStamina = settings->showStamina ? otherShare  : 0.0f;
            float wMagicka = settings->showMagicka ? otherShare  : 0.0f;
            const float wSum = wHealth + wStamina + wMagicka;

            if (wSum > 0.0f) {
                wHealth  /= wSum;
                wStamina /= wSum;
                wMagicka /= wSum;

                const float cx = (ribbonWidth > 0.0f) ? pPos.x + ribbonWidth * 0.5f
                                                      : pPos.x + portraitSize * 0.5f;
                const ImVec2 center(cx, pPos.y + portraitSize * 0.5f);
                const float thickness = (settings->hudHaloThickness > 0)
                    ? ScalePx(static_cast<float>(settings->hudHaloThickness))
                    : (std::max)(ScalePx(3.0f), portraitSize * 0.055f);
                const float radius = portraitSize * 0.5f + thickness * 0.5f + ScalePx(settings->hudHaloOffset);

                const HUDDraw::RingSegment segs[] = {
                    { healthPercent,  wHealth,  0xFF000000u | (static_cast<std::uint32_t>(settings->healthColor)  & 0xFFFFFFu) },
                    { staminaPercent, wStamina, 0xFF000000u | (static_cast<std::uint32_t>(settings->staminaColor) & 0xFFFFFFu) },
                    { magickaPercent, wMagicka, 0xFF000000u | (static_cast<std::uint32_t>(settings->magickaColor) & 0xFFFFFFu) },
                };
                HUDDraw::ResourceRingStyle ringStyle;
                ringStyle.radius    = radius;
                ringStyle.thickness = thickness;
                ringStyle.gapDeg    = static_cast<float>(settings->hudHaloGap);
                ringStyle.segments      = segs;
                ringStyle.segmentCount  = 3;
                HUDDraw::DrawResourceRing(dl, center, ringStyle);
            }
        }

        DrawSummonLifetimeBorder(dl, pPos,
            ImVec2(pPos.x + (ribbonWidth > 0.0f ? ribbonWidth : portraitSize), pPos.y + portraitSize),
            ribbonWidth > 0.0f ? 0 : settings->portraitShape,
            ScalePx(static_cast<float>(ribbonWidth > 0.0f ? settings->ribbonCornerRadius : settings->portraitCornerRadius)),
            actor, settings->hudHaloEnable && ribbonWidth <= 0.0f);

        const float rightEdgeX = (ribbonWidth > 0.0f) ? pPos.x + ribbonWidth
                                                       : pPos.x + portraitSize;

        // Low health warning pulse / Downed state
        const bool isDowned = (actor && actor->AsActorState() && actor->AsActorState()->IsBleedingOut()) || healthPercent <= 0.0f;
        if (isDowned) {
            // Steady full red color when downed (no flashing, solid red wash and border)
            ImU32 fillCol   = IM_COL32(220, 20, 20, 180);
            ImU32 borderCol = IM_COL32(255, 0, 0, 255);
            if (ribbonWidth > 0.0f) {
                ImVec2 pMax(pPos.x + ribbonWidth, pPos.y + portraitSize);
                float r = ScalePx(static_cast<float>(settings->ribbonCornerRadius));
                dl->AddRectFilled(pPos, pMax, fillCol, r);
                dl->AddRect(pPos, pMax, borderCol, r, 0, ScalePx(2.0f));
            } else {
                DrawDangerOverlay(dl, pPos, portraitSize, fillCol, borderCol, true);
            }
        } else if (healthPercent < 0.8f && settings->hudPortraitHitFlash) {
            bool isCritical = healthPercent < 0.25f;
            float pulseSpeed = isCritical ? 5.0f : 2.5f;
            float alpha = (sin(static_cast<float>(ImGui::GetTime()) * pulseSpeed) * 0.5f + 0.5f) * 0.4f + 0.1f;
            ImU32 fillCol   = IM_COL32(255, 0, 0, static_cast<int>(alpha * 255));
            ImU32 borderCol = IM_COL32(255, 0, 0, 200);
            if (ribbonWidth > 0.0f) {
                ImVec2 pMax(pPos.x + ribbonWidth, pPos.y + portraitSize);
                float r = ScalePx(static_cast<float>(settings->ribbonCornerRadius));
                dl->AddRectFilled(pPos, pMax, fillCol, r);
                if (isCritical)
                    dl->AddRect(pPos, pMax, borderCol, r, 0, ScalePx(2.0f));
            } else {
                DrawDangerOverlay(dl, pPos, portraitSize, fillCol, borderCol, isCritical);
            }
        }

        if (settings->levelBadgeOnPortrait) {
            char lvlBuf[16];
            snprintf(lvlBuf, sizeof(lvlBuf), "%d", level);
            HUDDraw::DrawBadge(dl, ImVec2(rightEdgeX - ScalePx(2.0f) + ScalePx(settings->hudLevelBadgeOffsetX), pPos.y + portraitSize - ScalePx(2.0f) + ScalePx(settings->hudLevelBadgeOffsetY)), 2, lvlBuf,
                MakeBadgeStyle(settings->hudBadgeBgColor, 0xFF000000u | settings->levelColor,
                               settings->hudBadgeFontScale, settings->hudLevelBadgeShowBorder));
        }

        if (actor && settings->showStateBadge) {
            static ID3D11ShaderResourceView* s_TexFollow  = nullptr;
            static ID3D11ShaderResourceView* s_TexWait    = nullptr;
            static ID3D11ShaderResourceView* s_TexCombat  = nullptr;
            static ID3D11ShaderResourceView* s_TexArchery = nullptr;
            static ID3D11ShaderResourceView* s_TexSpell   = nullptr;
            static ID3D11ShaderResourceView* s_TexMounted = nullptr;
            static ID3D11ShaderResourceView* s_TexDowned  = nullptr;
            static bool s_StateTexLoaded = false;
            if (!s_StateTexLoaded) {
                s_TexFollow  = TextureManager::GetPartyIcon("StateFollow");
                s_TexWait    = TextureManager::GetPartyIcon("StateWait");
                s_TexCombat  = TextureManager::GetPartyIcon("StateCombat");
                s_TexArchery = TextureManager::GetPartyIcon("StateArchery");
                s_TexSpell   = TextureManager::GetPartyIcon("StateSpell");
                s_TexMounted = TextureManager::GetPartyIcon("StateMounted");
                s_TexDowned  = TextureManager::GetPartyIcon("StateDowned");
                s_StateTexLoaded = true;
            }

            FollowerState state = GetFollowerState(actor);
            ID3D11ShaderResourceView* combatIcon = s_TexCombat;
            if (state == FollowerState::Combat) {
                auto* rh = actor->GetEquippedObject(false);
                auto* lh = actor->GetEquippedObject(true);
                if (auto* weap = rh ? rh->As<RE::TESObjectWEAP>() : nullptr) {
                    if (weap->IsBow() || weap->IsCrossbow()) {
                        combatIcon = s_TexArchery ? s_TexArchery : s_TexCombat;
                    } else if (weap->GetWeaponType() == RE::WEAPON_TYPE::kStaff) {
                        combatIcon = s_TexSpell ? s_TexSpell : s_TexCombat;
                    }
                }
                if (combatIcon == s_TexCombat) {
                    if (auto* lhWeap = lh ? lh->As<RE::TESObjectWEAP>() : nullptr) {
                        if (lhWeap->GetWeaponType() == RE::WEAPON_TYPE::kStaff) {
                            combatIcon = s_TexSpell ? s_TexSpell : s_TexCombat;
                        }
                    }
                }
                if (combatIcon == s_TexCombat && ((rh && rh->As<RE::SpellItem>()) || (lh && lh->As<RE::SpellItem>()))) {
                    combatIcon = s_TexSpell ? s_TexSpell : s_TexCombat;
                }
            }

            ID3D11ShaderResourceView* stateTex =
                (state == FollowerState::Downed)    ? s_TexDowned  :
                (state == FollowerState::Combat)    ? combatIcon   :
                (state == FollowerState::Mounted)   ? s_TexMounted :
                (state == FollowerState::Following) ? s_TexFollow  : s_TexWait;

            if (stateTex) {
                const float iconSize = (std::max)(portraitSize * settings->hudStateBadgeScale, ScalePx(12.0f));
                auto bs = MakeBadgeStyle(settings->hudStateBadgeBgColor, 0, 1.0f, settings->hudStateBadgeShowBorder);
                bs.icon     = reinterpret_cast<ImTextureID>(stateTex);
                bs.iconTint = 0xFF000000u | settings->hudStateBadgeIconTint;
                bs.iconSize = iconSize;
                bs.padX = bs.padY = ScalePx(2.0f);  // old bg was icon+1px; keep the pill snug
                ImVec2 badgeAnchor(rightEdgeX - ScalePx(2.0f) + ScalePx(settings->hudStateBadgeOffsetX), pPos.y + ScalePx(2.0f) + ScalePx(settings->hudStateBadgeOffsetY));
                HUDDraw::BadgeRect br = HUDDraw::DrawBadge(dl, badgeAnchor, 1, nullptr, bs);

                // Draw attack particle burst around the combat badge
                if (state == FollowerState::Combat) {
                    std::string key = "combat_badge_" + std::to_string(actor->GetFormID());
                    ImVec2 badgeCenter((br.min.x + br.max.x) * 0.5f, (br.min.y + br.max.y) * 0.5f);
                    float badgeR = (br.max.x - br.min.x) * 0.5f;
                    UIAnimation::DrawAttackParticles(dl, key, badgeCenter, badgeR);
                }
            }
        }

        if (actor && (settings->showDirectionArrow || settings->showDistanceMeters)) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                auto liveWorldPos = [](RE::TESObjectREFR* ref) -> RE::NiPoint3 {
                    if (ref && ref->Is3DLoaded()) {
                        if (auto* node = ref->Get3D()) {
                            return node->world.translate;
                        }
                    }
                    return ref ? ref->GetPosition() : RE::NiPoint3{};
                };
                const RE::NiPoint3 ap = liveWorldPos(actor);
                const RE::NiPoint3 pp = liveWorldPos(player);
                const float dx = ap.x - pp.x;
                const float dy = ap.y - pp.y;
                const float dz = ap.z - pp.z;
                const float distUnits = sqrtf(dx * dx + dy * dy + dz * dz);
                const float meters = distUnits / 70.0f;

                const float playerYaw = player->GetAngleZ();
                const float bearing = atan2f(dx, dy);
                float rad = bearing - playerYaw;
                while (rad >  3.14159265f) rad -= 6.28318530f;
                while (rad < -3.14159265f) rad += 6.28318530f;
                const float cosA = cosf(rad);
                const float sinA = sinf(rad);

                const float arrowSize = (std::max)(portraitSize * settings->hudArrowScale, ScalePx(10.0f));
                const float margin = ScalePx(2.0f);
                const ImVec2 center = ImVec2(pPos.x + margin + arrowSize * 0.5f + ScalePx(static_cast<float>(settings->hudArrowOffsetX)),
                                             pPos.y + portraitSize - margin - arrowSize * 0.5f + ScalePx(static_cast<float>(settings->hudArrowOffsetY)));

                static ID3D11ShaderResourceView* s_TexArrow = nullptr;
                static bool s_ArrowTexLoaded = false;
                if (settings->showDirectionArrow && !s_ArrowTexLoaded) {
                    s_TexArrow = TextureManager::GetPartyIcon("Direction");
                    s_ArrowTexLoaded = true;
                }

                const bool arrowAllowedByLocation = LocationUtils::ShouldShowDirectionArrow();

                if (settings->showDirectionArrow && arrowAllowedByLocation) {
                    if (settings->hudArrowShowBg) {
                        const float rad = arrowSize * 0.55f;
                        const std::uint32_t bg = settings->hudArrowBgColor;
                        const std::uint8_t a = (bg >> 24) & 0xFF;
                        const std::uint8_t r = (bg >> 16) & 0xFF, g = (bg >> 8) & 0xFF, b = bg & 0xFF;
                        auto lift = [](std::uint8_t c) { return static_cast<std::uint8_t>((std::min)(255.0f, c * 1.20f + 14.0f)); };
                        auto sink = [](std::uint8_t c) { return static_cast<std::uint8_t>(c * 0.55f); };

                        // Drop shadow: chip recipe scaled to the disc, respects Badge FX
                        if (settings->hudBadgeFx && a > 0) {
                            for (int i = 2; i >= 1; --i)
                                dl->AddCircleFilled(ImVec2(center.x + ScalePx(1.0f), center.y + ScalePx(2.0f)),
                                                    rad + ScalePx(static_cast<float>(i)), IM_COL32(0, 0, 0, static_cast<int>(28 * (a / 255.0f))), 24);
                        }

                        // Gradient fill at user-configured alpha (KeepAlpha shade)
                        if (a > 0) {
                            int v0 = dl->VtxBuffer.Size;
                            dl->AddCircleFilled(center, rad, IM_COL32(255, 255, 255, a), 24);
                            int v1 = dl->VtxBuffer.Size;
                            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1,
                                ImVec2(center.x, center.y - rad), ImVec2(center.x, center.y + rad),
                                IM_COL32(lift(r), lift(g), lift(b), 255),
                                IM_COL32(sink(r), sink(g), sink(b), 255));
                        }

                        // Metallic inner + dark outer ring, badge border colors: respects ArrowShowBorder
                        if (settings->hudArrowShowBorder) {
                            dl->AddCircle(center, rad - ScalePx(1.0f), IM_COL32(150, 138, 116, 0x50), 24, ScalePx(1.0f));
                            dl->AddCircle(center, rad, IM_COL32(6, 6, 8, 0xC8), 24, ScalePx(1.0f));
                        }
                    }

                    const float h = arrowSize * 0.5f;
                    auto rot = [&](float lx, float ly) {
                        return ImVec2(center.x + lx * cosA - ly * sinA,
                                      center.y + lx * sinA + ly * cosA);
                    };
                    const ImVec2 q_tl = rot(-h, -h);
                    const ImVec2 q_tr = rot( h, -h);
                    const ImVec2 q_br = rot( h,  h);
                    const ImVec2 q_bl = rot(-h,  h);

                    if (s_TexArrow) {
                        const ImU32 arrowCol = IM_COL32(
                            (settings->hudArrowTint >> 16) & 0xFF,
                            (settings->hudArrowTint >>  8) & 0xFF,
                             settings->hudArrowTint        & 0xFF,
                            255);
                        dl->AddImageQuad(reinterpret_cast<ImTextureID>(s_TexArrow),
                                         q_tl, q_tr, q_br, q_bl,
                                         ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1),
                                         arrowCol);
                    } else {
                        const ImU32 col = IM_COL32(
                            (settings->hudArrowTint >> 16) & 0xFF,
                            (settings->hudArrowTint >>  8) & 0xFF,
                             settings->hudArrowTint        & 0xFF,
                            240);
                        dl->AddTriangleFilled(rot(0.0f, -h), rot(-h * 0.75f, h * 0.6f), rot(h * 0.75f, h * 0.6f), col);
                    }
                }

                if (settings->showDistanceMeters && arrowAllowedByLocation && meters > 10.0f) {
                    char distBuf[16];
                    if (meters < 10.0f)       snprintf(distBuf, sizeof(distBuf), "%.1fm", meters);
                    else if (meters < 1000.0f) snprintf(distBuf, sizeof(distBuf), "%dm", (int)(meters + 0.5f));
                    else                       snprintf(distBuf, sizeof(distBuf), "%.1fk", meters / 1000.0f);

                    // Distance badge sizes off its own hudDistanceFontScale only. It used
                    auto bs = MakeBadgeStyle(settings->hudArrowBgColor,
                                             0xF0000000u | (settings->hudArrowTint & 0xFFFFFF),
                                              settings->hudDistanceFontScale,
                                             settings->hudArrowShowBorder);
                    bs.padX = bs.padY = ScalePx(2.0f);
                    float textX = (settings->showDirectionArrow
                        ? (pPos.x + margin + arrowSize + ScalePx(2.0f))
                        : (pPos.x + margin))
                        + ScalePx(static_cast<float>(settings->hudDistanceOffsetX));
                    HUDDraw::DrawBadge(dl,
                        ImVec2(textX - ScalePx(2.0f), pPos.y + portraitSize - margin + ScalePx(1.0f)
                               + ScalePx(static_cast<float>(settings->hudDistanceOffsetY))),
                        3, distBuf, bs);
                }
            }
        }

        if (actor && settings->qhEnabled && settings->qhShowBadgeOnFocus &&
            actor == g_QuickHealTarget)
        {
            static ID3D11ShaderResourceView* s_TexHeart = nullptr;
            static bool s_HeartLoaded = false;
            if (!s_HeartLoaded) {
                s_TexHeart = TextureManager::GetPartyIcon("Heal");
                s_HeartLoaded = true;
            }

            const char* keyStr = ImGuiRenderer::DIKToKeyName(settings->qhHotkey);
            char labelBuf[32];
            snprintf(labelBuf, sizeof(labelBuf), "%s", keyStr);

            const float bannerH  = ScalePx(22.0f);
            const float winScale = ImGui::GetCurrentWindow()->FontWindowScale;
            auto bs = MakeBadgeStyle(0xB9000000u /* old black 185 */, 0xE6FFFFFF,
                                      winScale * settings->qhBannerFontScale);
            bs.iconSize = bannerH - ScalePx(8.0f);
            bs.iconTint = 0xFF50DC64;
            if (s_TexHeart) bs.icon = reinterpret_cast<ImTextureID>(s_TexHeart);
            bs.rounding = ScalePx(4.0f);  // banner, not a pill

            float bannerX = pPos.x + ScalePx(static_cast<float>(settings->qhBannerOffsetX));
            float bannerY = pPos.y + portraitSize + ScalePx(static_cast<float>(settings->qhBannerOffsetY));
            HUDDraw::DrawBadge(dl, ImVec2(bannerX, bannerY), 3, labelBuf, bs);
        }
    }

    // Helper to draw an image with rounded corners and double border (or PNG frame overlay)
    static ImU32 HealFlashColor(float intensity, bool isBuff) {
        const int a = static_cast<int>(intensity * 205.0f);
        return isBuff ? IM_COL32(255, 150, 30, a) : IM_COL32(50, 235, 120, a);
    }

    static void DrawRoundedImage(ID3D11ShaderResourceView* texture, ImVec2 size, float rounding, float tintAlpha = 1.0f, float hitFlashIntensity = 0.0f, float healFlashIntensity = 0.0f, bool buffFlash = false) {
        if (!texture) return;

        auto settings = Settings::GetSingleton();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImU32 imgTint = IM_COL32(255, 255, 255, static_cast<int>(std::clamp(tintAlpha, 0.0f, 1.0f) * 255.0f));

        float shapeZoom = (std::max)(1.0f, settings->hudPortraitShapeZoom);
        float sHalf = 0.5f / shapeZoom;
        float sFU = (std::min)((std::max)(settings->hudPortraitShapeFocusU, sHalf), 1.0f - sHalf);
        float sFV = (std::min)((std::max)(settings->hudPortraitShapeFocusV, sHalf), 1.0f - sHalf);
        ImVec2 uvC0(sFU - sHalf, sFV - sHalf);
        ImVec2 uvC1(sFU + sHalf, sFV + sHalf);
        auto lerpUV = [&](float u, float v) {
            return ImVec2(uvC0.x + (uvC1.x - uvC0.x) * u, uvC0.y + (uvC1.y - uvC0.y) * v);
        };

        float effectiveRounding = rounding;
        float minDim = (std::min)(size.x, size.y);
        bool useDiamondShape = false;
        
        switch (settings->portraitShape) {
            case 1:
                effectiveRounding = minDim * 0.2f;
                break;
            case 2:
                effectiveRounding = minDim * 0.5f;
                break;
            case 3:
                useDiamondShape = true;
                break;
            default:
                effectiveRounding = rounding;
                break;
        }
        
        if (settings->hudPortraitBacklight)
            HUDDraw::DrawPortraitBackHalo(draw_list, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                          settings->hudPortraitBacklightColor);

        HUDDraw::PortraitFx portraitFx;
        portraitFx.topLight = portraitFx.scrim = portraitFx.vignette = portraitFx.rim =
            settings->hudPortraitFx;
        portraitFx.backlight      = settings->hudPortraitBacklight;
        portraitFx.backlightColor = settings->hudPortraitBacklightColor;
        const bool anyPortraitFx = settings->hudPortraitFx || settings->hudPortraitBacklight;

        if (useDiamondShape) {
            float cx = pos.x + size.x * 0.5f;
            float cy = pos.y + size.y * 0.5f;
            ImVec2 dTop(cx, pos.y);
            ImVec2 dRight(pos.x + size.x, cy);
            ImVec2 dBottom(cx, pos.y + size.y);
            ImVec2 dLeft(pos.x, cy);
            
            int dVtx0 = draw_list->VtxBuffer.Size;
            draw_list->AddImageQuad(
                reinterpret_cast<ImTextureID>(texture),
                dTop, dRight, dBottom, dLeft,
                lerpUV(0.5f, 0.0f), lerpUV(1.0f, 0.5f), lerpUV(0.5f, 1.0f), lerpUV(0.0f, 0.5f),
                imgTint
            );
            if (settings->hudPortraitFx)
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw_list, dVtx0, draw_list->VtxBuffer.Size,
                    pos, ImVec2(pos.x, pos.y + size.y),
                    IM_COL32_WHITE, IM_COL32(140, 148, 168, 255));

            if (anyPortraitFx)
                HUDDraw::DrawPortraitFx(draw_list, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                        1, 0.0f, portraitFx);

            // Draw diamond borders using PathStroke (follows diamond shape exactly)
            float outerWidth = ScalePx(static_cast<float>(settings->fhPortraitOuterBorderWidth));
            float innerWidth = ScalePx(static_cast<float>(settings->fhPortraitInnerBorderWidth));
            
            if (outerWidth > 0) {
                draw_list->PathLineTo(dTop);
                draw_list->PathLineTo(dRight);
                draw_list->PathLineTo(dBottom);
                draw_list->PathLineTo(dLeft);
                draw_list->PathLineTo(dTop);
                draw_list->PathStroke(hexToU32(settings->fhPortraitOuterBorderColor, 255), 0, outerWidth);
            }
            
            if (innerWidth > 0) {
                // Inset the inner border
                float inset = outerWidth > 0 ? outerWidth + ScalePx(1.0f) : ScalePx(2.0f);
                ImVec2 iTop(cx, pos.y + inset);
                ImVec2 iRight(pos.x + size.x - inset, cy);
                ImVec2 iBottom(cx, pos.y + size.y - inset);
                ImVec2 iLeft(pos.x + inset, cy);
                
                draw_list->PathLineTo(iTop);
                draw_list->PathLineTo(iRight);
                draw_list->PathLineTo(iBottom);
                draw_list->PathLineTo(iLeft);
                draw_list->PathLineTo(iTop);
                draw_list->PathStroke(hexToU32(settings->fhPortraitInnerBorderColor, 200), 0, innerWidth);
            }
            
            // Healing Priority: emerald restorative flash supersedes red damage flash
            if (healFlashIntensity > 0.0f) {
                const ImU32 healCol = HealFlashColor(healFlashIntensity, buffFlash);
                draw_list->AddQuadFilled(dTop, dRight, dBottom, dLeft, healCol);
            } else if (hitFlashIntensity > 0.0f) {
                const ImU32 flashCol = IM_COL32(255, 30, 30, static_cast<int>(hitFlashIntensity * 160.0f));
                draw_list->AddQuadFilled(dTop, dRight, dBottom, dLeft, flashCol);
            }

            ImGui::Dummy(size);
            return;
        } else if (effectiveRounding > 0.0f) {
            int pVtx0 = draw_list->VtxBuffer.Size;
            draw_list->AddImageRounded(
                reinterpret_cast<ImTextureID>(texture),
                pos,
                ImVec2(pos.x + size.x, pos.y + size.y),
                uvC0, uvC1,
                imgTint,
                effectiveRounding
            );
            if (settings->hudPortraitFx)
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw_list, pVtx0, draw_list->VtxBuffer.Size,
                    pos, ImVec2(pos.x, pos.y + size.y),
                    IM_COL32_WHITE, IM_COL32(140, 148, 168, 255));
        } else {
            int pVtx0 = draw_list->VtxBuffer.Size;
            draw_list->AddImage(
                reinterpret_cast<ImTextureID>(texture),
                pos,
                ImVec2(pos.x + size.x, pos.y + size.y),
                uvC0, uvC1,
                imgTint
            );
            if (settings->hudPortraitFx)
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw_list, pVtx0, draw_list->VtxBuffer.Size,
                    pos, ImVec2(pos.x, pos.y + size.y),
                    IM_COL32_WHITE, IM_COL32(140, 148, 168, 255));
        }

        if (anyPortraitFx)
            HUDDraw::DrawPortraitFx(draw_list, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                    0, effectiveRounding, portraitFx);

        ImVec2 pMax = ImVec2(pos.x + size.x, pos.y + size.y);
        float outerWidth = ScalePx(static_cast<float>(settings->fhPortraitOuterBorderWidth));
        float innerWidth = ScalePx(static_cast<float>(settings->fhPortraitInnerBorderWidth));
        
        if (outerWidth > 0) {
            draw_list->AddRect(pos, pMax, hexToU32(settings->fhPortraitOuterBorderColor, 255), effectiveRounding, 0, outerWidth);
        }
        if (innerWidth > 0) {
            float inset = outerWidth > 0 ? outerWidth + ScalePx(1.0f) : ScalePx(2.0f);
            float innerRounding = effectiveRounding > inset ? effectiveRounding - inset : 0;
            draw_list->AddRect(
                ImVec2(pos.x + inset, pos.y + inset), 
                ImVec2(pMax.x - inset, pMax.y - inset), 
                hexToU32(settings->fhPortraitInnerBorderColor, 200), innerRounding, 0, innerWidth);
        }
        
        // Healing Priority: emerald restorative flash supersedes red damage flash
        if (healFlashIntensity > 0.0f) {
            const ImU32 healCol = HealFlashColor(healFlashIntensity, buffFlash);
            if (effectiveRounding > 0.0f) {
                draw_list->AddRectFilled(pos, pMax, healCol, effectiveRounding);
            } else {
                draw_list->AddRectFilled(pos, pMax, healCol);
            }
        } else if (hitFlashIntensity > 0.0f) {
            const ImU32 flashCol = IM_COL32(255, 30, 30, static_cast<int>(hitFlashIntensity * 160.0f));
            if (effectiveRounding > 0.0f) {
                draw_list->AddRectFilled(pos, pMax, flashCol, effectiveRounding);
            } else {
                draw_list->AddRectFilled(pos, pMax, flashCol);
            }
        }

        ImGui::Dummy(size);
    }
    
    static std::chrono::steady_clock::time_point g_CombatExitTime;
    static bool g_WasInCombat = false;
    
    static std::set<std::string> g_QuestAllyNames;
    static std::set<RE::FormID> g_QuestAllyFormIDs;
    static bool g_QuestAllyNamesLoaded = false;
    
    static void ParsePartyListFile(const std::filesystem::path& path,
                                   const char* logTag,
                                   std::set<RE::FormID>& outFormIDs,
                                   std::set<std::string>* outNames) {
        std::ifstream file(path);
        if (!file.is_open()) return;

        size_t addedFormIDs = 0;
        size_t addedNames   = 0;

        std::string line;
        while (std::getline(file, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            if (line.empty() || line[0] == ';') continue;

            size_t pipePos = line.find('|');
            if (pipePos != std::string::npos && line.find("0x") != std::string::npos) {
                std::string pluginName = line.substr(0, pipePos);
                std::string formIdStr  = line.substr(pipePos + 1);

                RE::FormID localFormId = 0;
                if (formIdStr.size() > 2 && formIdStr[0] == '0' && (formIdStr[1] == 'x' || formIdStr[1] == 'X')) {
                    localFormId = std::stoul(formIdStr.substr(2), nullptr, 16);
                }

                if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
                    if (auto form = dataHandler->LookupForm(localFormId, pluginName)) {
                        outFormIDs.insert(form->GetFormID());
                        ++addedFormIDs;
                    } else {
                        logger::warn("{}: Could not find {}|0x{:X} (from {})",
                            logTag, pluginName, localFormId, PathUtils::ToUtf8(path.filename()));
                    }
                }
            } else if (outNames) {
                outNames->insert(line);
                ++addedNames;
            } else {
                logger::warn("{}: Invalid format '{}' in {} - expected Plugin.esp|0xFormID",
                    logTag, line, PathUtils::ToUtf8(path.filename()));
            }
        }

        logger::info("{}: Loaded {} FormIDs and {} names from {}",
            logTag, addedFormIDs, addedNames, PathUtils::ToUtf8(path.filename()));
    }

    static void LoadPartyListFromAllSources(const char* primaryFilename,
                                            const char* suffix,
                                            const char* logTag,
                                            std::set<RE::FormID>& outFormIDs,
                                            std::set<std::string>* outNames) {
        namespace fs = std::filesystem;
        const fs::path pluginsDir = "Data/SKSE/Plugins";

        ParsePartyListFile(pluginsDir / primaryFilename, logTag, outFormIDs, outNames);

        std::error_code ec;
        if (!fs::exists(pluginsDir, ec)) return;
        for (auto& entry : fs::directory_iterator(pluginsDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            const auto& p = entry.path();
            std::string name = PathUtils::ToUtf8(p.filename());
            if (name == primaryFilename) continue;
            if (name.size() > std::strlen(suffix) &&
                name.compare(name.size() - std::strlen(suffix), std::strlen(suffix), suffix) == 0) {
                ParsePartyListFile(p, logTag, outFormIDs, outNames);
            }
        }
    }

    static void LoadQuestAllyNames() {
        if (g_QuestAllyNamesLoaded) return;

        LoadPartyListFromAllSources(
            "party_allies.ini",
            "_party_allies.ini",
            "Quest Allies",
            g_QuestAllyFormIDs,
            &g_QuestAllyNames);

        g_QuestAllyNamesLoaded = true;
        logger::info("Quest Allies: Total {} FormIDs and {} names loaded from all sources",
            g_QuestAllyFormIDs.size(), g_QuestAllyNames.size());
    }
    
    static std::set<RE::FormID> g_BlacklistFormIDs;
    static std::set<std::string> g_BlacklistNames;
    static bool g_BlacklistLoaded = false;
    
    static void LoadBlacklist() {
        if (g_BlacklistLoaded) return;

        LoadPartyListFromAllSources(
            "party_blacklist.ini",
            "_party_blacklist.ini",
            "Blacklist",
            g_BlacklistFormIDs,
            &g_BlacklistNames);

        g_BlacklistLoaded = true;
        logger::info("Blacklist: Total {} FormIDs and {} names loaded from all sources",
            g_BlacklistFormIDs.size(), g_BlacklistNames.size());
    }
    
    static bool IsBlacklisted(RE::Actor* actor) {
        if (!actor) return false;
        
        if (!g_BlacklistLoaded) {
            LoadBlacklist();
        }
        
        auto baseForm = actor->GetActorBase();
        if (baseForm && g_BlacklistFormIDs.count(baseForm->GetFormID())) {
            return true;
        }
        
        const char* name = actor->GetName();
        if (name && g_BlacklistNames.count(name)) {
            return true;
        }
        
        return false;
    }
    
    static std::set<RE::FormID> g_WhitelistFormIDs;
    static bool g_WhitelistLoaded = false;
    
    static void LoadWhitelist() {
        if (g_WhitelistLoaded) return;

        LoadPartyListFromAllSources(
            "party_whitelist.ini",
            "_party_whitelist.ini",
            "Whitelist",
            g_WhitelistFormIDs,
            nullptr);

        g_WhitelistLoaded = true;
        logger::info("Whitelist: Total {} FormIDs loaded from all sources", g_WhitelistFormIDs.size());
    }
    
    static bool IsWhitelisted(RE::Actor* actor) {
        if (!actor) return false;
        
        if (!g_WhitelistLoaded) {
            LoadWhitelist();
        }
        
        auto baseForm = actor->GetActorBase();
        if (baseForm && g_WhitelistFormIDs.count(baseForm->GetFormID())) {
            return true;
        }
        
        return false;
    }
    
    static bool IsWhitelistedFollower(RE::Actor* actor) {
        if (!actor || actor->IsDead()) return false;
        if (!IsWhitelisted(actor)) return false;
        
        static RE::TESFaction* petFollowingFaction = nullptr;
        static RE::TESFaction* spiderFriendFaction = nullptr;
        static bool factionLookupDone = false;

        if (!factionLookupDone) {
            if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
                petFollowingFaction = dataHandler->LookupForm<RE::TESFaction>(0x2F1D, "Update.esm");
                if (!petFollowingFaction) {
                    logger::warn("Whitelist: Could not find PetFramework_PetFollowingFaction (Update.esm|0x2F1D)");
                }
                spiderFriendFaction = dataHandler->LookupForm<RE::TESFaction>(0x017964, "Dragonborn.esm");
                if (!spiderFriendFaction) {
                    logger::warn("Whitelist: Could not find DLC2ExpSpiderFriendFaction (Dragonborn.esm|0x017964)");
                }
            }
            factionLookupDone = true;
        }

        if (petFollowingFaction && actor->IsInFaction(petFollowingFaction)) {
            auto rank = actor->GetFactionRank(petFollowingFaction, false);
            if (rank >= 1) {
                logger::debug("Whitelist: {} is in PetFollowingFaction with rank {}", actor->GetName(), rank);
                return true;
            }
        }

        if (spiderFriendFaction && actor->IsInFaction(spiderFriendFaction)) {
            auto rank = actor->GetFactionRank(spiderFriendFaction, false);
            if (rank >= 0) {
                logger::debug("Whitelist: {} is in DLC2ExpSpiderFriendFaction with rank {}", actor->GetName(), rank);
                return true;
            }
        }
        
        if (auto currentPackage = actor->GetCurrentPackage()) {
            auto procedureType = currentPackage->packData.packType.get();
            
            if (procedureType == RE::PACKAGE_TYPE::kFollow ||
                procedureType == RE::PACKAGE_TYPE::kEscort ||
                procedureType == RE::PACKAGE_TYPE::kTravel ||
                procedureType == RE::PACKAGE_TYPE::kAccompany) {
                return true;
            }
        }
        
        return false;
    }
    
    bool ShouldShow() {
        auto player = RE::PlayerCharacter::GetSingleton();
        
        if (!player || !player->Is3DLoaded()) {
            return false;
        }
        
        if (MenuUtils::ShouldBlockOverlay()) {
            return false;
        }

        if (PlayerStatus::GetTransformState() != PlayerStatus::TransformState::None) {
            return false;
        }
        
        auto settings = Settings::GetSingleton();
        switch (settings->followerHudMode) {
            case 0: return false;
            case 1: return true;
            case 2: {
                bool inCombat = player->IsInCombat();

                bool anyHurt = false;
                if (!inCombat) {
                    for (auto* actor : GetCachedAllies()) {
                        if (actor && !actor->IsDead()) {
                            auto* avo = actor->AsActorValueOwner();
                            if (!avo) continue;
                            float cur = avo->GetActorValue(RE::ActorValue::kHealth);
                            float max = avo->GetBaseActorValue(RE::ActorValue::kHealth);
                            if (max > 0.0f && cur < max) { anyHurt = true; break; }
                        }
                    }
                }

                if (inCombat || anyHurt) {
                    g_WasInCombat = true;
                    return true;
                }

                if (g_WasInCombat) {
                    g_CombatExitTime = std::chrono::steady_clock::now();
                    g_WasInCombat = false;
                }
                
                auto elapsed = std::chrono::steady_clock::now() - g_CombatExitTime;
                auto delayMs = std::chrono::seconds(settings->fadeDelay);
                return elapsed < delayMs;
            }
            case 3: return true;
            default: return false;
        }
    }
    
    static bool IsQuestAlly(RE::Actor* actor) {
        if (!actor || actor->IsDead()) return false;
        
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || actor == player) return false;
        
        if (actor->IsPlayerTeammate()) return false;
        
        if (actor->IsSummoned() || actor->IsCommandedActor()) return false;
        
        auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005A1A4);
        if (followerFaction && actor->IsInFaction(followerFaction)) {
            if (player->IsInCombat() || actor->IsInCombat()) {
                return true;
            }
        }
        
        // Check 2: FormID or Name whitelist + AI package (prevents showing ALL combat NPCs)
        LoadQuestAllyNames();
        
        bool isWhitelisted = false;
        auto baseForm = actor->GetActorBase();
        if (baseForm && g_QuestAllyFormIDs.count(baseForm->GetFormID())) {
            isWhitelisted = true;
        }
        
        if (!isWhitelisted) {
            std::string name = actor->GetDisplayFullName();
            if (g_QuestAllyNames.find(name) != g_QuestAllyNames.end()) {
                isWhitelisted = true;
            }
        }
        
        if (isWhitelisted) {
            if (auto currentPackage = actor->GetCurrentPackage()) {
                auto procedureType = currentPackage->packData.packType.get();
                int pkgType = static_cast<int>(procedureType);
                
                if (procedureType == RE::PACKAGE_TYPE::kFollow ||
                    procedureType == RE::PACKAGE_TYPE::kEscort ||
                    procedureType == RE::PACKAGE_TYPE::kTravel ||
                    procedureType == RE::PACKAGE_TYPE::kAccompany) {
                    return true;
                }
                
                if (pkgType == 18 && (player->IsInCombat() || actor->IsInCombat())) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    void MarkSummonsDirty() {
        g_SummonsDirty = true;
        logger::debug("FollowerHUD: Summons cache marked dirty");
    }
    
    void MarkQuestAlliesDirty() {
        g_QuestAlliesDirty = true;
        logger::debug("FollowerHUD: Quest allies cache marked dirty");
    }
    
    void MarkCacheDirty() {
        g_SummonsDirty = true;
        g_FollowersDirty = true;
        g_QuestAlliesDirty = true;
        logger::debug("FollowerHUD: All caches marked dirty");
    }
    
    void RemoveFromCache(RE::Actor* actor) {
        if (!actor) return;
        std::lock_guard lk(g_CacheMutex);

        bool removed = false;
        
        auto itS = std::find(g_CachedSummons.begin(), g_CachedSummons.end(), actor);
        if (itS != g_CachedSummons.end()) {
            g_CachedSummons.erase(itS);
            removed = true;
        }
        
        auto itF = std::find(g_CachedFollowers.begin(), g_CachedFollowers.end(), actor);
        if (itF != g_CachedFollowers.end()) {
            g_CachedFollowers.erase(itF);
            removed = true;
        }
        
        auto itQ = std::find(g_CachedQuestAllies.begin(), g_CachedQuestAllies.end(), actor);
        if (itQ != g_CachedQuestAllies.end()) {
            g_CachedQuestAllies.erase(itQ);
            removed = true;
        }
        
        if (removed) {
            logger::debug("FollowerHUD: Removed actor from cache(s)");
        }
    }
    
    
    static void RefreshSummonsCache() {
        std::lock_guard lk(g_CacheMutex);
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        
        std::set<RE::Actor*> currentSummons;
        
        if (auto process = player->GetActorRuntimeData().currentProcess) {
            if (auto middleHigh = process->middleHigh) {
                for (auto& cmdActor : middleHigh->commandedActors) {
                    if (auto actor = cmdActor.commandedActor.get().get()) {
                         if (IsActorValid(actor) && !actor->IsDead()) {
                             currentSummons.insert(actor);
                         }
                    }
                }
            }
        }

        auto* settings = Settings::GetSingleton();
        if (settings && settings->showFollowerSummons && !g_CachedFollowers.empty()) {
            std::set<RE::Actor*> followerSet(g_CachedFollowers.begin(), g_CachedFollowers.end());
            if (auto* pl = RE::ProcessLists::GetSingleton()) {
                for (auto& handle : pl->highActorHandles) {
                    if (auto actor = handle.get().get()) {
                        if (!IsActorValid(actor)) continue;
                        if (actor->IsDead()) continue;
                        if (!actor->IsCommandedActor()) continue;
                        if (auto cmdr = actor->GetCommandingActor()) {
                            if (followerSet.count(cmdr.get())) {
                                currentSummons.insert(actor);
                            }
                        }
                    }
                }
            }
        }

        for (auto it = g_CachedSummons.begin(); it != g_CachedSummons.end(); ) {
            RE::Actor* actor = *it;
            if (currentSummons.find(actor) == currentSummons.end()) {
                it = g_CachedSummons.erase(it);
            } else {
                ++it;
            }
        }
        
        for (auto* actor : currentSummons) {
             if (std::find(g_CachedSummons.begin(), g_CachedSummons.end(), actor) == g_CachedSummons.end()) {
                 g_CachedSummons.push_back(actor);
             }
        }
        
        g_SummonsDirty = false;
    }
    
    static void RefreshFollowersCache() {
        std::lock_guard lk(g_CacheMutex);
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        
        std::set<RE::Actor*> loadedTeammates;
        if (auto processLists = RE::ProcessLists::GetSingleton()) {
            for (auto& handle : processLists->highActorHandles) {
                if (auto actor = handle.get().get()) {
                    if (actor->IsPlayerTeammate() && !actor->IsDead()) {
                        if (actor->IsSummoned() || actor->IsCommandedActor()) continue;
                        if (auto* base = actor->GetActorBase()) {
                            if (auto* race = base->GetRace()) {
                                const char* eid = race->GetFormEditorID();
                                if (eid && strcmp(eid, "HorseRace") == 0) continue;
                            }
                        }

                        loadedTeammates.insert(actor);
                    }
                }
            }
        }
        
        for (auto it = g_CachedFollowers.begin(); it != g_CachedFollowers.end(); ) {
            RE::Actor* actor = *it;
            
            if (loadedTeammates.find(actor) != loadedTeammates.end()) {
                ++it;
                continue;
            }
            
            
            if (IsActorValid(actor)) {
                if ((actor->IsPlayerTeammate() && !actor->IsDead()) ||
                    IsWhitelistedFollower(actor)) {
                    ++it;
                } else {
                    it = g_CachedFollowers.erase(it);
                }
            } else {
                it = g_CachedFollowers.erase(it);
            }
        }
        
        for (auto* actor : loadedTeammates) {
            if (std::find(g_CachedFollowers.begin(), g_CachedFollowers.end(), actor) == g_CachedFollowers.end()) {
                g_CachedFollowers.push_back(actor);
            }
        }
        
        if (!g_WhitelistLoaded) {
            LoadWhitelist();
        }
        if (!g_WhitelistFormIDs.empty()) {
            if (auto processLists = RE::ProcessLists::GetSingleton()) {
                for (auto& handle : processLists->highActorHandles) {
                    if (auto actor = handle.get().get()) {
                        if (std::find(g_CachedFollowers.begin(), g_CachedFollowers.end(), actor) != g_CachedFollowers.end()) {
                            continue;
                        }
                        
                        if (IsWhitelisted(actor)) {
                            logger::debug("Whitelist: Checking whitelisted actor: {} (3DLoaded={})", 
                                        actor->GetName(), actor->Is3DLoaded());
                        }
                        
                        if (IsWhitelistedFollower(actor) && actor->Is3DLoaded()) {
                            g_CachedFollowers.push_back(actor);
                            logger::debug("FollowerHUD: Added whitelisted follower: {}", actor->GetName());
                        }
                    }
                }
            }
        }
        
        g_FollowersDirty = false;
        logger::debug("FollowerHUD: Followers refreshed - {} found", g_CachedFollowers.size());
    }
    
    static void RefreshQuestAlliesCache() {
        std::lock_guard lk(g_CacheMutex);
        g_CachedQuestAllies.clear();
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            g_QuestAlliesDirty = false;
            return;
        }
        
        auto settings = Settings::GetSingleton();
        if (!settings->showQuestAllies) {
            g_QuestAlliesDirty = false;
            return;
        }
        
        if (auto processLists = RE::ProcessLists::GetSingleton()) {
            for (auto& handle : processLists->highActorHandles) {
                if (auto actor = handle.get().get()) {
                    if (actor->IsPlayerTeammate()) continue;
                    if (actor == player) continue;
                    
                    if (IsQuestAlly(actor) && actor->Is3DLoaded() && !actor->IsDead()) {
                        g_CachedQuestAllies.push_back(actor);
                    }
                }
            }
        }
        
        g_QuestAlliesDirty = false;
        logger::debug("FollowerHUD: Quest allies refreshed - {} found", g_CachedQuestAllies.size());
    }
    
    std::vector<RE::Actor*> GetCachedAllies() {
        std::lock_guard lk(g_CacheMutex);
        std::vector<RE::Actor*> merged;
        merged.reserve(g_CachedSummons.size() + g_CachedFollowers.size() + g_CachedQuestAllies.size());
        
        for (auto* actor : g_CachedSummons) {
            if (IsActorValid(actor) && !IsBlacklisted(actor)) merged.push_back(actor);
        }
        for (auto* actor : g_CachedFollowers) {
            if (IsActorValid(actor) && !IsBlacklisted(actor)) merged.push_back(actor);
        }
        for (auto* actor : g_CachedQuestAllies) {
            if (IsActorValid(actor) && !IsBlacklisted(actor)) merged.push_back(actor);
        }
        
        return merged;
    }
    
    void RefreshCache() {
        RefreshSummonsCache();
        RefreshFollowersCache();
        RefreshQuestAlliesCache();
        g_LastCacheRefresh = std::chrono::steady_clock::now();
    }
    
    std::vector<RE::Actor*> GetAllies() {
        std::lock_guard lk(g_CacheMutex);
        auto settings = Settings::GetSingleton();
        
        if (settings->showSummons) {
            RefreshSummonsCache();
            if (PerfStats::g_Enabled) ++PerfStats::g_Current.summonRefreshes;
        }
        if (settings->showFollowers) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_LastFollowerRefresh).count();
            if (g_FollowersDirty || elapsed >= 3) {
                RefreshFollowersCache();
                g_LastFollowerRefresh = now;
            }
        }
        if (g_QuestAlliesDirty && settings->showQuestAllies) {
            RefreshQuestAlliesCache();
        }
        
        std::vector<RE::Actor*> allies;
        allies.reserve(g_CachedSummons.size() + g_CachedFollowers.size() + g_CachedQuestAllies.size());
        std::set<RE::FormID> seenFormIDs;
        
        if (settings->showSummons) {
            for (auto* actor : g_CachedSummons) {
                if (IsActorValid(actor) && actor->Is3DLoaded() && !actor->IsDead() && !IsBlacklisted(actor)) {
                    RE::FormID formID = actor->GetFormID();
                    if (seenFormIDs.find(formID) == seenFormIDs.end()) {
                        seenFormIDs.insert(formID);
                        allies.push_back(actor);
                    }
                }
            }
        }
        
        if (settings->showFollowers) {
            for (auto* actor : g_CachedFollowers) {
                if (IsActorValid(actor) && actor->Is3DLoaded() && !actor->IsDead() && !IsBlacklisted(actor)) {
                    RE::FormID formID = actor->GetFormID();
                    if (seenFormIDs.find(formID) == seenFormIDs.end()) {
                        seenFormIDs.insert(formID);
                        allies.push_back(actor);
                    }
                }
            }
        }
        
        if (settings->showQuestAllies) {
            for (auto* actor : g_CachedQuestAllies) {
                if (IsActorValid(actor) && actor->Is3DLoaded() && !actor->IsDead() && !IsBlacklisted(actor)) {
                    RE::FormID formID = actor->GetFormID();
                    if (seenFormIDs.find(formID) == seenFormIDs.end()) {
                        seenFormIDs.insert(formID);
                        allies.push_back(actor);
                    }
                }
            }
        }
        
        static std::vector<RE::FormID> s_DisplayOrder;
        std::vector<RE::Actor*> ordered;
        ordered.reserve(allies.size());
        for (auto it = s_DisplayOrder.begin(); it != s_DisplayOrder.end(); ) {
            RE::Actor* found = nullptr;
            for (auto* a : allies)
                if (a->GetFormID() == *it) { found = a; break; }
            if (found) { ordered.push_back(found); ++it; }
            else       { it = s_DisplayOrder.erase(it); }
        }
        for (auto* a : allies) {
            if (std::find(s_DisplayOrder.begin(), s_DisplayOrder.end(), a->GetFormID()) == s_DisplayOrder.end()) {
                s_DisplayOrder.push_back(a->GetFormID());
                ordered.push_back(a);
            }
        }

        if (PerfStats::g_Enabled) PerfStats::g_Current.allyCount = static_cast<int>(ordered.size());
        return ordered;
    }

static void DrawCustomProgressBar(const char* label, float fraction, ImVec2 size,
                                   ImVec4 fgColor, float cornerRadius,
                                   const char* overlayText = nullptr,
                                   int outerBorderWidth = 0, ImVec4 borderColor = ImVec4(0,0,0,1),
                                   int innerBorderWidth = 0, ImVec4 innerBorderColor = ImVec4(0,0,0,0),
                                   ImVec4 gradientColor = ImVec4(-1,-1,-1,-1),
                                   float ghostFraction = 0.0f,
                                   float maxValue = 100.0f) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    auto settings = Settings::GetSingleton();
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 endPos = ImVec2(pos.x + size.x, pos.y + size.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (settings->barStyleEnabled) {
        ID3D11ShaderResourceView* bgTexture = BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, settings->barStyleIndex);
        ID3D11ShaderResourceView* fillTexture = BackgroundManager::GetBarFillTexture(settings->barStyleIndex);

        if (bgTexture && fillTexture) {
            const float uvMargin = 0.001f;
            drawList->AddImage(reinterpret_cast<ImTextureID>(bgTexture), pos, endPos,
                ImVec2(uvMargin, uvMargin), ImVec2(1.0f - uvMargin, 1.0f - uvMargin));

            if (fraction > 0.0f) {
                float visibleWidth = size.x * (std::max)(0.0f, (std::min)(1.0f, fraction));
                ImVec2 fillEnd = ImVec2(pos.x + visibleWidth, endPos.y);
                ImVec2 uvEnd = ImVec2((std::max)(0.0f, (std::min)(1.0f, fraction)), 1.0f);
                ImU32 tintColor = ImGui::ColorConvertFloat4ToU32(fgColor);
                drawList->AddImage(reinterpret_cast<ImTextureID>(fillTexture), pos, fillEnd, ImVec2(0, 0), uvEnd, tintColor);
            }

            if (ghostFraction > fraction + 0.001f) {
                float g0 = (std::max)(0.0f, (std::min)(1.0f, fraction));
                float g1 = (std::max)(0.0f, (std::min)(1.0f, ghostFraction));
                drawList->AddImage(reinterpret_cast<ImTextureID>(fillTexture),
                    ImVec2(pos.x + size.x * g0, pos.y), ImVec2(pos.x + size.x * g1, endPos.y),
                    ImVec2(g0, 0), ImVec2(g1, 1), IM_COL32(110, 106, 100, 150));
            }

            if (overlayText && strlen(overlayText) > 0) {
                ImVec2 textSize = ImGui::CalcTextSize(overlayText);
                ImVec2 textPos = ImVec2(pos.x + (size.x - textSize.x) * 0.5f,
                                         pos.y + (size.y - textSize.y) * 0.5f);
                float r = ((settings->barTextColor >> 16) & 0xFF) / 255.0f;
                float g = ((settings->barTextColor >> 8) & 0xFF) / 255.0f;
                float b = (settings->barTextColor & 0xFF) / 255.0f;
                ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.0f));
                drawList->AddText(ImVec2(textPos.x + ScalePx(1.0f), textPos.y + ScalePx(1.0f)), IM_COL32(0, 0, 0, 178), overlayText);
                drawList->AddText(textPos, textColor, overlayText);
            }

            ImGui::ItemSize(size);
            ImGui::ItemAdd(ImRect(pos, endPos), ImGui::GetID(label));
            return;
        }
    }

    int visualStyle = settings->barVisualStyle;

    auto vecToArgb = [](const ImVec4& v) -> std::uint32_t {
        return (static_cast<std::uint32_t>(v.w * 255.0f + 0.5f) << 24) |
               (static_cast<std::uint32_t>(v.x * 255.0f + 0.5f) << 16) |
               (static_cast<std::uint32_t>(v.y * 255.0f + 0.5f) << 8) |
                static_cast<std::uint32_t>(v.z * 255.0f + 0.5f);
    };

    HUDDraw::BarStyle bs;
    bs.width    = size.x;
    bs.height   = size.y;
    bs.fraction = fraction;
    bs.rounding = cornerRadius;
    bs.leftEnd  = settings->barEndStyleLeft;
    bs.rightEnd = settings->barEndStyle;

    bs.drawFill  = (visualStyle != 4);
    bs.drawTrack = settings->barTrackEnabled && (visualStyle != 4);
    bs.duotone   = settings->barDepthShading;
    bs.edgeLight = settings->barEdgeLightEnabled;
    bs.tip       = settings->barTipEnabled;
    bs.fillColor = vecToArgb(fgColor);
    if (visualStyle == 1 || visualStyle == 2) {
        ImVec4 gradEnd = (gradientColor.x >= 0)
            ? gradientColor
            : ImVec4(fgColor.x * 0.5f, fgColor.y * 0.5f, fgColor.z * 0.5f, fgColor.w);
        bs.fillColor2 = vecToArgb(gradEnd);
    }
    bs.gloss    = (visualStyle == 2);
    if (visualStyle == 3) {
        int points = settings->hudBarSegmentPoints;
        bs.segments = (points > 0) ? std::max(1, static_cast<int>(std::roundf(maxValue / static_cast<float>(points)))) : 10;
        bs.segmentGap = ScalePx(static_cast<float>(settings->hudBarSegmentGap));
    } else {
        bs.segments = 0;
    }
    bs.glow     = settings->barGlowEnabled;

    bs.outerBorderWidth = static_cast<float>(outerBorderWidth);
    bs.outerBorderColor = vecToArgb(borderColor);
    bs.innerBorderWidth = static_cast<float>(innerBorderWidth);
    bs.innerBorderColor = vecToArgb(innerBorderColor);
    bs.ghostFraction    = ghostFraction;

    HUDDraw::DrawBarStyled(drawList, pos, bs);

    if (overlayText && strlen(overlayText) > 0) {
        ImVec2 textSize = ImGui::CalcTextSize(overlayText);
        ImVec2 textPos = ImVec2(pos.x + (size.x - textSize.x) * 0.5f,
                                 pos.y + (size.y - textSize.y) * 0.5f);
        ImU32 textColor = IM_COL32((settings->barTextColor >> 16) & 0xFF,
                                   (settings->barTextColor >> 8) & 0xFF,
                                   settings->barTextColor & 0xFF, 255);
        HUDDraw::DrawTextStyled(drawList, textPos, textColor, overlayText, HUDDraw::TextFx{});
    }

    ImGui::ItemSize(size);
    ImGui::ItemAdd(ImRect(pos, endPos), ImGui::GetID(label));
}

    static ImVec2 RenderIdlePortraitStack(const std::vector<size_t>& pileIdx,
                                          const std::vector<RE::Actor*>& allies,
                                          bool isHorizontal, float stackT,
                                          bool growUpward, float anchorBottom,
                                          float bottomRowHeight, float paddingTop, const std::vector<ImVec2>& portraitPositions) {
        auto settings = Settings::GetSingleton();
        ImVec2 origin = ImGui::GetCursorScreenPos();

        const float portraitSize = ScalePx(static_cast<float>(settings->portraitSize));
        const float coinStep = portraitSize * 0.5f;
        const size_t n = pileIdx.size();

        float pileExtent = portraitSize + (n > 0 ? coinStep * static_cast<float>(n - 1) : 0.0f);

        float vBase = (!isHorizontal && growUpward) ? (anchorBottom - pileExtent) : origin.y;

        float hBase = (isHorizontal && growUpward) ? (anchorBottom - bottomRowHeight + paddingTop) : origin.y;

        bool inTravelPhase = stackT >= kStackFadePhase;
        float travelT = inTravelPhase
            ? (stackT - kStackFadePhase) / (1.0f - kStackFadePhase)
            : 0.0f;
        float coinAlpha = inTravelPhase ? 1.0f : (stackT / kStackFadePhase);

        for (size_t k = 0; k < n; ++k) {
            size_t globalIdx = pileIdx[k];
            RE::Actor* actor = allies[globalIdx];
            if (!IsActorValid(actor)) continue;

            ID3D11ShaderResourceView* portrait = TextureManager::GetPortrait(actor);
            if (!portrait) continue;

            float alongCoin = static_cast<float>(k) * coinStep;
            ImVec2 coinPos = isHorizontal ? ImVec2(origin.x + alongCoin, hBase)
                                          : ImVec2(origin.x, vBase + alongCoin);

            // Travel origin (travelT=0): the REAL ribbon position cached on the last
            ImVec2 startPos = (globalIdx < portraitPositions.size())
                ? portraitPositions[globalIdx]
                : coinPos;

            ImVec2 drawPos(startPos.x + (coinPos.x - startPos.x) * travelT,
                           startPos.y + (coinPos.y - startPos.y) * travelT);

            ImGui::SetCursorScreenPos(drawPos);
            DrawRoundedImage(portrait, ImVec2(portraitSize, portraitSize),
                             ScalePx(static_cast<float>(settings->portraitCornerRadius)), coinAlpha);
            DrawSummonLifetimeBorder(ImGui::GetWindowDrawList(), drawPos,
                ImVec2(drawPos.x + portraitSize, drawPos.y + portraitSize), settings->portraitShape,
                ScalePx(static_cast<float>(settings->portraitCornerRadius)), actor, false, coinAlpha);

        }

        return isHorizontal ? ImVec2(pileExtent, portraitSize) : ImVec2(portraitSize, pileExtent);
    }

    // Core HUD rendering logic - use within an existing ImGui frame
    static void RenderAllyWindow(const std::vector<RE::Actor*>& allies, bool allowDrag, bool summonWindow) {
        auto settings = Settings::GetSingleton();

        auto& state = summonWindow ? g_SummonWindow : g_FollowerWindow;
        auto& posX = summonWindow ? settings->hudSummonPosX : settings->hudPosX;
        auto& posY = summonWindow ? settings->hudSummonPosY : settings->hudPosY;
        const bool growUpward = summonWindow ? settings->hudSummonGrowUpward : settings->hudGrowUpward;
        const bool anchorRight = summonWindow ? settings->hudSummonAnchorRight : settings->hudAnchorRight;
        if (allies.empty()) {
            state.stackT = 0.0f;
            state.stackTickInit = false;
            state.stackCalm = false;
            state.stackPortraitPos.clear();
            return;
        }

        bool isHorizontal = (settings->hudLayout == 1);
        bool showText = settings->showTextOnBars;

        const bool stackFeatureEligible = settings->hudIdlePortraitStack
            && settings->followerHudMode == 1
            && settings->showPortrait
            && !allowDrag;

        if (stackFeatureEligible) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            bool inCombat = player && player->IsInCombat();
            bool anyHurt = false;
            if (!inCombat) {
                for (auto* actor : allies) {
                    if (!IsActorValid(actor) || actor->IsDead()) continue;
                    auto st = GetActorStats(actor);
                    if (st.health.current < st.health.GetMax() - 1.0f) { anyHurt = true; break; }
                }
            }
            bool calmNow = !inCombat && !anyHurt;

            const auto now = std::chrono::steady_clock::now();
            if (calmNow && !state.stackCalm) {
                state.stackCalmSince = now;  // calm just started: begin the restack hold
            }
            state.stackCalm = calmNow;

            bool wantStacked = calmNow &&
                std::chrono::duration<float>(now - state.stackCalmSince).count() >= kStackRestackHoldSeconds;

            // Own steady_clock delta: see state.stackLastTick declaration for why
            if (!state.stackTickInit) {
                state.stackLastTick = now;
                state.stackTickInit = true;
            }
            float dt = std::chrono::duration<float>(now - state.stackLastTick).count();
            state.stackLastTick = now;
            dt = std::clamp(dt, 0.0f, 0.1f);

            constexpr float kStackEaseSeconds = 0.35f;
            float step = dt / kStackEaseSeconds;
            state.stackT = wantStacked ? (std::min)(1.0f, state.stackT + step)
                                   : (std::max)(0.0f, state.stackT - step);
        } else {
            // Feature off / mode != Always / no portrait / Settings preview active: pin
            state.stackT = 0.0f;
            state.stackCalm = false;
            state.stackTickInit = false;
        }
        // Smoothstep: eases both ends of the transition instead of a linear crossfade.
        const float stackT = state.stackT * state.stackT * (3.0f - 2.0f * state.stackT);
        
        auto hexToImVec4 = [](int hex, float alpha = 1.0f) {
            float r = ((hex >> 16) & 0xFF) / 255.0f;
            float g = ((hex >> 8) & 0xFF) / 255.0f;
            float b = (hex & 0xFF) / 255.0f;
            return ImVec4(r, g, b, alpha);
        };
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        
        // Position from settings: with optional upward growth (both orientations:
        ImVec2 targetPos(static_cast<float>(posX), static_cast<float>(posY));
        if (growUpward) {
            targetPos.y -= state.cachedHeight;
        }
        if (anchorRight) {
            targetPos.x -= state.cachedWidth;
        }
        const bool forcedThisFrame = !allowDrag || state.forcePosition;
        if (forcedThisFrame) {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
            state.forcePosition = false;
        } else {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
        }

        ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoBackground;
        if (!allowDrag) {
            hudFlags |= ImGuiWindowFlags_NoMove;
        }
        
        ImGui::Begin(summonWindow ? "SummonHUD" : "FollowerHUD", nullptr, hudFlags);
        ImGui::SetWindowFontScale(1.0f);
        
        ImVec4 ribbonBgCol = hexToImVec4(settings->ribbonBgColor, settings->ribbonBgOpacity);
        ImVec4 ribbonBorderCol = hexToImVec4(settings->ribbonBorderColor, 1.0f);
        float ribbonRounding = ScalePx(settings->ribbonCornerRadius);
        float ribbonBorderSize = ScalePx(static_cast<float>(settings->ribbonBorderWidth));
        float ribbonPaddingTop = ScalePx(static_cast<float>(settings->ribbonPaddingTop));
        float ribbonPaddingRight = ScalePx(static_cast<float>(settings->ribbonPaddingRight));
        float ribbonPaddingBottom = ScalePx(static_cast<float>(settings->ribbonPaddingBottom));
        float ribbonPaddingLeft = ScalePx(static_cast<float>(settings->ribbonPaddingLeft));
        
        // Tier 1: Bar styling
        float barCornerRadius = ScalePx(settings->barCornerRadius);
        float barSpacing = ScalePx(static_cast<float>(settings->barSpacing));
        bool compactBars = settings->compactBars;
        float compactRatio = settings->compactBarRatio;
        
        int maxToShow = settings->maxFollowers;
        int maxPerWrap = settings->maxPerColumnOrRow;

        size_t shownCount = (std::min)(allies.size(), static_cast<size_t>((std::max)(maxToShow, 0)));
        std::vector<size_t> emitIdx;
        std::vector<bool>   emitGroupEnd;
        std::vector<int>    emitIndentCells(shownCount, 0);
        {
            size_t wrap       = static_cast<size_t>(maxPerWrap);
            size_t numGroups  = shownCount > 0 ? (shownCount - 1) / wrap + 1 : 0;
            bool   revGroups  = (anchorRight && !isHorizontal) ||
                                (growUpward  && isHorizontal);
            bool   revMembers = (anchorRight && isHorizontal) ||
                                (growUpward  && !isHorizontal);
            emitIdx.reserve(shownCount);
            emitGroupEnd.reserve(shownCount);
            for (size_t gi = 0; gi < numGroups; ++gi) {
                size_t g     = revGroups ? numGroups - 1 - gi : gi;
                size_t begin = g * wrap;
                size_t end   = (std::min)(begin + wrap, shownCount);
                // Bottom-align a partial column (vertical growUpward only): only
                if (growUpward && !isHorizontal && numGroups > 1 && end - begin < wrap)
                    emitIndentCells[emitIdx.size()] = static_cast<int>(wrap - (end - begin));
                if (revMembers) {
                    for (size_t j = end; j > begin; --j) { emitIdx.push_back(j - 1); emitGroupEnd.push_back(j - 1 == begin); }
                } else {
                    for (size_t j = begin; j < end; ++j) { emitIdx.push_back(j); emitGroupEnd.push_back(j == end - 1); }
                }
            }
        }

        int maxEffectRowsInLine = 0;

        bool runRibbon = stackT < kStackFadePhase;
        bool runCoin   = stackT > 0.0f;

        float ribbonFadeAlpha = runRibbon ? (1.0f - stackT / kStackFadePhase) : 0.0f;

        auto runCoinPass = [&]() {
            if (!runCoin) return;
            ImDrawList* stackDl = ImGui::GetWindowDrawList();
            stackDl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);
            // anchorBottom: this path only runs with !allowDrag, so forcedThisFrame is
            float anchorBottom = static_cast<float>(posY);

            std::vector<size_t> pileIdx;
            pileIdx.reserve(shownCount);
            bool reversePile = isHorizontal ? anchorRight : growUpward;
            if (reversePile) {
                for (size_t j = shownCount; j > 0; --j) pileIdx.push_back(j - 1);
            } else {
                for (size_t j = 0; j < shownCount; ++j) pileIdx.push_back(j);
            }

            size_t wrapForPile     = static_cast<size_t>(maxPerWrap);
            size_t numGroupsForPile = shownCount > 0 ? (shownCount - 1) / wrapForPile + 1 : 1;
            float rowHeight = state.stackFullHeight / static_cast<float>(numGroupsForPile); // numGroupsForPile is always >= 1

            ImVec2 pileSize = RenderIdlePortraitStack(pileIdx, allies, isHorizontal, stackT,
                                                       growUpward, anchorBottom,
                                                       rowHeight, ribbonPaddingTop, state.stackPortraitPos);
            stackDl->PopClipRect();

            if (!runRibbon) {
                ImGui::Dummy(pileSize);
            }
        };

        if (runRibbon) {

        // count before the loop writes into it by global index. Sized to allies.size()
        if (state.stackPortraitPos.size() != allies.size()) state.stackPortraitPos.resize(allies.size());

        if (!isHorizontal && !emitIdx.empty()) {
            ImGui::BeginGroup();
        }

        for (size_t i = 0; i < emitIdx.size(); i++) {
            size_t globalIdx = emitIdx[i];
            auto actor = allies[globalIdx];

            if (!IsActorValid(actor)) {
                continue;
            }
            
            auto actorStats = GetActorStats(actor);
            if (PerfStats::g_Enabled) ++PerfStats::g_Current.allyStatsQueries;
            
            const char* actorName = actor->GetDisplayFullName();
            if (!actorName || strlen(actorName) == 0) {
                 actorName = actor->GetName();
            }
            std::string displayName = (actorName && strlen(actorName) > 0) ? actorName : LocalizationManager::Get("Ally");
            if (settings->nameFirstWordOnly) {
                size_t spacePos = displayName.find(' ');
                if (spacePos != std::string::npos) {
                    displayName = displayName.substr(0, spacePos);
                }
            }
            if (settings->nameUppercase) {
                displayName = toUppercase(displayName);
            }
            
            float health = actorStats.health.current;
            float maxHealth = actorStats.health.GetMax();
            float magicka = actorStats.magicka.current;
            float maxMagicka = actorStats.magicka.GetMax();
            float stamina = actorStats.stamina.current;
            float maxStamina = actorStats.stamina.GetMax();
            
            float healthPercent = actorStats.health.GetPercent();
            float magickaPercent = actorStats.magicka.GetPercent();
            float staminaPercent = actorStats.stamina.GetPercent();
            if (settings->barSmoothDrain) {
                const std::uint64_t sid = static_cast<std::uint64_t>(actor->GetFormID()) << 2;
                healthPercent  = HUDDraw::TickBarValue(sid | 0, healthPercent);
                magickaPercent = HUDDraw::TickBarValue(sid | 1, magickaPercent);
                staminaPercent = HUDDraw::TickBarValue(sid | 2, staminaPercent);
            }
            
            std::uint16_t level = static_cast<std::uint16_t>(actorStats.level);
            
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ribbonBgCol);
            ImGui::PushStyleColor(ImGuiCol_Border, ribbonBorderCol);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ribbonRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, ribbonBorderSize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, (float)barSpacing));
            
            // Calculate size for the child window
            float barWidth = ScalePx(static_cast<float>(settings->hudBarWidth));
            float barHeight = ScalePx(static_cast<float>(settings->hudBarHeight));
            
            const float portraitSize = ScalePx(static_cast<float>(settings->portraitSize));
            ID3D11ShaderResourceView* portrait = nullptr;
            if (settings->showPortrait) {
                portrait = TextureManager::GetPortrait(actor);
            }
            bool isTopMode = (settings->portraitAlignment == 1);
            
            float contentHeight = (settings->showName || settings->showLevel) ? ImGui::GetTextLineHeight() : 0;
            
            bool hasHealth = settings->showHealth;
            bool hasMagicka = settings->showMagicka && maxMagicka > 0;
            bool hasStamina = settings->showStamina && maxStamina > 0;
            bool compact = compactBars && hasMagicka && hasStamina;
            
            if (hasHealth) {
                contentHeight += (float)barSpacing + barHeight;
            }
            if (!compact) {
                 if (hasMagicka) contentHeight += (float)barSpacing + (barHeight * 0.7f);
                 if (hasStamina) contentHeight += (float)barSpacing + (barHeight * 0.7f);
            } else {
                 contentHeight += (float)barSpacing + (barHeight * 0.7f);
            }
            
            if (hasHealth || hasMagicka || hasStamina) {
                contentHeight += barSpacing * 3 + ScalePx(4.0f);  // Bottom buffer (generous padding)
            }
            
            std::vector<EffectIconRenderer::EffectDisplayInfo> activeEffects;
            if (settings->showActiveEffects || settings->showEffectIcons) {
                activeEffects = EffectIconRenderer::GetActiveEffectsInfo(actor, settings->maxEffectsShown, settings->effectMinDuration, settings->effectMaxDuration, settings->hudHideManagedSummonEffects);
                
                if (settings->showActiveEffects && !activeEffects.empty()) {
                    float effectLineHeight = ImGui::GetTextLineHeight() * settings->effectTextScale;
                    contentHeight += (float)barSpacing;
                    contentHeight += effectLineHeight * static_cast<float>(activeEffects.size());
                }
            }

            const bool fillsRibbon = settings->hudPortraitFillsRibbon;
            // Reserve the ring's outer stroke, gloss and shadow independently of bar width.
            const float haloThickness = settings->hudHaloThickness > 0
                ? ScalePx(static_cast<float>(settings->hudHaloThickness))
                : (std::max)(ScalePx(3.0f), portraitSize * 0.055f);
            const float ringPadding = settings->hudHaloEnable
                ? haloThickness + ScalePx(static_cast<float>(settings->hudHaloOffset)) + ScalePx(4.0f) : 0.0f;
            // Backlight draws beyond the child clip without changing portrait or effect anchors.
            const float haloPadding = portrait && !fillsRibbon ? ringPadding : 0.0f;
            const float portraitSlotSize = portraitSize + haloPadding * 2.0f;
            if (portrait && settings->portraitShape == 2 && !fillsRibbon) {
                static int lastBacklight = -1;
                const int backlight = settings->hudPortraitBacklight ? 1 : 0;
                if (lastBacklight != backlight) {
                    logger::debug("[PortraitFX] Backlight toggle={} portrait={} layoutPadding={}", backlight, portraitSize, haloPadding);
                    lastBacklight = backlight;
                }
            }

            float totalRibbonHeight;
            float totalRibbonWidth;

            if (portrait && fillsRibbon) {
                totalRibbonWidth = barWidth + ribbonPaddingLeft + ribbonPaddingRight;
                totalRibbonHeight = (std::max)(portraitSize, contentHeight) + ribbonPaddingTop + ribbonPaddingBottom;
            } else if (portrait && isTopMode) {
                // TOP MODE: Portrait stacked above content
                totalRibbonHeight = portraitSlotSize + ScalePx(static_cast<float>(settings->portraitBottomMargin)) + contentHeight + ribbonPaddingTop + ribbonPaddingBottom;
                totalRibbonWidth = (haloPadding > 0.0f ? (std::max)(barWidth, portraitSlotSize) : barWidth) + ribbonPaddingLeft + ribbonPaddingRight;
            } else if (portrait) {
                // LEFT MODE: Portrait beside content Width = portrait + spacing + bars + horizontal padding
                totalRibbonWidth = portraitSlotSize + ScalePx(static_cast<float>(settings->portraitSpacing)) + barWidth + ribbonPaddingLeft + ribbonPaddingRight;
                totalRibbonHeight = (std::max)(portraitSlotSize, contentHeight) + ribbonPaddingTop + ribbonPaddingBottom;
            } else {
                totalRibbonHeight = contentHeight + ribbonPaddingTop + ribbonPaddingBottom;
                totalRibbonWidth = barWidth + ribbonPaddingLeft + ribbonPaddingRight;
            }

            if (settings->hudHaloEnable) {
                static ImVec2 lastHaloSize(-1.0f, -1.0f);
                if (lastHaloSize.x != totalRibbonWidth || lastHaloSize.y != totalRibbonHeight) {
                    logger::debug("[FollowerHalo] portrait={} ringPadding={} ribbon={}x{} top={} fill={}",
                        portraitSize, haloPadding, totalRibbonWidth, totalRibbonHeight, isTopMode, fillsRibbon);
                    lastHaloSize = ImVec2(totalRibbonWidth, totalRibbonHeight);
                }
            }

            if (emitIndentCells[i] > 0)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY()
                    + emitIndentCells[i] * (totalRibbonHeight + ScalePx(static_cast<float>(settings->ribbonMargin))));

            char childLabel[32];
            snprintf(childLabel, sizeof(childLabel), "Ribbon_%zu", i);
            
            // Capture screen position BEFORE BeginChild for icon positioning later
            const bool isDowned = (actor->AsActorState() && actor->AsActorState()->IsBleedingOut()) || healthPercent <= 0.0f;
            std::string portraitKey = "portrait_follower_" + std::to_string(actor->GetFormID());
            float shakeOffsetX = (settings->hudPortraitHitShake && !isDowned) ? ScalePx(UIAnimation::GetShakeOffsetX(portraitKey)) : 0.0f;
            float hitFlashIntensity = (settings->hudPortraitHitFlash && !isDowned) ? UIAnimation::GetHitFlashIntensity(portraitKey) : 0.0f;
            float healFlashIntensity = UIAnimation::GetHealFlashIntensity(portraitKey);
            const bool buffFlash = UIAnimation::GetHealFlashIsBuff(portraitKey);
            ImVec2 ribbonScreenPos = ImGui::GetCursorScreenPos();

            ImGui::BeginChild(childLabel, ImVec2(totalRibbonWidth, totalRibbonHeight), true, ImGuiWindowFlags_NoScrollbar);

            ImDrawList* fadeChildDl = ImGui::GetWindowDrawList();
            int fadeChildVtx0 = fadeChildDl->VtxBuffer.Size;

            ImGui::SetCursorPos(ImVec2(ribbonPaddingLeft, ribbonPaddingTop));
            ImGui::Indent(ribbonPaddingLeft);
            
            ImGui::SetWindowFontScale(settings->nameFontScale);
            ImFont* headingFont = ImGuiRenderer::GetHeadingFont();
            if (headingFont) ImGui::PushFont(headingFont);
            

            float drawPortraitSize = portraitSize;
            if (healthPercent < 0.25f && healthPercent > 0.0f && !isDowned) {
                constexpr float kPulseFreq = 1.8f;
                constexpr float kPulseAmp  = 0.07f;
                float scale = 1.0f + kPulseAmp * sinf(2.0f * 3.14159265f * kPulseFreq * static_cast<float>(ImGui::GetTime()));
                drawPortraitSize = portraitSize * scale;
            }
            float healScale = UIAnimation::GetHealScale(portraitKey);
            drawPortraitSize *= healScale;
            float portraitOffset = (drawPortraitSize - portraitSize) * 0.5f;

            if (portrait && fillsRibbon) {
                ImVec2 rMin = ImGui::GetWindowPos();
                ImVec2 rMax = ImVec2(rMin.x + totalRibbonWidth, rMin.y + totalRibbonHeight);
                rMin.x += shakeOffsetX;
                rMax.x += shakeOffsetX;

                // Cache this follower's real portrait top-left for the Idle Portrait
                if (globalIdx < state.stackPortraitPos.size()) state.stackPortraitPos[globalIdx] = rMin;

                float ribbonAspect = totalRibbonWidth / (std::max)(1.0f, totalRibbonHeight);
                float zoom = (std::max)(1.0f, settings->hudPortraitZoom);
                float uHalf = 0.5f, vHalf = 0.5f;
                if (ribbonAspect >= 1.0f) {
                    vHalf = 0.5f / ribbonAspect;
                } else {
                    uHalf = 0.5f * ribbonAspect;
                }
                uHalf /= zoom;
                vHalf /= zoom;
                float fU = settings->hudPortraitFocusU;
                float fV = settings->hudPortraitFocusV;
                fU = (std::min)((std::max)(fU, uHalf), 1.0f - uHalf);
                fV = (std::min)((std::max)(fV, vHalf), 1.0f - vHalf);
                ImVec2 uv0(fU - uHalf, fV - vHalf);
                ImVec2 uv1(fU + uHalf, fV + vHalf);

                ImDrawList* ribDl = ImGui::GetWindowDrawList();

                if (settings->hudPortraitBacklight)
                    HUDDraw::DrawPortraitBackHalo(ribDl, rMin, rMax, settings->hudPortraitBacklightColor);

                int rVtx0 = ribDl->VtxBuffer.Size;
                ribDl->AddImageRounded(
                    reinterpret_cast<ImTextureID>(portrait), rMin, rMax, uv0, uv1,
                    IM_COL32_WHITE, ribbonRounding);
                if (settings->hudPortraitFx)
                    ImGui::ShadeVertsLinearColorGradientKeepAlpha(ribDl, rVtx0, ribDl->VtxBuffer.Size,
                        rMin, ImVec2(rMin.x, rMax.y),
                        IM_COL32_WHITE, IM_COL32(140, 148, 168, 255));

                if (healFlashIntensity > 0.0f) {
                    ribDl->AddRectFilled(rMin, rMax, HealFlashColor(healFlashIntensity, buffFlash), ribbonRounding);
                } else if (hitFlashIntensity > 0.0f) {
                    ribDl->AddRectFilled(rMin, rMax, IM_COL32(255, 30, 30, static_cast<int>(hitFlashIntensity * 160.0f)), ribbonRounding);
                }

                if (settings->hudPortraitFx || settings->hudPortraitBacklight) {
                    HUDDraw::PortraitFx ribbonFx;
                    ribbonFx.topLight = ribbonFx.scrim = ribbonFx.vignette = ribbonFx.rim =
                        settings->hudPortraitFx;
                    ribbonFx.backlight      = settings->hudPortraitBacklight;
                    ribbonFx.backlightColor = settings->hudPortraitBacklightColor;
                    HUDDraw::DrawPortraitFx(ribDl, rMin, rMax, 0, ribbonRounding, ribbonFx);
                }

                DrawPortraitOverlays(ImGui::GetWindowDrawList(), rMin, totalRibbonHeight,
                                     healthPercent, level, actor, totalRibbonWidth,
                                     magickaPercent, staminaPercent);

                ImGui::SetCursorPos(ImVec2(ribbonPaddingLeft, ribbonPaddingTop));
            } else if (portrait && isTopMode) {
                // TOP MODE: Portrait above content with horizontal alignment
                const float portraitRowWidth = haloPadding > 0.0f ? (std::max)(barWidth, portraitSlotSize) : barWidth;
                float indent = haloPadding;
                switch (settings->portraitHAlign) {
                    case 1: indent = (portraitRowWidth - portraitSize) / 2.0f; break; // Center
                    case 2: indent = portraitRowWidth - portraitSize - haloPadding; break; // Right
                }
                if (indent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

                ImVec2 pPos = ImGui::GetCursorScreenPos();
                pPos.y += haloPadding;
                // Shift cursor so scaled portrait stays centered, draw, then restore layout
                ImVec2 drawPos = ImVec2(pPos.x - portraitOffset + shakeOffsetX, pPos.y - portraitOffset);
                ImGui::SetCursorScreenPos(drawPos);
                // Cache real portrait top-left for the Idle Portrait Stack's coin-travel
                if (globalIdx < state.stackPortraitPos.size()) state.stackPortraitPos[globalIdx] = drawPos;
                DrawRoundedImage(portrait, ImVec2(drawPortraitSize, drawPortraitSize), ScalePx(static_cast<float>(settings->portraitCornerRadius)), 1.0f, hitFlashIntensity, healFlashIntensity, buffFlash);
                ImGui::SetCursorScreenPos(ImVec2(pPos.x, pPos.y + portraitSize + haloPadding));  // restore layout
                // ribbonWidth=0: shape mode, right-anchored badges hang off the portrait edge.
                ImVec2 overlayPos = ImVec2(pPos.x + shakeOffsetX, pPos.y);
                DrawPortraitOverlays(ImGui::GetWindowDrawList(), overlayPos, portraitSize, healthPercent, level, actor, 0.0f,
                                     magickaPercent, staminaPercent);

                if (haloPadding > 0.0f)
                    ImGui::SetCursorPosX(ribbonPaddingLeft + (portraitRowWidth - barWidth) * 0.5f);
                if (settings->portraitBottomMargin > 0)
                    ImGui::Dummy(ImVec2(0, ScalePx(static_cast<float>(settings->portraitBottomMargin))));
            } else if (portrait) {
                ImVec2 pPos = ImGui::GetCursorScreenPos();
                pPos.x += haloPadding;
                pPos.y += haloPadding;
                // Shift cursor so scaled portrait stays centered, draw, then restore layout
                ImVec2 drawPos = ImVec2(pPos.x - portraitOffset + shakeOffsetX, pPos.y - portraitOffset);
                ImGui::SetCursorScreenPos(drawPos);
                // Cache real portrait top-left: see the identical comment in TOP MODE above.
                if (globalIdx < state.stackPortraitPos.size()) state.stackPortraitPos[globalIdx] = drawPos;
                DrawRoundedImage(portrait, ImVec2(drawPortraitSize, drawPortraitSize), ScalePx(static_cast<float>(settings->portraitCornerRadius)), 1.0f, hitFlashIntensity, healFlashIntensity, buffFlash);
                // ribbonWidth=0: shape mode, right-anchored badges hang off the portrait edge.
                ImVec2 overlayPos = ImVec2(pPos.x + shakeOffsetX, pPos.y);
                DrawPortraitOverlays(ImGui::GetWindowDrawList(), overlayPos, portraitSize, healthPercent, level, actor, 0.0f,
                                     magickaPercent, staminaPercent);
                // Position content group directly to the right of the fixed portrait slot.
                ImGui::SetCursorScreenPos(ImVec2(pPos.x + portraitSize + haloPadding + ScalePx(static_cast<float>(settings->portraitSpacing)) + shakeOffsetX, pPos.y - haloPadding));
            }
            
            ImGui::BeginGroup();
            
            if (portrait && !isTopMode) {
                float contentStartY = ImGui::GetCursorPosY();
                
                float textHeight = ImGui::GetTextLineHeight();
                float estimatedContentHeight = 0;
                
                if (settings->showName || settings->showLevel) {
                    estimatedContentHeight += textHeight + barSpacing;
                }
                
                if (!settings->hudHaloEnable) {
                    if (settings->showHealth) {
                        estimatedContentHeight += barHeight + barSpacing;
                    }

                    if (settings->showMagicka) {
                        estimatedContentHeight += barHeight * 0.7f + barSpacing;
                    }
                    if (settings->showStamina) {
                        estimatedContentHeight += barHeight * 0.7f;
                    }
                }
                
                switch (settings->contentVAlign) {
                    case 0:
                        break;
                    case 1:
                        if (estimatedContentHeight < portraitSize) {
                            ImGui::SetCursorPosY(contentStartY + (portraitSize - estimatedContentHeight) / 2);
                        }
                        break;
                    case 2:
                        if (estimatedContentHeight < portraitSize) {
                            ImGui::SetCursorPosY(contentStartY + portraitSize - estimatedContentHeight);
                        }
                        break;
                }
            }
            
            if (settings->showName || settings->showLevel) {
                char levelBuf[32];
                snprintf(levelBuf, sizeof(levelBuf), LocalizationManager::Get("LevelShort").c_str(), level);
                std::string levelStr = levelBuf;
                if (settings->levelUppercase) {
                    levelStr = toUppercase(levelStr);
                }

                std::string fullText;
                if (settings->showName && settings->showLevel) {
                    fullText = displayName + " (" + levelStr + ")";
                } else if (settings->showName) {
                    fullText = displayName;
                } else {
                    fullText = std::string(levelStr);
                }
                
                float startX = ImGui::GetCursorPosX();
                if (!portrait || isTopMode) {
                    float textWidth = ImGui::CalcTextSize(fullText.c_str()).x;
                    float contentWidth = barWidth;
                    
                    switch (settings->textAlignment) {
                        case 1:
                            startX = ribbonPaddingLeft + (contentWidth - textWidth) / 2;
                            break;
                        case 2:
                            startX = ribbonPaddingLeft + contentWidth - textWidth;
                            break;
                        default:
                            break;
                    }
                }
                
                ImU32 nameColU32 = hexToU32(settings->nameColor);
                ImU32 levelColU32 = hexToU32(settings->levelNameColor);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                
                ImVec2 windowPos = ImGui::GetWindowPos();
                float curY = ImGui::GetCursorPosY();
                ImVec2 screenPos = ImVec2(windowPos.x + startX, windowPos.y + curY);
                
                if (settings->showName && settings->showLevel) {
                    std::string nameWithSpace = displayName + " ";
                    float nameWidth = ImGui::CalcTextSize(nameWithSpace.c_str()).x;
                    std::string levelText = "(" + levelStr + ")";
                    
                    DrawTextWithShadow(drawList, screenPos, nameColU32, nameWithSpace.c_str(), true);
                    
                    ImVec2 levelPos = ImVec2(screenPos.x + nameWidth, screenPos.y);
                    DrawTextWithShadow(drawList, levelPos, levelColU32, levelText.c_str(), true);
                    
                    ImGui::Dummy(ImVec2(nameWidth + ImGui::CalcTextSize(levelText.c_str()).x, ImGui::GetTextLineHeight()));
                } else if (settings->showName) {
                    DrawTextWithShadow(drawList, screenPos, nameColU32, displayName.c_str(), true);
                    ImGui::Dummy(ImVec2(ImGui::CalcTextSize(displayName.c_str()).x, ImGui::GetTextLineHeight()));
                } else {
                    DrawTextWithShadow(drawList, screenPos, levelColU32, levelStr.c_str(), true);
                    ImGui::Dummy(ImVec2(ImGui::CalcTextSize(levelStr.c_str()).x, ImGui::GetTextLineHeight()));
                }
                
                ImGui::PopStyleVar();
                
                ImGui::Dummy(ImVec2(0, (float)barSpacing));
            }
            
            if (ImGuiRenderer::GetHeadingFont()) ImGui::PopFont();
            
            ImGui::SetWindowFontScale(settings->barTextFontScale);
            
            if (!settings->hudHaloEnable) {
            if (hasHealth) {
                char healthText[32] = "";
                if (showText) snprintf(healthText, sizeof(healthText), "%.0f/%.0f", health, maxHealth);
                char labelId[64];
                snprintf(labelId, sizeof(labelId), "##HP_%zu", i);
                const float hpGhost = settings->barDamageGhost
                    ? BarRenderer::TickGhost(0xF400000000000000ull | actor->GetFormID(), healthPercent)
                    : 0.0f;
                DrawCustomProgressBar(labelId, healthPercent, ImVec2(barWidth, barHeight),
                                       hexToImVec4(settings->healthColor), barCornerRadius, healthText,
                                       ScalePxInt(settings->barOuterBorderWidth), hexToImVec4(settings->barBorderColor, settings->barOuterBorderOpacity),
                                       ScalePxInt(settings->barInnerBorderWidth), hexToImVec4(settings->fhBarInnerBorderColor, settings->barInnerBorderOpacity),
                                       hexToImVec4(settings->healthGradientColor), hpGhost, maxHealth);
            }
            
            if (compact) {
                // Subtract gap from total width first, then split remaining space
                const float gapWidth = ScalePx(4.0f);
                float availableWidth = barWidth - gapWidth;
                float spWidth = availableWidth * compactRatio;
                float mpWidth = availableWidth * (1.0f - compactRatio);
                float smallBarHeight = barHeight;
                
                
                char staminaText[32] = "";
                if (showText) snprintf(staminaText, sizeof(staminaText), "%.0f", stamina);
                char spLabelId[64];
                snprintf(spLabelId, sizeof(spLabelId), "##SP_%zu", i);
                DrawCustomProgressBar(spLabelId, staminaPercent, ImVec2(spWidth, smallBarHeight),
                                       hexToImVec4(settings->staminaColor), barCornerRadius, staminaText,
                                       ScalePxInt(settings->barOuterBorderWidth), hexToImVec4(settings->barBorderColor, settings->barOuterBorderOpacity),
                                       ScalePxInt(settings->barInnerBorderWidth), hexToImVec4(settings->fhBarInnerBorderColor, settings->barInnerBorderOpacity),
                                       hexToImVec4(settings->staminaGradientColor));
                
                ImGui::SameLine(0, gapWidth);
                
                char magickaText[32] = "";
                if (showText) snprintf(magickaText, sizeof(magickaText), "%.0f", magicka);
                char mpLabelId[64];
                snprintf(mpLabelId, sizeof(mpLabelId), "##MP_%zu", i);
                DrawCustomProgressBar(mpLabelId, magickaPercent, ImVec2(mpWidth, smallBarHeight),
                                       hexToImVec4(settings->magickaColor), barCornerRadius, magickaText,
                                       ScalePxInt(settings->barOuterBorderWidth), hexToImVec4(settings->barBorderColor, settings->barOuterBorderOpacity),
                                       ScalePxInt(settings->barInnerBorderWidth), hexToImVec4(settings->fhBarInnerBorderColor, settings->barInnerBorderOpacity),
                                       hexToImVec4(settings->magickaGradientColor));
            } else {
                if (hasMagicka) {
                    char magickaText[32] = "";
                    if (showText) snprintf(magickaText, sizeof(magickaText), "%.0f/%.0f", magicka, maxMagicka);
                    char mpLabelId[64];
                    snprintf(mpLabelId, sizeof(mpLabelId), "##MP_%zu", i);
                    DrawCustomProgressBar(mpLabelId, magickaPercent, ImVec2(barWidth, barHeight),
                                           hexToImVec4(settings->magickaColor), barCornerRadius, magickaText,
                                           ScalePxInt(settings->barOuterBorderWidth), hexToImVec4(settings->barBorderColor, settings->barOuterBorderOpacity),
                                           ScalePxInt(settings->barInnerBorderWidth), hexToImVec4(settings->fhBarInnerBorderColor, settings->barInnerBorderOpacity),
                                           hexToImVec4(settings->magickaGradientColor));
                }

                if (hasStamina) {
                    char staminaText[32] = "";
                    if (showText) snprintf(staminaText, sizeof(staminaText), "%.0f/%.0f", stamina, maxStamina);
                    char spLabelId[64];
                    snprintf(spLabelId, sizeof(spLabelId), "##SP_%zu", i);
                    DrawCustomProgressBar(spLabelId, staminaPercent, ImVec2(barWidth, barHeight),
                                           hexToImVec4(settings->staminaColor), barCornerRadius, staminaText,
                                           ScalePxInt(settings->barOuterBorderWidth), hexToImVec4(settings->barBorderColor, settings->barOuterBorderOpacity),
                                           ScalePxInt(settings->barInnerBorderWidth), hexToImVec4(settings->fhBarInnerBorderColor, settings->barInnerBorderOpacity),
                                           hexToImVec4(settings->staminaGradientColor));
                }
            }
            }

            if (settings->showActiveEffects && !activeEffects.empty()) {
                ImGui::SetWindowFontScale(settings->effectTextScale);
                
                for (const auto& effect : activeEffects) {
                    ImVec4 effectColor;
                    auto effectType = EffectCategories::GetEffectType(effect.archetype);
                    bool isNegative = effect.isHostile || EffectCategories::IsNegativeArchetype(effect.archetype);
                    
                    if (isNegative) {
                        effectColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    } else if (effectType == EffectCategories::EffectType::Standalone) {
                        effectColor = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
                    } else if (effect.archetype == RE::EffectArchetypes::ArchetypeID::kValueModifier) {
                        effectColor = ImVec4(0.4f, 1.0f, 1.0f, 1.0f);
                    } else if (effect.archetype == RE::EffectArchetypes::ArchetypeID::kPeakValueModifier) {
                        effectColor = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                    } else if (effect.archetype == RE::EffectArchetypes::ArchetypeID::kAbsorb) {
                        effectColor = ImVec4(1.0f, 0.4f, 1.0f, 1.0f);
                    } else {
                        effectColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                    }
                    
                    char effectText[128];
                    int timeRemaining = static_cast<int>(effect.timeRemaining);
                    int magnitude = static_cast<int>(effect.magnitude);
                    
                    char timeStr[32];
                    if (timeRemaining >= 60) {
                        int minutes = timeRemaining / 60;
                        int seconds = timeRemaining % 60;
                        snprintf(timeStr, sizeof(timeStr), "%dm%ds", minutes, seconds);
                    } else {
                        snprintf(timeStr, sizeof(timeStr), "%ds", timeRemaining);
                    }
                    
                    if (magnitude > 0) {
                        snprintf(effectText, sizeof(effectText), "%s +%d (%s)", 
                                 effect.name.c_str(), magnitude, timeStr);
                    } else {
                        snprintf(effectText, sizeof(effectText), "%s (%s)", 
                                 effect.name.c_str(), timeStr);
                    }
                    
                    ImGui::TextColored(effectColor, "%s", effectText);
                }
            }
            
            ImGui::EndGroup();
            
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Unindent(ribbonPaddingLeft);
            
            ImGui::EndChild();

            if (ribbonFadeAlpha < 0.999f) {
                for (int vi = fadeChildVtx0; vi < fadeChildDl->VtxBuffer.Size; ++vi) {
                    ImDrawVert& v = fadeChildDl->VtxBuffer[vi];
                    unsigned int a = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
                    a = static_cast<unsigned int>(a * ribbonFadeAlpha);
                    v.col = (v.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                }
            }

            if (settings->showEffectIcons && !activeEffects.empty()) {
                float iconSize = ScalePx(static_cast<float>(settings->effectIconSize));
                float borderThickness = ScalePx(static_cast<float>(settings->effectTimerBorderWidth));
                float topPadding = borderThickness + ScalePx(2.0f);
                float iconSpacing = ScalePx(static_cast<float>(settings->effectIconVertSpacing));
                float totalIconWidth = iconSize + borderThickness * 2 + ScalePx(4.0f);
                int maxIconsPerColumn = settings->maxEffectIconsPerColumn;
                int placementMode = settings->effectIconPlacement;

                EffectIconRenderer::DrawOptions effectOpt;
                effectOpt.iconSize        = iconSize;
                effectOpt.borderThickness = borderThickness;
                effectOpt.showTimer       = settings->showEffectTimer;
                effectOpt.timerStyle      = settings->effectTimerStyle;
                effectOpt.showMagnitude   = settings->showEffectMagnitude;
                effectOpt.magnitudeScale  = settings->effectMagnitudeScale;
                effectOpt.bgPadding       = ScalePx(static_cast<float>(settings->effectIconBgPadding));
                effectOpt.buffColor       = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->effectBuffColor));
                effectOpt.debuffColor     = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->effectDebuffColor));
                effectOpt.bgFillRadius    = iconSize * settings->effectIconBorderRadius * 0.01f;
                effectOpt.showSourceBadge = settings->showEffectSourceBadge;
                effectOpt.iconFx.duotone      = settings->hudEffectIconDuotone;
                effectOpt.iconFx.shadow       = settings->hudEffectIconShadow;
                effectOpt.iconFx.outline      = settings->hudEffectIconOutline;
                effectOpt.iconFx.glow         = settings->hudEffectIconGlow;
                effectOpt.iconFx.gloss        = settings->hudEffectIconGloss;
                effectOpt.iconFx.outlineWidth = ScalePxInt(settings->hudEffectIconOutlineWidth);
                effectOpt.iconFx.outlineColor = settings->hudEffectIconOutlineColor;

                float startX, startY;
                bool isRowMajor = false;
                int iconsPerGroup = maxIconsPerColumn > 0 ? maxIconsPerColumn : 100;

                if (placementMode == 2) {
                    if (settings->overlayAnchor == 0)
                        startX = ribbonScreenPos.x + ribbonPaddingLeft;
                    else
                        startX = ribbonScreenPos.x + totalRibbonWidth - totalIconWidth - ribbonPaddingRight;
                    startY = ribbonScreenPos.y + topPadding;
                    startX += ScalePx(static_cast<float>(settings->overlayOffsetX));
                    startY += ScalePx(static_cast<float>(settings->overlayOffsetY));
                } else if (placementMode == 1 && isHorizontal) {
                    startX = ribbonScreenPos.x + ribbonPaddingLeft;
                    startY = ribbonScreenPos.y + totalRibbonHeight + ScalePx(static_cast<float>(settings->effectIconSpacing));
                    isRowMajor = true;
                    int rowsNeeded = (static_cast<int>(activeEffects.size()) + iconsPerGroup - 1) / iconsPerGroup;
                    if (rowsNeeded > maxEffectRowsInLine) maxEffectRowsInLine = rowsNeeded;
                } else if (placementMode == 3 && isHorizontal) {
                    startX = ribbonScreenPos.x + ribbonPaddingLeft;
                    int rowsNeeded = (static_cast<int>(activeEffects.size()) + iconsPerGroup - 1) / iconsPerGroup;
                    float singleRowH = iconSize + borderThickness * 2;
                    float blockH = singleRowH * rowsNeeded + iconSpacing * (std::max)(0, rowsNeeded - 1);
                    startY = ribbonScreenPos.y - blockH - ScalePx(static_cast<float>(settings->effectIconSpacing));
                    isRowMajor = true;
                } else {
                    // Inline, Below+Vertical: icons column to the right of the ribbon
                    startX = ribbonScreenPos.x + totalRibbonWidth + ScalePx(static_cast<float>(settings->effectIconSpacing));
                    startY = ribbonScreenPos.y + topPadding;
                }

                ImDrawList* drawList = ImGui::GetForegroundDrawList();
                int fadeIconVtx0 = drawList->VtxBuffer.Size;
                int iconIndex = 0;
                for (const auto& effect : activeEffects) {
                    int col = isRowMajor ? (iconIndex % iconsPerGroup) : (iconIndex / iconsPerGroup);
                    int row = isRowMajor ? (iconIndex / iconsPerGroup) : (iconIndex % iconsPerGroup);
                    ImVec2 iconPos = ImVec2(
                        startX + col * (totalIconWidth + ScalePx(4.0f)),
                        startY + row * (iconSize + iconSpacing));
                    if (EffectIconRenderer::DrawEffectIcon(effect, iconPos, drawList, effectOpt))
                        iconIndex++;
                }

                if (ribbonFadeAlpha < 0.999f) {
                    for (int vi = fadeIconVtx0; vi < drawList->VtxBuffer.Size; ++vi) {
                        ImDrawVert& v = drawList->VtxBuffer[vi];
                        unsigned int a = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
                        a = static_cast<unsigned int>(a * ribbonFadeAlpha);
                        v.col = (v.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                    }
                }
            }
            
            ImGui::PopStyleVar(4);
            ImGui::PopStyleColor(2);
            
            bool isLastInGroup = emitGroupEnd[i];
            bool isLastOverall = (i == emitIdx.size() - 1);
            
            if (!isLastOverall) {
                if (isHorizontal) {
                    if (isLastInGroup) {
                        float extraMargin = 0.0f;
                        
                        if (settings->effectIconPlacement == 1 && settings->showEffectIcons && maxEffectRowsInLine > 0) {
                            float iconSize = ScalePx(static_cast<float>(settings->effectIconSize));
                            float borderThickness = ScalePx(static_cast<float>(settings->effectTimerBorderWidth));
                            float vertSpacing = ScalePx(static_cast<float>(settings->effectIconVertSpacing));
                            float topStatsSpacing = ScalePx(static_cast<float>(settings->effectIconSpacing)); // Spacing from ribbon to first row

                            float singleRowHeight = iconSize + borderThickness * 2;
                            float totalIconBlockHeight = (singleRowHeight * maxEffectRowsInLine) + 
                                                         (vertSpacing * (maxEffectRowsInLine - 1));
                            
                            extraMargin = totalIconBlockHeight + topStatsSpacing;
                        }
                        
                        float groupGapV = ScalePx(static_cast<float>(settings->ribbonMargin));
                        if (settings->hudGroupGap > 0) {
                            float screenH = ImGui::GetIO().DisplaySize.y;
                            float calculated = screenH * (settings->hudGroupGap / 100.0f) - totalRibbonHeight;
                            groupGapV = (std::max)(groupGapV, calculated);
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + groupGapV + extraMargin);
                        maxEffectRowsInLine = 0;

                        // Right anchor: right-align a partial next row so its cards sit
                        if (anchorRight) {
                            size_t cardsInNextRow = emitIdx.size() - (i + 1);
                            if (cardsInNextRow < static_cast<size_t>(maxPerWrap)) {
                                float inRowGap = ScalePx(static_cast<float>(settings->ribbonMargin))
                                               + ScalePx(static_cast<float>(settings->ribbonEffectClearance));
                                float indent = (maxPerWrap - static_cast<int>(cardsInNextRow))
                                             * (totalRibbonWidth + inRowGap);
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
                            }
                        }
                    } else {
                        ImGui::SameLine();
                        ImGui::SameLine(0, ScalePx(static_cast<float>(settings->ribbonMargin)) + ScalePx(static_cast<float>(settings->ribbonEffectClearance)));
                    }
                } else {
                    if (isLastInGroup) {
                        ImGui::EndGroup();
                        ImGui::SameLine();
                        float groupGapH = ScalePx(static_cast<float>(settings->ribbonMargin)) + ScalePx(static_cast<float>(settings->ribbonEffectClearance));
                        if (settings->hudGroupGap > 0) {
                            float screenW = ImGui::GetIO().DisplaySize.x;
                            float calculated = screenW * (settings->hudGroupGap / 100.0f) - totalRibbonWidth;
                            groupGapH = (std::max)(groupGapH, calculated);
                        }
                        ImGui::SameLine(0, groupGapH);
                        ImGui::BeginGroup();
                    } else {
                        // Continue stacking down
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ScalePx(static_cast<float>(settings->ribbonMargin)));
                    }
                }
            }
        }
        
        if (!isHorizontal && !emitIdx.empty()) {
            ImGui::EndGroup();
        }
        }

        runCoinPass();

        {
            ImVec2 winSize = ImGui::GetWindowSize();
            bool sizeChanged = winSize.x != state.cachedWidth || winSize.y != state.cachedHeight;
            state.cachedWidth  = winSize.x;
            state.cachedHeight = winSize.y;
            // Ribbon-only refresh (runRibbon, not just stackT == 0: the ribbon still
            if (runRibbon) state.stackFullHeight = winSize.y;
            if (sizeChanged) state.forcePosition = true;

            if (allowDrag && !forcedThisFrame && !sizeChanged) {
                ImVec2 pos = ImGui::GetWindowPos();
                // Right anchor saves the right edge; growUpward saves the bottom edge
                posX = static_cast<int>(std::lround(
                    anchorRight ? pos.x + winSize.x : pos.x));
                posY = static_cast<int>(std::lround(
                    growUpward ? pos.y + winSize.y : pos.y));
            }
        }

        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
    
    void RenderContent(bool allowDrag) {
        auto* settings = Settings::GetSingleton();
        g_QuickHealTarget = (settings->qhEnabled && settings->qhShowBadgeOnFocus)
            ? QuickHeal::PickHealTarget() : nullptr;
        auto allies = GetAllies();
        static bool lastSplit = false;
        if (lastSplit != settings->hudSeparateSummons) {
            for (auto* state : {&g_FollowerWindow, &g_SummonWindow}) {
                state->stackT = 0.0f;
                state->stackTickInit = false;
                state->stackCalm = false;
                state->stackPortraitPos.clear();
                state->forcePosition = true;
            }
            logger::debug("[SummonHUD] Separate window={}", settings->hudSeparateSummons);
            lastSplit = settings->hudSeparateSummons;
        }
        if (!settings->hudSeparateSummons) {
            RenderAllyWindow(allies, allowDrag, false);
            return;
        }

        // Keep the existing total actor cap, filtering and stable order before partitioning.
        const size_t limit = static_cast<size_t>((std::max)(settings->maxFollowers, 0));
        if (allies.size() > limit) allies.resize(limit);
        std::vector<RE::Actor*> followers;
        std::vector<RE::Actor*> summons;
        {
            std::lock_guard<std::recursive_mutex> lock(g_CacheMutex);
            for (auto* actor : allies) {
                const bool summon = std::find(g_CachedSummons.begin(), g_CachedSummons.end(), actor) != g_CachedSummons.end();
                (summon ? summons : followers).push_back(actor);
            }
        }
        {
            std::lock_guard lock(g_SummonLifetimeMutex);
            g_VisibleSummonIDs.clear();
            for (auto* actor : summons) g_VisibleSummonIDs.push_back(actor->GetFormID());
        }
        RenderAllyWindow(followers, allowDrag, false);
        QueueSummonLifetimes(summons);
        RenderAllyWindow(summons, allowDrag, true);
    }

    bool Render() {
        if (MenuUtils::ShouldBlockOverlay()) {
            g_HudAlpha = 0.0f;
            return false;
        }

        bool shouldShow = ShouldShow() && !GetAllies().empty();

        if (!shouldShow) {
            g_HudAlpha = 0.0f;
            return false;
        }
        g_HudAlpha = 1.0f;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_HudAlpha);
        RenderContent();
        ImGui::PopStyleVar();

        ImGui::EndFrame();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());

        return true;
    }
}
