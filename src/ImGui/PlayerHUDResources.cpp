#include "../../includes/ImGui/PlayerHUDResources.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/TextureManager.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <RE/Skyrim.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace PlayerHUDResources {

    static bool g_ForcePos    = false;
    static bool g_ForceLocPos = false;
    static int  g_PrevGold   = -1;
    static float g_PrevCarry    = -1.0f;
    static float g_DisplayGold  = -1.0f;
    static RE::FormID g_PrevWeatherFormID = 0;
    static std::uint8_t g_PrevWeatherKind = 0xFF;

    // ── Master scale helpers ────────────────────────────────────────────────
    static float MasterScale()
    {
        const auto* settings = Settings::GetSingleton();
        return settings
            ? std::clamp(static_cast<float>(settings->presScale) / 100.0f, 0.5f, 2.0f)
            : 1.0f;
    }

    static float ScalePx(float value)
    {
        return value * MasterScale();
    }

    static int ScalePxInt0(int value)
    {
        if (value <= 0) return 0;
        return (std::max)(1, static_cast<int>(std::lround(ScalePx(static_cast<float>(value)))));
    }

    static int ScalePxSigned(int value)
    {
        return static_cast<int>(std::lround(ScalePx(static_cast<float>(value))));
    }

    constexpr float kCounterBlendSpeed = 18.0f;

    enum class WeatherKind : std::uint8_t {
        kUnknown = 0,
        kInterior,
        kClear,
        kCloudy,
        kRain,
        kSnow
    };

    struct WeatherDisplay {
        WeatherKind kind = WeatherKind::kUnknown;
        RE::FormID identityFormID = 0;
        std::string label;
        const char* primaryIcon = nullptr;
        const char* fallbackIcon = nullptr;
    };

    struct InteriorIconRule {
        const char* keywordEditorID;
        const char* iconName;
    };

    static RE::BGSKeyword* LookupKeyword(const char* editorID) {
        return editorID ? RE::TESForm::LookupByEditorID<RE::BGSKeyword>(editorID) : nullptr;
    }

    static bool LocationIsInteriorLike(RE::BGSLocation* location);

    static const char* GetInteriorIconName(RE::BGSLocation* location) {
        if (!location) {
            return "interior_generic";
        }

        static const InteriorIconRule rules[] = {
            { "LocTypeInn",               "interior_inn" },
            { "LocTypeCastle",            "interior_palace" },
            { "LocTypeJail",              "interior_jail" },
            { "LocTypeTemple",            "interior_temple" },
            { "LocTypeStore",             "interior_generic" },
            { "LocTypeCave",              "interior_cave" },
            { "LocTypeDungeon",           "interior_dungeon" },
            { "LocTypeDraugrCrypt",       "interior_dungeon" },
            { "LocTypeVampireLair",       "interior_dungeon" },
            { "LocTypeDragonPriestLair",  "interior_dungeon" },
            { "LocTypeMilitaryFort",      "interior_fort" },
            { "LocTypePlayerHouse",       "interior_home" },
            { "LocTypeDwelling",          "interior_home" }
        };

        static RE::BGSKeyword* cachedKeywords[std::size(rules)] = {};
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            for (std::size_t i = 0; i < std::size(rules); ++i) {
                cachedKeywords[i] = LookupKeyword(rules[i].keywordEditorID);
            }
        }

        for (std::size_t i = 0; i < std::size(rules); ++i) {
            if (cachedKeywords[i] && location->HasKeyword(cachedKeywords[i])) {
                return rules[i].iconName;
            }
        }

        return "interior_generic";
    }

    static bool LocationIsInteriorLike(RE::BGSLocation* location) {
        static const char* const kEnclosedBuildingKeywords[] = {
            "LocTypeInn",
            "LocTypeStore",
            "LocTypeDwelling",
            "LocTypePlayerHouse",
            "LocTypeTemple",
            "LocTypeJail",
        };

        static RE::BGSKeyword* cached[std::size(kEnclosedBuildingKeywords)] = {};
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            for (std::size_t i = 0; i < std::size(kEnclosedBuildingKeywords); ++i) {
                cached[i] = LookupKeyword(kEnclosedBuildingKeywords[i]);
            }
        }

        if (!location) {
            return false;
        }
        for (std::size_t i = 0; i < std::size(cached); ++i) {
            if (cached[i] && location->HasKeyword(cached[i])) {
                return true;
            }
        }
        return false;
    }

    static void AnimateTowards(float& displayValue, float targetValue, float dt, float snapEpsilon) {
        float t = 1.0f - std::exp(-kCounterBlendSpeed * dt);
        displayValue += (targetValue - displayValue) * t;
        if (std::abs(displayValue - targetValue) < snapEpsilon) {
            displayValue = targetValue;
        }
    }

    static WeatherDisplay GetCurrentWeatherDisplay(RE::PlayerCharacter* player) {
        if (!player) {
            return {};
        }

        auto* cell = player->GetParentCell();

        auto buildLocationDisplay = [](RE::TESObjectCELL* c, RE::BGSLocation* location) -> WeatherDisplay {
            std::string label;
            if (const char* cellName = c->GetFullName(); cellName && cellName[0] != '\0') {
                label = cellName;
            } else if (location) {
                if (const char* locationName = location->GetFullName(); locationName && locationName[0] != '\0') {
                    label = locationName;
                }
            }

            if (label.empty()) {
                label = LocalizationManager::Get("interior_fallback");
            }

            int maxChars = Settings::GetSingleton()->presLocMaxChars;
            if (maxChars > 3 && label.size() > static_cast<std::size_t>(maxChars)) {
                label.resize(static_cast<std::size_t>(maxChars) - 3);
                label += "...";
            }

            RE::FormID identity = location ? location->GetFormID() : c->GetFormID();
            return { WeatherKind::kInterior, identity, std::move(label), GetInteriorIconName(location), nullptr };
        };

        if (cell && cell->IsInteriorCell()) {
            return buildLocationDisplay(cell, cell->GetLocation());
        }

        if (cell) {
            auto* location = cell->GetLocation();
            if (LocationIsInteriorLike(location)) {
                return buildLocationDisplay(cell, location);
            }
        }

        auto* sky = RE::Sky::GetSingleton();
        if (!sky) {
            return {};
        }

        auto* w = sky->currentWeather ? sky->currentWeather : sky->lastWeather;
        if (!w) {
            return {};
        }

        RE::FormID identity = w->GetFormID();
        using Flag = RE::TESWeather::WeatherDataFlag;
        auto flags = w->data.flags;

        if (flags.any(Flag::kSnow)) {
            return { WeatherKind::kSnow, identity, LocalizationManager::Get("weather_snow"), "weather_snow", nullptr };
        }

        if (flags.any(Flag::kRainy)) {
            return { WeatherKind::kRain, identity, LocalizationManager::Get("weather_rain"), "weather_rain", nullptr };
        }

        if (flags.any(Flag::kCloudy)) {
            return { WeatherKind::kCloudy, identity, LocalizationManager::Get("weather_cloudy"), "weather_cloudy", "cloud" };
        }

        return { WeatherKind::kClear, identity, LocalizationManager::Get("weather_clear"), "weather_clear", "sun" };
    }

    static void DrawWindowBackground(std::uint32_t argb, int bgStyle = 0, int fadeMode = 0,
                                     int fadeAnchorPct = 25, int fadeLengthPct = 75) {
        if (((argb >> 24) & 0xFF) == 0) return;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 wmax(wp.x + ImGui::GetWindowWidth(), wp.y + ImGui::GetWindowHeight());
        if (bgStyle == 0) {
            ImU32 col = IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
            dl->AddRectFilled(wp, wmax, col);
        } else {
            HUDDraw::FadeBgStyle fs;
            fs.mode      = static_cast<HUDDraw::FadeBgMode>(fadeMode);
            fs.color     = argb;
            fs.solidFrac = fadeAnchorPct / 100.0f;
            fs.fadeFrac  = fadeLengthPct / 100.0f;
            HUDDraw::DrawFadeBg(dl, wp, wmax, fs);
        }
    }

    void ForceReposition() { g_ForcePos = true; g_ForceLocPos = true; }

    static void FadeVertexRange(ImDrawList* dl, int start, float alpha) {
        if (alpha >= 0.999f) return;
        for (int i = start; i < dl->VtxBuffer.Size; ++i) {
            ImDrawVert& v = dl->VtxBuffer[i];
            const unsigned int oldA = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
            const unsigned int newA = static_cast<unsigned int>(oldA * alpha + 0.5f);
            v.col = (v.col & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
        }
    }

    static WeatherDisplay g_LastWeatherDisplay;

    static void DetectWeatherLocationChange(RE::PlayerCharacter* player) {
        g_LastWeatherDisplay = GetCurrentWeatherDisplay(player);

        std::uint8_t currentWeatherKind = static_cast<std::uint8_t>(g_LastWeatherDisplay.kind);
        if (g_PrevWeatherKind == 0xFF) {
            g_PrevWeatherKind = currentWeatherKind;
            g_PrevWeatherFormID = g_LastWeatherDisplay.identityFormID;
        } else if (currentWeatherKind != g_PrevWeatherKind || g_LastWeatherDisplay.identityFormID != g_PrevWeatherFormID) {
            g_PrevWeatherKind = currentWeatherKind;
            g_PrevWeatherFormID = g_LastWeatherDisplay.identityFormID;
            HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerLocation);
        }
    }

    static void RenderWindow(bool allowDrag, bool visible = true, float alpha = 1.0f) {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerResourcesWidget) return;
        if (!visible) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;
        if (MenuUtils::ShouldBlockOverlay()) return;

        int gold = 0;
        if (auto* goldForm = RE::TESForm::LookupByID(0x0000000F))
            if (auto* goldObj = goldForm->As<RE::TESBoundObject>())
                gold = player->GetItemCount(goldObj);

        float carryWeight    = player->GetWeightInContainer();
        float carryWeightMax = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);

        if (g_DisplayGold < 0.0f || g_PrevGold < 0) {
            g_PrevGold = gold;
            g_DisplayGold = static_cast<float>(gold);
        } else {
            int goldDiff = gold - g_PrevGold;
            float carryDiff = carryWeight - g_PrevCarry;
            if (goldDiff != 0 || std::abs(carryDiff) > 0.05f) {
                g_PrevGold = gold;
                g_PrevCarry = carryWeight;
                HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerResources);

                if (std::abs(goldDiff) <= 1) {
                    g_DisplayGold = static_cast<float>(gold);
                }
            }
        }

        float bgPad = (((settings->presBgColor >> 24) & 0xFF) != 0)
                          ? ScalePx(static_cast<float>(settings->presBgPadding)) : ScalePx(4.0f);
        // Extra horizontal-only padding so the background card doesn't hug the icon/text left/right edges.
        float bgPadX = bgPad + ScalePx(6.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(bgPadX, bgPad));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(ScalePx(4.0f), ScalePx(3.0f)));

        const ImVec2 targetPos(static_cast<float>(settings->presPosX), static_cast<float>(settings->presPosY));
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

        ImGui::Begin("##playerHUDresources", nullptr, flags);
        ImDrawList* fadeDl = ImGui::GetWindowDrawList();
        const int fadeVtxStart = fadeDl->VtxBuffer.Size;
        ImGui::SetWindowFontScale(settings->presFontScale);
        DrawWindowBackground(settings->presBgColor, settings->presBgStyle, settings->presFadeMode,
                             settings->presFadeAnchor, settings->presFadeLength);

        float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.001f, 0.1f);
        AnimateTowards(g_DisplayGold, static_cast<float>(gold), dt, 0.15f);

        float autoIconSz = std::floor(ImGui::GetTextLineHeight() * 1.2f);
        float iconSz  = settings->presIconSize > 0 ? ScalePx(static_cast<float>(settings->presIconSize)) : autoIconSz;
        float textOff = (iconSz - ImGui::GetTextLineHeight()) * 0.5f;
        float spacing = ScalePx(static_cast<float>(settings->presSpacing));
        bool  horiz   = (settings->presLayout == 1);

        auto argbToU32 = [](std::uint32_t argb) -> ImU32 {
            return IM_COL32(
                (argb >> 16) & 0xFF,
                (argb >>  8) & 0xFF,
                 argb        & 0xFF,
                (argb >> 24) & 0xFF);
        };

        ImVec4 textCol(
            ((settings->presTextColor >> 16) & 0xFF) / 255.0f,
            ((settings->presTextColor >>  8) & 0xFF) / 255.0f,
             (settings->presTextColor        & 0xFF) / 255.0f,
            ((settings->presTextColor >> 24) & 0xFF) / 255.0f);

        auto* iconGold   = TextureManager::GetPartyIcon("gold");
        auto* iconWeight = TextureManager::GetPartyIcon("weight");
        const WeatherDisplay& weatherDisplay = g_LastWeatherDisplay;

        char buf[64];
        bool firstItem = true;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 textColU32 = IM_COL32(
            static_cast<int>(textCol.x * 255),
            static_cast<int>(textCol.y * 255),
            static_cast<int>(textCol.z * 255),
            static_cast<int>(textCol.w * 255));

        auto DrawItem = [&](ID3D11ShaderResourceView* icon, const char* text, ImU32 iconTint) {
            if (horiz && !firstItem)
                ImGui::SameLine(0.0f, spacing);
            else if (!horiz && !firstItem)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);
            firstItem = false;

            if (icon) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                HUDDraw::DrawIconStyled(dl, p, iconSz,
                    reinterpret_cast<ImTextureID>(icon),
                    HUDDraw::PackedToArgb(iconTint), HUDDraw::IconFx{});
                ImGui::Dummy(ImVec2(iconSz, iconSz));
                ImGui::SameLine(0.0f, ScalePx(4.0f));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOff);
            }

            ImVec2 textPos = ImGui::GetCursorScreenPos();
            ImVec2 textSize = ImGui::CalcTextSize(text);
            HUDDraw::DrawTextStyled(dl, textPos, textColU32, text, HUDDraw::TextFx{});
            ImGui::Dummy(ImVec2(textSize.x, textSize.y));
        };

        if (settings->presShowGold) {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(g_DisplayGold)));
            DrawItem(iconGold, buf, argbToU32(settings->presGoldColor));
        }

        if (settings->presShowCarryWeight) {
            snprintf(buf, sizeof(buf), "%.0f / %.0f", carryWeight, carryWeightMax);
            DrawItem(iconWeight, buf, argbToU32(settings->presCarryColor));
        }

        if (settings->presShowTime) {
            auto* calendar = RE::Calendar::GetSingleton();
            if (calendar) {
                float gameTime   = calendar->GetCurrentGameTime();
                float hoursFloat = std::fmod(gameTime, 1.0f) * 24.0f;
                int   hours24    = static_cast<int>(hoursFloat) % 24;
                int   minutes    = static_cast<int>((hoursFloat - std::floor(hoursFloat)) * 60.0f);
                bool  isNight    = hours24 < 6 || hours24 >= 20;
                if (settings->pres24HourTime) {
                    snprintf(buf, sizeof(buf), "%02d:%02d", hours24, minutes);
                } else {
                    bool isPM    = hours24 >= 12;
                    int  hours12 = hours24 % 12;
                    if (hours12 == 0) hours12 = 12;
                    snprintf(buf, sizeof(buf), "%d:%02d %s", hours12, minutes, isPM ? "PM" : "AM");
                }
                auto* iconTime = TextureManager::GetPartyIcon(isNight ? "moon" : "sun");
                DrawItem(iconTime, buf, argbToU32(settings->presTimeColor));
            }
        }

        if (settings->presShowWeather && !settings->presLocSeparate &&
            weatherDisplay.kind != WeatherKind::kUnknown) {
            auto* iconWeather = weatherDisplay.primaryIcon ? TextureManager::GetPartyIcon(weatherDisplay.primaryIcon) : nullptr;
            if (!iconWeather && weatherDisplay.fallbackIcon) {
                iconWeather = TextureManager::GetPartyIcon(weatherDisplay.fallbackIcon);
            }
            DrawItem(iconWeather, weatherDisplay.label.c_str(), argbToU32(settings->presWeatherColor));
        }

        ImGui::SetWindowFontScale(1.0f);

        if (allowDrag) {
            ImVec2 winPos = ImGui::GetWindowPos();
            settings->presPosX = static_cast<int>(std::lround(winPos.x));
            settings->presPosY = static_cast<int>(std::lround(winPos.y));
        }

        FadeVertexRange(fadeDl, fadeVtxStart, alpha);

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    static void RenderLocationWindow(bool allowDrag, bool visible = true, float alpha = 1.0f) {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerResourcesWidget) return;
        if (!settings->presLocSeparate || !settings->presShowWeather) return;
        if (!visible) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;
        if (MenuUtils::ShouldBlockOverlay()) return;

        const WeatherDisplay& wd = g_LastWeatherDisplay;
        if (wd.kind == WeatherKind::kUnknown) return;

        float bgPad = (((settings->presLocBgColor >> 24) & 0xFF) != 0)
                          ? ScalePx(static_cast<float>(settings->presLocBgPadding)) : ScalePx(4.0f);
        // Extra horizontal-only padding so the background card doesn't hug the icon/text left/right edges.
        float bgPadX = bgPad + ScalePx(6.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(bgPadX, bgPad));

        const bool alignR = settings->presLocAlignRight;
        float anchorLeftX = static_cast<float>(settings->presLocPosX);
        if (alignR) {
            float baseFont = ImGui::GetFontSize();
            float scale    = settings->presFontScale;
            float lineH    = baseFont * scale;
            float iconSzM  = settings->presIconSize > 0
                                 ? ScalePx(static_cast<float>(settings->presIconSize))
                                 : std::floor(lineH * 1.2f);
            float iconSlotM = settings->presLocIconDiamond ? iconSzM * 1.55f : iconSzM;
            float textW    = ImGui::CalcTextSize(wd.label.c_str()).x * scale;
            bool  hasIcon  = (wd.primaryIcon != nullptr) || (wd.fallbackIcon != nullptr);
            float contentW = (hasIcon ? iconSlotM + ScalePx(4.0f) : 0.0f) + textW;
            anchorLeftX = static_cast<float>(settings->presLocPosX) - contentW - 2.0f * bgPadX;
        }

        const ImVec2 targetPos(anchorLeftX, static_cast<float>(settings->presLocPosY));
        if (!allowDrag || g_ForceLocPos) {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
            g_ForceLocPos = false;
        } else {
            ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar  |
                                 ImGuiWindowFlags_NoResize     |
                                 ImGuiWindowFlags_NoScrollbar  |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoBackground;
        if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

        ImGui::Begin("##playerHUDlocation", nullptr, flags);
        ImDrawList* fadeDl = ImGui::GetWindowDrawList();
        const int fadeVtxStart = fadeDl->VtxBuffer.Size;
        ImGui::SetWindowFontScale(settings->presFontScale);

        DrawWindowBackground(settings->presLocBgColor, settings->presLocBgStyle, settings->presLocFadeMode,
                             settings->presLocFadeAnchor, settings->presLocFadeLength);

        float autoIconSz = std::floor(ImGui::GetTextLineHeight() * 1.2f);
        float iconSz  = settings->presIconSize > 0 ? ScalePx(static_cast<float>(settings->presIconSize)) : autoIconSz;
        float textOff = (iconSz - ImGui::GetTextLineHeight()) * 0.5f;

        ImU32 iconTint = IM_COL32(
            (settings->presWeatherColor >> 16) & 0xFF,
            (settings->presWeatherColor >>  8) & 0xFF,
             settings->presWeatherColor        & 0xFF,
            (settings->presWeatherColor >> 24) & 0xFF);
        ImU32 textColU32 = IM_COL32(
            (settings->presTextColor >> 16) & 0xFF,
            (settings->presTextColor >>  8) & 0xFF,
             settings->presTextColor        & 0xFF,
            (settings->presTextColor >> 24) & 0xFF);

        auto* icon = wd.primaryIcon ? TextureManager::GetPartyIcon(wd.primaryIcon) : nullptr;
        if (!icon && wd.fallbackIcon) icon = TextureManager::GetPartyIcon(wd.fallbackIcon);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        const bool  showDiamond = settings->presLocIconDiamond && icon;
        const float diamondSz   = iconSz * 1.55f;
        const float gap     = icon ? ScalePx(4.0f) : 0.0f;
        const float iconSlot = icon ? (showDiamond ? diamondSz : iconSz) : 0.0f;
        const float textW   = ImGui::CalcTextSize(wd.label.c_str()).x;
        const float rowH    = (std::max)(showDiamond ? diamondSz : iconSz, ImGui::GetTextLineHeight());

        ImVec2 origin = ImGui::GetCursorScreenPos();
        float iconSlotX = alignR ? origin.x + textW + gap : origin.x;
        float textX  = alignR ? origin.x               : origin.x + iconSlot + gap;
        float iconX  = iconSlotX + (iconSlot - iconSz) * 0.5f;
        float iconY  = showDiamond
            ? origin.y + (rowH - iconSz) * 0.5f
            : origin.y + (textOff < 0.0f ? -textOff : 0.0f);
        float textY  = origin.y + (textOff > 0.0f ?  textOff : 0.0f);
        if (rowH > ImGui::GetTextLineHeight()) {
            textY = origin.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
        }

        if (showDiamond) {
            float dCenterX = iconX + iconSz * 0.5f;
            float dCenterY = iconY + iconSz * 0.5f;
            float dMinX = dCenterX - diamondSz * 0.5f;
            float dMinY = dCenterY - diamondSz * 0.5f;
            auto diamondPts = [](float x, float y, float sz) {
                std::array<ImVec2, 4> p;
                p[0] = ImVec2(x + sz * 0.5f, y);
                p[1] = ImVec2(x + sz,        y + sz * 0.5f);
                p[2] = ImVec2(x + sz * 0.5f, y + sz);
                p[3] = ImVec2(x,             y + sz * 0.5f);
                return p;
            };
            const std::uint32_t chipTop    = HUDDraw::ChipTopFromArgb(settings->presLocIconDiamondColor);
            const std::uint32_t chipBottom = HUDDraw::ChipBottomFromArgb(settings->presLocIconDiamondColor);
            ImU32 topCol = IM_COL32((chipTop >> 16) & 0xFF, (chipTop >> 8) & 0xFF,
                                     chipTop        & 0xFF, (chipTop >> 24) & 0xFF);
            ImU32 botCol = IM_COL32((chipBottom >> 16) & 0xFF, (chipBottom >> 8) & 0xFF,
                                     chipBottom        & 0xFF, (settings->presLocIconDiamondColor >> 24) & 0xFF);

            auto pFill = diamondPts(dMinX, dMinY, diamondSz);
            dl->AddConvexPolyFilled(pFill.data(), (int)pFill.size(), topCol);
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, dl->VtxBuffer.Size - 4, dl->VtxBuffer.Size,
                ImVec2(dMinX, dMinY), ImVec2(dMinX, dMinY + diamondSz), topCol, botCol);

            const ImU32 rimCol = IM_COL32(255, 255, 255, 46);
            auto pRim = diamondPts(dMinX + ScalePx(1.5f), dMinY + ScalePx(1.5f), diamondSz - ScalePx(3.0f));
            dl->AddPolyline(pRim.data(), (int)pRim.size(), rimCol, ImDrawFlags_Closed, ScalePx(1.0f));

            if (settings->presLocIconDiamondBorderWidth > 0) {
                ImU32 borderCol = IM_COL32(
                    (settings->presLocIconDiamondBorderColor >> 16) & 0xFF,
                    (settings->presLocIconDiamondBorderColor >>  8) & 0xFF,
                     settings->presLocIconDiamondBorderColor        & 0xFF,
                    (settings->presLocIconDiamondBorderColor >> 24) & 0xFF);
                auto pBorder = diamondPts(dMinX, dMinY, diamondSz);
                dl->AddPolyline(pBorder.data(), (int)pBorder.size(), borderCol, ImDrawFlags_Closed,
                    static_cast<float>(ScalePxInt0(settings->presLocIconDiamondBorderWidth)));
            }
        }

        if (icon) {
            HUDDraw::DrawIconStyled(dl, ImVec2(iconX, iconY), iconSz,
                reinterpret_cast<ImTextureID>(icon),
                HUDDraw::PackedToArgb(iconTint), HUDDraw::IconFx{});
        }
        HUDDraw::DrawTextStyled(dl, ImVec2(textX, textY), textColU32, wd.label.c_str(), HUDDraw::TextFx{});

        ImGui::Dummy(ImVec2(iconSlot + gap + textW, rowH));

        ImGui::SetWindowFontScale(1.0f);

        if (allowDrag) {
            ImVec2 winPos = ImGui::GetWindowPos();
            float anchorX = alignR ? (winPos.x + ImGui::GetWindowWidth()) : winPos.x;
            settings->presLocPosX = static_cast<int>(std::lround(anchorX));
            settings->presLocPosY = static_cast<int>(std::lround(winPos.y));
        }

        FadeVertexRange(fadeDl, fadeVtxStart, alpha);

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    void RenderHUD() {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerResourcesWidget) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) return;

        DetectWeatherLocationChange(player);

        bool lootMenuOpen = false;
        if (auto* ui = RE::UI::GetSingleton()) {
            lootMenuOpen = ui->IsMenuOpen("LootMenu");
        }

        bool visibleRes = lootMenuOpen || HUDVisibility::IsVisible(HUDVisibility::WidgetID::PlayerResources);
        bool visibleLoc = settings->presLocSeparate && settings->presShowWeather &&
                           HUDVisibility::IsVisible(HUDVisibility::WidgetID::PlayerLocation);
        if (!visibleRes && !visibleLoc) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        float alphaRes = lootMenuOpen ? 1.0f : HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerResources);
        float alphaLoc = HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerLocation);
        RenderWindow(false, visibleRes, alphaRes);
        RenderLocationWindow(false, visibleLoc, alphaLoc);
        ImGui::EndFrame();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

    void RenderPreview() {
        DetectWeatherLocationChange(RE::PlayerCharacter::GetSingleton());
        RenderWindow(true);
        RenderLocationWindow(true);
    }

}
