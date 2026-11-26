
#include "../../includes/ImGui/PlayerHUDSupplies.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/Utils/HudItem.h"
#include "../../includes/Utils/SurvivalState.h"
#include "../../includes/Utils/SurvivalSupplies.h"

#include <RE/Skyrim.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace PlayerHUDSupplies {

    namespace {

        bool g_ForcePos = false;

        constexpr float kMessageIn = 0.25f;
        constexpr float kMessageHold = 2.0f;
        constexpr float kMessageOut = 0.35f;

        constexpr int kBadNeedStage = 2;

        std::string Loc(const char* key, const char* english)
        {
            std::string text = LocalizationManager::Get(key);
            return (text.empty() || text == key) ? std::string(english) : text;
        }

        ImTextureID PartyIcon(const char* name)
        {
            return reinterpret_cast<ImTextureID>(TextureManager::GetPartyIcon(name));
        }

        ImTextureID WidgetIcon(const char* name)
        {
            return reinterpret_cast<ImTextureID>(
                TextureManager::GetTexture(std::string("Data\\Interface\\PartySheet\\Widgets\\") + name + ".png"));
        }

        float Scale()
        {
            const auto* settings = Settings::GetSingleton();
            const float survival = std::clamp(static_cast<float>(settings->phSurvivalScale) / 100.0f, 0.5f, 2.0f);
            const float supplies = std::clamp(static_cast<float>(settings->phSuppliesScale) / 100.0f, 0.5f, 2.0f);
            return survival * supplies;
        }

        struct Row {
            const char*  key = "";
            ImTextureID  icon = 0;
            int          count = 0;
            HudItem::Status status = HudItem::Status::Normal;
            std::string  name;
            std::string  message;
            float        messageProgress = 0.0f;
            float        alpha = 1.0f;
            std::uint32_t color = 0;
        };

        int NeedStage(const SurvivalState::Snapshot& snapshot, const char* key)
        {
            for (const auto& need : snapshot.needs)
                if (!std::strcmp(need.status.key, key)) return need.status.stage;
            return -1;
        }

        bool PluginLoaded(const char* plugin)
        {
            auto* data = RE::TESDataHandler::GetSingleton();
            return data && data->LookupModByName(plugin);
        }

        float EmptyAlpha()
        {
            return std::clamp(Settings::GetSingleton()->phSuppliesEmptyOpacity, 0, 100) / 100.0f;
        }

        std::vector<Row> BuildRows(const SurvivalState::Snapshot& snapshot)
        {
            const float emptyAlpha = EmptyAlpha();
            const SurvivalSupplies::Totals totals = SurvivalSupplies::Count();
            std::vector<Row> rows;

            auto addRow = [&](const char* key, ImTextureID icon, int count, std::string name,
                              HudItem::Status status, const char* needKey, std::uint32_t color) {
                Row row{ key, icon, count, status };
                row.color = color;
                row.name = std::move(name);
                if (count == 0) {
                    const bool needIsBad = needKey && NeedStage(snapshot, needKey) >= kBadNeedStage;
                    if (needIsBad) {
                        row.status = HudItem::Status::Bad;
                    } else {
                        if (emptyAlpha <= 0.0f) return;
                        row.status = HudItem::Status::Normal;
                        row.alpha = emptyAlpha;
                    }
                }
                rows.push_back(std::move(row));
            };

            const auto* settings = Settings::GetSingleton();
            addRow("food", PartyIcon("source_food"), totals.food, Loc("Supplies.Food", "Food"), HudItem::Status::Normal,
                   "hunger", settings->phSuppliesFoodColor);
            if (NeedStage(snapshot, "thirst") >= 0)
                addRow("drinks", WidgetIcon("survival_drink"), totals.drinks, Loc("Supplies.Drinks", "Drinks"),
                       HudItem::Status::Normal, "thirst", settings->phSuppliesDrinksColor);
            if (PluginLoaded("SunHelmSurvival.esp"))
                addRow("waterskin", WidgetIcon("survival_thirst"), totals.waterskinSips, Loc("Supplies.Waterskin", "Waterskin"),
                       HudItem::Status::Normal, "thirst", settings->phSuppliesWaterskinColor);
            if (PluginLoaded("LastSeed.esp") || PluginLoaded("RealisticNeedsandDiseases.esp"))
                addRow("dirtywater", WidgetIcon("survival_dirtywater"), totals.dirtyWater, Loc("Supplies.DirtyWater", "Dirty water"),
                       HudItem::Status::Normal, nullptr, settings->phSuppliesDirtyWaterColor);
            addRow("firewood", PartyIcon("fire"), totals.firewood, Loc("Supplies.Firewood", "Firewood"), HudItem::Status::Normal,
                   nullptr, settings->phSuppliesFirewoodColor);
            if (PluginLoaded("Campfire.esm") || PluginLoaded("RealisticNeedsandDiseases.esp"))
                addRow("tents", WidgetIcon("survival_tent"), totals.tents, Loc("Supplies.Tents", "Tents"), HudItem::Status::Normal,
                       nullptr, settings->phSuppliesTentsColor);
            addRow("torches", PartyIcon("light"), totals.torches, Loc("Supplies.Torches", "Torches"), HudItem::Status::Normal,
                   nullptr, settings->phSuppliesTorchesColor);
            return rows;
        }

        std::vector<Row> SampleRows()
        {
            const float emptyAlpha = EmptyAlpha();
            std::vector<Row> rows;
            const auto* settings = Settings::GetSingleton();
            Row food{ "food", PartyIcon("source_food"), 6 };
            food.name = Loc("Supplies.Food", "Food");
            food.color = settings->phSuppliesFoodColor;
            Row water{ "dirtywater", WidgetIcon("survival_dirtywater"), 2 };
            water.name = Loc("Supplies.DirtyWater", "Dirty water");
            water.color = settings->phSuppliesDirtyWaterColor;
            Row wood{ "firewood", PartyIcon("fire"), 0 };
            wood.name = Loc("Supplies.Firewood", "Firewood");
            wood.color = settings->phSuppliesFirewoodColor;
            wood.alpha = emptyAlpha;
            rows.push_back(food);
            rows.push_back(water);
            if (emptyAlpha > 0.0f) rows.push_back(wood);
            return rows;
        }

        void TickMessage(Row& row, bool allowMessages)
        {
            using Clock = std::chrono::steady_clock;
            static std::unordered_map<std::string, Clock::time_point> changedAt;
            static std::unordered_map<std::string, int> lastDelta;

            const std::string watchKey = std::string("supplies.") + row.key;
            const UIAnimation::WatchResult change = UIAnimation::Watch(watchKey, static_cast<float>(row.count), 0.5f, 0.5f);
            if (change.changed) {
                changedAt[row.key] = Clock::now();
                lastDelta[row.key] = static_cast<int>(std::lround(change.delta));
            }
            if (!allowMessages) return;

            auto it = changedAt.find(row.key);
            if (it == changedAt.end()) return;
            const float age = std::chrono::duration<float>(Clock::now() - it->second).count();
            row.messageProgress = UIAnimation::AgeFade(age, kMessageIn, kMessageHold, kMessageOut);
            if (row.messageProgress <= 0.0f) return;

            char text[96];
            snprintf(text, sizeof(text), "%+d %s", lastDelta[row.key], row.name.c_str());
            row.message = text;
        }

        bool UsesPennant() { return Settings::GetSingleton()->phSuppliesStyle == 1; }

        HUDDraw::PennantDir ClothDirection()
        {
            const auto* settings = Settings::GetSingleton();
            if (settings->phSuppliesClothDir > 0)
                return static_cast<HUDDraw::PennantDir>(settings->phSuppliesClothDir - 1);
            return HudItem::AutoClothDirection(static_cast<HUDFlow::Corner>(settings->phSuppliesAnchor),
                                               static_cast<HUDFlow::Axis>(settings->phSuppliesLayout));
        }

        HudItem::Style BuildStyle()
        {
            const auto* settings = Settings::GetSingleton();
            const float scale = Scale();
            HudItem::Style style;
            style.font = ImGui::GetFont();
            style.fontSize = style.font->LegacySize * settings->phSurvivalLabelScale * scale;
            style.discSize = 46.0f * scale;
            style.iconSize = 30.0f * scale;
            style.discBorderWidth = settings->phSuppliesBorderWidth > 0
                ? (std::max)(1.0f, static_cast<float>(settings->phSuppliesBorderWidth) * scale) : 0.0f;
            style.discRounding = static_cast<float>(settings->phSuppliesCornerRadius) / 100.0f;
            style.discColor = settings->phSuppliesBgColor;
            style.discBorderColor = settings->phSuppliesBorderColor;
            style.stripColor = settings->phSuppliesStripColor;
            style.padX = 10.0f * scale;
            style.padY = 3.0f * scale;
            style.gap = 5.0f * scale;
            if (static_cast<HUDFlow::Axis>(settings->phSuppliesLayout) == HUDFlow::Axis::Vertical)
                style.stripMinWidth = 170.0f * scale;

            style.messageAtEnd = settings->phSuppliesPopup != 0;
            if (UsesPennant()) {
                style.pennant = true;
                style.clothDirection = ClothDirection();
                style.discSize = 40.0f * scale;
                style.clothWidth = 26.0f * scale;
                style.clothLength = 50.0f * scale;
                style.iconSize = 18.0f * scale;
            }

            constexpr std::uint32_t kNameColor = 0xFFF0F0F0;
            constexpr std::uint32_t kCountPlateTint = 0xE7444341;
            style.normalColor = kNameColor;
            style.countBadge = HUDDraw::BadgePresetStyle(HUDDraw::BadgePreset_Plain);
            HUDDraw::TintBadge(style.countBadge, kCountPlateTint);
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

        void RenderWindow(bool allowDrag)
        {
            auto* settings = Settings::GetSingleton();
            if (!settings->enablePlayerHud || !settings->phSuppliesEnable) return;

            const SurvivalState::Snapshot& snapshot = SurvivalState::Get();
            std::vector<Row> rows;
            if (snapshot.available)
                rows = BuildRows(snapshot);
            else if (allowDrag)
                rows = SampleRows();
            for (Row& row : rows)
                TickMessage(row, settings->phSuppliesShowMessages && !allowDrag);
            if (rows.empty()) return;

            std::vector<HudItem::Content> items;
            items.reserve(rows.size());
            for (const Row& row : rows) {
                HudItem::Content item;
                item.icon = row.icon;
                item.label = settings->phSuppliesShowNames ? row.name.c_str() : "";
                item.alpha = row.alpha;
                item.color = row.color;
                item.count = row.count;
                item.status = row.status;
                item.message = row.message.c_str();
                item.messageProgress = row.messageProgress;
                items.push_back(item);
            }

            HUDFlow::Options flow;
            flow.corner = static_cast<HUDFlow::Corner>(settings->phSuppliesAnchor);
            flow.axis = static_cast<HUDFlow::Axis>(settings->phSuppliesLayout);
            flow.maxItems = settings->phSuppliesMaxItems;
            flow.wrapCount = settings->phSuppliesWrapAfter;
            flow.gapX = flow.gapY = static_cast<float>(settings->phSurvivalCardSpacing) * Scale();

            HudItem::Style style = BuildStyle();

            std::vector<ImVec2> sizes;
            for (const auto& item : items) sizes.push_back(HudItem::Measure(item, style));
            const ImVec2 contentSize = HUDFlow::Arrange(sizes, flow).size;
            const ImVec2 anchor(static_cast<float>(settings->phSuppliesPosX), static_cast<float>(settings->phSuppliesPosY));

            static ImVec2 lastSize(-1.0f, -1.0f);
            if (contentSize.x != lastSize.x || contentSize.y != lastSize.y) {
                lastSize = contentSize;
                g_ForcePos = true;
            }
            const ImVec2 targetPos = HUDFlow::TopLeftFromAnchor(anchor, contentSize, flow.corner);
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
            ImGui::Begin("##playerHUDsupplies", nullptr, flags);

            const ImVec2 windowPos = ImGui::GetWindowPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);
            HudItem::DrawList(dl, HUDFlow::AnchorFromTopLeft(windowPos, contentSize, flow.corner), items, style, flow);
            dl->PopClipRect();
            ImGui::Dummy(contentSize);

            if (allowDrag && !forcedThisFrame) {
                if (std::fabs(windowPos.x - targetPos.x) > 1.5f || std::fabs(windowPos.y - targetPos.y) > 1.5f) {
                    const ImVec2 newAnchor = HUDFlow::AnchorFromTopLeft(windowPos, contentSize, flow.corner);
                    settings->phSuppliesPosX = static_cast<int>(std::lround(newAnchor.x));
                    settings->phSuppliesPosY = static_cast<int>(std::lround(newAnchor.y));
                }
            }

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }

    void ForceReposition() { g_ForcePos = true; }

    void RenderHUD()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->phSuppliesEnable) return;
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
