
#include "../../includes/ImGui/PlayerHUDEnvironment.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/Utils/HudItem.h"
#include "../../includes/Utils/WeatherDisplay.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/SurvivalState.h"

#include <RE/Skyrim.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace PlayerHUDEnvironment {

    namespace {

        bool g_ForcePos = false;

        constexpr float kFadeSeconds = 0.35f;

        constexpr int kNameLimit = 28;

        ImTextureID PartyIcon(const char* name)
        {
            return reinterpret_cast<ImTextureID>(TextureManager::GetPartyIcon(name));
        }

        ImTextureID WidgetIcon(const char* name)
        {
            return reinterpret_cast<ImTextureID>(
                TextureManager::GetTexture(std::string("Data\\Interface\\PartySheet\\Widgets\\") + name + ".png"));
        }

        std::uint32_t ColdColor() { return Settings::GetSingleton()->phSurvivalColdColor; }
        std::uint32_t FireColor() { return Settings::GetSingleton()->phSurvivalFireColor; }

        float Scale()
        {
            const auto* settings = Settings::GetSingleton();
            const float survival = std::clamp(static_cast<float>(settings->phSurvivalScale) / 100.0f, 0.5f, 2.0f);
            const float own = std::clamp(static_cast<float>(settings->phEnvScale) / 100.0f, 0.5f, 2.0f);
            return survival * own;
        }

        std::string Loc(const char* key, const char* english)
        {
            std::string text = LocalizationManager::Get(key);
            return (text.empty() || text == key) ? std::string(english) : text;
        }

        struct Card {
            ImTextureID stateIcon = 0, weatherIcon = 0;
            std::string area;
            std::string headline, sub, areaLabel, weatherLabel;
            std::uint32_t color = FireColor();
            int   trend = 0, selfStage = -1;
            float self = -1.0f, areaFill = -1.0f, fireFill = 0.0f, stageStart = -1.0f, stageEnd = -1.0f;
            bool  fire = false, fireSpent = false, water = false, sheltered = false, warmArea = false, held = false;
            bool  danger = false;
        };

        std::string StageWord(int stage) { return PlayerHUDSurvival::GetColdStageName(stage); }

        int StageSeverity(int stage) { return stage == 0 ? 0 : stage <= 2 ? 1 : stage <= 4 ? 2 : 3; }

        std::uint32_t SeverityColor(int severity, std::uint32_t alarmColor)
        {
            return severity == 0 ? FireColor() : severity <= 2 ? ColdColor() : alarmColor;
        }

        void FillArea(Card& card, const SurvivalState::Snapshot& s, std::uint32_t alarmColor)
        {
            if (!s.ambient) { card.area = s.regionWord; return; }
            const auto& a = *s.ambient;
            int severity = 0;

            if (a.hasTemperature && a.stage < 0) {
                const float t = a.temperature;
                card.area = t >= 5.0f  ? Loc("Condition.Mild", "Mild")
                          : t >= 0.0f  ? Loc("Condition.Cool", "Cool")
                          : t >= -9.0f ? Loc("Condition.Cold", "Cold")
                                       : Loc("Condition.Freezing", "Freezing");
                severity = t >= 5.0f ? 0 : t >= 0.0f ? 1 : 2;
            } else if (a.stage >= 0 && a.areaFill >= 0.0f) {
                card.area = StageWord(a.stage);
                severity = StageSeverity(a.stage);
            } else if (a.stage >= 0) {
                card.area = a.stage == 0 ? Loc("Condition.WarmArea", "Warm area")
                          : a.stage <= 2 ? Loc("Condition.CoolArea", "Cool area")
                                         : Loc("Condition.FreezingArea", "Freezing area");
                severity = a.stage == 0 ? 0 : a.stage <= 2 ? 1 : 2;
            } else {
                card.area = s.regionWord;
            }
            card.areaLabel = a.inFreezingWater ? StageWord(5) : card.area;
            if (a.inFreezingWater) {
                card.area = Loc("Condition.FreezingWater", "Freezing water");
                severity = 3;
            }

            card.water = a.inFreezingWater;
            card.warmArea = severity == 0;
            card.danger = severity == 3;
            card.color = SeverityColor(severity, alarmColor);
        }

        void FillCold(Card& card, const PlayerHUDSurvival::AmbientCold& a)
        {
            card.trend = a.trend;
            card.fire = a.nearFire && a.trend <= 0;
            if (a.hasTemperature) {
                constexpr float kSettleSeconds = 6.0f, kResetSeconds = 0.01f;
                const bool stillCold = card.fire && a.trend == 0 && a.selfStage > 0 && !a.inFreezingWater;
                card.fireSpent = stillCold;
                if (UIAnimation::Step("phEnvSpentFire", stillCold, kSettleSeconds, kResetSeconds) < 1.0f) card.fireSpent = false;
            }
            card.sheltered = a.sheltered;
            card.selfStage = a.selfStage;
            card.held = a.coldHeld;
            card.self = a.selfFill;
            card.areaFill = a.areaFill;
            card.fireFill = a.fireFill;
            card.stageStart = a.stageStart;
            card.stageEnd = a.stageEnd;
        }

        void FillWeather(Card& card, const SurvivalState::Snapshot& s)
        {
            const auto weather = WeatherDisplay::Current(RE::PlayerCharacter::GetSingleton(), kNameLimit);
            if (weather.kind == WeatherDisplay::Kind::kUnknown) return;

            const bool clearNight = weather.kind == WeatherDisplay::Kind::kClear && s.night;
            if (weather.primaryIcon) card.weatherIcon = PartyIcon(clearNight ? "moon" : weather.primaryIcon);
            if (!card.weatherIcon && weather.fallbackIcon) card.weatherIcon = PartyIcon(weather.fallbackIcon);
            card.weatherLabel = s.night ? Loc("Condition.Night", "Night") : Loc("Condition.Day", "Day");

            if (card.area.empty()) card.area = weather.label;
            else if (weather.kind == WeatherDisplay::Kind::kInterior) card.sub = weather.label;
        }

        bool HasScale(const Card& card) { return card.areaFill >= 0.0f; }

        bool WarmedByFire(const Card& card) { return card.fire && !card.water; }

        bool AreaApplies(const Card& card) { return !WarmedByFire(card) && !card.held; }

        void FillHeadline(Card& card, const SurvivalState::Snapshot& s)
        {
            constexpr std::uint32_t kSpentFireColor = 0xFFA9A39B;
            if (card.water)                          card.headline = card.area;
            else if (card.trend < 0 && card.fire)    card.headline = Loc("Condition.WarmingByFire", "Warming by the fire");
            else if (card.trend < 0)                 card.headline = Loc("Condition.HeadWarming", "Warming up");
            else if (card.trend > 0)                 card.headline = Loc("Condition.HeadColder", "Getting colder");
            else if (card.fireSpent)                 card.headline = Loc("Condition.NotWarming", "Not warming up");
            else if (card.fire)                      card.headline = Loc("Condition.ByFire", "By the fire");
            else if (HasScale(card) && card.selfStage >= 0) {
                card.headline = StageWord(card.selfStage);
                if (!card.danger) card.color = SeverityColor(StageSeverity(card.selfStage), card.color);
            }
            else                                     card.headline = card.area;

            if (card.sub.empty() && card.headline != card.area) card.sub = card.area;
            if (card.sheltered) {
                constexpr int kFullShelter = 2;
                const std::string shelter = s.ambient->shelterLevel >= kFullShelter
                    ? Loc("Condition.Sheltered", "Sheltered") : Loc("Condition.PartlySheltered", "Partly sheltered");
                card.sub += card.sub.empty() ? shelter : ", " + shelter;
            }

            if (card.fire && !card.water) card.danger = false;
            if (card.fireSpent) card.color = kSpentFireColor;
            else if (!card.danger && (card.fire || card.trend < 0)) card.color = FireColor();

            if (!s.ambient)                             card.stateIcon = card.weatherIcon;
            else if (card.water)                        card.stateIcon = WidgetIcon("survival_wetness");
            else if (card.fire || card.trend < 0)       card.stateIcon = PartyIcon("fire");
            else if (card.trend > 0)                    card.stateIcon = WidgetIcon("survival_cold");
            else if (card.sheltered)                    card.stateIcon = WidgetIcon("survival_tent");
            else if (card.selfStage >= 0 ? card.selfStage == 0 : card.warmArea)
                                                        card.stateIcon = PartyIcon(s.indoors ? "interior_home" : "weather_clear");
            else                                        card.stateIcon = WidgetIcon("survival_cold");
        }

        Card BuildCard()
        {
            const auto& s = SurvivalState::Get();
            Card card;
            FillArea(card, s, Settings::GetSingleton()->phSurvivalAlarmColor);
            if (s.ambient) FillCold(card, *s.ambient);
            FillWeather(card, s);
            FillHeadline(card, s);
            constexpr float kSamePlace = 0.01f;
            if (!HasScale(card)) card.areaLabel = card.sub;
            else if (!AreaApplies(card) || card.areaLabel == card.headline ||
                     std::fabs(card.self - card.areaFill) < kSamePlace) card.areaLabel.clear();
            if (HasScale(card) && WarmedByFire(card) && card.selfStage >= 0) card.areaLabel = StageWord(card.selfStage);
            return card;
        }

        HudItem::Style BuildStyle()
        {
            const auto* settings = Settings::GetSingleton();
            const float scale = Scale();
            HudItem::Style style;
            style.font = ImGui::GetFont();
            style.fontSize = style.font->LegacySize * settings->phSurvivalLabelScale * scale;
            style.iconSize = 26.0f * scale;
            style.padX = 5.0f * scale;
            style.padY = 3.0f * scale;
            style.gap = 5.0f * scale;
            style.showCount = false;
            style.descriptionScale = 1.0f;

            style.normalColor = settings->phSurvivalLabelColor;
            style.badColor = settings->phSurvivalAlarmColor;
            style.pulseBad = settings->phSurvivalAlarmPulse;

            style.iconFx.duotone = settings->phSurvivalIconDuotone;
            style.iconFx.shadow = settings->phSurvivalIconShadow;
            style.iconFx.outline = settings->phSurvivalIconOutline;
            style.iconFx.glow = settings->phSurvivalIconGlow;
            style.iconFx.gloss = settings->phSurvivalIconGloss;
            style.iconFx.outlineWidth = (std::max)(1, static_cast<int>(std::lround(settings->phSurvivalIconOutlineWidth * scale)));
            style.iconFx.outlineColor = settings->phSurvivalIconOutlineColor;

            style.textFx.shadow = settings->phSurvivalTextShadow;
            style.textFx.outline = settings->phSurvivalTextOutline;
            style.textFx.outlineOpacity = settings->phSurvivalTextOutlineOpacity;
            style.textFx.duotone = settings->phSurvivalTextDuotone;
            return style;
        }

        struct LineLayout {
            ImVec2 size;
            float lineX, lineW, lineY, textY, gap, weatherIcon;
            float icon;
        };

        LineLayout MeasureLine(const Card& card, const HudItem::Style& style)
        {
            const float scale = Scale();
            LineLayout l;
            l.icon = 30.0f * scale;
            l.gap = 12.0f * scale;
            l.weatherIcon = style.fontSize * 1.8f;

            l.lineX = 0.0f;
            if (card.weatherIcon) {
                const float day = HudItem::TextSize(style, style.fontSize, Loc("Condition.Day", "Day").c_str()).x;
                const float night = HudItem::TextSize(style, style.fontSize, Loc("Condition.Night", "Night").c_str()).x;
                l.lineX = l.weatherIcon + 4.0f * scale + (std::max)(day, night) + 4.0f * scale;
            }
            l.lineW = 360.0f * scale;
            l.lineY = l.icon + 8.0f * scale;
            l.textY = l.lineY + 10.0f * scale;
            l.size = ImVec2(l.lineX + l.lineW, l.textY + style.fontSize * 1.3f + 2.0f * scale);
            return l;
        }

        ImU32 Packed(std::uint32_t argb, float alpha) { return HudItem::ArgbToPacked(HudItem::FadeArgb(argb, alpha)); }

        void DrawText(ImDrawList* dl, ImVec2 pos, std::uint32_t argb, const std::string& text, const HudItem::Style& style, float alpha)
        {
            HUDDraw::TextFx fx = style.textFx;
            fx.font = style.font;
            fx.fontSize = style.fontSize;
            HUDDraw::DrawTextStyled(dl, pos, Packed(argb, alpha), text.c_str(), fx);
        }

        void DrawFireGlow(ImDrawList* dl, ImVec2 center, float size, const Card& card, float alpha)
        {
            if (!card.fire || card.fireSpent) return;
            dl->AddCircleFilled(center, size * 0.75f, Packed(FireColor(), 0.10f * alpha));
            dl->AddCircleFilled(center, size * 0.55f, Packed(FireColor(), 0.14f * alpha));
        }

        void DrawFadedStroke(ImDrawList* dl, float x0, float x1, float y, float h, ImU32 left, ImU32 right)
        {
            dl->AddRectFilledMultiColor(ImVec2(x0, y - h * 0.5f), ImVec2(x1, y + h * 0.5f), left, right, right, left);
        }

        void DrawBaseLine(ImDrawList* dl, float x0, float x1, float y, float alpha)
        {
            const float scale = Scale();
            const float fade = (x1 - x0) * 0.14f;
            for (const auto& [color, h] : { std::pair{ 0x78000000u, 6.0f * scale }, std::pair{ 0xB4DCDCDCu, 2.0f * scale } }) {
                const ImU32 on = Packed(color, alpha);
                const ImU32 off = on & ~IM_COL32_A_MASK;
                DrawFadedStroke(dl, x0, x0 + fade, y, h, off, on);
                DrawFadedStroke(dl, x0 + fade, x1 - fade, y, h, on, on);
                DrawFadedStroke(dl, x1 - fade, x1, y, h, on, off);
            }
        }

        void DrawDashes(ImDrawList* dl, float from, float to, float y, float h, ImU32 col)
        {
            const float dash = 4.0f * Scale(), step = dash * 1.7f;
            for (float x = from; x < to; x += step)
                dl->AddRectFilled(ImVec2(x, y - h * 0.5f), ImVec2((std::min)(x + dash, to), y + h * 0.5f), col);
        }

        void DrawDiamond(ImDrawList* dl, ImVec2 c, float r, float alpha)
        {
            dl->AddQuadFilled(ImVec2(c.x, c.y - r - 1.5f), ImVec2(c.x + r + 1.5f, c.y), ImVec2(c.x, c.y + r + 1.5f), ImVec2(c.x - r - 1.5f, c.y), Packed(0xC8000000, alpha));
            dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), Packed(0xFFF4F4F0, alpha));
        }

        float StageEdge(const Card& card)
        {
            constexpr float kMinGap = 0.015f;
            if (card.areaLabel.empty() || card.stageStart < 0.0f || !AreaApplies(card)) return -1.0f;
            const float edge = card.areaFill >= card.self ? card.stageStart : card.stageEnd;
            return std::fabs(edge - card.areaFill) < kMinGap ? -1.0f : edge;
        }

        void DrawHollowDiamond(ImDrawList* dl, ImVec2 c, float r, float alpha)
        {
            const ImVec2 top(c.x, c.y - r), right(c.x + r, c.y), bottom(c.x, c.y + r), left(c.x - r, c.y);
            dl->AddQuadFilled(top, right, bottom, left, Packed(0xE61E2226, alpha));
            dl->AddQuad(top, right, bottom, left, Packed(0xFFF4F4F0, alpha), 1.2f * Scale());
        }

        float TargetFill(const Card& card)
        {
            if (AreaApplies(card)) return card.areaFill;
            return card.trend < 0 ? card.fireFill : card.self;
        }

        void PlaceLabels(float lo, float hi, float headX, float headW, float areaX, float areaW, float gap,
                         float& headLeft, float& areaLeft)
        {
            const auto centered = [&](float x, float w) { return (std::max)(lo, (std::min)(x - w * 0.5f, hi - w)); };
            headLeft = centered(headX, headW);
            areaLeft = centered(areaX, areaW);
            if (areaW <= 0.0f) return;
            if (areaX >= headX) {
                areaLeft = (std::max)(areaLeft, headLeft + headW + gap);
                if (areaLeft + areaW > hi) {
                    areaLeft = hi - areaW;
                    headLeft = (std::min)(headLeft, areaLeft - gap - headW);
                }
            } else {
                areaLeft = (std::min)(areaLeft, headLeft - gap - areaW);
                if (areaLeft < lo) {
                    areaLeft = lo;
                    headLeft = (std::max)(headLeft, areaLeft + areaW + gap);
                }
            }
        }

        void DrawLine(ImDrawList* dl, ImVec2 pos, const Card& card, const LineLayout& l, const HudItem::Style& style, float alpha)
        {
            const float scale = Scale();
            const float x0 = pos.x + l.lineX, x1 = x0 + l.lineW, y = pos.y + l.lineY;
            DrawBaseLine(dl, x0, x1, y, alpha);

            const float inset = l.lineW * 0.08f;
            const float left = x0 + inset, width = l.lineW - inset * 2.0f;
            float iconX = x0 + l.lineW * 0.5f;
            float areaLabelX = iconX;

            if (HasScale(card)) {
                const float self = UIAnimation::Follow("phEnvSelf", card.self, 6.0f, 6.0f);
                const float area = UIAnimation::Follow("phEnvArea", TargetFill(card), 6.0f, 6.0f);
                const float selfX = left + width * self, areaX = left + width * area;
                const float solidEnd = (std::min)(selfX, areaX);
                const float h = 4.0f * scale;

                const ImU32 solid = Packed(card.danger ? card.color : ColdColor(), alpha);
                constexpr float kFullLeadIn = 0.02f;
                const ImU32 leadIn = Packed(card.danger ? card.color : ColdColor(), alpha * std::clamp(self / kFullLeadIn, 0.0f, 1.0f));
                DrawFadedStroke(dl, x0, left, y, h, leadIn & ~IM_COL32_A_MASK, leadIn);
                DrawFadedStroke(dl, left, solidEnd, y, h, solid, solid);
                DrawDashes(dl, solidEnd, (std::max)(selfX, areaX), y, h, Packed(areaX >= selfX ? ColdColor() : FireColor(), alpha));
                const float edge = StageEdge(card);
                if (edge >= 0.0f) DrawHollowDiamond(dl, ImVec2(left + width * edge, y), 4.0f * scale, alpha);
                const float diamond = UIAnimation::Step("phEnvDiamond", AreaApplies(card), kFadeSeconds, kFadeSeconds);
                if (diamond > 0.01f) DrawDiamond(dl, ImVec2(areaX, y), 4.5f * scale, alpha * diamond);
                iconX = selfX;
                areaLabelX = WarmedByFire(card) ? iconX : edge >= 0.0f ? (areaX + left + width * edge) * 0.5f : areaX;
            }

            const ImVec2 iconCenter(iconX, pos.y + l.icon * 0.5f);
            DrawFireGlow(dl, iconCenter, l.icon, card, alpha);
            if (card.stateIcon)
                HUDDraw::DrawIconStyled(dl, ImVec2(iconCenter.x - l.icon * 0.5f, pos.y), l.icon, card.stateIcon,
                                        HudItem::FadeArgb(card.color, alpha), style.iconFx);

            const bool showArea = !card.areaLabel.empty();
            const float headW = HudItem::TextSize(style, style.fontSize, card.headline.c_str()).x;
            const float areaW = showArea ? HudItem::TextSize(style, style.fontSize, card.areaLabel.c_str()).x : 0.0f;
            float headLeft = 0.0f, areaLeft = 0.0f;
            PlaceLabels(x0, x1, iconX, headW, areaLabelX, areaW, l.gap, headLeft, areaLeft);
            const float textY = pos.y + l.textY;
            DrawText(dl, ImVec2(headLeft, textY), card.color, card.headline, style, alpha);
            if (showArea) DrawText(dl, ImVec2(areaLeft, textY), style.normalColor, card.areaLabel, style, alpha);

            if (l.lineX <= 0.0f) return;
            HUDDraw::DrawIconStyled(dl, ImVec2(pos.x, y - l.weatherIcon * 0.5f), l.weatherIcon, card.weatherIcon,
                                    HudItem::FadeArgb(0xFFDCDCDC, alpha), style.iconFx);
            DrawText(dl, ImVec2(pos.x + l.weatherIcon + 4.0f * scale, y - style.fontSize * 0.5f), style.normalColor,
                     card.weatherLabel, style, alpha);
        }

        void RenderWindow(bool allowDrag)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings->enablePlayerHud || !settings->phEnvEnable) return;
            if (!allowDrag && !PlayerHUDSurvival::IsAvailable()) return;

            const Card card = BuildCard();
            if (card.headline.empty() && !card.stateIcon) return;

            const HudItem::Style style = BuildStyle();
            const LineLayout layout = MeasureLine(card, style);
            const ImVec2 contentSize = layout.size;
            constexpr auto corner = HUDFlow::Corner::TopLeft;
            const ImVec2 anchor(static_cast<float>(settings->phEnvPosX), static_cast<float>(settings->phEnvPosY));

            static ImVec2 lastSize(-1.0f, -1.0f);
            if (contentSize.x != lastSize.x || contentSize.y != lastSize.y) {
                lastSize = contentSize;
                g_ForcePos = true;
            }
            const ImVec2 targetPos = HUDFlow::TopLeftFromAnchor(anchor, contentSize, corner);
            const bool forcedThisFrame = g_ForcePos;
            if (!allowDrag || g_ForcePos) {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
                g_ForcePos = false;
            } else {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
            }

            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoBackground;
            if (!allowDrag) flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
            ImGui::Begin("##playerHUDenvironment", nullptr, flags);

            const ImVec2 windowPos = ImGui::GetWindowPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);
            const ImVec2 pinned = HUDFlow::AnchorFromTopLeft(windowPos, contentSize, corner);
            const ImVec2 topLeft = HUDFlow::TopLeftFromAnchor(pinned, contentSize, corner);
            DrawLine(dl, topLeft, card, layout, style, 1.0f);
            dl->PopClipRect();
            ImGui::Dummy(contentSize);

            if (allowDrag && !forcedThisFrame) {
                if (std::fabs(windowPos.x - targetPos.x) > 1.5f || std::fabs(windowPos.y - targetPos.y) > 1.5f) {
                    const ImVec2 newAnchor = HUDFlow::AnchorFromTopLeft(windowPos, contentSize, corner);
                    settings->phEnvPosX = static_cast<int>(std::lround(newAnchor.x));
                    settings->phEnvPosY = static_cast<int>(std::lround(newAnchor.y));
                }
            }

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }

    void ForceReposition() { g_ForcePos = true; }

    bool HidesResourcesWeather()
    {
        const auto* settings = Settings::GetSingleton();
        return settings->enablePlayerHud && settings->phEnvEnable && settings->phEnvHideResourcesWeather &&
               PlayerHUDSurvival::IsAvailable();
    }

    bool HidesColdNeed()
    {
        const auto* settings = Settings::GetSingleton();
        return settings->enablePlayerHud && settings->phEnvEnable && settings->phEnvHideColdNeed;
    }

    void RenderHUD()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->phEnvEnable) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderWindow(false);
        ImGui::EndFrame();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

    void RenderPreview() { RenderWindow(true); }
}
