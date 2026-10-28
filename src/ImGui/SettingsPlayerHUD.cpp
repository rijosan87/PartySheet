#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Settings.h"
#include "../../includes/ImGui/PlayerHUD.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/ImGui/PlayerHUDEquipment.h"
#include "../../includes/ImGui/PlayerHUDResistances.h"
#include "../../includes/ImGui/PlayerHUDResources.h"
#include "../../includes/ImGui/PlayerHUDEffects.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/ImGui/WidgetPinnedItems.h"
#include "../../includes/ImGui/WidgetCollectibles.h"
#include "../../includes/ImGui/WidgetStableMount.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/PNGIIBridge.h"
#include "../../includes/PlayerHUDResistancesPresets.h"
#include "../../includes/PlayerHUDSurvivalPresets.h"
#include "../../includes/PlayerHUDDiseasePresets.h"
#include "../../includes/PlayerHUDEquipmentPresets.h"
#include "../../includes/PlayerHUDBarPresets.h"

#include <imgui.h>
#include <cfloat>

namespace ImGuiRenderer {
    void RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type);
}

namespace ImGuiRenderer::SettingsUI {

    static void DrawPositionSlider(const char* id, std::int32_t& value, int minimum, int maximum, const char* format) {
        const float buttonSize = ImGui::GetFrameHeight();
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth((std::max)(1.0f, ImGui::GetContentRegionAvail().x - 2.0f * (buttonSize + gap)));
        ImGui::SliderInt(id, &value, minimum, maximum, format);
        ImGui::PushID(id);
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(buttonSize, buttonSize)))
            value = std::clamp(value > minimum ? value - 1 : minimum, minimum, maximum);
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(buttonSize, buttonSize)))
            value = std::clamp(value < maximum ? value + 1 : maximum, minimum, maximum);
        ImGui::PopID();
    }
    static bool BeginAppearanceGroup(const char* id, const char* label) {
        ImGui::Spacing();
        const float avail = ImGui::GetContentRegionAvail().x;
        const float textW = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - textW) * 0.5f);
        ImGui::TextDisabled("%s", label);
        ImGui::Spacing();
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
            return false;
        ImGui::TableSetupColumn("##c", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("##s", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        return true;
    }
    static void NextAppearanceStyleCol() { ImGui::TableSetColumnIndex(1); }
    static void EndAppearanceGroup() {
        ImGui::EndTable();
        ImGui::Spacing();
        ImGui::Separator();
    }

    [[maybe_unused]] static void PushInnerHeader() {
        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(28, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(38, 58, 86, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(50, 76, 110, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    }

    [[maybe_unused]] static void PopInnerHeader() {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    static const char* kPlayerHUDLabels[] = {
        "Player Stats", "Equipment", "Resistances",
        "Resources",    "Survival",  "Disease",
        "Active Effects",
    };

    static void DrawPlayerHUDSectionHeading(int sel) {
        if (sel < 0 || sel >= IM_ARRAYSIZE(kPlayerHUDLabels)) return;

        ImDrawList* dl  = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float  w  = ImGui::GetContentRegionAvail().x;
        const float  padX = 8.0f, padY = 5.0f;
        const float  th = ImGui::GetTextLineHeight() + padY * 2.0f;

        dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + th), IM_COL32(40, 60, 88, 255), 3.0f);
        dl->AddRectFilled(p0, ImVec2(p0.x + 3.0f, p0.y + th), IM_COL32(96, 152, 210, 255), 3.0f);
        dl->AddText(ImVec2(p0.x + padX, p0.y + padY), IM_COL32(220, 232, 248, 255), kPlayerHUDLabels[sel]);

        ImGui::Dummy(ImVec2(w, th));
        ImGui::Spacing();
    }

    static int DrawPlayerHUDGrid(int& sel) {
        const char** kLabels = kPlayerHUDLabels;
        constexpr int kCount = IM_ARRAYSIZE(kPlayerHUDLabels);
        constexpr int kPerRow  = 3;

        const float avail   = ImGui::GetContentRegionAvail().x;
        const float gap     = 6.0f;
        const float cardW   = (avail - gap * (kPerRow - 1)) / kPerRow;
        const float cardH   = 34.0f;

        // Grid labels also occur in other HUD sections; keep their IDs local to the picker.
        ImGui::PushID("PlayerHUDGrid");
        for (int i = 0; i < kCount; ++i) {
            if (i % kPerRow != 0) ImGui::SameLine(0.0f, gap);

            const bool active = (sel == i);
            ImGui::PushStyleColor(ImGuiCol_Button,        active ? IM_COL32(50, 76, 110, 255) : IM_COL32(24, 36, 56, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? IM_COL32(58, 88, 124, 255) : IM_COL32(34, 50, 74, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(64, 96, 134, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

            if (ImGui::Button(kLabels[i], ImVec2(cardW, cardH)))
                sel = active ? -1 : i;

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::PopID();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        return sel;
    }

    void RenderPlayerHUDSection(Settings* settings, const ImVec2& displaySize) {
        if (!settings) return;

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 28, 46, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 58, 84, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        bool open = ImGui::CollapsingHeader("Player HUD");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (!open) return;

        ImGui::Indent(10);
        ImGui::Spacing();

        if (ImGui::Checkbox("Enable Player HUD", &settings->enablePlayerHud))
            settings->Save();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        static int s_phSel = -1;
        DrawPlayerHUDGrid(s_phSel);
        DrawPlayerHUDSectionHeading(s_phSel);

        bool statusOpen = (s_phSel == 0);

        if (statusOpen) {
            ImGui::Indent(10);
            ImGui::Spacing();

            if (ImGui::Checkbox("Enable Player Status##phStatusEnable", &settings->enablePlayerStatusWidget))
                settings->Save();

            ImGui::Spacing();
            ImGui::TextDisabled("Presets");
            ImGui::Spacing();
            {
                static int presetIndex = 0;
                static constexpr const char* presetNames[] = {
                    "1 - Animated Portrait", "2 - Custom Bars", "3 - Scaled Bars", "4 - Level Emblem", "5 - No Portrait", "6 - Halo", "7 - Curved Status"
                };
                // Combo reads every advertised label when opened; keep both arrays in sync.
                static_assert(IM_ARRAYSIZE(presetNames) == PlayerHUDBarPresets::kPresets.size());
                static bool loggedPresetCounts = false;
                if (!loggedPresetCounts) {
                    logger::debug("[PlayerHUDPreset] labels={}, presets={}", IM_ARRAYSIZE(presetNames), PlayerHUDBarPresets::kPresets.size());
                    loggedPresetCounts = true;
                }
                ImGui::SetNextItemWidth(300.0f);
                if (ImGui::Combo("##phStatusPreset", &presetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
                    PlayerHUDBarPresets::ApplyPreset(*settings, static_cast<std::size_t>(presetIndex));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static constexpr ImGuiColorEditFlags kPHArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;

            if (ImGui::CollapsingHeader("General##PHGeneral")) {
                const char* modeItems[] = { "Always", "Combat Only" };
                ImGui::TextDisabled("Layout / Behavior");
                ImGui::Spacing();

                if (settings->playerHudMode < 1) settings->playerHudMode = 1;
                int modeIndex = settings->playerHudMode - 1;
                if (ImGui::Combo("Mode##phMode", &modeIndex, modeItems, 2))
                    settings->playerHudMode = modeIndex + 1;

                if (settings->playerHudMode == 2) {
                    ImGui::SliderInt("Fade Delay (s)##phFade", &settings->phFadeDelay, 0, 10);
                }

                if (ImGui::SliderInt("Pos X##phPosX", &settings->phPosX, 0, static_cast<int>(displaySize.x)))
                    PlayerHUD::ForceReposition();
                if (ImGui::SliderInt("Pos Y##phPosY", &settings->phPosY, 0, static_cast<int>(displaySize.y)))
                    PlayerHUD::ForceReposition();

                ImGui::Checkbox("Dynamic Bars##phDynamic", &settings->phDynamicBars);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("HUD Visibility Trigger.\nFades HP/MP/SP bars when full + idle and you are not battle-ready.");

                if (settings->phDynamicBars) {
                    ImGui::Indent(20);
                    ImGui::Checkbox("Keep Portrait Visible##phPortAlways", &settings->phPortraitAlwaysVisible);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Portrait stays at full opacity when the bars fade out,\nkeeping it as a fixed anchor instead of vanishing with them.");
                    ImGui::Unindent(20);
                }

                ImGui::Spacing();
                if (ImGui::BeginTable("##PHItemsTable", 4, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show/Hide");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Portrait##phShowPort", &settings->phShowPortrait);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Name##phName", &settings->phShowName);
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("Health##phHP", &settings->phShowHealth);
                    ImGui::TableSetColumnIndex(3); ImGui::Checkbox("Magicka##phMP", &settings->phShowMagicka);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Stamina##phSP", &settings->phShowStamina);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("XP Bar##phXP", &settings->phShowXP);
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("XP as Bar##phXPAsBar", &settings->phXPAsBar);
                    ImGui::TableSetColumnIndex(3); ImGui::Checkbox("Bar Text##phBarText", &settings->phShowBarText);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Lvl Badge##phLvlBadge", &settings->phLevelBadgeOnPortrait);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Regen Pip##phRegenPip", &settings->phShowRegenPip);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show regen rate (dot + +X.X/s) inside the left edge of each bar");
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("Stat Bonus##phShowFortify", &settings->phShowFortify);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show net stat modifier outside the left of each bar: green +N for buffs, red -N for penalties");
                    ImGui::TableSetColumnIndex(3); ImGui::Checkbox("Bar Icons##phBarIcons", &settings->phShowBarIcons);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show a health/magicka/stamina icon left of each bar.\nOnly appears when the Portrait is hidden (no room otherwise).");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Halo##phHalo", &settings->phHaloEnable);

                    ImGui::EndTable();
                }
                if (settings->phHaloEnable) {
                    ImGui::Indent(20);
                    ImGui::TextDisabled("Halo");
                    ImGui::SliderInt("Health Share##phHaloHealthShare", &settings->phHaloHealthShare, 50, 90, "%d%%");
                    ImGui::SliderInt("Thickness##phHaloThickness", &settings->phHaloThickness, 0, 12,
                                     settings->phHaloThickness == 0 ? "Auto" : "%d px");
                    ImGui::SliderInt("Segment Gap##phHaloGap", &settings->phHaloGap, 0, 10, "%d deg");
                    ImGui::SliderInt("Ring Offset##phHaloOffset", &settings->phHaloOffset, 0, 12, "%d px");
                    ImGui::Unindent(20);
                }
            }

            if (ImGui::CollapsingHeader("Bars##PHBars")) {
                const bool xpAsBar = settings->phShowXP && (!settings->phShowPortrait || settings->phXPAsBar);
                const bool showAnnotations = settings->phShowBarText || settings->phShowRegenPip || settings->phShowFortify;
                bool overrideStyle = settings->phHealthBarStyleIndex > 0 ||
                                     settings->phMagickaBarStyleIndex > 0 ||
                                     settings->phStaminaBarStyleIndex > 0;

                ImGui::TextDisabled("Shared");
                ImGui::Spacing();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phBarW", &settings->phBarWidth, 60, 600, "Bar Width %d");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phBarSpacing", &settings->phBarSpacing, -12, 20, "Bar Spacing %d px");
                const float rowWidth = ImGui::GetContentRegionAvail().x;
                const float controlPadding = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
                const float scaleWidth = controlPadding + ImGui::CalcTextSize("Scale Bar Width to Stat").x;
                const float ghostWidth = controlPadding + ImGui::CalcTextSize("Damage Ghost").x;
                const float flashWidth = controlPadding + ImGui::CalcTextSize("Empty Flash").x;
                const float minGap = ImGui::GetStyle().ItemSpacing.x;
                const bool allOnRow = scaleWidth + ghostWidth + flashWidth + minGap * 2.0f <= rowWidth;
                const bool ghostOnFirstRow = allOnRow || scaleWidth + ghostWidth + minGap <= rowWidth;
                const float sharedGap = allOnRow ? (rowWidth - scaleWidth - ghostWidth - flashWidth) * 0.5f : minGap;
                ImGui::Checkbox("Scale Bar Width to Stat##phScaledW", &settings->phScaledBarWidth);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Bar width scales with each stat's max value.\nThe largest stat gets the full Bar Width; others shrink proportionally.\nMinimum width is 170px, capped by Bar Width, so text and regen pip stay readable.");
                if (ghostOnFirstRow)
                    ImGui::SameLine(0.0f, allOnRow ? sharedGap : rowWidth - scaleWidth - ghostWidth);
                ImGui::Checkbox("Damage Ghost##phGhost", &settings->phDamageGhost);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("When you take damage, a pale segment lingers at the pre-hit HP value,\nthen drains down to the new value (vanilla-style hit feedback).");

                if (allOnRow)
                    ImGui::SameLine(0.0f, sharedGap);
                else if (!ghostOnFirstRow && ghostWidth + flashWidth + minGap <= rowWidth)
                    ImGui::SameLine(0.0f, rowWidth - ghostWidth - flashWidth);
                {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phBarPulseColor);
                    if (ImGui::ColorEdit4("Empty Flash##phBarPulseCol", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phBarPulseColor = ColorUtils::Vec4ToArgb(c);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Warning flash color when Magicka/Stamina hits empty.\nAlpha = flash strength, 0 = disable the flash.");
                }

                ImGui::Checkbox("Custom Bar Style##phBarStyleEnable", &settings->phBarStyleEnabled);
                if (settings->phBarStyleEnabled) {
                    ImGui::SameLine();
                    ImGuiRenderer::RenderBackgroundPicker("PHBarStylePicker", settings->phBarStyleIndex, BackgroundManager::Type::BarStyle);
                    ImGui::Indent(20);
                    if (ImGui::Checkbox("Override##phIndividualBarStyles", &overrideStyle)) {
                        const int selection = overrideStyle ? std::max(1, settings->phBarStyleIndex) : 0;
                        settings->phHealthBarStyleIndex = selection;
                        settings->phMagickaBarStyleIndex = selection;
                        settings->phStaminaBarStyleIndex = selection;
                    }
                    ImGui::Unindent(20);
                }

                auto DrawIndividualWidth = [&](const char* label, const char* id, std::int32_t& width) {
                    ImGui::BeginDisabled(settings->phScaledBarWidth);
                    // Reserve the leftmost position for inheritance; actual widths start at 100 px.
                    int displayWidth = width > 0 ? std::clamp(width, 100, 600) : 99;
                    char format[64];
                    snprintf(format, sizeof(format), width == 0 ? "%s Shared" : "%s %%d px", label);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt(id, &displayWidth, 99, 600, format))
                        width = displayWidth <= 99 ? 0 : std::clamp(displayWidth, 100, 600);
                    ImGui::EndDisabled();
                };
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Health");
                ImGui::Spacing();
                if (settings->phBarStyleEnabled && overrideStyle) {
                    int selection = settings->phHealthBarStyleIndex > 0 ? settings->phHealthBarStyleIndex : std::max(1, settings->phBarStyleIndex);
                    const int previous = selection;
                    ImGuiRenderer::RenderBackgroundPicker("PHHealthBarStylePicker", selection, BackgroundManager::Type::BarStyle);
                    if (selection != previous) settings->phHealthBarStyleIndex = selection;
                }
                DrawIndividualWidth("Health Width", "##phHealthBarWidth", settings->phHealthBarWidth);
                {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phHealthColor);
                    ImGui::SetNextItemWidth(-40.0f);
                    ImGui::SliderInt("##phBarH", &settings->phBarHeight, 4, 600, "Health Bar Height %d");
                    ImGui::SameLine();
                    if (ImGui::ColorEdit4("##phHCol", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phHealthColor = ColorUtils::Vec4ToArgb(c);
                }
                DrawPositionSlider("##phHpOffX", settings->phHpOffsetX, -3840, 3840, "Bar X %d px");
                DrawPositionSlider("##phHpOffY", settings->phHpOffsetY, -2160, 2160, "Bar Y %d px");
                if (showAnnotations) {
                    DrawPositionSlider("##phHpAnnotationOffsetX", settings->phHpAnnotationOffsetX, -3840, 3840, "Annotation X %d px");
                    DrawPositionSlider("##phHpAnnotationOffsetY", settings->phHpAnnotationOffsetY, -2160, 2160, "Annotation Y %d px");
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Magicka");
                ImGui::Spacing();
                if (settings->phBarStyleEnabled && overrideStyle) {
                    int selection = settings->phMagickaBarStyleIndex > 0 ? settings->phMagickaBarStyleIndex : std::max(1, settings->phBarStyleIndex);
                    const int previous = selection;
                    ImGuiRenderer::RenderBackgroundPicker("PHMagickaBarStylePicker", selection, BackgroundManager::Type::BarStyle);
                    if (selection != previous) settings->phMagickaBarStyleIndex = selection;
                }
                DrawIndividualWidth("Magicka Width", "##phMagickaBarWidth", settings->phMagickaBarWidth);
                {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phMagickaColor);
                    ImGui::SetNextItemWidth(-40.0f);
                    ImGui::SliderInt("##phBarHMp", &settings->phMagickaBarHeight, 4, 600, "Magicka Bar Height %d");
                    ImGui::SameLine();
                    if (ImGui::ColorEdit4("##phMCol", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phMagickaColor = ColorUtils::Vec4ToArgb(c);
                }
                DrawPositionSlider("##phMpOffX", settings->phMpOffsetX, -3840, 3840, "Bar X %d px");
                DrawPositionSlider("##phMpOffY", settings->phMpOffsetY, -2160, 2160, "Bar Y %d px");
                if (showAnnotations) {
                    DrawPositionSlider("##phMpAnnotationOffsetX", settings->phMpAnnotationOffsetX, -3840, 3840, "Annotation X %d px");
                    DrawPositionSlider("##phMpAnnotationOffsetY", settings->phMpAnnotationOffsetY, -2160, 2160, "Annotation Y %d px");
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Stamina");
                ImGui::Spacing();
                if (settings->phBarStyleEnabled && overrideStyle) {
                    int selection = settings->phStaminaBarStyleIndex > 0 ? settings->phStaminaBarStyleIndex : std::max(1, settings->phBarStyleIndex);
                    const int previous = selection;
                    ImGuiRenderer::RenderBackgroundPicker("PHStaminaBarStylePicker", selection, BackgroundManager::Type::BarStyle);
                    if (selection != previous) settings->phStaminaBarStyleIndex = selection;
                }
                DrawIndividualWidth("Stamina Width", "##phStaminaBarWidth", settings->phStaminaBarWidth);
                {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phStaminaColor);
                    ImGui::SetNextItemWidth(-40.0f);
                    ImGui::SliderInt("##phBarHSp", &settings->phStaminaBarHeight, 4, 600, "Stamina Bar Height %d");
                    ImGui::SameLine();
                    if (ImGui::ColorEdit4("##phSCol", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phStaminaColor = ColorUtils::Vec4ToArgb(c);
                }
                DrawPositionSlider("##phSpOffX", settings->phSpOffsetX, -3840, 3840, "Bar X %d px");
                DrawPositionSlider("##phSpOffY", settings->phSpOffsetY, -2160, 2160, "Bar Y %d px");
                if (showAnnotations) {
                    DrawPositionSlider("##phSpAnnotationOffsetX", settings->phSpAnnotationOffsetX, -3840, 3840, "Annotation X %d px");
                    DrawPositionSlider("##phSpAnnotationOffsetY", settings->phSpAnnotationOffsetY, -2160, 2160, "Annotation Y %d px");
                }
                if (settings->phShowXP) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("XP");
                    ImGui::Spacing();
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phXPColor);
                    if (xpAsBar) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##phXPBarWidth", &settings->phXPBarWidth, 0, 1000, "XP Bar Width %d px");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = match the stat bar width");
                        ImGui::SetNextItemWidth(-40.0f);
                        ImGui::SliderInt("##phBarHXP", &settings->phXPBarHeight, 4, 50, "XP Bar Height %d");
                        ImGui::SameLine();
                    }
                    if (ImGui::ColorEdit4("##phXCol", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phXPColor = ColorUtils::Vec4ToArgb(c);
                    if (xpAsBar) {
                        DrawPositionSlider("##phXPBarOffX", settings->phXPBarOffsetX, -3840, 3840, "XP Bar X %d px");
                        DrawPositionSlider("##phXPBarOffY", settings->phXPBarOffsetY, -2160, 2160, "XP Bar Y %d px");
                        DrawPositionSlider("##phXPAnnotationOffsetX", settings->phXPAnnotationOffsetX, -3840, 3840, "XP Annotation X %d px");
                        DrawPositionSlider("##phXPAnnotationOffsetY", settings->phXPAnnotationOffsetY, -2160, 2160, "XP Annotation Y %d px");
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (settings->phShowBarText || (settings->phShowBarIcons && !settings->phShowPortrait)) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        char offLabel[48];
                        snprintf(offLabel, sizeof(offLabel), "Bar Text Y Offset %.1f px", settings->phBarTextOffsetY * 0.1f);
                        ImGui::SliderInt("##phBarTextOffsetY", &settings->phBarTextOffsetY, -100, 100, offLabel);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bar icons grow by the absolute offset and stay centered on their bars.");
                    }
                }
                if (ImGui::SmallButton("Reset Bar Positions##phResetOffsets")) {
                    settings->phHpOffsetX = settings->phHpOffsetY = 0;
                    settings->phMpOffsetX = settings->phMpOffsetY = 0;
                    settings->phSpOffsetX = settings->phSpOffsetY = 0;
                    settings->phXPBarOffsetX = settings->phXPBarOffsetY = 0;
                }
                if ((showAnnotations || xpAsBar) && ImGui::SmallButton("Reset Annotation Positions##phResetAnnotationOffsets")) {
                    settings->phHpAnnotationOffsetX = settings->phHpAnnotationOffsetY = 0;
                    settings->phMpAnnotationOffsetX = settings->phMpAnnotationOffsetY = 0;
                    settings->phSpAnnotationOffsetX = settings->phSpAnnotationOffsetY = 0;
                    settings->phXPAnnotationOffsetX = settings->phXPAnnotationOffsetY = 0;
                }

                if (showAnnotations) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Text & Readouts");
                    ImGui::Spacing();
                }
                if (settings->phShowRegenPip) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    int regenFontScaleInt = static_cast<int>(settings->phRegenFontScale * 100);
                    if (ImGui::SliderInt("##phRegenFontScale", &regenFontScaleInt, 50, 150, "Regen Font Scale %d%%"))
                        settings->phRegenFontScale = static_cast<float>(regenFontScaleInt) / 100.0f;
                }
                if (settings->phShowFortify) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    int fortifyFontScaleInt = static_cast<int>(settings->phFortifyFontScale * 100);
                    if (ImGui::SliderInt("##phFortifyFontScale", &fortifyFontScaleInt, 50, 150, "Stat Bonus Font Scale %d%%"))
                        settings->phFortifyFontScale = static_cast<float>(fortifyFontScaleInt) / 100.0f;
                    ImGui::Checkbox("Fortify on Right##phFortifyRight", &settings->phFortifyRight);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Draw the Fortify bonus at the bar's right edge instead of to the left.\nWith Scale Bar Width to Stat, each number follows its own bar's edge.");
                }

                if (settings->phShowBarText) {
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phBarTextColor);
                        if (ImGui::ColorEdit4("Bar Text##phBarTextCol", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phBarTextColor = ColorUtils::Vec4ToArgb(c);
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        int barTextScaleInt = static_cast<int>(settings->phBarTextFontScale * 100);
                        if (ImGui::SliderInt("##phBarTextScale", &barTextScaleInt, 50, 150, "Bar Text Font Scale %d%%"))
                            settings->phBarTextFontScale = static_cast<float>(barTextScaleInt) / 100.0f;
                    }
                }

                if (!settings->phBarStyleEnabled) {
                    static const char* visualStyles[]   = { "Flat", "Gradient", "Glass", "Segmented" };
                    static const char* rightEndStyles[] = { "Normal", "Slant Forward", "Slant Back", "Arrow", "Diamond" };
                    static const char* leftEndStyles[]  = { "Normal", "Slant Forward", "Slant Back", "Arrow", "Diamond" };

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Text("Build Your Own Bar");
                    ImGui::Spacing();

                    ImGui::TextDisabled("Fill");
                    ImGui::Combo("Style##phVisualStyle", &settings->phBarVisualStyle, visualStyles, 4);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Flat: Solid color\nGradient: Left-to-right fade\nGlass: adds a top sheen\nSegmented: divider lines by stat points\nAll styles support end shapes and the depth passes");
                    if (settings->phBarVisualStyle == 3) {
                        ImGui::SliderInt("Points per Segment##phBarSegPts", &settings->phBarSegmentPoints, 25, 500);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Stat points per segment cell; segment count = max stat / this value");
                        ImGui::SliderInt("Segment Gap##phBarSegGap", &settings->phBarSegmentGap, 0, 10);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Gap carved between segments (px); 0 = hairline notch lines");
                    }
                    ImGui::SliderFloat("Radius##phBars", &settings->phBarCornerRadius, 0.0f, 15.0f, "%.1f");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Depth & Light");
                    ImGui::Checkbox("Track##phBars", &settings->phBarTrack);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Dark recessed well behind the fill (empty part of the bar)");
                    ImGui::SameLine();
                    ImGui::Checkbox("Depth##phBars", &settings->phBarDepthShading);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Vertical shading on the fill: lit top edge, deep bottom");
                    ImGui::SameLine();
                    ImGui::Checkbox("Edge Light##phBars", &settings->phBarEdgeLight);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Brightness that follows the current value's edge");
                    ImGui::SameLine();
                    ImGui::Checkbox("Tip##phBars", &settings->phBarTip);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hot bright strip right at the fill edge");
                    ImGui::SameLine();
                    ImGui::Checkbox("Glow Effect##phBars", &settings->phBarGlow);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Outer bloom around the filled part");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Borders");
                    ImGui::SliderInt("Outer Width##phBarOB", &settings->phBarOuterBorderWidth, 0, 6);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("0 = no outer border");
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phBarOuterBorderColor);
                        if (ImGui::ColorEdit4("Outer Color##phBarOBC", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phBarOuterBorderColor = ColorUtils::Vec4ToArgb(c);
                    }
                    ImGui::SliderInt("Inner Width##phBarIB", &settings->phBarInnerBorderWidth, 0, 6);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("0 = no inner border");
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phBarInnerBorderColor);
                        if (ImGui::ColorEdit4("Inner Color##phBarIBC", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phBarInnerBorderColor = ColorUtils::Vec4ToArgb(c);
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("Shape");
                    ImGui::Combo("Right End##phBars", &settings->phBarEndStyle, rightEndStyles, 5);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Shape of right bar ending:\nNormal: Standard rectangle\nSlant Forward/Back: Diagonal cut\nArrow: Pointed chevron\nDiamond: Diamond point");
                    ImGui::Combo("Left End##phBars", &settings->phBarEndStyleLeft, leftEndStyles, 5);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Shape of left bar ending:\nNormal: Standard rectangle\nSlant Forward/Back: Diagonal cut\nArrow: Pointed chevron\nDiamond: Diamond point");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Motion");
                    ImGui::Checkbox("Smooth Drain##phBars", &settings->phBarSmoothDrain);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Damage drains the bar smoothly (~250ms) so the lit edge travels;\nheals still fill near-instantly");
                }

            }

            if (ImGui::CollapsingHeader("Portrait##PHPortrait")) {
                ImGui::TextDisabled("Frame");
                ImGui::Spacing();

                ImGui::Checkbox("Portrait Frame##phFrameEnable", &settings->phPortraitFrameEnabled);
                if (settings->phPortraitFrameEnabled) {
                    ImGui::SameLine();
                    ImGuiRenderer::RenderBackgroundPicker("PHFramePicker", settings->phPortraitFrameIndex, BackgroundManager::Type::PortraitFrame);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    int frameScaleInt = static_cast<int>(settings->phPortraitFrameScale * 100);
                    if (ImGui::SliderInt("##phFrameScale", &frameScaleInt, 50, 200, "Frame Scale %d%%"))
                        settings->phPortraitFrameScale = static_cast<float>(frameScaleInt) / 100.0f;
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Content / Shape");
                ImGui::Spacing();

                enum class PortraitStyle { Auto, Custom, Animated, Emblem };
                PortraitStyle style = settings->phPortraitMode == 1  ? PortraitStyle::Emblem
                                       : settings->phPortraitAnimEnabled ? PortraitStyle::Animated
                                       : settings->phPortraitIndex > 0   ? PortraitStyle::Custom
                                                                          : PortraitStyle::Auto;
                int styleInt = static_cast<int>(style);
                if (ImGui::Combo("Portrait Style##phPortraitStyle", &styleInt,
                                  "Auto\0Custom Image\0Animated\0Level Emblem\0")) {
                    switch (static_cast<PortraitStyle>(styleInt)) {
                    case PortraitStyle::Animated:
                        settings->phPortraitMode = 0;
                        settings->phPortraitAnimEnabled = true;
                        break;
                    case PortraitStyle::Auto:
                        settings->phPortraitMode = 0;
                        settings->phPortraitAnimEnabled = false;
                        settings->phPortraitIndex = 0;
                        break;
                    case PortraitStyle::Custom:
                        settings->phPortraitMode = 0;
                        settings->phPortraitAnimEnabled = false;
                        if (settings->phPortraitIndex == 0) settings->phPortraitIndex = 1;
                        break;
                    case PortraitStyle::Emblem:
                        settings->phPortraitMode = 1;
                        break;
                    }
                    style = static_cast<PortraitStyle>(styleInt);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Auto: name/FormID portrait.\nCustom Image: Portrait-N.png override.\nAnimated: player-sprites.png sheet.\nLevel Emblem: stylized level badge.");

                ImGui::Indent(20.0f);
                if (style == PortraitStyle::Animated) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    int animFps = static_cast<int>(settings->phPortraitAnimFps);
                    if (ImGui::SliderInt("##phPortraitAnimFps", &animFps, 1, 15, "Animation Speed %d fps"))
                        settings->phPortraitAnimFps = static_cast<float>(animFps);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Playback frames per second. Loop time = frames / fps.\nLower = calmer; higher = busier. 6-8 suits an idle portrait.\nGrid layout is set in the INI (PortraitAnimCols/Rows/Frames).");
                } else if (style == PortraitStyle::Custom) {
                    if (settings->phPortraitIndex < 1) settings->phPortraitIndex = 1;
                    ImGuiRenderer::RenderBackgroundPicker("PHPortraitPicker", settings->phPortraitIndex, BackgroundManager::Type::PlayerPortrait);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select a portrait override from Data\\Interface\\PartySheet\\Portraits\\Portrait-N.png");
                } else if (style == PortraitStyle::Emblem) {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phEmblemFillColor);
                    if (ImGui::ColorEdit4("Emblem Color##phEmblemFillColor", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phEmblemFillColor = ColorUtils::Vec4ToArgb(c);
                }
                ImGui::Unindent(20.0f);

                const char* portraitShapes[] = { "Square", "Circle", "Diamond" };
                ImGui::Combo("Portrait Shape##phPortraitShape", &settings->phPortraitShape, portraitShapes, 3);

                ImGui::Checkbox("Hit Shake##phHitShake", &settings->phPortraitHitShake);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Briefly shakes the portrait horizontally when taking a hit");
                ImGui::SameLine();
                ImGui::Checkbox("Hit Flash/Pulse##phHitFlash", &settings->phPortraitHitFlash);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Red flash on the portrait when taking an unblocked hit.\n"
                                      "Also controls the pulsing red low-health warning below 80%% HP\n"
                                      "(faster with a border below 25%%).");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Size / Border");
                ImGui::Spacing();

                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phPortraitSize", &settings->phPortraitSize, 32, 256, "Portrait Size %d");
                DrawPositionSlider("##phPortraitOffsetX", settings->phPortraitOffsetX, -3840, 3840, "Portrait X %d px");
                DrawPositionSlider("##phPortraitOffsetY", settings->phPortraitOffsetY, -2160, 2160, "Portrait Y %d px");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phBorderW", &settings->phPortraitBorderWidth, 0, 10, "Border / XP Ring Width %d");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Thickness of the portrait border. When XP is shown, this same\nborder becomes the XP progress ring around the portrait.");
                if (settings->phPortraitBorderWidth > 0) {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->phPortraitBorderColor);
                    if (ImGui::ColorEdit4("Border##phBorderCol", reinterpret_cast<float*>(&c), kPHArgb))
                        settings->phPortraitBorderColor = ColorUtils::Vec4ToArgb(c);
                    bool radiusEligible = (settings->phPortraitShape == 0);
                    if (!radiusEligible) ImGui::BeginDisabled();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    int radiusInt = static_cast<int>(settings->phPortraitBorderRadius * 100);
                    if (ImGui::SliderInt("##phBorderR", &radiusInt, 0, 100, "Border Radius %d%%"))
                        settings->phPortraitBorderRadius = static_cast<float>(radiusInt) / 100.0f;
                    if (!radiusEligible) ImGui::EndDisabled();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Layout");
                ImGui::Spacing();

                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##phColSpacing", &settings->phPortraitColumnSpacing, 0, 120, "Column Spacing %d px");

                if (ImGui::CollapsingHeader("Badges##PHBadges")) {
                    ImGui::Indent(20);
                    ImGui::Spacing();
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phBadgeBgColor);
                        if (ImGui::ColorEdit4("Background##phBadgeBg", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phBadgeBgColor = ColorUtils::Vec4ToArgb(c);
                    }
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phBadgeFontColor);
                        if (ImGui::ColorEdit4("Text##phBadgeText", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phBadgeFontColor = ColorUtils::Vec4ToArgb(c);
                    }
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phBadgeBorderColor);
                        if (ImGui::ColorEdit4("Border##phBadgeBorder", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phBadgeBorderColor = ColorUtils::Vec4ToArgb(c);
                    }
                    int badgeFontScaleInt = static_cast<int>(settings->phBadgeFontScale * 100);
                    if (ImGui::SliderInt("Font Scale##phBadgeScale", &badgeFontScaleInt, 50, 150, "%d%%"))
                        settings->phBadgeFontScale = static_cast<float>(badgeFontScaleInt) / 100.0f;
                    ImGui::SliderFloat("Border Radius##phBadgeRadius", &settings->phBadgeRadius, -1.0f, 32.0f,
                        settings->phBadgeRadius < 0.0f ? "Auto" : "%.1f px");
                    ImGui::SliderFloat("Padding X##phBadgePaddingX", &settings->phBadgePaddingX, 0.0f, 20.0f, "%.1f px");
                    ImGui::SliderFloat("Padding Y##phBadgePaddingY", &settings->phBadgePaddingY, 0.0f, 20.0f, "%.1f px");
                    ImGui::Spacing();
                    ImGui::TextDisabled("Level");
                    ImGui::SliderInt("Offset X##phLevelBadgeOffsetX", &settings->phLevelBadgeOffsetX, -100, 100, "%d px");
                    ImGui::SliderInt("Offset Y##phLevelBadgeOffsetY", &settings->phLevelBadgeOffsetY, -100, 100, "%d px");
                    ImGui::Spacing();
                    ImGui::TextDisabled("XP");
                    ImGui::SliderInt("Offset X##phXPBadgeOffsetX", &settings->phXPBadgeOffsetX, -100, 100, "%d px");
                    ImGui::SliderInt("Offset Y##phXPBadgeOffsetY", &settings->phXPBadgeOffsetY, -100, 100, "%d px");
                    ImGui::Unindent(20);
                }

                if (settings->phShowName) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Name");
                    ImGui::Spacing();
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->phNameColor);
                        if (ImGui::ColorEdit4("Name##phNameCol", reinterpret_cast<float*>(&c), kPHArgb))
                            settings->phNameColor = ColorUtils::Vec4ToArgb(c);
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    int nameScaleInt = static_cast<int>(settings->phNameFontScale * 100);
                    if (ImGui::SliderInt("##phNameScale", &nameScaleInt, 50, 150, "Name Font Scale %d%%"))
                        settings->phNameFontScale = static_cast<float>(nameScaleInt) / 100.0f;
                }
            }

            ImGui::Unindent(10);
        }

        bool equipOpen = (s_phSel == 1);

        if (equipOpen) {
            ImGui::Indent(10);
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget##peEnable", &settings->enablePlayerEquipWidget);
            ImGui::Checkbox("Use HUD Visibility Trigger##peHVTrigger", &settings->hvTriggerPlayerEquip);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When enabled, the widget fades in when you draw/sheathe a weapon (R key)\nor enter combat, then fades out after the HUD Visibility delay.");

            if (settings->enablePlayerEquipWidget) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Presets");
                ImGui::Spacing();
                {
                    static int presetIndex = 0;
                    static constexpr const char* presetNames[] = {
                        "1 - Default", "2 - Compact", "3 - Minimal", "4 - Weapon Focus"
                    };
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo("##pePreset", &presetIndex, presetNames, (int)PlayerHUDEquipmentPresets::kPresets.size()))
                        PlayerHUDEquipmentPresets::ApplyPreset(*settings, static_cast<std::size_t>(presetIndex));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Slot Order");
                ImGui::Spacing();

                {
                    const char* slotNames[] = { "Right Hand", "Left Hand", "Shout/Power" };
                    int* order[3] = { &settings->peSlotOrder0, &settings->peSlotOrder1, &settings->peSlotOrder2 };
                    for (int i = 0; i < 3; ++i) {
                        if (i > 0) ImGui::SameLine();
                        int prev = *order[i];
                        char label[16]; snprintf(label, sizeof(label), "Slot %d##peOrder%d", i + 1, i);
                        ImGui::SetNextItemWidth(120.0f);
                        if (ImGui::Combo(label, order[i], slotNames, 3) && *order[i] != prev) {
                            for (int j = 0; j < 3; ++j) {
                                if (j != i && *order[j] == *order[i]) {
                                    *order[j] = prev;
                                    break;
                                }
                            }
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Layout");
                ImGui::Spacing();

                const char* layoutItems[] = { "Row", "Column", "Grid (2-col)" };
                ImGui::Combo("Layout##peLayout", &settings->peLayout, layoutItems, 3);

                if (ImGui::SliderInt("Pos X##pePosX", &settings->pePosX, 0, static_cast<int>(displaySize.x)))
                    PlayerHUDEquipment::ForceReposition();
                if (ImGui::SliderInt("Pos Y##pePosY", &settings->pePosY, 0, static_cast<int>(displaySize.y)))
                    PlayerHUDEquipment::ForceReposition();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Items (Show / Hide)");
                ImGui::Spacing();

                if (ImGui::BeginTable("##PEItemsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##PEItemCol0", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                    ImGui::TableSetupColumn("##PEItemCol1", ImGuiTableColumnFlags_WidthStretch, 0.5f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Names##peNames",       &settings->peShowNames);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Show Slot Badge##peBadge",  &settings->peShowSlotBadge);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Ammo Count##peAmmo",   &settings->peShowAmmoCount);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Show Weapon Damage##peDmg",  &settings->peShowDamage);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Slot Icon BG##peIBgEn", &settings->peIconBgEnabled);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("Show Durability##peDura", &settings->peShowDurability);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Show wear pips on weapons (requires EquipmentDurabilitySystem-NG).");

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                {
                    static const char* chargeStyleItems[] = { "Number Badge", "Vertical Bar", "Off" };
                    ImGui::Combo("Enchant Charge Display##peChargeStyle", &settings->peChargeStyle, chargeStyleItems, 3);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("How the enchant charge on the equipped weapon/shield is shown.");
                }

                ImGui::Checkbox("Custom Slot Background##peIBgTex", &settings->peUseIconBgTexture);
                if (settings->peUseIconBgTexture) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGuiRenderer::RenderBackgroundPicker("PEIconBgPicker", settings->peIconBgIndex,
                                                         BackgroundManager::Type::Inspectcard);
                }

                ImGui::Spacing();
                ImGui::Separator();

                static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                ImVec4 c;

                if (BeginAppearanceGroup("##PEIconsGrp", "Icons")) {
                    if (settings->peIconBgEnabled) {
                        c = ColorUtils::ArgbToVec4(settings->peIconBgColor);
                        if (ImGui::ColorEdit4("Slot Icon BG##peIBgCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->peIconBgColor = ColorUtils::Vec4ToArgb(c);
                        if (!settings->peUseIconBgTexture) {
                            const char* bgStyles[] = { "Solid", "Fade" };
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::Combo("Style##peIconBgStyle", &settings->peIconBgStyle, bgStyles, IM_ARRAYSIZE(bgStyles));
                            if (settings->peIconBgStyle == 1) {
                                const char* fadeModes[] = { "Fade Right", "Fade Left", "Fade Down", "Fade Up",
                                                            "Center Out", "Edge Origin", "Diagonal TL", "Diagonal BL" };
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::Combo("Fade Mode##peIconBgFadeMode", &settings->peIconBgFadeMode, fadeModes, IM_ARRAYSIZE(fadeModes));
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::SliderInt("Solid Anchor##peIconBgFadeAnchor", &settings->peIconBgFadeAnchor, 0, 60, "%d%%");
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::SliderInt("Fade Length##peIconBgFadeLength", &settings->peIconBgFadeLength, 10, 100, "%d%%");
                            }
                        }
                    }
                    NextAppearanceStyleCol();
                    ImGui::TextDisabled("Icon Source");
                    {
                        static const char* styleItems[] = { "Item Art", "PNG Icon Injector" };
                        // Glyph resolve happens in GatherSlots, so a style flip must force a regather.
                        if (ImGui::Combo("Icon Style##peIconStyle", &settings->peIconStyle, styleItems, 2))
                            PlayerHUDEquipment::MarkSlotsDirty();
                    }
                    if (!PNGIIBridge::Available()) {
                        ImGui::TextDisabled("PNG Icon Injector not installed, using item art");
                    }
                    if (settings->peIconStyle == 1) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peGlyphScale", &settings->peGlyphScale, 0.40f, 1.00f, "Glyph Size %.2f");
                        c = ColorUtils::ArgbToVec4(settings->peGlyphColor);
                        if (ImGui::ColorEdit4("Glyph Color##peGlyphColor", reinterpret_cast<float*>(&c), kArgb))
                            settings->peGlyphColor = ColorUtils::Vec4ToArgb(c);
                        ImGui::Checkbox("Tint by Element##peGlyphTint", &settings->peGlyphTintByElement);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Poison or enchant element color replaces the glyph color (Aura palette).");
                    }

                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peIcon", &settings->peIconSize, 24, 128, "Icon Size %d");
                    if (settings->peLayout == 2) {
                        ImGui::TextDisabled("Per-slot size ignored in Grid layout");
                    } else {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peScaleR", &settings->peSlotScale0, 0.3f, 2.5f, "R Hand Size x%.2f");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peScaleL", &settings->peSlotScale1, 0.3f, 2.5f, "L Hand Size x%.2f");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peScaleZ", &settings->peSlotScale2, 0.3f, 2.5f, "Shout Size x%.2f");
                    }
                    // Item art also draws for PNGII misses, but hiding these keeps one size control per style.
                    if (settings->peIconStyle == 0 || !PNGIIBridge::Available()) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##peBgPad", &settings->peIconBgPadding, 0, 32, "Icon Padding %d");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peZoom", &settings->peIconZoom, 1.0f, 3.0f, "Icon Zoom %.2f");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peFU", &settings->peIconFocusU, 0.0f, 1.0f, "Focus U %.2f");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peFV", &settings->peIconFocusV, 0.0f, 1.0f, "Focus V %.2f");
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("Icon FX (spells + PNG Icon Injector)");
                    ImGui::Checkbox("Duotone##peSpellDuo", &settings->peSpellIconDuotone);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Apply a lit-top → shaded-bottom gradient to spell and shout icons.");
                    ImGui::SameLine();
                    ImGui::Checkbox("Shadow##peSpellSh", &settings->peSpellIconShadow);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Add the same rim-light + soft shadow stack used by other HUD icons.");
                    ImGui::Checkbox("Outline##peSpellOl", &settings->peSpellIconOutline);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Draw a dark contour ring around the spell icon silhouette.");
                    if (settings->peSpellIconOutline) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##peSpellOlW", &settings->peSpellIconOutlineWidth, 1, 3, "Outline Width %d px");
                    }

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float deg = settings->peWeaponRotation * 57.2958f;
                        if (ImGui::SliderFloat("##peWR", &deg, -90.0f, 90.0f, "Weapon Tilt %.0f deg"))
                            settings->peWeaponRotation = deg / 57.2958f;
                    }
                    EndAppearanceGroup();
                }

                if (BeginAppearanceGroup("##PESlotGrp", "Slot")) {
                    NextAppearanceStyleCol();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peSpacing", &settings->peSlotSpacing, 0, 100, "Slot Spacing %d");
                    if (settings->peIconBgEnabled || settings->peUseIconBgTexture) {
                        static const char* shapeItems[] = { "Rectangle", "Circle", "Diamond" };
                        ImGui::Combo("Backplate Shape##peIBgShape", &settings->peIconBgShape, shapeItems, 3);
                    }
                    if (settings->peUseIconBgTexture) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderFloat("##peIBgTexOpacity", &settings->peIconBgTexOpacity, 0.0f, 1.0f, "Slot BG Opacity %.2f");
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peSlotOffsetX1", &settings->peSlotOffsetX1, -200, 200, "2nd Slot Offset X %d px");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Nudges the 2nd drawn slot (after slot order) without affecting layout spacing.");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peSlotOffsetY1", &settings->peSlotOffsetY1, -200, 200, "2nd Slot Offset Y %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peSlotOffsetX2", &settings->peSlotOffsetX2, -200, 200, "3rd Slot Offset X %d px");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Nudges the 3rd drawn slot (after slot order) without affecting layout spacing.");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peSlotOffsetY2", &settings->peSlotOffsetY2, -200, 200, "3rd Slot Offset Y %d px");
                    EndAppearanceGroup();
                }

                if (BeginAppearanceGroup("##PEBorderGrp", "Border")) {
                    c = ColorUtils::ArgbToVec4(settings->peIconBorderColor);
                    if (ImGui::ColorEdit4("Border##peIBC", reinterpret_cast<float*>(&c), kArgb))
                        settings->peIconBorderColor = ColorUtils::Vec4ToArgb(c);
                    NextAppearanceStyleCol();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peIBW", &settings->peIconBorderWidth, 0, 6, "Border Width %d");
                    if (settings->peIconBgShape == 0) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##peIBR", &settings->peIconBorderRadius, 0, 50, "Border Radius %d");
                    }
                    EndAppearanceGroup();
                }

                if (BeginAppearanceGroup("##PEAuraGrp", "Aura")) {
                    ImGui::Checkbox("Show Aura##peShowAura", &settings->peShowAura);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Master toggle for the weapon poison/enchant edge ring and inner glow wash.");
                    ImGui::BeginDisabled(!settings->peShowAura);
                    c = ColorUtils::ArgbToVec4(settings->peAuraPoisonColor);
                    if (ImGui::ColorEdit4("Poison##peAuraPoison", reinterpret_cast<float*>(&c), kArgb))
                        settings->peAuraPoisonColor = ColorUtils::Vec4ToArgb(c);
                    c = ColorUtils::ArgbToVec4(settings->peAuraFireColor);
                    if (ImGui::ColorEdit4("Fire##peAuraFire", reinterpret_cast<float*>(&c), kArgb))
                        settings->peAuraFireColor = ColorUtils::Vec4ToArgb(c);
                    c = ColorUtils::ArgbToVec4(settings->peAuraFrostColor);
                    if (ImGui::ColorEdit4("Frost##peAuraFrost", reinterpret_cast<float*>(&c), kArgb))
                        settings->peAuraFrostColor = ColorUtils::Vec4ToArgb(c);
                    c = ColorUtils::ArgbToVec4(settings->peAuraShockColor);
                    if (ImGui::ColorEdit4("Shock##peAuraShock", reinterpret_cast<float*>(&c), kArgb))
                        settings->peAuraShockColor = ColorUtils::Vec4ToArgb(c);
                    c = ColorUtils::ArgbToVec4(settings->peAuraOtherColor);
                    if (ImGui::ColorEdit4("Enchanted (Other)##peAuraOther", reinterpret_cast<float*>(&c), kArgb))
                        settings->peAuraOtherColor = ColorUtils::Vec4ToArgb(c);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Alpha controls glow/ring intensity, not just opacity.");
                    ImGui::EndDisabled();
                    EndAppearanceGroup();
                }

                if (BeginAppearanceGroup("##PEBadgeGrp", "Badge")) {
                    c = ColorUtils::ArgbToVec4(settings->peBadgeBgColor);
                    if (ImGui::ColorEdit4("Badge BG##peBadgeBg", reinterpret_cast<float*>(&c), kArgb))
                        settings->peBadgeBgColor = ColorUtils::Vec4ToArgb(c);
                    NextAppearanceStyleCol();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float fs = settings->peBadgeFontScale;
                        if (ImGui::SliderFloat("##peBadgeFontScale", &fs, 0.5f, 1.5f, "Badge Font Scale %.2f"))
                            settings->peBadgeFontScale = fs;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peBadgeOffsetX", &settings->peBadgeOffsetX, -20, 40, "Badge H Offset %d");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peBadgeOffsetY", &settings->peBadgeOffsetY, -20, 40, "Badge V Offset %d");
                    EndAppearanceGroup();
                }

                if (settings->peShowNames && BeginAppearanceGroup("##PENameGrp", "Name")) {
                    c = ColorUtils::ArgbToVec4(settings->peNameColor);
                    if (ImGui::ColorEdit4("Name##peNameCol", reinterpret_cast<float*>(&c), kArgb))
                        settings->peNameColor = ColorUtils::Vec4ToArgb(c);
                    c = ColorUtils::ArgbToVec4(settings->peNameBgColor);
                    if (ImGui::ColorEdit4("Name BG##peNameBg", reinterpret_cast<float*>(&c), kArgb))
                        settings->peNameBgColor = ColorUtils::Vec4ToArgb(c);
                    NextAppearanceStyleCol();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float fs = settings->peNameFontScale;
                        if (ImGui::SliderFloat("##peNameFontScale", &fs, 0.5f, 1.5f, "Name Font Scale %.2f"))
                            settings->peNameFontScale = fs;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##peNameMaxChars", &settings->peNameMaxChars, 0, 60,
                        settings->peNameMaxChars == 0 ? "Name Max Chars: No limit" : "Name Max Chars %d");
                    EndAppearanceGroup();
                }

                if ((settings->peChargeStyle == 1 || settings->peShowDurability) &&
                    BeginAppearanceGroup("##PEChargeGrp", "Charge / Durability")) {
                    NextAppearanceStyleCol();
                    if (settings->peChargeStyle == 1) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##peChargeBarWidth", &settings->peChargeBarWidth, 4, 40, "Charge Bar Width %d px");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        {
                            int hpct = (int)(settings->peChargeBarHeight * 100.0f + 0.5f);
                            if (ImGui::SliderInt("##peChargeBarHeight", &hpct, 5, 100, "Charge Bar Height %d%%"))
                                settings->peChargeBarHeight = hpct / 100.0f;
                        }
                    }
                    if (settings->peShowDurability) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        int dpct = (int)(settings->peDurabilityScale * 100.0f + 0.5f);
                        if (ImGui::SliderInt("##peDuraScale", &dpct, 30, 200, "Durability Pip Scale %d%%"))
                            settings->peDurabilityScale = dpct / 100.0f;
                    }
                    EndAppearanceGroup();
                }
            }

            ImGui::Spacing();
            ImGui::Unindent(10);
        }

        bool resistOpen = (s_phSel == 2);

        if (resistOpen) {
            ImGui::Indent(10);
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget##prEnable", &settings->enablePlayerResistWidget);
            ImGui::Checkbox("Use HUD Visibility Trigger##prHVTrigger", &settings->hvTriggerPlayerResist);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When enabled, the widget fades in when combat starts or armor is equipped,\nthen fades out after the HUD Visibility delay.");

            ImGui::Indent(20);
            ImGui::Checkbox("Show in Combat Only##hvShowResistCombatOnly", &settings->hvShowResistCombatOnly);
            ImGui::Unindent(20);

            if (settings->enablePlayerResistWidget) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Presets");
                ImGui::Spacing();
                {
                    static int presetIndex = 0;
                    static constexpr const char* presetNames[] = {
                        "1 - Default", "2 - Compact", "3 - Vivid", "4 - Minimal", "5 - Halo"
                    };
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo("##prPreset", &presetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
                        PlayerHUDResistancesPresets::ApplyPreset(*settings, static_cast<std::size_t>(presetIndex));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Layout / Behavior");
                ImGui::Spacing();

                const char* prLayoutItems[] = { "Vertical", "Horizontal", "Radial" };
                ImGui::Combo("Layout##prLayout", &settings->prLayout, prLayoutItems, 3);

                if (settings->prLayout == 2) {
                    ImGui::SliderInt("Radius##prRadialRadius", &settings->prRadialRadius, 30, 200, "%d px");
                    ImGui::SliderInt("Start Angle##prRadialStartAngle", &settings->prRadialStartAngle, -180, 180, "%d\xc2\xb0");
                    ImGui::Checkbox("Value Rings##prRadialShowRing", &settings->prRadialShowRing);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Draw the value arc ring around each icon.\nTurn off to use Icon Fill (or nothing) as the meter instead.");
                }

                ImGui::Checkbox("Show Values##prShowValue", &settings->prShowValue);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Show the numeric value next to/under each icon.\nOff = icons only.");
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::Checkbox("Icon Fill##prIconFill", &settings->prIconFill);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("The icon itself becomes the meter: a faint ghost icon\nfills bottom-up with the resist value (like the Survival widget).\nWorks in all layouts.");
                if (ImGui::SliderInt("Pos X##prPosX", &settings->prPosX, 0, static_cast<int>(displaySize.x)))
                    PlayerHUDResistances::ForceReposition();
                if (ImGui::SliderInt("Pos Y##prPosY", &settings->prPosY, 0, static_cast<int>(displaySize.y)))
                    PlayerHUDResistances::ForceReposition();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Appearance");
                ImGui::Spacing();

                if (ImGui::BeginTable("##PRAppearanceTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##PRColorCol", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("##PRSizeCol",  ImGuiTableColumnFlags_WidthStretch, 0.6f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                        ImVec4 c;
                        c = ColorUtils::ArgbToVec4(settings->prRibbonColor);
                        if (ImGui::ColorEdit4("Ribbon##prRibbon", reinterpret_cast<float*>(&c), kArgb))
                            settings->prRibbonColor = ColorUtils::Vec4ToArgb(c);
                        {
                            const char* bgStyles[] = { "Solid", "Fade" };
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::Combo("Style##prRibbonBgStyle", &settings->prRibbonBgStyle, bgStyles, IM_ARRAYSIZE(bgStyles));
                            if (settings->prRibbonBgStyle == 1) {
                                const char* fadeModes[] = { "Fade Right", "Fade Left", "Fade Down", "Fade Up",
                                                            "Center Out", "Edge Origin", "Diagonal TL", "Diagonal BL" };
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::Combo("Fade Mode##prRibbonFadeMode", &settings->prRibbonFadeMode, fadeModes, IM_ARRAYSIZE(fadeModes));
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::SliderInt("Solid Anchor##prRibbonFadeAnchor", &settings->prRibbonFadeAnchor, 0, 60, "%d%%");
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::SliderInt("Fade Length##prRibbonFadeLength", &settings->prRibbonFadeLength, 10, 100, "%d%%");
                            }
                        }
                        ImGui::Spacing();
                        ImGui::TextDisabled("Resist Colors");
                        c = ColorUtils::ArgbToVec4(settings->prArmorColor);     if (ImGui::ColorEdit4("Armor##prArmor",     reinterpret_cast<float*>(&c), kArgb)) settings->prArmorColor     = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prDmgResistColor); if (ImGui::ColorEdit4("Phys Res##prPhys",   reinterpret_cast<float*>(&c), kArgb)) settings->prDmgResistColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prFireColor);      if (ImGui::ColorEdit4("Fire##prFire",       reinterpret_cast<float*>(&c), kArgb)) settings->prFireColor      = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prFrostColor);     if (ImGui::ColorEdit4("Frost##prFrost",     reinterpret_cast<float*>(&c), kArgb)) settings->prFrostColor     = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prShockColor);     if (ImGui::ColorEdit4("Shock##prShock",     reinterpret_cast<float*>(&c), kArgb)) settings->prShockColor     = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prMagicColor);     if (ImGui::ColorEdit4("Magic##prMagic",     reinterpret_cast<float*>(&c), kArgb)) settings->prMagicColor     = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prPoisonColor);    if (ImGui::ColorEdit4("Poison##prPoison",   reinterpret_cast<float*>(&c), kArgb)) settings->prPoisonColor    = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->prDiseaseColor);   if (ImGui::ColorEdit4("Disease##prDisease", reinterpret_cast<float*>(&c), kArgb)) settings->prDiseaseColor   = ColorUtils::Vec4ToArgb(c);
                        ImGui::Spacing();
                        ImGui::TextDisabled("Movement");
                        ImGui::Checkbox("Show Move Speed##prShowSpeed", &settings->prShowSpeed);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Append a Move Speed entry (SpeedMult %, 100 = normal)\nto this strip, alongside Armor and the resists.");
                        if (settings->prShowSpeed) {
                            c = ColorUtils::ArgbToVec4(settings->prSpeedColor); if (ImGui::ColorEdit4("Speed##prSpeed", reinterpret_cast<float*>(&c), kArgb)) settings->prSpeedColor = ColorUtils::Vec4ToArgb(c);
                        }
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##prIconSz", &settings->prIconSize, 12, 128, "Icon Size %d");
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float prFs = settings->prFontScale;
                        if (ImGui::SliderFloat("##prFontScale", &prFs, 0.5f, 1.5f, "Font Scale %.2f"))
                            settings->prFontScale = prFs;
                    }
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##prSpacing", &settings->prSpacing, 0, 40, "Spacing %d px");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Icon Depth");
                    ImGui::Checkbox("Duotone##prIconDuo", &settings->prIconDuotone);
                    ImGui::SameLine();
                    ImGui::Checkbox("Shadow##prIconSh", &settings->prIconShadow);
                    ImGui::Checkbox("Glow##prIconGlow", &settings->prIconGlow);
                    ImGui::SameLine();
                    ImGui::Checkbox("Gloss##prIconGloss", &settings->prIconGloss);
                    ImGui::Checkbox("Outline##prIconOl", &settings->prIconOutline);
                    if (settings->prIconOutline) {
                        ImGui::SameLine();
                        const float swatchW = ImGui::GetFrameHeight();
                        float olSliderW = ImGui::GetContentRegionAvail().x - swatchW - ImGui::GetStyle().ItemSpacing.x;
                        ImGui::SetNextItemWidth(olSliderW < 80.0f ? 80.0f : olSliderW);
                        ImGui::SliderInt("##prIconOlW", &settings->prIconOutlineWidth, 1, 3, "Outline Width %d px");
                        ImGui::SameLine();
                        static constexpr ImGuiColorEditFlags kOlArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                        ImVec4 oc = ColorUtils::ArgbToVec4(settings->prIconOutlineColor);
                        if (ImGui::ColorEdit4("##prIconOlC", reinterpret_cast<float*>(&oc), kOlArgb))
                            settings->prIconOutlineColor = ColorUtils::Vec4ToArgb(oc);
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("Text Depth");
                    ImGui::Checkbox("Shadow##prTxtSh", &settings->prTextShadow);
                    ImGui::SameLine();
                    ImGui::Checkbox("Duotone##prTxtDuo", &settings->prTextDuotone);
                    ImGui::SameLine();
                    ImGui::Checkbox("Outline##prTxtOl", &settings->prTextOutline);
                    if (settings->prTextOutline) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##prTxtOlA", &settings->prTextOutlineOpacity, 0, 255, "Outline Opacity %d");
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("Background");
                    ImGui::Checkbox("Enable Background##prBgTint", &settings->prBgTintEnabled);
                    if (settings->prBgTintEnabled) {
                        const char* prBgStyleItems[] = { "Flat Tint", "Layered Chip" };
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::Combo("Style##prBgStyle", &settings->prBgStyle, prBgStyleItems, 2);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Flat Tint: dim per-resist colored fill (classic).\nLayered Chip: dark gradient card with shadow, double border\nand an element-colored accent strip.");
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::SliderInt("##prCornerRadius", &settings->prCornerRadius, 0, 12, "Corner Radius %d");

                        if (settings->prBgStyle == 0) {
                            ImGui::TextDisabled("Flat Tint");
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            ImGui::SliderInt("##prBgTintOpacity", &settings->prBgTintOpacity, 0, 255, "Tint Opacity %d");
                        } else {
                            ImGui::TextDisabled("Layered Chip");
                            static constexpr ImGuiColorEditFlags kChipArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                            ImVec4 cc;
                            cc = ColorUtils::ArgbToVec4(settings->prChipTopColor);
                            if (ImGui::ColorEdit4("Chip Top##prChipTop", reinterpret_cast<float*>(&cc), kChipArgb))
                                settings->prChipTopColor = ColorUtils::Vec4ToArgb(cc);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gradient top color. Its alpha sets the whole chip fill opacity.");
                            cc = ColorUtils::ArgbToVec4(settings->prChipBottomColor);
                            if (ImGui::ColorEdit4("Chip Bottom##prChipBottom", reinterpret_cast<float*>(&cc), kChipArgb))
                                settings->prChipBottomColor = ColorUtils::Vec4ToArgb(cc);
                            cc = ColorUtils::ArgbToVec4(settings->prChipOuterBorderColor);
                            if (ImGui::ColorEdit4("Outer Border##prChipOuterB", reinterpret_cast<float*>(&cc), kChipArgb))
                                settings->prChipOuterBorderColor = ColorUtils::Vec4ToArgb(cc);
                            cc = ColorUtils::ArgbToVec4(settings->prChipInnerBorderColor);
                            if (ImGui::ColorEdit4("Inner Border##prChipInnerB", reinterpret_cast<float*>(&cc), kChipArgb))
                                settings->prChipInnerBorderColor = ColorUtils::Vec4ToArgb(cc);
                            ImGui::Checkbox("Drop Shadow##prChipShadow",   &settings->prChipShadow);
                            ImGui::SameLine();
                            ImGui::Checkbox("Top Highlight##prChipHl",     &settings->prChipHighlight);
                            ImGui::Checkbox("Grain Texture##prChipGrain",  &settings->prChipGrain);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Grunge overlay on the chip fill (Frames\\ChipGrain.png).\nMakes the flat gradient read as a surface.");
                            if (settings->prChipGrain) {
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                ImGui::SliderInt("##prChipGrainOp", &settings->prChipGrainOpacity, 0, 255, "Grain Opacity %d");
                            }
                            ImGui::Checkbox("Color Accent Strip##prChipAcc", &settings->prChipAccent);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thin strip along the chip bottom in the resist's color.\nBuff/debuff states still show their strip when this is off.");
                            ImGui::Checkbox("Neutral Value Text##prChipTxt", &settings->prChipNeutralText);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ivory value text instead of the resist color.");
                        }
                    }

                    ImGui::EndTable();
                }
            }

            ImGui::Spacing();
            ImGui::Unindent(10);
        }

        bool resourcesOpen = (s_phSel == 3);

        if (resourcesOpen) {
            ImGui::Indent(10);
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget##presEnable", &settings->enablePlayerResourcesWidget);
            ImGui::Checkbox("Use HUD Visibility Trigger##presHVTrigger", &settings->hvTriggerPlayerResources);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When enabled, the widget fades in when gold or carry weight changes,\nthen fades out after the HUD Visibility delay.");

            if (settings->enablePlayerResourcesWidget) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Layout / Behavior");
                ImGui::Spacing();

                const char* presLayoutItems[] = { "Vertical", "Horizontal" };
                ImGui::Combo("Layout##presLayout", &settings->presLayout, presLayoutItems, 2);
                if (ImGui::SliderInt("Pos X##presPosX", &settings->presPosX, 0, static_cast<int>(displaySize.x)))
                    PlayerHUDResources::ForceReposition();
                if (ImGui::SliderInt("Pos Y##presPosY", &settings->presPosY, 0, static_cast<int>(displaySize.y)))
                    PlayerHUDResources::ForceReposition();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Items (Show / Hide)");
                ImGui::Spacing();

                if (ImGui::BeginTable("##presShowHide", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Gold##presGold",       &settings->presShowGold);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Show Carry Weight##presCW", &settings->presShowCarryWeight);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Time##presTime",       &settings->presShowTime);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Show Weather/Location##presWeather", &settings->presShowWeather);
                    ImGui::EndTable();
                }

                if (settings->presShowTime) {
                    ImGui::Indent(20);
                    ImGui::Checkbox("24-Hour Clock##pres24Hour", &settings->pres24HourTime);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("On: 19:05. Off: 7:05 PM.");
                    ImGui::Unindent(20);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Appearance");
                ImGui::Spacing();

                if (ImGui::BeginTable("##PRESAppearanceTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##PRESColorCol", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("##PRESSizeCol",  ImGuiTableColumnFlags_WidthStretch, 0.6f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                        ImVec4 c;
                        c = ColorUtils::ArgbToVec4(settings->presTextColor);
                        if (ImGui::ColorEdit4("Text##presTextCol", reinterpret_cast<float*>(&c), kArgb))
                            settings->presTextColor = ColorUtils::Vec4ToArgb(c);
                        if (settings->presShowGold) {
                            c = ColorUtils::ArgbToVec4(settings->presGoldColor);
                            if (ImGui::ColorEdit4("Gold Icon##presGoldCol", reinterpret_cast<float*>(&c), kArgb))
                                settings->presGoldColor = ColorUtils::Vec4ToArgb(c);
                        }
                        if (settings->presShowCarryWeight) {
                            c = ColorUtils::ArgbToVec4(settings->presCarryColor);
                            if (ImGui::ColorEdit4("Carry Icon##presCarryCol", reinterpret_cast<float*>(&c), kArgb))
                                settings->presCarryColor = ColorUtils::Vec4ToArgb(c);
                        }
                        if (settings->presShowTime) {
                            c = ColorUtils::ArgbToVec4(settings->presTimeColor);
                            if (ImGui::ColorEdit4("Time Icon##presTimeCol", reinterpret_cast<float*>(&c), kArgb))
                                settings->presTimeColor = ColorUtils::Vec4ToArgb(c);
                        }
                        if (settings->presShowWeather) {
                            c = ColorUtils::ArgbToVec4(settings->presWeatherColor);
                            if (ImGui::ColorEdit4("Weather Icon##presWeatherCol", reinterpret_cast<float*>(&c), kArgb))
                                settings->presWeatherColor = ColorUtils::Vec4ToArgb(c);
                        }
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##presIconSize", &settings->presIconSize, 0, 64, settings->presIconSize == 0 ? "Icon Size Auto" : "Icon Size %d");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    {
                        float presFs = settings->presFontScale;
                        if (ImGui::SliderFloat("##presFontScale", &presFs, 0.5f, 1.5f, "Font Scale %.2f"))
                            settings->presFontScale = presFs;
                    }
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##presSpacing", &settings->presSpacing, 0, 40, "Spacing %d px");

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Background");
                ImGui::Spacing();

                static constexpr ImGuiColorEditFlags kBgArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                {
                    ImVec4 c = ColorUtils::ArgbToVec4(settings->presBgColor);
                    if (ImGui::ColorEdit4("Background##presBgCol", reinterpret_cast<float*>(&c), kBgArgb))
                        settings->presBgColor = ColorUtils::Vec4ToArgb(c);
                }
                {
                    const char* bgStyles[] = { "Solid", "Fade" };
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::Combo("Style##presBgStyle", &settings->presBgStyle, bgStyles, IM_ARRAYSIZE(bgStyles));
                    if (settings->presBgStyle == 1) {
                        const char* fadeModes[] = { "Fade Right", "Fade Left", "Fade Down", "Fade Up",
                                                    "Center Out", "Edge Origin", "Diagonal TL", "Diagonal BL" };
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::Combo("Fade Mode##presFadeMode", &settings->presFadeMode, fadeModes, IM_ARRAYSIZE(fadeModes));
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::SliderInt("Solid Anchor##presFadeAnchor", &settings->presFadeAnchor, 0, 60, "%d%%");
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::SliderInt("Fade Length##presFadeLength", &settings->presFadeLength, 10, 100, "%d%%");
                    }
                }
                ImGui::SliderInt("Bg Padding##presBgPad", &settings->presBgPadding, 0, 24, "%d px");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Weather / Location Window");
                ImGui::Spacing();

                ImGui::Checkbox("Separate Window##presLocSep", &settings->presLocSeparate);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Draw Weather / Location in its own draggable window\ninstead of inline with the other resources.\nRequires Show Weather to be on.");

                ImGui::Checkbox("Use HUD Visibility Trigger##hvTriggerPlayerLocation", &settings->hvTriggerPlayerLocation);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("When enabled, the Weather / Location window fades in on a\nweather or location change, then fades out on its own timer\n(independent of the Resources widget).");

                if (settings->presLocSeparate) {
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Right Align##presLocAlignR", &settings->presLocAlignRight))
                        PlayerHUDResources::ForceReposition();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Anchor the window's right edge at Loc Pos X and grow leftward,\nso icon + text sit to the LEFT of the anchor. Keeps a screen-corner\nplacement pinned as the label length changes.");

                    if (ImGui::SliderInt("Loc Pos X##presLocPosX", &settings->presLocPosX, 0, static_cast<int>(displaySize.x)))
                        PlayerHUDResources::ForceReposition();
                    if (ImGui::SliderInt("Loc Pos Y##presLocPosY", &settings->presLocPosY, 0, static_cast<int>(displaySize.y)))
                        PlayerHUDResources::ForceReposition();
                    {
                        ImVec4 c = ColorUtils::ArgbToVec4(settings->presLocBgColor);
                        if (ImGui::ColorEdit4("Window Background##presLocBgCol", reinterpret_cast<float*>(&c), kBgArgb))
                            settings->presLocBgColor = ColorUtils::Vec4ToArgb(c);
                    }
                    {
                        const char* bgStyles[] = { "Solid", "Fade" };
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::Combo("Style##presLocBgStyle", &settings->presLocBgStyle, bgStyles, IM_ARRAYSIZE(bgStyles));
                        if (settings->presLocBgStyle == 1) {
                            const char* fadeModes[] = { "Fade Right", "Fade Left", "Fade Down", "Fade Up",
                                                        "Center Out", "Edge Origin", "Diagonal TL", "Diagonal BL" };
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::Combo("Fade Mode##presLocFadeMode", &settings->presLocFadeMode, fadeModes, IM_ARRAYSIZE(fadeModes));
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::SliderInt("Solid Anchor##presLocFadeAnchor", &settings->presLocFadeAnchor, 0, 60, "%d%%");
                            ImGui::SetNextItemWidth(120.0f);
                            ImGui::SliderInt("Fade Length##presLocFadeLength", &settings->presLocFadeLength, 10, 100, "%d%%");
                        }
                    }
                    ImGui::SliderInt("Window Bg Padding##presLocBgPad", &settings->presLocBgPadding, 0, 24, "%d px");

                    ImGui::Checkbox("Icon Diamond Backdrop##presLocIconDiamond", &settings->presLocIconDiamond);
                    if (settings->presLocIconDiamond) {
                        ImGui::Indent(20);
                        {
                            ImVec4 c = ColorUtils::ArgbToVec4(settings->presLocIconDiamondColor);
                            if (ImGui::ColorEdit4("Backdrop Color##presLocIconDiamondColor", reinterpret_cast<float*>(&c), kBgArgb))
                                settings->presLocIconDiamondColor = ColorUtils::Vec4ToArgb(c);
                        }
                        ImGui::SliderInt("Border Width##presLocIconDiamondBorderWidth", &settings->presLocIconDiamondBorderWidth, 0, 6, "%d px");
                        {
                            ImVec4 c = ColorUtils::ArgbToVec4(settings->presLocIconDiamondBorderColor);
                            if (ImGui::ColorEdit4("Border Color##presLocIconDiamondBorderColor", reinterpret_cast<float*>(&c), kBgArgb))
                                settings->presLocIconDiamondBorderColor = ColorUtils::Vec4ToArgb(c);
                        }
                        ImGui::Unindent(20);
                    }
                }

                ImGui::SliderInt("Location Name Max Chars##presLocMax", &settings->presLocMaxChars, 0, 60,
                    settings->presLocMaxChars == 0 ? "No limit" : "%d chars");
            }

            ImGui::Spacing();
            ImGui::Unindent(10);
        }

        bool survivalOpen = (s_phSel == 4);

        if (survivalOpen) {
            ImGui::Indent(10);
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget##phSurvivalEnable", &settings->enablePlayerSurvivalWidget);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shows Cold / Hunger / Fatigue cards when Survival Mode (ccqdrsse001-survivalmode.esl) is active.");

            if (settings->enablePlayerSurvivalWidget) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Presets");
                ImGui::Spacing();
                {
                    static int presetIndex = 0;
                    static constexpr const char* presetNames[] = {
                        "1 - Default", "2 - Compact", "3 - Vivid", "4 - Minimal", "5 - Arc"
                    };
                    static_assert(IM_ARRAYSIZE(presetNames) == PlayerHUDSurvivalPresets::kPresets.size());
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo("##phSurvPreset", &presetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
                        PlayerHUDSurvivalPresets::ApplyPreset(*settings, static_cast<std::size_t>(presetIndex));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Layout");
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Style:");
                ImGui::SameLine();
                ImGui::RadioButton("Standard##phSurvStyle", &settings->phSurvivalStyle, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Compact##phSurvStyle", &settings->phSurvivalStyle, 1);

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Direction:");
                ImGui::SameLine();
                ImGui::RadioButton("Horizontal##phSurvDir", &settings->phSurvivalDirection, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Vertical##phSurvDir", &settings->phSurvivalDirection, 1);

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Indicator:");
                ImGui::SameLine();
                ImGui::RadioButton("Pips##phSurvIndStyle", &settings->phSurvivalIndicatorStyle, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Progress Bar##phSurvIndStyle", &settings->phSurvivalIndicatorStyle, 1);
                const float indicatorIndent = ImGui::CalcTextSize("Indicator:").x + ImGui::GetStyle().ItemSpacing.x;
                ImGui::Indent(indicatorIndent);
                ImGui::RadioButton("None##phSurvIndStyle", &settings->phSurvivalIndicatorStyle, 2);
                ImGui::SameLine();
                ImGui::RadioButton("Arc##phSurvIndStyle", &settings->phSurvivalIndicatorStyle, 3);
                ImGui::Unindent(indicatorIndent);
                ImGui::Checkbox("Icon Fill##phSurvFill", &settings->phSurvivalIconFill);
                if (settings->phSurvivalIndicatorStyle == 0) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Pip Design:");
                    ImGui::SameLine();
                    ImGui::RadioButton("Circle##phSurvPip", &settings->phSurvivalPipShape, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("Diamond##phSurvPip", &settings->phSurvivalPipShape, 1);
                }
                if (settings->phSurvivalIndicatorStyle == 3 && ImGui::CollapsingHeader("Arc##phSurvArc")) {
                    ImGui::Indent(20);
                    ImGui::SliderInt("Sweep##phSurvArc", &settings->phSurvivalArcSweep, 1, 360, "%d deg");
                    ImGui::SliderInt("Rotation##phSurvArc", &settings->phSurvivalArcRotation, 0, 360, "%d deg");
                    ImGui::SliderFloat("Radius##phSurvArc", &settings->phSurvivalArcRadius, 4.0f, 80.0f, "%.1f px");
                    ImGui::SliderFloat("Thickness##phSurvArc", &settings->phSurvivalArcThickness, 0.5f, 12.0f, "%.1f px");
                    ImGui::Unindent(20);
                }

                ImGui::Spacing();
                if (ImGui::SliderInt("Pos X##phSurvivalPosX", &settings->phSurvivalPosX, 0, static_cast<int>(displaySize.x)))
                    PlayerHUDSurvival::ForceReposition();
                if (ImGui::SliderInt("Pos Y##phSurvivalPosY", &settings->phSurvivalPosY, 0, static_cast<int>(displaySize.y)))
                    PlayerHUDSurvival::ForceReposition();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Placement");
                ImGui::SliderInt("Padding X##phSurvPaddingX", &settings->phSurvivalInnerPaddingX,
                                 -1, 40, settings->phSurvivalInnerPaddingX < 0 ? "Default" : "%d px");
                ImGui::SliderInt("Padding Y##phSurvPaddingY", &settings->phSurvivalInnerPaddingY,
                                 -1, 40, settings->phSurvivalInnerPaddingY < 0 ? "Default" : "%d px");
                if (ImGui::CollapsingHeader("Fine Adjustments##phSurvPlacement")) {
                    ImGui::Indent(20);
                    static int selectedElement = 0;
                    const bool available[] = { true, settings->phSurvivalShowLabel,
                                               settings->phSurvivalShowStage, settings->phSurvivalIndicatorStyle != 2,
                                               settings->phSurvivalShowPercentage };
                    if (!available[selectedElement]) selectedElement = 0;
                    const char* elementNames[] = { "Icon", "Label", "Stage", "Indicator", "Percentage" };
                    std::int32_t* offsetX[] = { &settings->phSurvivalIconOffsetX, &settings->phSurvivalLabelOffsetX,
                                               &settings->phSurvivalStageOffsetX, &settings->phSurvivalIndicatorOffsetX,
                                               &settings->phSurvivalPercentageOffsetX };
                    std::int32_t* offsetY[] = { &settings->phSurvivalIconOffsetY, &settings->phSurvivalLabelOffsetY,
                                               &settings->phSurvivalStageOffsetY, &settings->phSurvivalIndicatorOffsetY,
                                               &settings->phSurvivalPercentageOffsetY };
                    for (int i = 0; i < IM_ARRAYSIZE(elementNames); ++i) {
                        if (i > 0 && i != 4) ImGui::SameLine();
                        ImGui::PushID(i);
                        ImGui::BeginDisabled(!available[i]);
                        ImGui::RadioButton(elementNames[i], &selectedElement, i);
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                    DrawPositionSlider("##phSurvElementX", *offsetX[selectedElement], -300, 300, "X %d px");
                    DrawPositionSlider("##phSurvElementY", *offsetY[selectedElement], -300, 300, "Y %d px");
                    if (ImGui::Button("Reset Selected##phSurvPlacement")) {
                        *offsetX[selectedElement] = 0;
                        *offsetY[selectedElement] = 0;
                    }
                    if (settings->phSurvivalIconFill)
                        ImGui::TextDisabled("Icon Fill follows the icon offsets.");
                    ImGui::Unindent(20);
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Items (Show / Hide)");
                ImGui::Spacing();
                ImGui::Checkbox("Label##phSurvShowLabel", &settings->phSurvivalShowLabel);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Show the need name (Cold / Hunger / Fatigue).");
                ImGui::SameLine();
                ImGui::Checkbox("Stage##phSurvShowStage", &settings->phSurvivalShowStage);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Show the stage description (Warm / Starving / etc.).");
                ImGui::SameLine();
                ImGui::Checkbox("Alarm Pulse##phSurvAlarmPulse", &settings->phSurvivalAlarmPulse);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Pulsing alarm ring around a need's card once it reaches its final stage.");
                ImGui::Checkbox("Percentage##phSurvPercentage", &settings->phSurvivalShowPercentage);
                ImGui::SetNextItemWidth(200.0f);
                ImGui::SliderInt("##phSurvivalHideBelowStage", &settings->phSurvivalHideBelowStage, 0, 5, "Auto-Hide Below Stage %d");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Hide cards below the selected stage.\n0 keeps all cards visible.");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Appearance");
                ImGui::Spacing();

                if (ImGui::BeginTable("##PhSurvAppearanceTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##PhSurvColorCol", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("##PhSurvSizeCol",  ImGuiTableColumnFlags_WidthStretch, 0.6f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                        ImVec4 c;
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalBgColor);
                        if (ImGui::ColorEdit4("Card BG##phSurvBg", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalBgColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalLabelColor);
                        if (ImGui::ColorEdit4("Label##phSurvLabel", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalLabelColor = ColorUtils::Vec4ToArgb(c);
                        ImGui::BeginDisabled(!settings->phSurvivalShowBorder);
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalBorderColor);
                        if (ImGui::ColorEdit4("Border Color##phSurvBorderColor", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalBorderColor = ColorUtils::Vec4ToArgb(c);
                        ImGui::EndDisabled();

                        ImGui::Spacing();
                        ImGui::TextDisabled("Need Colors");
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalColdColor);
                        if (ImGui::ColorEdit4("Cold##phSurvCold", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalColdColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalHungerColor);
                        if (ImGui::ColorEdit4("Hunger##phSurvHunger", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalHungerColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalFatigueColor);
                        if (ImGui::ColorEdit4("Fatigue##phSurvFatigue", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalFatigueColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalThirstColor);
                        if (ImGui::ColorEdit4("Thirst##phSurvThirst", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalThirstColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phSurvivalAlarmColor);
                        if (ImGui::ColorEdit4("Final Stage##phSurvAlarm", reinterpret_cast<float*>(&c), kArgb))
                            settings->phSurvivalAlarmColor = ColorUtils::Vec4ToArgb(c);
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSurvivalIconSz", &settings->phSurvivalIconSize, 8, 96, "Icon Size %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSurvivalCardW", &settings->phSurvivalCardWidth, 40, 300, "Card Width %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSurvivalCardSpacing", &settings->phSurvivalCardSpacing, 0, 60, "Card Spacing %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSurvivalBgRadius", &settings->phSurvivalBgCornerRadius, 0, 40, "BG Corner Radius %d px");
                    ImGui::Spacing();
                    {
                        int labelScaleInt = static_cast<int>(settings->phSurvivalLabelScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##phSurvLabelScale", &labelScaleInt, 40, 120, "Label Scale %d%%"))
                            settings->phSurvivalLabelScale = static_cast<float>(labelScaleInt) / 100.0f;
                    }
                    {
                        int stageScaleInt = static_cast<int>(settings->phSurvivalStageScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##phSurvStageScale", &stageScaleInt, 40, 120, "Stage Scale %d%%"))
                            settings->phSurvivalStageScale = static_cast<float>(stageScaleInt) / 100.0f;
                    }
                    if (settings->phSurvivalShowPercentage) {
                        int percentageScaleInt = static_cast<int>(settings->phSurvivalPercentageScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##phSurvPercentageScale", &percentageScaleInt, 30, 200, "Percentage Scale %d%%"))
                            settings->phSurvivalPercentageScale = static_cast<float>(percentageScaleInt) / 100.0f;
                    }
                    ImGui::BeginDisabled(!settings->phSurvivalShowBorder);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderFloat("##phSurvBorderWidth", &settings->phSurvivalBorderWidth, 0.5f, 6.0f, "Border Width %.1f px");
                    ImGui::EndDisabled();

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("BG Depth");
                ImGui::Checkbox("Border##phSurvBorder", &settings->phSurvivalShowBorder);
                ImGui::SameLine();
                ImGui::Checkbox("Texture##phSurvBgTexture", &settings->phSurvivalBgTexture);
                ImGui::SameLine();
                ImGui::Checkbox("Duotone##phSurvBgDuotone", &settings->phSurvivalBgDuotone);

                ImGui::Spacing();
                ImGui::TextDisabled("Indicator Depth");
                ImGui::Checkbox("Shadow##phSurvIndShadow", &settings->phSurvivalIndicatorShadow);
                ImGui::SameLine();
                ImGui::Checkbox("Duotone##phSurvIndDuotone", &settings->phSurvivalIndicatorDuotone);
                ImGui::SameLine();
                ImGui::Checkbox("Glow##phSurvIndGlow", &settings->phSurvivalIndicatorGlow);
                ImGui::SameLine();
                ImGui::Checkbox("Highlight##phSurvIndHighlight", &settings->phSurvivalIndicatorHighlight);

                ImGui::Spacing();
                ImGui::TextDisabled("Text Depth");
                ImGui::Checkbox("Shadow##phSurvTxSh", &settings->phSurvivalTextShadow);
                ImGui::SameLine();
                ImGui::Checkbox("Duotone##phSurvTxDuo", &settings->phSurvivalTextDuotone);
                ImGui::SameLine();
                ImGui::Checkbox("Outline##phSurvTxOl", &settings->phSurvivalTextOutline);
                if (settings->phSurvivalTextOutline) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phSurvTxOlOp", &settings->phSurvivalTextOutlineOpacity, 0, 255, "Outline Opacity %d");
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Icon Depth");
                ImGui::Checkbox("Duotone##phSurvIconDuo", &settings->phSurvivalIconDuotone);
                ImGui::SameLine();
                ImGui::Checkbox("Shadow##phSurvIconSh", &settings->phSurvivalIconShadow);
                ImGui::SameLine();
                ImGui::Checkbox("Glow##phSurvIconGlow", &settings->phSurvivalIconGlow);
                ImGui::SameLine();
                ImGui::Checkbox("Gloss##phSurvIconGloss", &settings->phSurvivalIconGloss);

                ImGui::Checkbox("Outline##phSurvIconOl", &settings->phSurvivalIconOutline);
                if (settings->phSurvivalIconOutline) {
                    ImGui::SameLine();
                    const float swatchW = ImGui::GetFrameHeight();
                    float olSliderW = ImGui::GetContentRegionAvail().x - swatchW - ImGui::GetStyle().ItemSpacing.x;
                    ImGui::SetNextItemWidth(olSliderW < 80.0f ? 80.0f : olSliderW);
                    ImGui::SliderInt("##phSurvIconOlW", &settings->phSurvivalIconOutlineWidth, 1, 3, "Outline Width %d px");
                    ImGui::SameLine();
                    static constexpr ImGuiColorEditFlags kSurvOlArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 oc = ColorUtils::ArgbToVec4(settings->phSurvivalIconOutlineColor);
                    if (ImGui::ColorEdit4("##phSurvIconOlC", reinterpret_cast<float*>(&oc), kSurvOlArgb))
                        settings->phSurvivalIconOutlineColor = ColorUtils::Vec4ToArgb(oc);
                }
            }

            ImGui::Unindent(10);
        }

        bool diseaseOpen = (s_phSel == 5);

        if (diseaseOpen) {
            ImGui::Indent(10);
            ImGui::Spacing();

            ImGui::Checkbox("Enable Widget##phDiseaseEnable", &settings->enablePlayerDiseaseWidget);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shows contracted diseases (Rockjoint, Ataxia, etc.). Empty when healthy.\nSeverity (Severe / Crippling) shows when Survival Mode is active.");

            if (settings->enablePlayerDiseaseWidget) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Presets");
                ImGui::Spacing();
                {
                    static int presetIndex = 0;
                    static constexpr const char* presetNames[] = {
                        "1 - Default", "2 - Compact", "3 - Minimal", "4 - Grouped"
                    };
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo("##phDiseasePreset", &presetIndex, presetNames, 4))
                        PlayerHUDDiseasePresets::ApplyPreset(*settings, static_cast<std::size_t>(presetIndex));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Layout");
                ImGui::Spacing();

                static const char* kStyles[] = { "Standard", "Compact" };
                ImGui::Combo("Style##phDiseaseStyle", &settings->phDiseaseStyle, kStyles, 2);
                static const char* kDirs[] = { "Horizontal", "Vertical" };
                ImGui::Combo("Direction##phDiseaseDir", &settings->phDiseaseDirection, kDirs, 2);

                ImGui::Checkbox("Grouped##phDiseaseGrouped", &settings->phDiseaseGrouped);
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("One card with a total count. Shows the most recently changed disease,\nor the worst one after loading or a cure.");

                ImGui::Checkbox("Idle Collapse##phDiseaseIdleCollapse", &settings->phDiseaseIdleCollapse);
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Hides names and severity after five seconds without changes.\nGrouped: keeps the badge. Individual: keeps small icons.");

                ImGui::Checkbox("Hide in Combat##phDiseaseHideInCombat", &settings->phDiseaseHideInCombat);

                ImGui::Spacing();
                if (ImGui::SliderInt("Pos X##phDiseasePosX", &settings->phDiseasePosX, 0, static_cast<int>(displaySize.x)))
                    PlayerHUDDisease::ForceReposition();
                if (ImGui::SliderInt("Pos Y##phDiseasePosY", &settings->phDiseasePosY, 0, static_cast<int>(displaySize.y)))
                    PlayerHUDDisease::ForceReposition();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Items (Show / Hide)");
                ImGui::Spacing();
                ImGui::Checkbox("Name##phDiseaseShowLabel", &settings->phDiseaseShowLabel);
                ImGui::SameLine();
                ImGui::Checkbox("Severity##phDiseaseShowStage", &settings->phDiseaseShowStage);
                ImGui::SameLine();
                ImGui::Checkbox("Pips##phDiseaseShowPips", &settings->phDiseaseShowPips);
                ImGui::SameLine();
                // Legacy ShowBorder field/key retained for saved presets.
                ImGui::Checkbox("Alarm Pulse##phDiseaseShowBorder", &settings->phDiseaseShowBorder);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Pulse at the highest disease stage.\nUses the card above 1%% BG opacity, otherwise the icon.\nGrouped glows behind the pips instead.");
                ImGui::SameLine();
                ImGui::Checkbox("Magnitude##phDiseaseShowMagnitude", &settings->phDiseaseShowMagnitude);
                ImGui::TextDisabled("Severity and pips require a staged disease backend.");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Appearance");
                ImGui::Spacing();

                if (ImGui::BeginTable("##PhDiseaseAppearanceTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##PhDisColorCol", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("##PhDisSizeCol",  ImGuiTableColumnFlags_WidthStretch, 0.6f);

                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size / Style");

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    {
                        static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                        ImVec4 c;
                        c = ColorUtils::ArgbToVec4(settings->phDiseaseBgColor);
                        if (ImGui::ColorEdit4("Card BG##phDiseaseBg", reinterpret_cast<float*>(&c), kArgb))
                            settings->phDiseaseBgColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phDiseaseIconBgColor);
                        if (ImGui::ColorEdit4("Icon BG##phDiseaseIconBg", reinterpret_cast<float*>(&c), kArgb))
                            settings->phDiseaseIconBgColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phDiseaseLabelColor);
                        if (ImGui::ColorEdit4("Text##phDiseaseLabelColor", reinterpret_cast<float*>(&c), kArgb))
                            settings->phDiseaseLabelColor = ColorUtils::Vec4ToArgb(c);

                        ImGui::Spacing();
                        ImGui::TextDisabled("Severity Colors");
                        c = ColorUtils::ArgbToVec4(settings->phDiseaseColor);
                        if (ImGui::ColorEdit4("Disease##phDiseaseColor", reinterpret_cast<float*>(&c), kArgb))
                            settings->phDiseaseColor = ColorUtils::Vec4ToArgb(c);
                        c = ColorUtils::ArgbToVec4(settings->phDiseaseAlarmColor);
                        if (ImGui::ColorEdit4("Crippling##phDiseaseAlarm", reinterpret_cast<float*>(&c), kArgb))
                            settings->phDiseaseAlarmColor = ColorUtils::Vec4ToArgb(c);
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phDiseaseIconSz", &settings->phDiseaseIconSize, 8, 96, "Icon Size %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phDiseaseCardW", &settings->phDiseaseCardWidth, 40, 300, "Card Width %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phDiseaseCardSpace", &settings->phDiseaseCardSpacing, 0, 60, "Card Spacing %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phDiseaseBgRadius", &settings->phDiseaseBgCornerRadius, 0, 40, "BG Corner Radius %d px");
                    ImGui::Spacing();
                    {
                        int s = static_cast<int>(settings->phDiseaseLabelScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##phDiseaseLabelScale", &s, 40, 120, "Name Scale %d%%"))
                            settings->phDiseaseLabelScale = static_cast<float>(s) / 100.0f;
                    }
                    {
                        int s = static_cast<int>(settings->phDiseaseStageScale * 100);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("##phDiseaseStageScale", &s, 40, 120, "Text Scale %d%%"))
                            settings->phDiseaseStageScale = static_cast<float>(s) / 100.0f;
                    }

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Text Depth");
                ImGui::Checkbox("Shadow##phDiseaseTxSh", &settings->phDiseaseTextShadow);
                ImGui::SameLine();
                ImGui::Checkbox("Duotone##phDiseaseTxDuo", &settings->phDiseaseTextDuotone);
                ImGui::SameLine();
                ImGui::Checkbox("Outline##phDiseaseTxOl", &settings->phDiseaseTextOutline);
                if (settings->phDiseaseTextOutline) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##phDiseaseTxOlOp", &settings->phDiseaseTextOutlineOpacity, 0, 255, "Outline Opacity %d");
                }

                ImGui::Spacing();
                ImGui::TextDisabled("BG Depth");
                ImGui::Checkbox("Border##phDiseaseBgBorder", &settings->phDiseaseBgBorder);
                if (settings->phDiseaseBgBorder) {
                    ImGui::SameLine();
                    static constexpr ImGuiColorEditFlags kDisBorderArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 bc = ColorUtils::ArgbToVec4(settings->phDiseaseBgBorderColor);
                    if (ImGui::ColorEdit4("##phDiseaseBgBorderC", reinterpret_cast<float*>(&bc), kDisBorderArgb))
                        settings->phDiseaseBgBorderColor = ColorUtils::Vec4ToArgb(bc);
                }
                ImGui::SameLine();
                ImGui::Checkbox("Texture##phDiseaseBgTexture", &settings->phDiseaseBgTexture);
                ImGui::SameLine();
                ImGui::Checkbox("Duotone##phDiseaseBgDuotone", &settings->phDiseaseBgDuotone);

                ImGui::Spacing();
                ImGui::TextDisabled("Icon Depth");
                ImGui::Checkbox("Duotone##phDiseaseIconDuo", &settings->phDiseaseIconDuotone);
                ImGui::SameLine();
                ImGui::Checkbox("Shadow##phDiseaseIconSh", &settings->phDiseaseIconShadow);
                ImGui::SameLine();
                ImGui::Checkbox("Glow##phDiseaseIconGlow", &settings->phDiseaseIconGlow);
                ImGui::SameLine();
                ImGui::Checkbox("Gloss##phDiseaseIconGloss", &settings->phDiseaseIconGloss);
                ImGui::SameLine();
                ImGui::Checkbox("Border##phDiseaseIconBorder", &settings->phDiseaseIconBorder);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Box lines and severity ring around the Icon BG.\nTurn off with a transparent Icon BG for a bare icon.");

                ImGui::Checkbox("Outline##phDiseaseIconOl", &settings->phDiseaseIconOutline);
                if (settings->phDiseaseIconOutline) {
                    ImGui::SameLine();
                    const float swatchW = ImGui::GetFrameHeight();
                    float olSliderW = ImGui::GetContentRegionAvail().x - swatchW - ImGui::GetStyle().ItemSpacing.x;
                    ImGui::SetNextItemWidth(olSliderW < 80.0f ? 80.0f : olSliderW);
                    ImGui::SliderInt("##phDiseaseIconOlW", &settings->phDiseaseIconOutlineWidth, 1, 3, "Outline Width %d px");
                    ImGui::SameLine();
                    static constexpr ImGuiColorEditFlags kDiseaseOlArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 oc = ColorUtils::ArgbToVec4(settings->phDiseaseIconOutlineColor);
                    if (ImGui::ColorEdit4("##phDiseaseIconOlC", reinterpret_cast<float*>(&oc), kDiseaseOlArgb))
                        settings->phDiseaseIconOutlineColor = ColorUtils::Vec4ToArgb(oc);
                }
            }

            ImGui::Unindent(10);
        }

        if (s_phSel == 6)
            RenderPlayerHUDEffectsSection(settings, displaySize);

        ImGui::Spacing();
        ImGui::Unindent(10);
    }

    void RenderHUDVisibilitySection(Settings* settings) {
        if (!settings) return;

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 28, 46, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 58, 84, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        bool open = ImGui::CollapsingHeader("HUD Visibility");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (!open) return;

        ImGui::Indent(10);
        ImGui::Spacing();

        ImGui::TextDisabled("Hide widgets during exploration.");
        ImGui::Spacing();

        if (ImGui::Checkbox("Hide Vanilla HP/MP/SP Bars##hvHideVanilla", &settings->hvHideVanillaBars))
            HUDVisibility::ApplyVanillaBarVisibility();
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hides Skyrim's built-in health, magicka, stamina bars,\narrow count, and enchantment charge meters.");

        ImGui::Spacing();
        ImGui::Checkbox("Enable HUD Visibility System##hvEnabled", &settings->hvEnabled);

        if (settings->hvEnabled) {
            ImGui::Spacing();

            int delayInt = static_cast<int>(settings->hvFadeOutDelay);
            if (ImGui::SliderInt("Fade-Out Delay (sec)##hvDelay", &delayInt, 1, 60))
                settings->hvFadeOutDelay = static_cast<float>(delayInt);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("How long a widget stays fully visible after its last trigger");

            ImGui::SliderFloat("Fade Duration (sec)##hvFadeDur", &settings->hvFadeDuration, 0.1f, 3.0f, "%.1f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("How long the fade-out itself takes once the delay expires.\nAlso used by the Player HUD status widget's combat-mode hide.");
        }

        ImGui::Spacing();
        ImGui::Unindent(10);
    }

    void RenderScalingSection(Settings* settings) {
        if (!settings) return;

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 28, 46, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 42, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 58, 84, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        bool open = ImGui::CollapsingHeader("Scaling");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (!open) return;

        ImGui::Indent(10);
        ImGui::Spacing();

        ImGui::TextDisabled("Whole-widget master scaling controls.");
        ImGui::Spacing();

        // 1. HUD Settings Applying this while the slider is active moves the control under the
        static int pendingMenuScale = settings->menuScale;
        ImGui::SliderInt("HUD Settings UI##ScalingMenuScale", &pendingMenuScale, 50, 200, "%d%%");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings->menuScale = pendingMenuScale;
        }

        // 2. Follower HUD
        int hudScale = settings->hudScale;
        if (ImGui::SliderInt("Follower HUD##ScalingHudScale", &hudScale, 50, 200, "%d%%")) {
            settings->hudScale = hudScale;
            FollowerHUD::ForceReposition();
        }

        // 3. Character Sheet
        int csScale = static_cast<int>(settings->csScale * 100);
        if (ImGui::SliderInt("Character Sheet##ScalingCSScale", &csScale, 50, 200, "%d%%")) {
            settings->csScale = static_cast<float>(csScale) / 100.0f;
        }

        int csMenuScale = static_cast<int>(settings->csMenuScale * 100);
        if (ImGui::SliderInt("Character Sheet Menu##ScalingCSMenuScale", &csMenuScale, 50, 200, "%d%%")) {
            settings->csMenuScale = static_cast<float>(csMenuScale) / 100.0f;
        }

        // 4. Inspect Card
        int icScale = static_cast<int>(settings->inspectCardScale * 100);
        if (ImGui::SliderInt("Inspect Card##ScalingICScale", &icScale, 50, 200, "%d%%")) {
            settings->inspectCardScale = static_cast<float>(icScale) / 100.0f;
        }

        // 5. Party Sheet V2
        int psV2Scale = static_cast<int>(settings->psV2Scale * 100);
        if (ImGui::SliderInt("Party Sheet V2##ScalingPSV2Scale", &psV2Scale, 50, 250, "%d%%")) {
            settings->psV2Scale = static_cast<float>(psV2Scale) / 100.0f;
        }

        // 6. Player HUD Resistances
        int prScale = settings->prScale;
        if (ImGui::SliderInt("Player Resistances##ScalingPRScale", &prScale, 50, 200, "%d%%")) {
            settings->prScale = prScale;
            PlayerHUDResistances::ForceReposition();
        }

        // 7. Player Stats
        int phScale = settings->phScale;
        if (ImGui::SliderInt("Player Stats##ScalingPHScale", &phScale, 50, 200, "%d%%")) {
            settings->phScale = phScale;
            PlayerHUD::ForceReposition();
        }

        // 8. Player Equipment
        int peScale = settings->peScale;
        if (ImGui::SliderInt("Player Equipment##ScalingPEScale", &peScale, 50, 200, "%d%%")) {
            settings->peScale = peScale;
            PlayerHUDEquipment::ForceReposition();
        }

        // 9. Player Survival
        int phSurvivalScale = settings->phSurvivalScale;
        if (ImGui::SliderInt("Player Survival##ScalingSurvivalScale", &phSurvivalScale, 50, 200, "%d%%")) {
            settings->phSurvivalScale = phSurvivalScale;
            PlayerHUDSurvival::ForceReposition();
        }

        // 10. Player Disease
        int phDiseaseScale = settings->phDiseaseScale;
        if (ImGui::SliderInt("Player Disease##ScalingDiseaseScale", &phDiseaseScale, 50, 200, "%d%%")) {
            settings->phDiseaseScale = phDiseaseScale;
            PlayerHUDDisease::ForceReposition();
        }

        // 11. Player Active Effects
        int phEffectScale = settings->phEffectScale;
        if (ImGui::SliderInt("Player Active Effects##ScalingEffectScale", &phEffectScale, 50, 200, "%d%%")) {
            settings->phEffectScale = phEffectScale;
            PlayerHUDEffects::ForceReposition();
        }

        // 12. Pinned Items
        int ptScale = settings->ptScale;
        if (ImGui::SliderInt("Pinned Items##ScalingPinnedScale", &ptScale, 50, 200, "%d%%")) {
            settings->ptScale = ptScale;
            WidgetPinnedItems::ForceReposition();
        }

        // 13. Collectibles
        int ccScale = settings->ccScale;
        if (ImGui::SliderInt("Collectibles##ScalingCollectiblesScale", &ccScale, 50, 200, "%d%%")) {
            settings->ccScale = ccScale;
            WidgetCollectibles::ForceReposition();
        }

        // 14. Player Resources
        int presScale = settings->presScale;
        if (ImGui::SliderInt("Player Resources##ScalingResourcesScale", &presScale, 50, 200, "%d%%")) {
            settings->presScale = presScale;
            PlayerHUDResources::ForceReposition();
        }

        // 15. Stable Mount
        int smScale = settings->smScale;
        if (ImGui::SliderInt("Stable Mount##ScalingMountScale", &smScale, 50, 200, "%d%%")) {
            settings->smScale = smScale;
            WidgetStableMount::ForceReposition();
        }

        ImGui::Spacing();
        ImGui::Unindent(10);
    }

}
