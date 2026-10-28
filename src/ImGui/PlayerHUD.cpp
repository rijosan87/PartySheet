#include "../../includes/ImGui/PlayerHUD.h"
#include "../../includes/PlayerStatus.h"
#include "../../includes/ImGui/PlayerHUDPenalties.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/Settings.h"
#include "../../includes/ActorStats.h"
#include "../../includes/TextureManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/Utils/BarRenderer.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/FontManager.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/UIAnimation.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace PlayerHUD {

    // Gilded Rune geometry from IMGUI PlayGround/components/PartySheetElements.cpp.
    static void DrawLevelUpRune(ImDrawList* draw, float x, float y, float height, float alpha)
    {
        const float width = height * 0.65f;
        const float shoulder = height * 0.43f;
        const float shaft = width * 0.30f;
        const float center = x + width * 0.5f;
        const ImVec2 points[] = {{center, y}, {x + width, y + shoulder},
            {center + shaft * 0.5f, y + shoulder}, {center + shaft * 0.5f, y + height},
            {center - shaft * 0.5f, y + height}, {center - shaft * 0.5f, y + shoulder},
            {x, y + shoulder}};
        ImVec2 shadowPoints[IM_ARRAYSIZE(points)];
        const float shadowOffset = (std::max)(1.0f, height / 16.0f);
        for (int i = 0; i < IM_ARRAYSIZE(points); ++i)
            shadowPoints[i] = ImVec2(points[i].x + shadowOffset, points[i].y + shadowOffset);
        draw->AddConcavePolyFilled(shadowPoints, IM_ARRAYSIZE(shadowPoints),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.55f * alpha)));

        const float blend = static_cast<float>(0.5 - 0.5 * std::cos(ImGui::GetTime() * 3.141592653589793));
        const ImVec4 green(0.35f, 0.82f, 0.40f, alpha);
        const ImVec4 rim(std::min(green.x * 1.25f, 1.0f), std::min(green.y * 1.25f, 1.0f),
            std::min(green.z * 1.25f, 1.0f), alpha);
        const ImVec4 fill(UIAnimation::Lerp(green.x * 0.45f, rim.x, blend),
            UIAnimation::Lerp(green.y * 0.45f, rim.y, blend),
            UIAnimation::Lerp(green.z * 0.45f, rim.z, blend), alpha);
        draw->AddConcavePolyFilled(points, IM_ARRAYSIZE(points), ImGui::ColorConvertFloat4ToU32(fill));
        draw->AddPolyline(points, IM_ARRAYSIZE(points), ImGui::ColorConvertFloat4ToU32(rim),
            ImDrawFlags_Closed, (std::max)(1.0f, height * 0.055f));
    }

    static float MasterScale()
    {
        const auto* settings = Settings::GetSingleton();
        return settings ? std::clamp(static_cast<float>(settings->phScale) / 100.0f, 0.5f, 2.0f) : 1.0f;
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

    static float  g_HudAlpha      = 0.0f;
    static bool   g_ForcePos      = false;
    static bool   g_WasInCombat   = false;
    static std::chrono::steady_clock::time_point g_CombatExitTime;

    // ── Dynamic-bar fade tracking ────────────────────────────────────────────
    static constexpr float kDynIdleSeconds = 2.0f;
    static constexpr float kDynFadeSeconds = 0.5f;
    enum class DynBar { HP = 0, MP = 1, SP = 2, XP = 3, Count = 4 };
    static float g_DynLastValue[(int)DynBar::Count]      = { -1.0f, -1.0f, -1.0f, -1.0f };
    static float g_DynLastChangeTime[(int)DynBar::Count] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static bool  g_DynPrevBattleReady = false;
    static float g_DynMaxAlphaLastFrame = 1.0f;

    static float ComputeDynamicAlpha(DynBar which, float current, float maxVal, bool battleReady, bool allowDrag) {
        const auto* settings = Settings::GetSingleton();
        const int idx = (int)which;
        const float now = (float)ImGui::GetTime();

        const float prev = g_DynLastValue[idx];
        if (prev < 0.0f || std::fabs(current - prev) > 0.01f) {
            g_DynLastValue[idx] = current;
            g_DynLastChangeTime[idx] = now;
        }

        if (g_DynPrevBattleReady && !battleReady) {
            for (int i = 0; i < (int)DynBar::Count; ++i)
                g_DynLastChangeTime[i] = now;
        }
        if (which == DynBar::HP) g_DynPrevBattleReady = battleReady;

        if (!settings->phDynamicBars || allowDrag || battleReady) return 1.0f;
        if (which != DynBar::XP) {
            if (maxVal <= 0.0f) return 1.0f;
            if (current < maxVal - 0.01f || current <= 0.0f) return 1.0f;
        }

        const float idle = now - g_DynLastChangeTime[idx];
        if (idle <= kDynIdleSeconds) return 1.0f;
        const float t = (idle - kDynIdleSeconds) / kDynFadeSeconds;
        if (t >= 1.0f) return 0.0f;
        return 1.0f - t;
    }

    static HUDDraw::BadgeStyle MakeBadgeStyle() {
        const auto* settings = Settings::GetSingleton();
        HUDDraw::BadgeStyle bs;
        bs.fontScale = settings->phBadgeFontScale;
        bs.rounding = settings->phBadgeRadius;
        bs.padX = settings->phBadgePaddingX;
        bs.padY = settings->phBadgePaddingY;
        bs.innerBorder = settings->phBadgeBorderColor;
        // Keep the existing dark contour, but let zero border opacity hide both lines.
        const auto borderAlpha = (settings->phBadgeBorderColor >> 24) & 0xFF;
        bs.outerBorder = ((std::min)(255u, borderAlpha * 5u / 2u) << 24) | 0x00060608u;
        const std::uint32_t bg = settings->phBadgeBgColor;
        const std::uint8_t a = (bg >> 24) & 0xFF, r = (bg >> 16) & 0xFF,
                           g = (bg >> 8) & 0xFF,  b =  bg        & 0xFF;
        auto lift = [](std::uint8_t c) { return static_cast<std::uint32_t>((std::min)(255.0f, c * 1.20f + 14.0f)); };
        auto sink = [](std::uint8_t c) { return static_cast<std::uint32_t>(c * 0.55f); };
        bs.bgTop    = (static_cast<std::uint32_t>(a) << 24) | (lift(r) << 16) | (lift(g) << 8) | lift(b);
        bs.bgBottom = 0xFF000000u | (sink(r) << 16) | (sink(g) << 8) | sink(b);
        bs.textColor = settings->phBadgeFontColor;
        return bs;
    }

    static ID3D11ShaderResourceView* GetPlayerSpriteFrame(ImVec2& uvMin, ImVec2& uvMax) {
        auto* settings = Settings::GetSingleton();
        if (!settings->phPortraitAnimEnabled) return nullptr;

        const int cols   = (std::max)(1, settings->phPortraitAnimCols);
        const int rows   = (std::max)(1, settings->phPortraitAnimRows);
        const int frames = (std::max)(1, settings->phPortraitAnimFrames);
        const float fps  = (std::max)(0.1f, settings->phPortraitAnimFps);

        auto* tex = TextureManager::GetTexture(
            "Data\\Interface\\PartySheet\\Portraits\\player-sprites.png");
        if (!tex) return nullptr;
        int frame = static_cast<int>(ImGui::GetTime() * fps) % frames;
        int col   = frame % cols;
        int row   = frame / cols;
        const float cellW = 1.0f / cols;
        const float cellH = 1.0f / rows;
        uvMin = ImVec2(col       * cellW, row       * cellH);
        uvMax = ImVec2((col + 1) * cellW, (row + 1) * cellH);
        return tex;
    }

    void Init() {
        PlayerHUDSurvival::Init();
        PlayerHUDDisease::Init();
    }

    void ForceReposition() { g_ForcePos = true; }

    bool ShouldShow() {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return false;
        if (MenuUtils::ShouldBlockOverlay()) return false;

        auto settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerStatusWidget) return false;
        switch (settings->playerHudMode) {
            case 0: return false;
            case 1: return true;
            case 2: {
                bool inCombat = player->IsInCombat();
                if (inCombat) {
                    g_WasInCombat = true;
                    return true;
                } else if (g_WasInCombat) {
                    g_CombatExitTime = std::chrono::steady_clock::now();
                    g_WasInCombat = false;
                }
                auto elapsed = std::chrono::steady_clock::now() - g_CombatExitTime;
                auto delayMs = std::chrono::seconds(settings->phFadeDelay);
                return elapsed < delayMs;
            }
            default: return false;
        }
    }

    void RenderContent(bool allowDrag) {
        auto settings = Settings::GetSingleton();
        auto player   = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;

        const auto transformState = PlayerStatus::GetTransformState();
        const bool isWerewolf    = (transformState == PlayerStatus::TransformState::Werewolf);
        const bool isVampireLord = (transformState == PlayerStatus::TransformState::VampireLord);
        const bool isWerebear    = (transformState == PlayerStatus::TransformState::Werebear);

        // Compute the margin needed to prevent the window clip rect from cutting off
        const float haloPortraitSize = ScalePx(static_cast<float>(settings->phPortraitSize));
        const float haloThickness = settings->phHaloThickness > 0
            ? ScalePx(static_cast<float>(settings->phHaloThickness))
            : (std::max)(ScalePx(3.0f), haloPortraitSize * 0.055f);
        const float haloOffset = ScalePx(static_cast<float>(settings->phHaloOffset));
        float frameMargin = 0.0f;
        if (settings->phShowPortrait) {
            float portSzEarly = ScalePx(static_cast<float>(settings->phPortraitSize));
            if (settings->phPortraitFrameEnabled) {
                float frameSzEarly = portSzEarly * settings->phPortraitFrameScale;
                frameMargin = (std::max)(frameMargin, (frameSzEarly - portSzEarly) * 0.5f);
            }
            if (settings->phPortraitBorderWidth > 0) {
                frameMargin = (std::max)(frameMargin, ScalePx(static_cast<float>(settings->phPortraitBorderWidth)) * 0.5f);
            }
            if (settings->phHaloEnable)
                frameMargin = (std::max)(frameMargin, haloThickness + haloOffset + ScalePx(4.0f));
            frameMargin = (std::max)(0.0f, frameMargin);
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(frameMargin, frameMargin));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, ScalePx(4.0f)));

        const ImVec2 targetPos(static_cast<float>(settings->phPosX) - frameMargin,
                               static_cast<float>(settings->phPosY) - frameMargin);
        const bool forcedThisFrame = g_ForcePos;
        if (!allowDrag || g_ForcePos) {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
            g_ForcePos = false;
        } else {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar  |
                                 ImGuiWindowFlags_NoResize     |
                                 ImGuiWindowFlags_NoScrollbar  |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoBackground;
        if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

        ImGui::Begin("PlayerHUD##status", nullptr, flags);

        auto stats = GetActorStats(player);
        const auto survivalPenalties = PlayerHUDPenalties::GetSurvivalPenalties();
        const auto tempPenalties = PlayerHUDPenalties::GetTemporaryPenalties(player);

        // WindowPadding already insets the cursor by frameMargin on each side.
        float padX         = ScalePx(8.0f);
        float padY         = ScalePx(8.0f);
        float barW         = ScalePx(static_cast<float>(settings->phBarWidth));
        float barH         = ScalePx(static_cast<float>(settings->phBarHeight));        // Health bar height
        float barHMp       = ScalePx(static_cast<float>(settings->phMagickaBarHeight));
        float barHSp       = ScalePx(static_cast<float>(settings->phStaminaBarHeight));
        float portSz       = ScalePx(static_cast<float>(settings->phPortraitSize));
        const bool hasRightColumn = settings->phShowName || settings->phShowHealth ||
            (settings->phShowMagicka && !isWerewolf && !isWerebear) || settings->phShowStamina ||
            (settings->phShowXP && (!settings->phShowPortrait || settings->phXPAsBar));
        float colSpacing   = hasRightColumn ? ScalePx(static_cast<float>(settings->phPortraitColumnSpacing)) : 0.0f;
        float barSpacing   = ScalePx(static_cast<float>(settings->phBarSpacing));
        // User Y nudge for all in/around-bar text (bar text, regen pip, fortify) at once.
        const float barTextOffsetY = ScalePx(static_cast<float>(settings->phBarTextOffsetY) * 0.1f);

        ImDrawList* dl  = ImGui::GetWindowDrawList();
        const bool hasBarOffset =
            settings->phHpOffsetX || settings->phHpOffsetY ||
            settings->phMpOffsetX || settings->phMpOffsetY ||
            settings->phSpOffsetX || settings->phSpOffsetY ||
            settings->phXPBarOffsetX || settings->phXPBarOffsetY;
        // The Fortify readout draws to the LEFT of each bar, outside the auto-sized window.
        const bool hasAnnotationOffset =
            settings->phHpAnnotationOffsetX ||
            settings->phHpAnnotationOffsetY ||
            settings->phMpAnnotationOffsetX ||
            settings->phMpAnnotationOffsetY ||
            settings->phSpAnnotationOffsetX ||
            settings->phSpAnnotationOffsetY ||
            settings->phXPAnnotationOffsetX ||
            settings->phXPAnnotationOffsetY;
        const bool needsFullClip = hasBarOffset || hasAnnotationOffset ||
            (settings->phShowPortrait && (settings->phPortraitOffsetX || settings->phPortraitOffsetY)) ||
            settings->phShowFortify ||
            (settings->phShowBarIcons && !settings->phShowPortrait);
        bool clipPushed = false;
        if (needsFullClip) {
            const ImVec2 ds = ImGui::GetIO().DisplaySize;
            dl->PushClipRect(ImVec2(0.0f, 0.0f), ds, false);
            clipPushed = true;
        } else if (frameMargin > 0.0f) {
            ImVec2 wMin = ImGui::GetWindowPos();
            ImVec2 wMax = ImVec2(wMin.x + ImGui::GetWindowWidth(), wMin.y + ImGui::GetWindowHeight());
            dl->PushClipRect(ImVec2(wMin.x - frameMargin, wMin.y - frameMargin),
                             ImVec2(wMax.x + frameMargin, wMax.y + frameMargin), false);
            clipPushed = true;
        }
        ImVec2      pos = ImGui::GetCursorScreenPos();

        float curX = pos.x + padX;
        float curY = pos.y + padY;

        float phHealthPercent = (stats.health.GetMax() > 0.0f) ?
            std::clamp(stats.health.current / stats.health.GetMax(), 0.0f, 1.0f) : 0.0f;
        const bool isDowned = (player->AsActorState() && player->AsActorState()->IsBleedingOut()) || phHealthPercent <= 0.0f;

        // Portrait hit feedback (shake and flash): suppressed when downed
        const float phShakeOffsetX = (settings->phPortraitHitShake && !isDowned) ? ScalePx(UIAnimation::GetShakeOffsetX("portrait_player")) : 0.0f;
        const float phHitFlash     = (settings->phPortraitHitFlash && !isDowned) ? UIAnimation::GetHitFlashIntensity("portrait_player") : 0.0f;

        // Low-health pulse (shared by the portrait red wash and the emblem outer ring):
        const bool  phLowHealth      = ((phHealthPercent < 0.8f && phHealthPercent > 0.0f && settings->phPortraitHitFlash) || isDowned);
        const bool  phCriticalHealth = (phHealthPercent < 0.25f && phHealthPercent > 0.0f) && !isDowned;
        const float phPulseSpeed     = phCriticalHealth ? 5.0f : 2.5f;
        const float phPulseAlpha     = isDowned ? 0.70f : ((sinf(static_cast<float>(ImGui::GetTime()) * phPulseSpeed) * 0.5f + 0.5f) * 0.4f + 0.1f);

        float drawPortSz = portSz;
        if (phCriticalHealth) {
            constexpr float kPulseFreq = 1.8f;
            constexpr float kPulseAmp  = 0.07f;
            float scale = 1.0f + kPulseAmp * sinf(2.0f * 3.14159265f * kPulseFreq * static_cast<float>(ImGui::GetTime()));
            drawPortSz = portSz * scale;
        }
        float portOffset = (drawPortSz - portSz) * 0.5f;

        const bool battleReadyNow =
            (player->AsActorState() && player->AsActorState()->IsWeaponDrawn()) ||
            player->IsInCombat();

        const float portraitAlpha =
            (!settings->phDynamicBars || settings->phPortraitAlwaysVisible ||
             allowDrag || battleReadyNow) ? 1.0f : g_DynMaxAlphaLastFrame;

        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        const int portraitVtxStart = dl->VtxBuffer.Size;

        if (settings->phShowPortrait) {
            const bool isDiamond = (settings->phPortraitShape == 2);
            const bool isCircle  = (settings->phPortraitShape == 1);
            const bool isEmblem  = (settings->phPortraitMode == 1);
            // Drawing offsets leave the portrait space reserved for bar layout unchanged.
            const float portX = curX + phShakeOffsetX + ScalePx(static_cast<float>(settings->phPortraitOffsetX));
            const float portY = curY + ScalePx(static_cast<float>(settings->phPortraitOffsetY));

            if (settings->phPortraitFrameEnabled && !isEmblem) {
                float frameSz = portSz * settings->phPortraitFrameScale;
                float frameOff = (frameSz - portSz) * 0.5f;
                ImVec2 frameMin(portX - frameOff, portY - frameOff);
                ImVec2 frameMax(portX + portSz + frameOff, portY + portSz + frameOff);
                ID3D11ShaderResourceView* frame = BackgroundManager::GetBackground(
                    BackgroundManager::Type::PortraitFrame, settings->phPortraitFrameIndex);
                if (frame) {
                    dl->AddImage(reinterpret_cast<ImTextureID>(frame), frameMin, frameMax);
                }
            }
            float portraitRounding = isCircle ? (drawPortSz * 0.5f)
                                               : (settings->phPortraitBorderRadius * drawPortSz * 0.5f);

            auto diamondPts = [&](float x, float y, float sz) {
                std::array<ImVec2, 4> p;
                p[0] = ImVec2(x + sz * 0.5f, y);
                p[1] = ImVec2(x + sz,        y + sz * 0.5f);
                p[2] = ImVec2(x + sz * 0.5f, y + sz);
                p[3] = ImVec2(x,             y + sz * 0.5f);
                return p;
            };

            if (isEmblem) {
                // Level emblem: fill in the selected shape + centered level number,
                ImVec2 fillMin(portX, portY);
                ImVec2 fillMax(portX + portSz, portY + portSz);
                const std::uint32_t chipTop    = HUDDraw::ChipTopFromArgb(settings->phEmblemFillColor);
                const std::uint32_t chipBottom = HUDDraw::ChipBottomFromArgb(settings->phEmblemFillColor);
                ImU32 topCol = IM_COL32((chipTop >> 16) & 0xFF, (chipTop >> 8) & 0xFF,
                                         chipTop        & 0xFF, (chipTop >> 24) & 0xFF);
                ImU32 botCol = IM_COL32((chipBottom >> 16) & 0xFF, (chipBottom >> 8) & 0xFF,
                                         chipBottom        & 0xFF, (settings->phEmblemFillColor >> 24) & 0xFF);
                const int emblemVtx0 = dl->VtxBuffer.Size;
                if (isDiamond) {
                    auto p = diamondPts(fillMin.x, fillMin.y, portSz);
                    dl->AddConvexPolyFilled(p.data(), (int)p.size(), topCol);
                } else {
                    dl->AddRectFilled(fillMin, fillMax, topCol, portraitRounding);
                }
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(
                    dl, emblemVtx0, dl->VtxBuffer.Size,
                    fillMin, ImVec2(fillMin.x, fillMax.y), topCol, botCol);
                const ImU32 rimCol = IM_COL32(255, 255, 255, 46);
                if (isDiamond) {
                    auto p = diamondPts(fillMin.x + ScalePx(1.5f), fillMin.y + ScalePx(1.5f), portSz - ScalePx(3.0f));
                    dl->AddPolyline(p.data(), (int)p.size(), rimCol, ImDrawFlags_Closed, ScalePx(1.0f));
                } else {
                    dl->AddRect(ImVec2(fillMin.x + ScalePx(1.5f), fillMin.y + ScalePx(1.5f)),
                                ImVec2(fillMax.x - ScalePx(1.5f), fillMax.y - ScalePx(1.5f)),
                                rimCol, portraitRounding, 0, ScalePx(1.0f));
                }
                if (phHitFlash > 0.0f) {
                    const ImU32 flashCol = IM_COL32(255, 30, 30, static_cast<int>(phHitFlash * 160.0f));
                    if (isDiamond) {
                        auto p = diamondPts(fillMin.x, fillMin.y, portSz);
                        dl->AddConvexPolyFilled(p.data(), (int)p.size(), flashCol);
                    } else {
                        dl->AddRectFilled(fillMin, fillMax, flashCol, portraitRounding);
                    }
                }
                ImGui::SetCursorScreenPos(ImVec2(curX, curY));
                ImGui::Dummy(ImVec2(portSz, portSz));

                char lvlBuf[16];
                snprintf(lvlBuf, sizeof(lvlBuf), "%d", static_cast<int>(player->GetLevel()));
                ImFont* font = ImGuiRenderer::GetHeadingFont();
                if (!font) font = ImGui::GetFont();
                float emblemFontSize = portSz * 0.45f;
                ImVec2 ts = font->CalcTextSizeA(emblemFontSize, FLT_MAX, 0.0f, lvlBuf);
                ImVec2 numPos((fillMin.x + fillMax.x) * 0.5f - ts.x * 0.5f,
                              (fillMin.y + fillMax.y) * 0.5f - ts.y * 0.5f);
                HUDDraw::TextFx fx;
                fx.font = font;
                fx.fontSize = emblemFontSize;
                HUDDraw::DrawTextStyled(dl, numPos, IM_COL32_WHITE, lvlBuf, fx);

                if (phLowHealth) {
                    // Soft glow, not a hard line: three expanding passes with falling
                    const float basePad = ScalePx(static_cast<float>(settings->phPortraitBorderWidth)) + ScalePx(2.0f);
                    const float expands[] = { 0.0f, ScalePx(2.0f), ScalePx(4.0f) };
                    const int   alphas[]  = { 160, 70, 30 };
                    const float coreW     = phCriticalHealth ? ScalePx(3.0f) : ScalePx(2.0f);
                    for (int gi = 2; gi >= 0; --gi) {
                        const float pad = basePad + expands[gi];
                        const float w   = coreW + expands[gi];
                        const ImU32 col = IM_COL32(255, 40, 40,
                                                   static_cast<int>(phPulseAlpha * alphas[gi]));
                        if (isDiamond) {
                            auto p = diamondPts(fillMin.x - pad, fillMin.y - pad, portSz + pad * 2.0f);
                            dl->AddPolyline(p.data(), (int)p.size(), col, ImDrawFlags_Closed, w);
                        } else {
                            dl->AddRect(ImVec2(fillMin.x - pad, fillMin.y - pad),
                                        ImVec2(fillMax.x + pad, fillMax.y + pad),
                                        col, portraitRounding > 0.0f ? portraitRounding + pad : 0.0f,
                                        0, w);
                        }
                    }
                }
            } else {
                ID3D11ShaderResourceView* portrait = nullptr;
                ImVec2 spriteUVMin(0, 0), spriteUVMax(1, 1);
                {
                    auto transform = PlayerStatus::GetTransformState();
                    if (transform == PlayerStatus::TransformState::Werewolf)
                        portrait = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Portraits\\Werewolf.jpg");
                    else if (transform == PlayerStatus::TransformState::VampireLord)
                        portrait = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Portraits\\VampireLord.jpg");
                    else if (transform == PlayerStatus::TransformState::Werebear)
                        portrait = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Portraits\\Werebear.jpg");
                    if (!portrait) {
                        if (auto* sprite = GetPlayerSpriteFrame(spriteUVMin, spriteUVMax))
                            portrait = sprite;
                        if (!portrait && settings->phPortraitIndex > 0)
                            portrait = BackgroundManager::GetBackground(BackgroundManager::Type::PlayerPortrait, settings->phPortraitIndex);
                        if (!portrait)
                            portrait = TextureManager::GetPortrait(player);
                    }
                }
                if (portrait) {
                    ImVec2 portMin(portX - portOffset, portY - portOffset);
                    ImVec2 portMax(portX + drawPortSz - portOffset, portY + drawPortSz - portOffset);

                    if (isDiamond) {
                        ImVec2 uv0 = spriteUVMin, uv1 = spriteUVMax;
                        ImVec2 uvTop   ((uv0.x + uv1.x) * 0.5f, uv0.y);
                        ImVec2 uvRight (uv1.x, (uv0.y + uv1.y) * 0.5f);
                        ImVec2 uvBottom((uv0.x + uv1.x) * 0.5f, uv1.y);
                        ImVec2 uvLeft  (uv0.x, (uv0.y + uv1.y) * 0.5f);
                        auto p = diamondPts(portMin.x, portMin.y, drawPortSz);
                        dl->AddImageQuad(reinterpret_cast<ImTextureID>(portrait),
                            p[0], p[1], p[2], p[3], uvTop, uvRight, uvBottom, uvLeft, IM_COL32_WHITE);
                    } else {
                        dl->AddImageRounded(reinterpret_cast<ImTextureID>(portrait),
                            portMin, portMax,
                            spriteUVMin, spriteUVMax, IM_COL32_WHITE, portraitRounding);
                    }
                    if (phLowHealth) {
                        ImU32 pulseCol = IM_COL32(255, 0, 0, static_cast<int>(phPulseAlpha * 255));
                        if (isDiamond) {
                            auto p = diamondPts(portMin.x, portMin.y, drawPortSz);
                            dl->AddConvexPolyFilled(p.data(), (int)p.size(), pulseCol);
                        } else {
                            dl->AddRectFilled(portMin, portMax, pulseCol, portraitRounding);
                        }
                    }
                    // Hit flash overlay on portrait taking hit
                    if (phHitFlash > 0.0f) {
                        const ImU32 flashCol = IM_COL32(255, 30, 30, static_cast<int>(phHitFlash * 160.0f));
                        if (isDiamond) {
                            auto p = diamondPts(portMin.x, portMin.y, drawPortSz);
                            dl->AddConvexPolyFilled(p.data(), (int)p.size(), flashCol);
                        } else {
                            dl->AddRectFilled(portMin, portMax, flashCol, portraitRounding);
                        }
                    }
                    ImGui::SetCursorScreenPos(ImVec2(curX, curY));
                    ImGui::Dummy(ImVec2(portSz, portSz));
                }
            }
            if (settings->phPortraitBorderWidth > 0 && isDiamond) {
                float bw = ScalePx(static_cast<float>(settings->phPortraitBorderWidth));
                ImU32 bc = IM_COL32((settings->phPortraitBorderColor >> 16) & 0xFF,
                                    (settings->phPortraitBorderColor >> 8) & 0xFF,
                                     settings->phPortraitBorderColor & 0xFF,
                                    (settings->phPortraitBorderColor >> 24) & 0xFF);
                auto p = diamondPts(portX, portY, portSz);
                dl->AddPolyline(p.data(), (int)p.size(), bc, ImDrawFlags_Closed, bw);

                if (settings->phShowXP && !settings->phXPAsBar) {
                    float xpFrac = 0.0f;
                    if (auto* runtimeData = player->GetInfoRuntimeData().skills)
                        if (runtimeData->data) {
                            float needed = runtimeData->data->levelThreshold;
                            if (needed > 0.0f)
                                xpFrac = std::clamp(runtimeData->data->xp / needed, 0.0f, 1.0f);
                        }

                    ImU32 xpCol = IM_COL32((settings->phXPColor >> 16) & 0xFF,
                                           (settings->phXPColor >>  8) & 0xFF,
                                            settings->phXPColor        & 0xFF,
                                           (settings->phXPColor >> 24) & 0xFF);

                    const float edgeLen = std::sqrt(2.0f) * portSz * 0.5f;
                    float fillLen = 4.0f * edgeLen * xpFrac;
                    for (int i = 0; i < 4 && fillLen > 0.0f; ++i) {
                        ImVec2 a = p[i];
                        ImVec2 b = p[(i + 1) % 4];
                        if (fillLen < edgeLen) {
                            float t = fillLen / edgeLen;
                            b = ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
                        }
                        dl->AddLine(a, b, xpCol, bw);
                        fillLen -= edgeLen;
                    }

                    char xpBuf[8];
                    snprintf(xpBuf, sizeof(xpBuf), "%.0f%%", xpFrac * 100.0f);
                    HUDDraw::DrawBadge(dl, ImVec2(portX + ScalePx(2.0f + settings->phXPBadgeOffsetX), portY + ScalePx(2.0f + settings->phXPBadgeOffsetY)), 0, xpBuf,
                                       MakeBadgeStyle());
                }
                if (settings->phLevelBadgeOnPortrait && !isEmblem) {
                    char lvlBuf[16];
                    snprintf(lvlBuf, sizeof(lvlBuf), "%d", static_cast<int>(player->GetLevel()));
                    HUDDraw::DrawBadge(dl, ImVec2(portX + portSz - ScalePx(2.0f) + ScalePx(settings->phLevelBadgeOffsetX), portY + portSz - ScalePx(2.0f) + ScalePx(settings->phLevelBadgeOffsetY)), 2, lvlBuf,
                                       MakeBadgeStyle());
                }
            } else if (settings->phPortraitBorderWidth > 0) {
                float bw = ScalePx(static_cast<float>(settings->phPortraitBorderWidth));
                ImU32 bc = IM_COL32((settings->phPortraitBorderColor >> 16) & 0xFF,
                                    (settings->phPortraitBorderColor >> 8) & 0xFF,
                                     settings->phPortraitBorderColor & 0xFF,
                                    (settings->phPortraitBorderColor >> 24) & 0xFF);

                if (settings->phShowXP && !settings->phXPAsBar) {
                    float xpFrac = 0.0f;
                    if (auto* runtimeData = player->GetInfoRuntimeData().skills)
                        if (runtimeData->data) {
                            float needed = runtimeData->data->levelThreshold;
                            if (needed > 0.0f)
                                xpFrac = std::clamp(runtimeData->data->xp / needed, 0.0f, 1.0f);
                        }

                    ImU32 xpCol = IM_COL32((settings->phXPColor >> 16) & 0xFF,
                                           (settings->phXPColor >>  8) & 0xFF,
                                            settings->phXPColor        & 0xFF,
                                           (settings->phXPColor >> 24) & 0xFF);

                    ImVec2 portMin(portX, portY);
                    ImVec2 portMax(portX + portSz, portY + portSz);

                    dl->AddRect(portMin, portMax, bc, portraitRounding, 0, bw);

                    float r = std::min(portraitRounding, portSz * 0.5f);
                    float x0 = portX, y0 = portY;
                    float x1 = portX + portSz, y1 = portY + portSz;

                    constexpr int kArcSegs = 12;
                    std::vector<ImVec2> pts;
                    pts.reserve(4 * (kArcSegs + 1));

                    auto addArc = [&](float cx, float cy, float startDeg, float endDeg) {
                        for (int i = 0; i <= kArcSegs; ++i) {
                            float a = (startDeg + (endDeg - startDeg) * i / kArcSegs) * (3.14159265358979323846f / 180.0f);
                            pts.push_back(ImVec2(cx + std::cos(a) * r, cy + std::sin(a) * r));
                        }
                    };

                    addArc(x0 + r, y0 + r, 180.0f, 270.0f);
                    addArc(x1 - r, y0 + r, 270.0f, 360.0f);
                    addArc(x1 - r, y1 - r, 0.0f, 90.0f);
                    addArc(x0 + r, y1 - r, 90.0f, 180.0f);
                    pts.push_back(pts.front());

                    std::vector<float> cumLen(pts.size(), 0.0f);
                    for (size_t i = 1; i < pts.size(); ++i) {
                        float dx = pts[i].x - pts[i-1].x;
                        float dy = pts[i].y - pts[i-1].y;
                        cumLen[i] = cumLen[i-1] + std::sqrt(dx*dx + dy*dy);
                    }
                    float totalPerim = cumLen.back();
                    float fillLen    = totalPerim * xpFrac;

                    for (size_t i = 1; i < pts.size(); ++i) {
                        if (cumLen[i-1] >= fillLen) break;
                        ImVec2 a = pts[i-1];
                        ImVec2 b = pts[i];
                        if (cumLen[i] > fillLen) {
                            float t = (fillLen - cumLen[i-1]) / (cumLen[i] - cumLen[i-1]);
                            b = ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
                        }
                        dl->AddLine(a, b, xpCol, bw);
                    }

                    {
                        char xpBuf[8];
                        snprintf(xpBuf, sizeof(xpBuf), "%.0f%%", xpFrac * 100.0f);
                        HUDDraw::DrawBadge(dl, ImVec2(portX + ScalePx(2.0f + settings->phXPBadgeOffsetX), portY + ScalePx(2.0f + settings->phXPBadgeOffsetY)), 0, xpBuf,
                                           MakeBadgeStyle());
                    }
                } else {
                    dl->AddRect(ImVec2(portX, portY), ImVec2(portX + portSz, portY + portSz), bc, portraitRounding, 0, bw);
                }
            }
            if (settings->phLevelBadgeOnPortrait && !isEmblem && !isDiamond) {
                char lvlBuf[16];
                snprintf(lvlBuf, sizeof(lvlBuf), "%d", static_cast<int>(player->GetLevel()));
                HUDDraw::DrawBadge(dl, ImVec2(portX + portSz - ScalePx(2.0f) + ScalePx(settings->phLevelBadgeOffsetX), portY + portSz - ScalePx(2.0f) + ScalePx(settings->phLevelBadgeOffsetY)), 2, lvlBuf,
                                   MakeBadgeStyle());
            }
            if (settings->phHaloEnable) {
                // Match the bars' unpenalized max so Survival reductions remain visible.
                auto fraction = [](float current, float maximum, float penalty) {
                    const float usable = 1.0f - penalty;
                    const float fullMax = maximum > 0.0f && usable > 0.0f ? maximum / usable : maximum;
                    return fullMax > 0.0f ? std::clamp(current / fullMax, 0.0f, 1.0f) : 0.0f;
                };
                const bool hasMagicka = !isWerewolf && !isWerebear;
                const float healthShare = settings->phHaloHealthShare / 100.0f;
                const float otherShare = (1.0f - healthShare) * 0.5f;
                const float shareSum = healthShare + otherShare * (hasMagicka ? 2.0f : 1.0f);
                const HUDDraw::RingSegment segments[] = {
                    { fraction(stats.health.current, stats.health.GetMax(), survivalPenalties.health),
                      healthShare / shareSum, settings->phHealthColor },
                    { fraction(stats.stamina.current, stats.stamina.GetMax(), survivalPenalties.stamina),
                      otherShare / shareSum, settings->phStaminaColor },
                    { fraction(stats.magicka.current, stats.magicka.GetMax(), survivalPenalties.magicka),
                      hasMagicka ? otherShare / shareSum : 0.0f, settings->phMagickaColor },
                };
                HUDDraw::ResourceRingStyle ring;
                ring.radius = portSz * 0.5f + haloThickness * 0.5f + haloOffset;
                ring.thickness = haloThickness;
                ring.gapDeg = static_cast<float>(settings->phHaloGap);
                ring.segments = segments;
                ring.segmentCount = 3;
                HUDDraw::DrawResourceRing(dl, ImVec2(portX + portSz * 0.5f, portY + portSz * 0.5f), ring);
            }
            curX += portSz + colSpacing;
        }

        if (portraitAlpha < 0.999f) {
            for (int i = portraitVtxStart; i < dl->VtxBuffer.Size; ++i) {
                ImDrawVert& v = dl->VtxBuffer[i];
                const unsigned int oldA = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
                const unsigned int newA = static_cast<unsigned int>(oldA * portraitAlpha + 0.5f);
                v.col = (v.col & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
            }
        }

        dl->ChannelsSetCurrent(0);

        float textX = curX;

        float rightColH = 0.0f;
        if (settings->phShowPortrait) {
            if (settings->phShowName) {
                const char* name = player->GetName();
                if (name && name[0]) {
                    ImFont* font = ImGuiRenderer::GetHeadingFont();
                    if (!font) font = ImGui::GetFont();
                    float sz = font->FontSize * settings->phNameFontScale;
                    rightColH += font->CalcTextSizeA(sz, FLT_MAX, 0.0f, name).y + barSpacing;
                }
            }
            if (settings->phShowHealth)                                    rightColH += barH   + barSpacing;
            if (settings->phShowMagicka && !isWerewolf && !isWerebear)    rightColH += barHMp + barSpacing;
            if (settings->phShowStamina)                                  rightColH += barHSp + barSpacing;
            if (settings->phShowXP && settings->phXPAsBar)                rightColH += static_cast<float>(settings->phXPBarHeight) + barSpacing;
            if (rightColH > 0.0f) rightColH -= barSpacing;
            float portraitCenterY = curY + portSz * 0.5f;
            curY = portraitCenterY - rightColH * 0.5f;
        }

        if (settings->phShowName) {
            const char* name = player->GetName();
            if (name && name[0]) {
                ImFont* font = ImGuiRenderer::GetHeadingFont();
                if (!font) font = ImGui::GetFont();
                float nameFontSize = font->FontSize * settings->phNameFontScale;
                ImVec2 nameSize = font->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, name);
                ImU32 nameCol = IM_COL32((settings->phNameColor >> 16) & 0xFF,
                                         (settings->phNameColor >> 8) & 0xFF,
                                          settings->phNameColor & 0xFF,
                                         (settings->phNameColor >> 24) & 0xFF);
                // Hard-coded drop shadow to match the bar text / regen pip / stat bonus,
                dl->AddText(font, nameFontSize, ImVec2(textX + ScalePx(1.0f), curY + ScalePx(1.0f)), IM_COL32(0, 0, 0, 180), name);
                dl->AddText(font, nameFontSize, ImVec2(textX, curY), nameCol, name);
                curY += nameSize.y + barSpacing;
            }
        }

        ID3D11ShaderResourceView* barBgTex   = nullptr;
        ID3D11ShaderResourceView* barFillTex = nullptr;
        if (settings->phBarStyleEnabled) {
            barBgTex   = BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, settings->phBarStyleIndex);
            barFillTex = BackgroundManager::GetBarFillTexture(settings->phBarStyleIndex);
        }
        struct BarTextures {
            ID3D11ShaderResourceView* bg = nullptr;
            ID3D11ShaderResourceView* fill = nullptr;
        };
        const BarTextures shared{barBgTex, barFillTex};
        auto ResolveBarTextures = [&](int overrideIndex) -> BarTextures {
            if (!settings->phBarStyleEnabled) return {};
            if (overrideIndex > 0) {
                BarTextures candidate{
                    BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, overrideIndex),
                    BackgroundManager::GetBarFillTexture(overrideIndex)
                };
                // Never mix a background and fill from different selections.
                if (candidate.bg && candidate.fill) return candidate;
            }
            return (shared.bg && shared.fill) ? shared : BarTextures{};
        };
        const BarTextures hpTextures = ResolveBarTextures(settings->phHealthBarStyleIndex);
        const BarTextures mpTextures = ResolveBarTextures(settings->phMagickaBarStyleIndex);
        const BarTextures spTextures = ResolveBarTextures(settings->phStaminaBarStyleIndex);

        bool  showBarText = settings->phShowBarText;
        ImU32 barTextCol  = IM_COL32((settings->phBarTextColor >> 16) & 0xFF,
                                     (settings->phBarTextColor >>  8) & 0xFF,
                                      settings->phBarTextColor        & 0xFF,
                                     (settings->phBarTextColor >> 24) & 0xFF);

        auto DrawBarText = [&](float x, float y, float w, float h, float current, float max, float a) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f/%.0f", current, max);
            ImFont* font = ImGui::GetFont();
            float fontSize = font->FontSize * settings->phBarTextFontScale;
            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
            ImU32 col = (barTextCol & ~IM_COL32_A_MASK) |
                        (static_cast<ImU32>(((barTextCol >> IM_COL32_A_SHIFT) & 0xFF) * a) << IM_COL32_A_SHIFT);
            ImVec2 textPos(x + (w - ts.x) * 0.5f, y + (h - ts.y) * 0.5f + barTextOffsetY);
            HUDDraw::TextFx fx;
            fx.font = font;
            fx.fontSize = fontSize;
            HUDDraw::DrawTextStyled(dl, textPos, col, buf, fx);
        };

        auto DrawCodeBar = [&](float x, float y, float w, float h, float frac, ImU32 col, float dynA, float ghostFrac, float maxVal = 100.0f) {
            const std::uint32_t argb = ((col >> IM_COL32_A_SHIFT) & 0xFF) << 24 |
                                        ((col >> IM_COL32_R_SHIFT) & 0xFF) << 16 |
                                        ((col >> IM_COL32_G_SHIFT) & 0xFF) << 8  |
                                        ((col >> IM_COL32_B_SHIFT) & 0xFF);
            HUDDraw::BarStyle bs;
            bs.width    = w;
            bs.height   = h;
            bs.fraction = frac;
            bs.rounding = ScalePx(settings->phBarCornerRadius);
            bs.leftEnd  = settings->phBarEndStyleLeft;
            bs.rightEnd = settings->phBarEndStyle;
            bs.drawTrack = settings->phBarTrack;
            bs.duotone   = settings->phBarDepthShading;
            bs.edgeLight = settings->phBarEdgeLight;
            bs.tip       = settings->phBarTip;
            bs.glow      = settings->phBarGlow;
            bs.gloss     = (settings->phBarVisualStyle == 2);
            if (settings->phBarVisualStyle == 3) {
                const int points = settings->phBarSegmentPoints;
                bs.segments = (points > 0) ? std::max(1, static_cast<int>(std::roundf(maxVal / static_cast<float>(points)))) : 10;
                bs.segmentGap = ScalePx(static_cast<float>(settings->phBarSegmentGap));
            } else {
                bs.segments = 0;
            }
            bs.outerBorderWidth = ScalePx(static_cast<float>(settings->phBarOuterBorderWidth));
            bs.outerBorderColor = settings->phBarOuterBorderColor;
            bs.innerBorderWidth = ScalePx(static_cast<float>(settings->phBarInnerBorderWidth));
            bs.innerBorderColor = settings->phBarInnerBorderColor;
            bs.fillColor = argb;
            if (settings->phBarVisualStyle == 1 || settings->phBarVisualStyle == 2) {
                const std::uint32_t a = (argb >> 24) & 0xFF;
                const std::uint32_t r = (argb >> 16) & 0xFF;
                const std::uint32_t g = (argb >> 8)  & 0xFF;
                const std::uint32_t b = argb & 0xFF;
                bs.fillColor2 = (a << 24) | ((r / 2) << 16) | ((g / 2) << 8) | (b / 2);
            }
            bs.ghostFraction = ghostFrac;
            bs.alpha = dynA;
            HUDDraw::DrawBarStyled(dl, ImVec2(x, y), bs);
        };

        auto DrawCodeBarPulse = [&](float x, float y, float w, float h, std::uint32_t pulseArgb) {
            HUDDraw::BarStyle bs;
            bs.width    = w;
            bs.height   = h;
            bs.fraction = 1.0f;
            bs.rounding = ScalePx(settings->phBarCornerRadius);
            bs.leftEnd  = settings->phBarEndStyleLeft;
            bs.rightEnd = settings->phBarEndStyle;
            bs.fillColor = pulseArgb;
            bs.drawTrack = false;
            bs.shadow    = false;
            bs.duotone   = false;
            bs.gloss     = false;
            bs.highlight = false;
            bs.tip       = false;
            bs.edgeLight = false;
            bs.glow      = false;
            bs.segments  = 0;
            bs.alpha     = 1.0f;
            HUDDraw::DrawBarStyled(dl, ImVec2(x, y), bs);
        };

        const std::uint32_t pulseSettingA = (settings->phBarPulseColor >> 24) & 0xFF;
        const std::uint32_t pulseR = (settings->phBarPulseColor >> 16) & 0xFF;
        const std::uint32_t pulseG = (settings->phBarPulseColor >> 8)  & 0xFF;
        const std::uint32_t pulseB =  settings->phBarPulseColor        & 0xFF;

        float regenHP = 0.0f, regenMP = 0.0f, regenSP = 0.0f;
        if (settings->phShowRegenPip) {
            regenHP = stats.health.GetMax()  * (stats.healthRegen  / 100.0f);
            regenMP = stats.magicka.GetMax() * (stats.magickaRegen / 100.0f);
            regenSP = stats.stamina.GetMax() * (stats.staminaRegen / 100.0f);
        }

        const float statBonusHP = settings->phShowFortify ? (stats.health.perm  + stats.health.temp)  : 0.0f;
        const float statBonusMP = settings->phShowFortify ? (stats.magicka.perm + stats.magicka.temp) : 0.0f;
        const float statBonusSP = settings->phShowFortify ? (stats.stamina.perm + stats.stamina.temp) : 0.0f;

        auto DrawStatBonus = [&](float x, float y, float w, float h, float bonusVal, float a) {
            if (bonusVal == 0.0f) return;
            ImFont* font     = ImGui::GetFont();
            float   fontSize = font->FontSize * settings->phFortifyFontScale;
            char    buf[16];
            if (bonusVal > 0.0f)
                snprintf(buf, sizeof(buf), "+%.0f", bonusVal);
            else
                snprintf(buf, sizeof(buf), "%.0f", bonusVal);
            const int aTxt = static_cast<int>(220 * a);
            ImU32   color = bonusVal > 0.0f ? IM_COL32(180, 230, 180, aTxt) : IM_COL32(230, 130, 130, aTxt);
            ImVec2  ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
            // Gap between the Fortify number and the bar edge scales with the portrait
            const float gap = std::max(ScalePx(6.0f), colSpacing * 0.10f);
            float   tx = settings->phFortifyRight ? (x + w + gap) : (x - ts.x - gap);
            float   ty = y + (h - ts.y) * 0.5f + barTextOffsetY;
            dl->AddText(font, fontSize, ImVec2(tx + ScalePx(1.0f), ty + ScalePx(1.0f)), IM_COL32(0, 0, 0, static_cast<int>(180 * a)), buf);
            dl->AddText(font, fontSize, ImVec2(tx, ty), color, buf);
        };

        auto DrawRegenPip = [&](float x, float y, float h, float regenVal, float a) {
            if (regenVal <= 0.0f) return;
            ImFont* font     = ImGui::GetFont();
            float   fontSize = font->FontSize * settings->phRegenFontScale;
            float   pip      = std::max(ScalePx(4.0f), fontSize * 0.55f);
            float   cx       = x + ScalePx(5.0f) + pip;
            float   cy       = y + h * 0.5f + barTextOffsetY;
            dl->AddCircleFilled(ImVec2(cx, cy), pip * 0.5f, IM_COL32(200, 200, 200, static_cast<int>(90 * a)));
            char buf[16];
            snprintf(buf, sizeof(buf), "+%.1f/s", regenVal);
            float tx = cx + pip * 0.5f + ScalePx(3.0f);
            float ty = cy - fontSize * 0.5f;
            dl->AddText(font, fontSize, ImVec2(tx + ScalePx(1.0f), ty + ScalePx(1.0f)), IM_COL32(0, 0, 0, static_cast<int>(180 * a)), buf);
            dl->AddText(font, fontSize, ImVec2(tx, ty), IM_COL32(200, 200, 200, static_cast<int>(180 * a)), buf);
        };

        if (settings->debugEnableVerboseLogging) {
            static double lastBarDiagnostic = -2.0;
            const double now = ImGui::GetTime();
            if (now - lastBarDiagnostic >= 2.0) {
                lastBarDiagnostic = now;
                logger::debug("[PlayerHUDBarDiag] regen hp={} mp={} sp={} bonus hp={} mp={} sp={} penalty hp={} mp={} sp={} ends left={} right={} textures hp={} mp={} sp={}",
                    regenHP, regenMP, regenSP, statBonusHP, statBonusMP, statBonusSP,
                    survivalPenalties.health, survivalPenalties.magicka, survivalPenalties.stamina,
                    settings->phBarEndStyleLeft, settings->phBarEndStyle,
                    hpTextures.fill != nullptr, mpTextures.fill != nullptr, spTextures.fill != nullptr);
            }
        }

        // Survival penalty: near-black segment on the right
        auto DrawPenaltyOverlay = [&](float x, float y, float w, float h, float penaltyPct, float a, ID3D11ShaderResourceView* fillTex) {
            if (penaltyPct <= 0.0f) return;
            const ImU32 tint = IM_COL32(20, 20, 20, static_cast<int>(220 * a));
            if (fillTex) {
                PlayerHUDPenalties::DrawPenaltySegment(dl, x, y, w, h, penaltyPct, tint, fillTex);
            } else {
                // A rectangle hides slanted right caps. Clip the bar's own silhouette to the penalty region.
                const float left = x + w * (1.0f - std::clamp(penaltyPct, 0.0f, 1.0f));
                dl->PushClipRect(ImVec2(left, y), ImVec2(x + w, y + h), true);
                DrawCodeBarPulse(x, y, w, h, HUDDraw::PackedToArgb(tint));
                dl->PopClipRect();
            }
        };

        auto GetDisplayMax = [](float maxValue, float penaltyPct) {
            const float usableFraction = 1.0f - penaltyPct;
            if (maxValue <= 0.0f || usableFraction <= 0.0f) {
                return maxValue;
            }
            return maxValue / usableFraction;
        };

        float dynAlphaFrameMax = 0.0f;
        bool  dynAlphaSeenThisFrame = false;

        float highestStatMax = 0.0f;
        if (settings->phScaledBarWidth) {
            const float hpDM = GetDisplayMax(stats.health.GetMax(),  survivalPenalties.health);
            const float mpDM = GetDisplayMax(stats.magicka.GetMax(), survivalPenalties.magicka);
            const float spDM = GetDisplayMax(stats.stamina.GetMax(), survivalPenalties.stamina);
            if (settings->phShowHealth)                                    highestStatMax = std::max(highestStatMax, hpDM);
            if (settings->phShowMagicka && !isWerewolf && !isWerebear)     highestStatMax = std::max(highestStatMax, mpDM);
            if (settings->phShowStamina)                                   highestStatMax = std::max(highestStatMax, spDM);
        }

        auto ScaledW = [&](float maxVal) -> float {
            if (!settings->phScaledBarWidth || maxVal <= 0.0f || highestStatMax <= 0.0f) return barW;
            // Floor at 170px (or phBarWidth, whichever is smaller) so bar text + regen pip
            const float minW = std::min(ScalePx(170.0f), barW);
            return std::max(minW, barW * (maxVal / highestStatMax));
        };

        auto ResolveBarWidth = [&](float displayMax, int overrideWidth) -> float {
            // Stat scaling uses only the shared width, even for invalid maxima.
            if (settings->phScaledBarWidth) return ScaledW(displayMax);
            return overrideWidth > 0 ? ScalePx(static_cast<float>(std::clamp(overrideWidth, 100, 600))) : barW;
        };
        float widestBarW = barW;

        const bool showBarIcons = settings->phShowBarIcons && !settings->phShowPortrait;
        HUDDraw::IconFx barIconFx;
        barIconFx.duotone = true;
        barIconFx.shadow  = true;
        barIconFx.outline = false;
        barIconFx.glow    = false;
        barIconFx.gloss   = false;
        auto DrawBarIcon = [&](ID3D11ShaderResourceView* icon, float bx, float by, float bh, float dynA) {
            if (!showBarIcons || !icon || dynA <= 0.0f) return;
            // Either text nudge direction gives thin bars larger, readable icons.
            const float size = bh + std::abs(barTextOffsetY);
            const ImVec2 iconPos(bx - ScalePx(6.0f) - size, by + (bh - size) * 0.5f);
            const std::uint32_t alphaByte = static_cast<std::uint32_t>(std::clamp(dynA, 0.0f, 1.0f) * 255.0f);
            const std::uint32_t argb = (alphaByte << 24) | 0x00FFFFFFu;
            HUDDraw::DrawIconStyled(dl, iconPos, size, reinterpret_cast<ImTextureID>(icon), argb, barIconFx);
        };

        if (settings->phShowHealth) {
            // Split-bar offset: draw at anchor + per-bar offset, but the stack advance
            const float bx = textX + ScalePx((float)settings->phHpOffsetX);
            const float by = curY  + ScalePx((float)settings->phHpOffsetY);
            const float displayMax = GetDisplayMax(stats.health.GetMax(), survivalPenalties.health);
            const float textMax = displayMax;
            const float barW = ResolveBarWidth(displayMax, settings->phHealthBarWidth);
            widestBarW = std::max(widestBarW, barW);
            const bool useCodeBar = !(hpTextures.bg && hpTextures.fill);
            float frac = (displayMax > 0.0f) ? std::clamp(stats.health.current / displayMax, 0.0f, 1.0f) : 0.0f;
            const float survFrac = std::clamp(survivalPenalties.health, 0.0f, 1.0f);
            const float dynA = ComputeDynamicAlpha(DynBar::HP, stats.health.current, displayMax, battleReadyNow, allowDrag);
            dynAlphaSeenThisFrame = true; dynAlphaFrameMax = std::max(dynAlphaFrameMax, dynA);
            ImU32 col  = IM_COL32((settings->phHealthColor >> 16) & 0xFF,
                                  (settings->phHealthColor >>  8) & 0xFF,
                                   settings->phHealthColor        & 0xFF,
                                  (settings->phHealthColor >> 24) & 0xFF);
            if (useCodeBar && settings->phBarSmoothDrain)
                frac = HUDDraw::TickBarValue((0x504C5952ull << 2) | 0, frac);
            const float ghostFrac = settings->phDamageGhost
                ? BarRenderer::TickGhost(0x50485048ull /* 'PHPH' player HP */, frac)
                : 0.0f;
            if (dynA > 0.0f) {
                if (useCodeBar) {
                    DrawCodeBar(bx, by, barW, barH, frac, col, dynA, ghostFrac, displayMax);
                } else {
                    BarRenderer::Style bar;
                    bar.width = barW; bar.height = barH; bar.fillFraction = frac;
                    bar.fillColor = col; bar.bgTex = hpTextures.bg; bar.fillTex = hpTextures.fill;
                    bar.alpha = dynA;
                    bar.ghostFraction = ghostFrac;
                    BarRenderer::Draw(dl, ImVec2(bx, by), bar);
                }
                DrawPenaltyOverlay(bx, by, barW, barH, survivalPenalties.health, dynA, hpTextures.fill);
                const float ax = bx + ScalePx(static_cast<float>(settings->phHpAnnotationOffsetX));
                const float ay = by + ScalePx(static_cast<float>(settings->phHpAnnotationOffsetY));
                DrawRegenPip(ax, ay, barH, regenHP, dynA);
                DrawStatBonus(ax, ay, barW, barH, statBonusHP, dynA);
                if (showBarText) DrawBarText(ax, ay, barW, barH, stats.health.current, textMax, dynA);
                DrawBarIcon(TextureManager::GetPartyIcon("health"), bx, by, barH, dynA);
            }
            curY += barH + barSpacing;
        }

        if (settings->phShowMagicka && !isWerewolf && !isWerebear) {
            const float bx = textX + ScalePx((float)settings->phMpOffsetX);
            const float by = curY  + ScalePx((float)settings->phMpOffsetY);
            const float bh = barHMp;
            const float displayMax = GetDisplayMax(stats.magicka.GetMax(), survivalPenalties.magicka);
            const float textMax = displayMax;
            const float barW = ResolveBarWidth(displayMax, settings->phMagickaBarWidth);
            widestBarW = std::max(widestBarW, barW);
            const bool useCodeBar = !(mpTextures.bg && mpTextures.fill);
            float frac = (displayMax > 0.0f) ? std::clamp(stats.magicka.current / displayMax, 0.0f, 1.0f) : 0.0f;
            const float survPenaltyFrac = std::clamp(survivalPenalties.magicka, 0.0f, 1.0f);
            const float dynA = ComputeDynamicAlpha(DynBar::MP, stats.magicka.current, displayMax, battleReadyNow, allowDrag);
            dynAlphaSeenThisFrame = true; dynAlphaFrameMax = std::max(dynAlphaFrameMax, dynA);
            ImU32 col  = IM_COL32((settings->phMagickaColor >> 16) & 0xFF,
                                  (settings->phMagickaColor >>  8) & 0xFF,
                                   settings->phMagickaColor        & 0xFF,
                                  (settings->phMagickaColor >> 24) & 0xFF);
            if (useCodeBar && settings->phBarSmoothDrain)
                frac = HUDDraw::TickBarValue((0x504C5952ull << 2) | 1, frac);
            if (dynA > 0.0f) {
                if (useCodeBar) {
                    DrawCodeBar(bx, by, barW, bh, frac, col, dynA, 0.0f, displayMax);
                } else {
                    BarRenderer::Style bar;
                    bar.width = barW; bar.height = bh; bar.fillFraction = frac;
                    bar.fillColor = col; bar.bgTex = mpTextures.bg; bar.fillTex = mpTextures.fill;
                    bar.alpha = dynA;
                    BarRenderer::Draw(dl, ImVec2(bx, by), bar);
                }
                DrawPenaltyOverlay(bx, by, barW, bh, survivalPenalties.magicka, dynA, mpTextures.fill);
                const float ax = bx + ScalePx(static_cast<float>(settings->phMpAnnotationOffsetX));
                const float ay = by + ScalePx(static_cast<float>(settings->phMpAnnotationOffsetY));
                DrawRegenPip(ax, ay, bh, regenMP, dynA);
                DrawStatBonus(ax, ay, barW, bh, statBonusMP, dynA);
                if (showBarText) DrawBarText(ax, ay, barW, bh, stats.magicka.current, textMax, dynA);
                DrawBarIcon(TextureManager::GetPartyIcon("magicka"), bx, by, bh, dynA);
                if (frac < 0.01f && pulseSettingA > 0) {
                    float pulseAlpha = (sinf(static_cast<float>(ImGui::GetTime()) * 2.5f) * 0.5f + 0.5f) * 0.3f + 0.05f;
                    std::uint32_t pulseA = static_cast<std::uint32_t>(pulseAlpha * 255 * dynA * (pulseSettingA / 255.0f));
                    if (useCodeBar) {
                        DrawCodeBarPulse(bx, by, barW, bh, (pulseA << 24) | (pulseR << 16) | (pulseG << 8) | pulseB);
                    } else {
                        ImU32 pulseCol = IM_COL32(pulseR, pulseG, pulseB, pulseA);
                        if (mpTextures.fill)
                            dl->AddImage(reinterpret_cast<ImTextureID>(mpTextures.fill), ImVec2(bx, by), ImVec2(bx + barW, by + bh), ImVec2(0,0), ImVec2(1,1), pulseCol);
                        else
                            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + bh), pulseCol);
                    }
                }
            }
            curY += bh + barSpacing;
        }

        if (settings->phShowStamina) {
            const float bx = textX + ScalePx((float)settings->phSpOffsetX);
            const float by = curY  + ScalePx((float)settings->phSpOffsetY);
            const float bh = barHSp;
            const float displayMax = GetDisplayMax(stats.stamina.GetMax(), survivalPenalties.stamina);
            const float textMax = displayMax;
            const float barW = ResolveBarWidth(displayMax, settings->phStaminaBarWidth);
            widestBarW = std::max(widestBarW, barW);
            const bool useCodeBar = !(spTextures.bg && spTextures.fill);
            float frac = (displayMax > 0.0f) ? std::clamp(stats.stamina.current / displayMax, 0.0f, 1.0f) : 0.0f;
            const float survFrac = std::clamp(survivalPenalties.stamina, 0.0f, 1.0f);
            const float dynA = ComputeDynamicAlpha(DynBar::SP, stats.stamina.current, displayMax, battleReadyNow, allowDrag);
            dynAlphaSeenThisFrame = true; dynAlphaFrameMax = std::max(dynAlphaFrameMax, dynA);
            ImU32 col  = IM_COL32((settings->phStaminaColor >> 16) & 0xFF,
                                  (settings->phStaminaColor >>  8) & 0xFF,
                                   settings->phStaminaColor        & 0xFF,
                                  (settings->phStaminaColor >> 24) & 0xFF);
            if (useCodeBar && settings->phBarSmoothDrain)
                frac = HUDDraw::TickBarValue((0x504C5952ull << 2) | 2, frac);
            if (dynA > 0.0f) {
                if (useCodeBar) {
                    DrawCodeBar(bx, by, barW, bh, frac, col, dynA, 0.0f, displayMax);
                } else {
                    BarRenderer::Style bar;
                    bar.width = barW; bar.height = bh; bar.fillFraction = frac;
                    bar.fillColor = col; bar.bgTex = spTextures.bg; bar.fillTex = spTextures.fill;
                    bar.alpha = dynA;
                    BarRenderer::Draw(dl, ImVec2(bx, by), bar);
                }
                DrawPenaltyOverlay(bx, by, barW, bh, survivalPenalties.stamina, dynA, spTextures.fill);
                const float ax = bx + ScalePx(static_cast<float>(settings->phSpAnnotationOffsetX));
                const float ay = by + ScalePx(static_cast<float>(settings->phSpAnnotationOffsetY));
                DrawRegenPip(ax, ay, bh, regenSP, dynA);
                DrawStatBonus(ax, ay, barW, bh, statBonusSP, dynA);
                if (showBarText) DrawBarText(ax, ay, barW, bh, stats.stamina.current, textMax, dynA);
                DrawBarIcon(TextureManager::GetPartyIcon("stamina"), bx, by, bh, dynA);
                if (frac < 0.01f && pulseSettingA > 0) {
                    float pulseAlpha = (sinf(static_cast<float>(ImGui::GetTime()) * 2.5f) * 0.5f + 0.5f) * 0.3f + 0.05f;
                    std::uint32_t pulseA = static_cast<std::uint32_t>(pulseAlpha * 255 * dynA * (pulseSettingA / 255.0f));
                    if (useCodeBar) {
                        DrawCodeBarPulse(bx, by, barW, bh, (pulseA << 24) | (pulseR << 16) | (pulseG << 8) | pulseB);
                    } else {
                        ImU32 pulseCol = IM_COL32(pulseR, pulseG, pulseB, pulseA);
                        if (spTextures.fill)
                            dl->AddImage(reinterpret_cast<ImTextureID>(spTextures.fill), ImVec2(bx, by), ImVec2(bx + barW, by + bh), ImVec2(0,0), ImVec2(1,1), pulseCol);
                        else
                            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + bh), pulseCol);
                    }
                }
            }
            curY += bh + barSpacing;
        }

        // XP bar: shown as a flat bar when portrait is hidden, or when XP as Bar is enabled.
        if (settings->phShowXP && (!settings->phShowPortrait || settings->phXPAsBar)) {
            float currentXP = 0.0f, xpNeeded = 1.0f;
            if (auto* runtimeData = player->GetInfoRuntimeData().skills)
                if (runtimeData->data) {
                    currentXP = runtimeData->data->xp;
                    xpNeeded  = runtimeData->data->levelThreshold;
                }
            float frac = (xpNeeded > 0.0f) ? std::clamp(currentXP / xpNeeded, 0.0f, 1.0f) : 0.0f;
            const bool levelUpReady = xpNeeded > 0.0f && currentXP >= xpNeeded;
            if (!allowDrag) {
                static bool seenLevelUpState = false;
                static bool lastLevelUpReady = false;
                if (seenLevelUpState && levelUpReady != lastLevelUpReady)
                    logger::debug("[PlayerXP] Level-up rune {}: level={}, xp={}, threshold={}",
                        levelUpReady ? "shown" : "cleared", player->GetLevel(), currentXP, xpNeeded);
                lastLevelUpReady = levelUpReady;
                seenLevelUpState = true;
            }
            const float dynA = ComputeDynamicAlpha(DynBar::XP, currentXP, xpNeeded, battleReadyNow, allowDrag);
            dynAlphaSeenThisFrame = true; dynAlphaFrameMax = std::max(dynAlphaFrameMax, dynA);
            if (dynA > 0.0f) {
                // XP height is its own setting (only reachable here: portrait off).
                float xpBarH = ScalePx(static_cast<float>(settings->phXPBarHeight));
                const float xpBx = textX + ScalePx((float)settings->phXPBarOffsetX);
                const float xpBy = curY  + ScalePx((float)settings->phXPBarOffsetY);
                const float xpBarW = (settings->phXPBarWidth > 0) ? ScalePx(static_cast<float>(settings->phXPBarWidth)) : barW;
                ImU32 col = IM_COL32((settings->phXPColor >> 16) & 0xFF,
                                     (settings->phXPColor >>  8) & 0xFF,
                                      settings->phXPColor        & 0xFF,
                                     (settings->phXPColor >> 24) & 0xFF);
                DrawCodeBar(xpBx, xpBy, xpBarW, xpBarH, frac, col, dynA, 0.0f, xpNeeded);
            }
            curY += ScalePx(static_cast<float>(settings->phXPBarHeight)) + barSpacing;

            if (dynA > 0.0f) {
                char lvlBuf[48];
                snprintf(lvlBuf, sizeof(lvlBuf),
                         LocalizationManager::Get("LevelShort").c_str(),
                         static_cast<int>(player->GetLevel()));
                char xpValues[64];
                snprintf(xpValues, sizeof(xpValues), "%.0f / %.0f", currentXP, xpNeeded);
                const std::string xpText = std::string(xpValues) + " " + LocalizationManager::Get("Common.XP");

                const float lineX = textX + ScalePx((float)settings->phXPBarOffsetX) + ScalePx(static_cast<float>(settings->phXPAnnotationOffsetX));
                const float lineY = curY + ScalePx((float)settings->phXPBarOffsetY) + ScalePx(static_cast<float>(settings->phXPAnnotationOffsetY));
                const int   txtA  = (int)(((settings->phBarTextColor >> 24) & 0xFF) * dynA);
                const ImU32 lvlCol = IM_COL32((settings->phBarTextColor >> 16) & 0xFF,
                                              (settings->phBarTextColor >>  8) & 0xFF,
                                               settings->phBarTextColor        & 0xFF, txtA);
                const ImU32 xpCol = IM_COL32((settings->phBarTextColor >> 16) & 0xFF,
                                              (settings->phBarTextColor >>  8) & 0xFF,
                                               settings->phBarTextColor        & 0xFF, (int)(txtA * 0.7f));
                ImFont*     hFont   = FontManager::GetSingleton()->GetHeadingFont();
                ImFont*     bFont   = ImGui::GetFont();
                if (!hFont) hFont = bFont;
                const float lvlSz   = hFont->FontSize * 0.8f;
                const float xpSz   = bFont->FontSize * 0.8f;
                const float lvlAsc  = hFont->Ascent * (lvlSz / hFont->FontSize);
                const float xpAsc  = bFont->Ascent * (xpSz / bFont->FontSize);
                const float baseY   = lineY + std::max(lvlAsc, xpAsc);
                const float lvlY    = baseY - lvlAsc;
                const float xpY    = baseY - xpAsc;
                // Use the shared duotone and soft shadow, keeping the existing font hierarchy.
                HUDDraw::TextFx textFx;
                textFx.shadow = true;
                textFx.duotone = true;
                textFx.outline = false;
                textFx.font = hFont;
                textFx.fontSize = lvlSz;
                HUDDraw::DrawTextStyled(dl, ImVec2(lineX, lvlY), lvlCol, lvlBuf, textFx);
                const float lvlW = hFont->CalcTextSizeA(lvlSz, FLT_MAX, 0.0f, lvlBuf).x;
                float xpX = lineX + lvlW + ScalePx(6.0f);
                if (levelUpReady) {
                    // Match visible digit bounds rather than the font's padded line height.
                    const ImFontGlyph* digit = hFont->FindGlyph('0');
                    const float fontScale = lvlSz / hFont->FontSize;
                    const float arrowH = digit ? (digit->Y1 - digit->Y0) * fontScale : lvlSz;
                    const float arrowY = digit ? lvlY + digit->Y0 * fontScale : lvlY;
                    DrawLevelUpRune(dl, xpX, arrowY, arrowH, txtA / 255.0f);
                    xpX += arrowH * 0.65f + ScalePx(6.0f);
                }
                textFx.font = bFont;
                textFx.fontSize = xpSz;
                HUDDraw::DrawTextStyled(dl, ImVec2(xpX, xpY), xpCol, xpText.c_str(), textFx);
            }
            curY += ImGui::GetTextLineHeight() + barSpacing;
        }

        if (dynAlphaSeenThisFrame) g_DynMaxAlphaLastFrame = dynAlphaFrameMax;

        dl->ChannelsMerge();

        // Advance cursor so AlwaysAutoResize sizes the window correctly
        float totalW = (settings->phShowPortrait ? portSz + colSpacing : 0.0f) + (hasRightColumn ? widestBarW : 0.0f) + padX * 2.0f;
        float totalH = curY - pos.y + padY;
        if (settings->phShowPortrait)
            totalH = (std::max)(totalH, portSz + padY * 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y));
        ImGui::Dummy(ImVec2(totalW, totalH));

        if (clipPushed)
            dl->PopClipRect();

        if (allowDrag && !forcedThisFrame) {
            ImVec2 winPos = ImGui::GetWindowPos();
            const float dx = winPos.x - targetPos.x;
            const float dy = winPos.y - targetPos.y;
            if (std::fabs(dx) > 1.5f || std::fabs(dy) > 1.5f) {
                settings->phPosX = static_cast<int>(std::lround(winPos.x + frameMargin));
                settings->phPosY = static_cast<int>(std::lround(winPos.y + frameMargin));
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    bool Render() {
        if (MenuUtils::ShouldBlockOverlay()) {
            g_HudAlpha = 0.0f;
            return false;
        }

        static std::chrono::steady_clock::time_point s_lastFadeTick{};
        const auto nowTick = std::chrono::steady_clock::now();
        float dt = (s_lastFadeTick.time_since_epoch().count() != 0)
            ? std::chrono::duration<float>(nowTick - s_lastFadeTick).count()
            : 0.016f;
        s_lastFadeTick = nowTick;
        dt = std::clamp(dt, 0.0f, 0.1f);
        if (ShouldShow()) {
            g_HudAlpha = (std::min)(g_HudAlpha + dt * 5.0f, 1.0f);
        } else {
            const float dur = (std::max)(0.1f, Settings::GetSingleton()->hvFadeDuration);
            g_HudAlpha = (std::max)(g_HudAlpha - dt / dur, 0.0f);
            if (g_HudAlpha <= 0.0f) return false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const float eased = g_HudAlpha * g_HudAlpha * (3.0f - 2.0f * g_HudAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased);
        RenderContent(false);
        ImGui::PopStyleVar();

        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(eased);
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());

        return true;
    }

}
