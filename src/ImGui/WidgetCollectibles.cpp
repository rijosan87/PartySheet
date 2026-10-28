#include "../../includes/ImGui/WidgetCollectibles.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/BarRenderer.h"
#include "../../includes/Utils/IconRenderer.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/ImGui/WidgetDrawHelpers.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Settings.h"
#include "../../includes/SettingsIni.h"
#include "../../includes/TextureManager.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/PlayerStatus.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/ImGui/WidgetCollectiblesInternal.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/Utils/WidgetJsonParser.h"
#include "../../includes/HUDVisibility.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <Windows.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

namespace WidgetCollectibles {

    std::vector<CollectibleEntry> g_Collectibles;
    std::vector<int>              g_CachedCounts;
    bool                          g_ForceCollectiblesPos = true;
    bool                          g_Loaded               = false;

    void ForceReposition()
    {
        g_ForceCollectiblesPos = true;
    }

    bool IsQuestComplete(const CollectibleEntry& e)
    {
        if (e.questResolvedID == 0) return false;
        auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(e.questResolvedID);
        return quest && quest->IsCompleted();
    }

    namespace {

        // ── Master scale helpers ────────────────────────────────────────────────
        static float MasterScale()
        {
            const auto* settings = Settings::GetSingleton();
            return settings
                ? std::clamp(static_cast<float>(settings->ccScale) / 100.0f, 0.5f, 2.0f)
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

        static int GetItemCount(const CollectibleEntry& e)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return 0;
            int total = 0;
            for (const auto& item : e.items) {
                if (item.resolvedFormID == 0) continue;
                auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(item.resolvedFormID);
                if (form) total += player->GetItemCount(form);
            }
            return total;
        }

        static void RenderCollectiblesWindow(bool allowDrag)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings || !settings->enableCollectiblesWidget) return;
            if (MenuUtils::ShouldBlockOverlay()) return;

            int visibleCount = 0;
            for (size_t i = 0; i < g_Collectibles.size(); ++i) {
                const auto& e = g_Collectibles[i];
                if (!e.enabled) continue;
                if (IsQuestComplete(e) && !allowDrag) continue;
                int count = (i < g_CachedCounts.size()) ? g_CachedCounts[i] : 0;
                if (count == 0) continue;
                ++visibleCount;
            }
            if (visibleCount == 0) return;

            ImVec2 targetPos(static_cast<float>(settings->ccPosX),
                             static_cast<float>(settings->ccPosY));
            if (g_ForceCollectiblesPos) {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
                g_ForceCollectiblesPos = false;
            } else {
                ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
            }

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar      |
                ImGuiWindowFlags_NoResize        |
                ImGuiWindowFlags_AlwaysAutoResize|
                ImGuiWindowFlags_NoScrollbar     |
                ImGuiWindowFlags_NoSavedSettings;
            if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            if (ImGui::Begin("##collectibles", nullptr, flags)) {

                ImGui::SetWindowFontScale(settings->ccLabelScale);

                ImVec4 bgCol      = ColorUtils::ArgbToVec4(settings->ccBgColor);
                float  gap        = ScalePx(static_cast<float>(settings->ccRowSpacing));
                float  padX       = ScalePx(8.0f);
                float  padY       = ScalePx(8.0f);
                float  padBottom  = ScalePx(static_cast<float>(settings->ccPaddingBottom));
                float  padRight   = ScalePx(10.0f);  // extra right breathing room inside card
                float  itemGap    = ScalePx(5.0f);   // gap between label row and count/bar row
                float  iconSz     = ScalePx(static_cast<float>(settings->ccIconSize));
                float  lineH      = ImGui::GetTextLineHeightWithSpacing();
                float  barH       = ScalePx(static_cast<float>(settings->ccBarHeight));
                float  barW       = ScalePx(static_cast<float>(settings->ccBarWidth));
                bool   vertical   = (settings->ccCardStyle == 1);
                bool   rowFlow    = (settings->ccDirection == 1);
                float  curCardX   = 0.0f;
                ImDrawList* dl    = ImGui::GetWindowDrawList();
                ImVec2 winPos     = ImGui::GetWindowPos();
                ImU32  bgU32      = ImGui::ColorConvertFloat4ToU32(bgCol);
                ImU32  barFillCol = IM_COL32(
                    (settings->ccBarColor >> 16) & 0xFF,
                    (settings->ccBarColor >>  8) & 0xFF,
                     settings->ccBarColor        & 0xFF, 255);
                ImU32  iconBgCol  = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->ccIconBgColor));
                ImU32  iconBdrCol = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->ccIconBorderColor));

                IconRenderer::Style iconTemplate;
                iconTemplate.bgColor      = iconBgCol;
                iconTemplate.chip         = true;
                iconTemplate.glowEnabled  = settings->ccIconGlowEnabled;
                iconTemplate.borderColor  = iconBdrCol;
                iconTemplate.borderWidth  = ScalePxInt0(settings->ccIconBorderWidth);
                IconRenderer::SetZoom(iconTemplate, settings->ccIconZoom, settings->ccIconFocusU, settings->ccIconFocusV);

                auto DrawBar = [&](float screenX, float screenY, float bW, float fraction, const char* countText) {
                    BarRenderer::Style bar;
                    bar.width         = bW;
                    bar.height        = barH;
                    bar.fillFraction  = fraction;
                    bar.fillColor     = barFillCol;
                    bar.text          = countText;
                    bar.textFontScale = settings->ccBarTextScale;
                    if (settings->ccBarStyleEnabled) {
                        bar.bgTex   = BackgroundManager::GetBackground(BackgroundManager::Type::BarStyle, settings->ccBarStyleIndex);
                        bar.fillTex = BackgroundManager::GetBarFillTexture(settings->ccBarStyleIndex);
                    }
                    BarRenderer::Draw(dl, ImVec2(screenX, screenY), bar);
                };

                bool firstVisible = true;
                float maxCardHeight = 0.0f;
                for (size_t i = 0; i < g_Collectibles.size(); ++i) {
                    auto& e = g_Collectibles[i];
                    if (!e.enabled) continue;
                    bool questDone = IsQuestComplete(e);
                    if (questDone && !allowDrag) continue;

                    int  count   = (i < g_CachedCounts.size()) ? g_CachedCounts[i] : 0;
                    if (count == 0) continue;
                    bool goalMet = questDone || (count >= e.goal);
                    ImVec4 textCol = ColorUtils::RgbToVec4(goalMet ? settings->ccGoalMetColor : settings->ccTextColor);
                    float fraction = (e.goal > 0) ? (std::clamp)(static_cast<float>(count) / static_cast<float>(e.goal), 0.0f, 1.0f) : 0.0f;
                    char countBuf[32];
                    if (questDone)
                        snprintf(countBuf, sizeof(countBuf), "Complete");
                    else
                        snprintf(countBuf, sizeof(countBuf), "%d / %d", count, e.goal);

                    if (rowFlow) {
                        if (!firstVisible) curCardX += gap;
                        ImGui::SetCursorPos(ImVec2(curCardX, 0.0f));
                    } else {
                        if (!firstVisible) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap);
                    }
                    firstVisible = false;

                    float cardStartY = rowFlow ? 0.0f : ImGui::GetCursorPosY();

                    if (vertical) {
                        float contentW  = std::max(iconSz, barW);
                        float cardWidth = contentW + padX + padRight;

                        float labelH = 0.0f;
                        if (settings->ccShowLabel) {
                            ImVec2 wrappedSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, barW);
                            labelH = wrappedSz.y + itemGap;
                        }
                        float countRowH = settings->ccBarStyleEnabled ? barH : lineH;
                        float cardHeight = padY
                            + (e.icon ? iconSz + itemGap : 0.0f)
                            + labelH + countRowH + padBottom;

                        ImGui::Dummy(ImVec2(rowFlow ? curCardX + cardWidth : cardWidth, 0.0f));

                        ImVec2 cMin = ImVec2(winPos.x + curCardX, winPos.y + cardStartY);
                        ImVec2 cMax = ImVec2(cMin.x + cardWidth, cMin.y + cardHeight);
                        dl->AddRectFilled(cMin, cMax, bgU32, ScalePx(4.0f));

                        float curY    = cardStartY + padY;
                        float centerX = curCardX + (cardWidth - iconSz) * 0.5f;

                        if (e.icon) {
                            float screenIconX = winPos.x + centerX;
                            float screenIconY = winPos.y + curY;
                            IconRenderer::Style icon = iconTemplate;
                            icon.texture      = e.icon;
                            icon.size         = iconSz;
                            icon.borderRadius = iconSz * settings->ccIconBorderRadius * 0.01f;
                            icon.bounceOffsetY = ScalePx(UIAnimation::GetBounceOffsetY("cc_bounce_" + e.id));
                            ImGui::SetCursorScreenPos(ImVec2(screenIconX, screenIconY));
                            IconRenderer::Draw(dl, ImVec2(screenIconX, screenIconY), icon);
                            curY += iconSz + itemGap;
                        }

                        if (settings->ccShowLabel) {
                            float labelAreaX = curCardX + (cardWidth - barW) * 0.5f;
                            ImVec2 lblSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, barW);
                            ImVec2 labelPos = ImVec2(winPos.x + labelAreaX, winPos.y + curY);
                            ImU32 labelCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            HUDDraw::DrawTextStyledWrapped(dl, labelPos, barW, labelCol,
                                                           e.label.c_str(), HUDDraw::TextFx{}, true);
                            ImGui::SetCursorPos(ImVec2(labelAreaX, curY));
                            ImGui::Dummy(ImVec2(barW, lblSz.y));
                            curY += labelH;
                        }

                        if (settings->ccBarStyleEnabled) {
                            float barX = curCardX + (cardWidth - barW) * 0.5f;
                            ImGui::SetWindowFontScale(1.0f);
                            DrawBar(winPos.x + barX, winPos.y + curY, barW, fraction, countBuf);
                            ImGui::SetWindowFontScale(settings->ccLabelScale);
                            ImGui::Dummy(ImVec2(barW, barH));
                        } else {
                            float textW = ImGui::CalcTextSize(countBuf).x;
                            float textH = ImGui::GetTextLineHeight();
                            float countX = curCardX + (cardWidth - textW) * 0.5f;
                            ImVec2 countPos = ImVec2(winPos.x + countX, winPos.y + curY);
                            ImU32 countCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            HUDDraw::DrawTextStyled(dl, countPos, countCol, countBuf, HUDDraw::TextFx{});
                            ImGui::SetCursorPos(ImVec2(countX, curY));
                            ImGui::Dummy(ImVec2(textW, textH));
                        }

                        if (rowFlow) { curCardX += cardWidth; maxCardHeight = std::max(maxCardHeight, cardHeight); }
                        else ImGui::SetCursorPosY(cardStartY + cardHeight);

                    } else {
                        // ---- HORIZONTAL layout ----
                        float textX      = e.icon ? (padX + iconSz + ScalePx(8.0f)) : padX;
                        float hBarW      = barW;
                        float cardWidth  = textX + hBarW + padRight;

                        float labelH = 0.0f;
                        if (settings->ccShowLabel) {
                            ImVec2 wrappedSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, hBarW);
                            labelH = wrappedSz.y + itemGap;
                        }
                        float countRowH  = settings->ccBarStyleEnabled ? barH : lineH;
                        float textRows   = labelH + countRowH;
                        float cardHeight = std::max(iconSz, textRows) + padY + padBottom;

                        ImGui::Dummy(ImVec2(rowFlow ? curCardX + cardWidth : cardWidth, 0.0f));

                        ImVec2 cMin = ImVec2(winPos.x + curCardX, winPos.y + cardStartY);
                        ImVec2 cMax = ImVec2(cMin.x + cardWidth, cMin.y + cardHeight);
                        dl->AddRectFilled(cMin, cMax, bgU32, ScalePx(4.0f));

                        float rowStartY = cardStartY + padY;

                        if (e.icon) {
                            float screenIconX = winPos.x + curCardX + padX;
                            float screenIconY = winPos.y + rowStartY;
                            IconRenderer::Style icon = iconTemplate;
                            icon.texture      = e.icon;
                            icon.size         = iconSz;
                            icon.borderRadius = iconSz * settings->ccIconBorderRadius * 0.01f;
                            icon.bounceOffsetY = ScalePx(UIAnimation::GetBounceOffsetY("cc_bounce_" + e.id));
                            ImGui::SetCursorScreenPos(ImVec2(screenIconX, screenIconY));
                            IconRenderer::Draw(dl, ImVec2(screenIconX, screenIconY), icon);
                        }

                        float adjustedTextX = curCardX + textX;
                        float textCurY = rowStartY;
                        if (settings->ccShowLabel) {
                            float offsetY = e.icon ? (iconSz - textRows) * 0.5f : 0.0f;
                            textCurY = rowStartY + (offsetY > 0.0f ? offsetY : 0.0f);
                            ImVec2 lblSz = ImGui::CalcTextSize(e.label.c_str(), nullptr, false, hBarW);
                            ImVec2 labelPos = ImVec2(winPos.x + adjustedTextX, winPos.y + textCurY);
                            ImU32 labelCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            HUDDraw::DrawTextStyledWrapped(dl, labelPos, hBarW, labelCol,
                                                           e.label.c_str(), HUDDraw::TextFx{});
                            ImGui::SetCursorPos(ImVec2(adjustedTextX, textCurY));
                            ImGui::Dummy(ImVec2(lblSz.x, lblSz.y));
                            textCurY += labelH;
                        } else if (e.icon) {
                            float offsetY = (iconSz - countRowH) * 0.5f;
                            textCurY = rowStartY + (offsetY > 0.0f ? offsetY : 0.0f);
                        }

                        if (settings->ccBarStyleEnabled) {
                            ImGui::SetWindowFontScale(1.0f);
                            DrawBar(winPos.x + adjustedTextX, winPos.y + textCurY, hBarW, fraction, countBuf);
                            ImGui::SetWindowFontScale(settings->ccLabelScale);
                            ImGui::Dummy(ImVec2(hBarW, barH));
                        } else {
                            float textW = ImGui::CalcTextSize(countBuf).x;
                            float textH = ImGui::GetTextLineHeight();
                            ImVec2 countPos = ImVec2(winPos.x + adjustedTextX, winPos.y + textCurY);
                            ImU32 countCol = ImGui::ColorConvertFloat4ToU32(textCol);
                            HUDDraw::DrawTextStyled(dl, countPos, countCol, countBuf, HUDDraw::TextFx{});
                            ImGui::SetCursorPos(ImVec2(adjustedTextX, textCurY));
                            ImGui::Dummy(ImVec2(textW, textH));
                        }

                        if (rowFlow) { curCardX += cardWidth; maxCardHeight = std::max(maxCardHeight, cardHeight); }
                        else ImGui::SetCursorPosY(cardStartY + cardHeight);
                    }
                }

                if (rowFlow && maxCardHeight > 0.0f)
                    ImGui::SetCursorPosY(maxCardHeight);

                ImGui::SetWindowFontScale(1.0f);

                // Capture dragged position
                if (allowDrag) {
                    ImVec2 pos = ImGui::GetWindowPos();
                    settings->ccPosX = static_cast<int>(pos.x);
                    settings->ccPosY = static_cast<int>(pos.y);
                }
            }
            ImGui::End();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

    }

    void Init()
    {
        g_Collectibles.clear();
        g_CachedCounts.clear();
        g_ForceCollectiblesPos = true;
        g_Loaded               = false;

        const char* jsonPath = "Data/SKSE/Plugins/PartySheet_Widgets.json";
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            logger::warn("WidgetCollectibles: Could not open {}", jsonPath);
            return;
        }

        std::stringstream buf;
        buf << file.rdbuf();
        std::string content = buf.str();
        file.close();

        std::string collectiblesArray = WidgetJsonParser::ExtractArray(content, "collectibles");
        if (collectiblesArray.empty()) {
            logger::warn("WidgetCollectibles: No 'collectibles' array in JSON");
            return;
        }

        for (const auto& obj : WidgetJsonParser::SplitObjects(collectiblesArray)) {
            CollectibleEntry entry;
            entry.id      = WidgetJsonParser::ExtractValue(obj, "id");
            {
                std::string jsonLabel = WidgetJsonParser::ExtractValue(obj, "label");
                std::string locKey    = "Widgets." + entry.id;
                std::string locLabel  = LocalizationManager::Get(locKey);
                entry.label = (locLabel != locKey) ? locLabel : (!jsonLabel.empty() ? jsonLabel : entry.id);
            }
            entry.enabled = WidgetJsonParser::ParseBool(WidgetJsonParser::ExtractValue(obj, "enabled"), true);

            {
                std::wstring wkey(entry.id.begin(), entry.id.end());
                wkey += L"_enabled";
                const int ini = SettingsIni::ReadPresetIntOnce(L"Collectibles", wkey.c_str(), -1);
                if (ini != -1)
                    entry.enabled = (ini != 0);
            }

            std::string goalStr = WidgetJsonParser::ExtractValue(obj, "goal");
            try { entry.goal = std::stoi(goalStr); } catch (...) { entry.goal = 0; }

            if (entry.id.empty() || entry.label.empty() || entry.goal <= 0) {
                logger::warn("WidgetCollectibles: Skipping invalid entry (id='{}' label='{}' goal={})",
                    entry.id, entry.label, entry.goal);
                continue;
            }

            std::string itemsArray = WidgetJsonParser::ExtractArray(obj, "items");
            for (const auto& itemObj : WidgetJsonParser::SplitObjects(itemsArray)) {
                CollectibleItemRef ref;
                ref.mod         = WidgetJsonParser::ExtractValue(itemObj, "mod");
                ref.localFormID = WidgetJsonParser::ParseHexFormID(WidgetJsonParser::ExtractValue(itemObj, "formId"));
                if (!ref.mod.empty() && ref.localFormID != 0) {
                    ref.resolvedFormID = PlayerStatus::ResolveFormID(ref.mod, ref.localFormID);
                    if (ref.resolvedFormID == 0) {
                        auto* dh = RE::TESDataHandler::GetSingleton();
                        bool dataReady = dh && dh->GetLoadedModCount() > 0;
                        if (dataReady)
                            logger::warn("WidgetCollectibles: Could not resolve item formId {:X} in '{}'",
                                ref.localFormID, ref.mod);
                        else
                            logger::trace("WidgetCollectibles: Deferred item formId {:X} in '{}' (pre-load)",
                                ref.localFormID, ref.mod);
                    }
                    entry.items.push_back(ref);
                }
            }

            {
                std::string questKey = "\"quest\"";
                size_t qPos = obj.find(questKey);
                if (qPos != std::string::npos) {
                    size_t qObjStart = obj.find('{', qPos + questKey.size());
                    if (qObjStart != std::string::npos) {
                        size_t qObjEnd = obj.find('}', qObjStart);
                        if (qObjEnd != std::string::npos) {
                            std::string questObj = obj.substr(qObjStart, qObjEnd - qObjStart + 1);
                            entry.questMod     = WidgetJsonParser::ExtractValue(questObj, "mod");
                            entry.questLocalID = WidgetJsonParser::ParseHexFormID(WidgetJsonParser::ExtractValue(questObj, "formId"));
                            if (!entry.questMod.empty() && entry.questLocalID != 0) {
                                entry.questResolvedID = PlayerStatus::ResolveFormID(
                                    entry.questMod, entry.questLocalID);
                                if (entry.questResolvedID == 0) {
                                    auto* dh = RE::TESDataHandler::GetSingleton();
                                    bool dataReady = dh && dh->GetLoadedModCount() > 0;
                                    if (dataReady)
                                        logger::warn("WidgetCollectibles: Could not resolve quest {:X} in '{}'",
                                            entry.questLocalID, entry.questMod);
                                    else
                                        logger::trace("WidgetCollectibles: Deferred quest {:X} in '{}' (pre-load)",
                                            entry.questLocalID, entry.questMod);
                                }
                            }
                        }
                    }
                }
            }

            std::string iconPath = "Data\\Interface\\PartySheet\\Widgets\\" + entry.id + ".png";
            entry.icon = TextureManager::GetTexture(iconPath);

            g_Collectibles.push_back(std::move(entry));
        }

        g_CachedCounts.resize(g_Collectibles.size(), 0);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            for (size_t i = 0; i < g_Collectibles.size(); ++i)
                g_CachedCounts[i] = GetItemCount(g_Collectibles[i]);
        }
        g_Loaded = true;
    }

    void RenderPreview()
    {
        RenderCollectiblesWindow(true);
    }

    void RenderHUD()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings || !settings->enableCollectiblesWidget) return;
        if (MenuUtils::ShouldBlockOverlay()) return;
        if (settings->ccHideInCombat) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && player->IsInCombat()) return;
        }
        if (!g_Loaded || g_Collectibles.empty()) return;
        if (!HUDVisibility::IsVisible(HUDVisibility::WidgetID::Collectibles)) return;

        bool anyVisible = false;
        for (size_t i = 0; i < g_Collectibles.size(); ++i) {
            if (!g_Collectibles[i].enabled) continue;
            if (IsQuestComplete(g_Collectibles[i])) continue;
            anyVisible = true;
            break;
        }
        if (!anyVisible) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, HUDVisibility::GetAlpha(HUDVisibility::WidgetID::Collectibles));
        RenderCollectiblesWindow(false);
        ImGui::PopStyleVar();

        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(HUDVisibility::GetAlpha(HUDVisibility::WidgetID::Collectibles));
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

}
