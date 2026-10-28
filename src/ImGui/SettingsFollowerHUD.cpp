#include "../../includes/ImGui/RendererSections.h"

#include "../../includes/Settings.h"
#include "../../includes/FollowerHUDPresets.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/QuickHeal.h"
#include "../../includes/InputHandler.h"
#include "../../includes/Utils/ColorUtils.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <Windows.h>

namespace ImGuiRenderer {
    void RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type);
}

namespace ImGuiRenderer::SettingsUI {

    void RenderFollowerHUDSection(Settings* settings, const ImVec2& displaySize)
    {
        if (!settings) {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(26, 56, 48, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(36, 74, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(46, 92, 80, 255));
        bool fhOpen = ImGui::CollapsingHeader("Follower HUD");
        ImGui::PopStyleColor(3);
        if (fhOpen) {
            ImGui::Indent(10);

            ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(18, 38, 32, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(26, 56, 48, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(36, 74, 64, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

            // ── Enable toggle (maps to followerHudMode == 0) ──
            static int s_lastNonZeroMode = 1;
            bool fhEnabled = settings->followerHudMode != 0;
            if (settings->followerHudMode != 0)
                s_lastNonZeroMode = settings->followerHudMode;
            if (ImGui::Checkbox("Enable Follower HUD", &fhEnabled)) {
                settings->followerHudMode = fhEnabled ? s_lastNonZeroMode : 0;
                settings->Save();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static int selectedPreset = 0;
            if (selectedPreset < 0 || selectedPreset >= static_cast<int>(FollowerHUDPresets::kPresets.size())) {
                selectedPreset = 0;
            }
            ImGui::TextDisabled("Preset Styles");
            const char* presetNames[] = {
                FollowerHUDPresets::kPresets[0].name,
                FollowerHUDPresets::kPresets[1].name,
                FollowerHUDPresets::kPresets[2].name,
                "4- Minimal (Halo)",
                FollowerHUDPresets::kPresets[4].name
            };
            if (ImGui::Combo("##FollowerHudPresets", &selectedPreset, presetNames, static_cast<int>(FollowerHUDPresets::kPresets.size()))) {
                FollowerHUDPresets::ApplyPreset(*settings, static_cast<std::size_t>(selectedPreset));
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Applies immediately for preview.\nClick Save Settings to persist.");
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::CollapsingHeader("Behavior")) {
                ImGui::Indent(20);

                ImGui::TextDisabled("Mode");

                const char* hudModes[] = { "Always", "Combat Only" };
                int modeIndex = (settings->followerHudMode > 0 && settings->followerHudMode <= 2) ? settings->followerHudMode - 1 : 0;
                if (ImGui::Combo("HUD Mode", &modeIndex, hudModes, 2))
                    settings->followerHudMode = modeIndex + 1;

                if (settings->followerHudMode == 2) {
                    ImGui::SliderInt("Fade Delay##hudFadeDelay", &settings->fadeDelay, 0, 30, "%d s");
                    if (settings->fadeDelay < 0) settings->fadeDelay = 0;
                    if (settings->fadeDelay > 30) settings->fadeDelay = 30;
                }

                {
                    bool stackEligible = (settings->followerHudMode == 1) && settings->showPortrait;
                    if (!stackEligible) ImGui::BeginDisabled();
                    ImGui::Checkbox("Idle Portrait Stack", &settings->hudIdlePortraitStack);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Out of combat with all followers unhurt for a few seconds,\nthe party collapses to overlapping portraits (a coin stack);\nunstacks instantly when combat starts or anyone is hurt.\nRequires Show Portrait; Always mode only.");
                    }
                    if (!stackEligible) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(settings->followerHudMode != 1
                                ? "Only available in Always mode."
                                : "Requires Show Portrait to be enabled.");
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Show");

                if (ImGui::BeginTable("BehaviorToggles", 3, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Checkbox("Summons", &settings->showSummons)) {
                        FollowerHUD::MarkSummonsDirty();
                    }
                    ImGui::TableNextColumn(); ImGui::Checkbox("Followers", &settings->showFollowers);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Quest Allies", &settings->showQuestAllies);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Detect temporary quest companions.\nOnly shows while in combat with them.");
                    }
                    ImGui::EndTable();
                }

                {
                    bool summonsEnabled = settings->showSummons;
                    if (!summonsEnabled) ImGui::BeginDisabled();
                    ImGui::Indent(20);
                    if (ImGui::Checkbox("Follower Summons", &settings->showFollowerSummons)) {
                        FollowerHUD::MarkSummonsDirty();
                    }
                    if (ImGui::IsItemHovered() && summonsEnabled) {
                        ImGui::SetTooltip("Also include summons commanded by your followers\n(e.g. a follower's conjured familiar or atronach).");
                    }
                    if (ImGui::Checkbox("Separate Summons Window", &settings->hudSeparateSummons))
                        FollowerHUD::ForceReposition();
                    if (ImGui::IsItemHovered() && summonsEnabled)
                        ImGui::SetTooltip("Move visible summons to a separate window.\nDrag it while HUD Settings is open, then Save Settings.\nUses the same layout and styling as followers.");
                    if (!settings->hudSeparateSummons) ImGui::BeginDisabled();
                    ImGui::Checkbox("Hide Managed Summon Effects", &settings->hudHideManagedSummonEffects);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hide summon entries from player and follower HUD effects\nonly while their summons have visible countdown borders.\nUnmatched or hidden summons keep their effects visible.");
                    if (!settings->hudSeparateSummons) ImGui::EndDisabled();
                    ImGui::Unindent(20);
                    if (!summonsEnabled) ImGui::EndDisabled();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Limits");

                ImGui::SliderInt("Max Actors##hudMaxActors", &settings->maxFollowers, 1, 30);
                if (settings->maxFollowers < 1) settings->maxFollowers = 1;
                if (settings->maxFollowers > 30) settings->maxFollowers = 30;

                ImGui::Unindent(20);
            }

            if (ImGui::CollapsingHeader("Layout")) {
                ImGui::Indent(20);
                ImGui::TextDisabled("Orientation");
                const char* layouts[] = { "Vertical", "Horizontal" };
                if (ImGui::Combo("Orientation##hudLayout", &settings->hudLayout, layouts, 2))
                    FollowerHUD::ForceReposition();

                ImGui::SliderInt("Wrap After##hudWrap", &settings->maxPerColumnOrRow, 1, 30);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Wrap to next column (vertical) or row (horizontal)\nafter this many followers.");

                ImGui::SliderInt("Group Gap##hudGroupGap", &settings->hudGroupGap, 0, 100, "%d%%");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%% of screen between groups. Best with 2 columns or 2 rows.\nNote: the gap area is part of the HUD window and will block\nclicks on Settings sliders behind it.");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Position");

                bool isVertical = (settings->hudLayout == 0);
                if (ImGui::Checkbox("Grow Upward", &settings->hudGrowUpward))
                    FollowerHUD::OnGrowUpwardToggled();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(isVertical
                        ? "Anchor the HUD's bottom edge at Y Position so new\ncards appear above existing ones."
                        : "Anchor the HUD's bottom edge at Y Position so new\nwrapped rows appear above existing ones.");
                }

                ImGui::SameLine();
                if (ImGui::Checkbox("Anchor Right", &settings->hudAnchorRight))
                    FollowerHUD::ForceReposition();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("X Position becomes the HUD's RIGHT edge. The first\nfollower stays pinned and new columns grow to the left.");

                if (ImGui::SliderInt("X Position##Hud", &settings->hudPosX, 0, static_cast<int>(displaySize.x)))
                    FollowerHUD::ForceReposition();
                if (ImGui::SliderInt("Y Position##Hud", &settings->hudPosY, 0, static_cast<int>(displaySize.y)))
                    FollowerHUD::ForceReposition();
                ImGui::TextDisabled("Tip: drag the HUD while Settings is open.");
                if (settings->hudSeparateSummons) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Summon Window");
                    if (ImGui::Checkbox("Grow Upward##SummonHUD", &settings->hudSummonGrowUpward))
                        FollowerHUD::OnGrowUpwardToggled(true);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Anchor the summon window's bottom edge.\nNew summons or wrapped rows appear above existing ones.");
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Anchor Right##SummonHUD", &settings->hudSummonAnchorRight))
                        FollowerHUD::ForceReposition();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Anchor the summon window's right edge.\nNew columns or cards appear to the left.");
                }
                ImGui::Unindent(20);
            }
            if (ImGui::CollapsingHeader("Name & Level")) {
                ImGui::Indent(20);

                int nameScale = static_cast<int>(settings->nameFontScale * 100);
                const char* alignments[] = { "Left", "Center", "Right" };

                ImVec4 nameCol = ColorUtils::RgbToVec4(settings->nameColor);
                ImVec4 levelNameCol = ColorUtils::RgbToVec4(settings->levelNameColor);
                ImVec4 shadowCol = ColorUtils::RgbToVec4(settings->textShadowColor);
                int shadowOpacity = static_cast<int>(settings->textShadowOpacity * 100);

                ImGui::TextDisabled("Name");

                ImGui::Checkbox("Show Name", &settings->showName);
                ImGui::SameLine();
                ImGui::Checkbox("First Word Only", &settings->nameFirstWordOnly);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Show only the first word of follower name\n(e.g. 'Lydia' instead of 'Lydia the Fearless')");
                }
                ImGui::SameLine();
                ImGui::Checkbox("Uppercase##Name", &settings->nameUppercase);

                ImGui::SetNextItemWidth(-40.0f);
                if (ImGui::SliderInt("##NameFont", &nameScale, 50, 150, "Name %d%%")) {
                    settings->nameFontScale = static_cast<float>(nameScale) / 100.0f;
                }
                ImGui::SameLine();
                if (ImGui::ColorEdit3("##NameColor", (float*)&nameCol, ImGuiColorEditFlags_NoInputs)) {
                    settings->nameColor = ColorUtils::Vec4ToRgb(nameCol);
                }

                ImGui::Combo("Alignment##NameAlign", &settings->textAlignment, alignments, 3);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Name Alignment");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Level");

                ImGui::Checkbox("Show Level (Name)", &settings->showLevel);
                ImGui::SameLine();
                ImGui::Checkbox("Uppercase##Level", &settings->levelUppercase);

                if (ImGui::ColorEdit3("Level Color##LevelColor", (float*)&levelNameCol, ImGuiColorEditFlags_NoInputs)) {
                    settings->levelNameColor = ColorUtils::Vec4ToRgb(levelNameCol);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Text Shadow");

                ImGui::Checkbox("Enable##TextShadow", &settings->textShadowEnabled);
                if (settings->textShadowEnabled) {
                    ImGui::SameLine();
                    if (ImGui::ColorEdit3("##ShadowColor", (float*)&shadowCol, ImGuiColorEditFlags_NoInputs)) {
                        settings->textShadowColor = ColorUtils::Vec4ToRgb(shadowCol);
                    }

                    ImGui::SetNextItemWidth(90);
                    ImGui::SliderInt("##ShadowX", &settings->textShadowOffsetX, -5, 5, "X %d");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90);
                    ImGui::SliderInt("##ShadowY", &settings->textShadowOffsetY, -5, 5, "Y %d");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-40.0f);
                    if (ImGui::SliderInt("##ShadowOpacity", &shadowOpacity, 0, 100, "Opacity %d%%")) {
                        settings->textShadowOpacity = static_cast<float>(shadowOpacity) / 100.0f;
                    }
                }

                ImGui::Unindent(20);
            }

            if (ImGui::CollapsingHeader("Badges  (Direction, Level, State)")) {
                ImGui::Indent(20);

                if (ImGui::BeginTable("BadgesShowHide", 3, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show/Hide");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Direction Arrow##bShowArrow", &settings->showDirectionArrow);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Distance (m)##bShowDist",    &settings->showDistanceMeters);
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("Level Badge##bShowLevel",    &settings->levelBadgeOnPortrait);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("State Badge##bShowState",     &settings->showStateBadge);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Arrow BG Pill##bShowArrowBg", &settings->hudArrowShowBg);
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("Badge FX##bBadgeFx",          &settings->hudBadgeFx);
                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Direction & Distance");
                if (!settings->showDirectionArrow && !settings->showDistanceMeters) ImGui::BeginDisabled();
                {
                    static constexpr ImGuiColorEditFlags kBadgeArgb =
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 bgCol = ColorUtils::ArgbToVec4(settings->hudArrowBgColor);
                    if (ImGui::ColorEdit4("Background##ArrowBg", reinterpret_cast<float*>(&bgCol), kBadgeArgb))
                        settings->hudArrowBgColor = ColorUtils::Vec4ToArgb(bgCol);
                    ImGui::SameLine();
                    ImVec4 arrowCol = ColorUtils::RgbToVec4(settings->hudArrowTint);
                    if (ImGui::ColorEdit3("Arrow/Text##ArrowTint", reinterpret_cast<float*>(&arrowCol), ImGuiColorEditFlags_NoInputs))
                        settings->hudArrowTint = ColorUtils::Vec4ToRgb(arrowCol);
                    ImGui::SameLine();
                    ImGui::Checkbox("Show Border##ArrowBorder", &settings->hudArrowShowBorder);

                    if (settings->showDirectionArrow) {
                        int arrowScale = static_cast<int>(settings->hudArrowScale * 100);
                        if (ImGui::SliderInt("Arrow Size##ArrowSz", &arrowScale, 10, 50, "%d%%"))
                            settings->hudArrowScale = arrowScale / 100.0f;
                        ImGui::SliderInt("Arrow Offset X##ArrowOX", &settings->hudArrowOffsetX, -50, 50, "%dpx");
                        ImGui::SliderInt("Arrow Offset Y##ArrowOY", &settings->hudArrowOffsetY, -50, 50, "%dpx");
                    }
                    if (settings->showDistanceMeters) {
                        int distScale = static_cast<int>(settings->hudDistanceFontScale * 100);
                        if (ImGui::SliderInt("Distance Scale##DistSc", &distScale, 50, 200, "%d%%"))
                            settings->hudDistanceFontScale = distScale / 100.0f;
                        ImGui::SliderInt("Dist Offset X##DistOX", &settings->hudDistanceOffsetX, -80, 80, "%dpx");
                        ImGui::SliderInt("Dist Offset Y##DistOY", &settings->hudDistanceOffsetY, -80, 50, "%dpx");
                    }
                }
                if (!settings->showDirectionArrow && !settings->showDistanceMeters) ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Level Badge");
                if (!settings->levelBadgeOnPortrait) ImGui::BeginDisabled();
                {
                    static constexpr ImGuiColorEditFlags kBadgeArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 badgeBgCol = ColorUtils::ArgbToVec4(settings->hudBadgeBgColor);
                    if (ImGui::ColorEdit4("Background##LevelBadgeBg", reinterpret_cast<float*>(&badgeBgCol), kBadgeArgb))
                        settings->hudBadgeBgColor = ColorUtils::Vec4ToArgb(badgeBgCol);
                    ImGui::SameLine();
                    ImVec4 levelCol = ColorUtils::RgbToVec4(settings->levelColor);
                    if (ImGui::ColorEdit3("Level Text##LevelBadgeColor", reinterpret_cast<float*>(&levelCol), ImGuiColorEditFlags_NoInputs))
                        settings->levelColor = ColorUtils::Vec4ToRgb(levelCol);
                    ImGui::SameLine();
                    ImGui::Checkbox("Show Border##LevelBadgeBorder", &settings->hudLevelBadgeShowBorder);

                    int badgeScale = static_cast<int>(settings->hudBadgeFontScale * 100);
                    if (ImGui::SliderInt("Font##LevelBadge", &badgeScale, 50, 150, "%d%%"))
                        settings->hudBadgeFontScale = static_cast<float>(badgeScale) / 100.0f;

                    ImGui::SliderInt("Offset X##LevelBadgeOX", &settings->hudLevelBadgeOffsetX, -50, 50, "%dpx");
                    ImGui::SliderInt("Offset Y##LevelBadgeOY", &settings->hudLevelBadgeOffsetY, -50, 50, "%dpx");
                }
                if (!settings->levelBadgeOnPortrait) ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("State Badge");
                if (!settings->showStateBadge) ImGui::BeginDisabled();
                {
                    static constexpr ImGuiColorEditFlags kStateBadgeArgb =
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 stateBgCol = ColorUtils::ArgbToVec4(settings->hudStateBadgeBgColor);
                    if (ImGui::ColorEdit4("Background##StateBadgeBg", reinterpret_cast<float*>(&stateBgCol), kStateBadgeArgb))
                        settings->hudStateBadgeBgColor = ColorUtils::Vec4ToArgb(stateBgCol);
                    ImGui::SameLine();
                    ImVec4 stateIconCol = ColorUtils::RgbToVec4(settings->hudStateBadgeIconTint);
                    if (ImGui::ColorEdit3("Icon Hue##StateBadgeIcon", reinterpret_cast<float*>(&stateIconCol), ImGuiColorEditFlags_NoInputs))
                        settings->hudStateBadgeIconTint = ColorUtils::Vec4ToRgb(stateIconCol);
                    ImGui::SameLine();
                    ImGui::Checkbox("Show Border##StateBadgeBorder", &settings->hudStateBadgeShowBorder);

                    int stBadgeScale = static_cast<int>(settings->hudStateBadgeScale * 100);
                    if (ImGui::SliderInt("State Scale##StateBadgeSz", &stBadgeScale, 10, 50, "%d%%"))
                        settings->hudStateBadgeScale = stBadgeScale / 100.0f;

                    ImGui::SliderInt("Offset X##StateBadgeOX", &settings->hudStateBadgeOffsetX, -50, 50, "%dpx");
                    ImGui::SliderInt("Offset Y##StateBadgeOY", &settings->hudStateBadgeOffsetY, -50, 50, "%dpx");
                }
                if (!settings->showStateBadge) ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Corner Radius");
                {
                    int r = settings->hudBadgeRadius;
                    if (ImGui::SliderInt("All Badges##BadgeRadius", &r, 0, 16, "%d px"))
                        settings->hudBadgeRadius = r;
                }

                ImGui::Unindent(20);
            }

            if (ImGui::CollapsingHeader("Bars")) {
                ImGui::Indent(20);
                const char* rightEndStyles[] = {
                    "Right End - Normal",
                    "Right End - Slant Forward",
                    "Right End - Slant Backward",
                    "Right End - Arrow",
                    "Right End - Diamond"
                };
                const char* leftEndStyles[] = {
                    "Left End - Normal",
                    "Left End - Slant Forward",
                    "Left End - Slant Backward",
                    "Left End - Arrow",
                    "Left End - Diamond"
                };
                const char* visualStyles[] = { "Flat", "Gradient", "Glass", "Segmented", "None" };

                float barScale = settings->barTextFontScale;
                int compactPercent = static_cast<int>(settings->compactBarRatio * 100);
                int outerOpacity = static_cast<int>(settings->barOuterBorderOpacity * 100);
                int innerOpacity = static_cast<int>(settings->barInnerBorderOpacity * 100);

                bool showGradient = (settings->barVisualStyle == 1 || settings->barVisualStyle == 2);

                ImVec4 hpCol = ColorUtils::RgbToVec4(settings->healthColor);
                ImVec4 hpGradCol = ColorUtils::RgbToVec4(settings->healthGradientColor);
                ImVec4 mpCol = ColorUtils::RgbToVec4(settings->magickaColor);
                ImVec4 mpGradCol = ColorUtils::RgbToVec4(settings->magickaGradientColor);
                ImVec4 spCol = ColorUtils::RgbToVec4(settings->staminaColor);
                ImVec4 spGradCol = ColorUtils::RgbToVec4(settings->staminaGradientColor);
                ImVec4 btCol = ColorUtils::RgbToVec4(settings->barTextColor);
                ImVec4 borderCol = ColorUtils::RgbToVec4(settings->barBorderColor);
                ImVec4 innerBorderCol = ColorUtils::RgbToVec4(settings->fhBarInnerBorderColor);

                if (ImGui::BeginTable("BarsShowHide", 3, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show/Hide");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Compact Bars", &settings->compactBars);

                    ImGui::TableSetColumnIndex(1);
                    if (!settings->barStyleEnabled) {
                        ImGui::Checkbox("Glow Effect", &settings->barGlowEnabled);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Add an outer glow around the bars (Best for flat bar)");
                        }
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Health", &settings->showHealth);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Show Magicka", &settings->showMagicka);
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("Show Stamina", &settings->showStamina);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Bar Text", &settings->showTextOnBars);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Halo", &settings->hudHaloEnable);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Replace the bars with a resource ring around the portrait");
                    }
                    ImGui::EndTable();
                }

                ImGui::BeginDisabled(settings->hudHaloEnable);
                ImGui::Checkbox("Custom Bar Style##fhBarStyleEnable", &settings->barStyleEnabled);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use custom images for bars");
                if (settings->barStyleEnabled) {
                    ImGui::SameLine();
                    RenderBackgroundPicker("BarStylePicker", settings->barStyleIndex, BackgroundManager::Type::BarStyle);
                }
                ImGui::EndDisabled();

                ImGui::Spacing();

                if (ImGui::BeginTable("BarsStyleTable", 2, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("colors", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                    ImGui::TableSetupColumn("style", ImGuiTableColumnFlags_WidthStretch, 0.65f);
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Size");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::ColorEdit3("Bar Text##BarsColor", (float*)&btCol, ImGuiColorEditFlags_NoInputs)) {
                        settings->barTextColor = ColorUtils::Vec4ToRgb(btCol);
                    }
                    if (ImGui::ColorEdit3("Health##BarsColor", (float*)&hpCol, ImGuiColorEditFlags_NoInputs)) {
                        settings->healthColor = ColorUtils::Vec4ToRgb(hpCol);
                    }
                    if (ImGui::ColorEdit3("Magicka##BarsColor", (float*)&mpCol, ImGuiColorEditFlags_NoInputs)) {
                        settings->magickaColor = ColorUtils::Vec4ToRgb(mpCol);
                    }
                    if (ImGui::ColorEdit3("Stamina##BarsColor", (float*)&spCol, ImGuiColorEditFlags_NoInputs)) {
                        settings->staminaColor = ColorUtils::Vec4ToRgb(spCol);
                    }
                    if (!settings->barStyleEnabled && showGradient) {
                        if (ImGui::ColorEdit3("Health End##BarsColor", (float*)&hpGradCol, ImGuiColorEditFlags_NoInputs)) {
                            settings->healthGradientColor = ColorUtils::Vec4ToRgb(hpGradCol);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Health Gradient End Color");
                        }
                        if (ImGui::ColorEdit3("Magicka End##BarsColor", (float*)&mpGradCol, ImGuiColorEditFlags_NoInputs)) {
                            settings->magickaGradientColor = ColorUtils::Vec4ToRgb(mpGradCol);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Magicka Gradient End Color");
                        }
                        if (ImGui::ColorEdit3("Stamina End##BarsColor", (float*)&spGradCol, ImGuiColorEditFlags_NoInputs)) {
                            settings->staminaGradientColor = ColorUtils::Vec4ToRgb(spGradCol);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Stamina Gradient End Color");
                        }
                    }

                    ImGui::TableSetColumnIndex(1);

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderFloat("Bar Text Scale##Bars", &barScale, 0.5f, 1.5f, "Bar Text Scale %.2f"))
                        settings->barTextFontScale = barScale;
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("Bar Width##Bars", &settings->hudBarWidth, 50, 300, "Bar Width %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("Bar Height##Bars", &settings->hudBarHeight, 8, 30, "Bar Height %d px");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("Bar Spacing##Bars", &settings->barSpacing, 0, 10, "Bar Spacing %d");
                    if (settings->compactBars) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderInt("MP/SP Ratio %##Bars", &compactPercent, 30, 70, "MP/SP Ratio %d%%"))
                            settings->compactBarRatio = static_cast<float>(compactPercent) / 100.0f;
                    }

                    ImGui::EndTable();
                }

                if (!settings->barStyleEnabled) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::Text("Build Your Own Bar");
                    ImGui::Spacing();

                    ImGui::TextDisabled("Fill");
                    ImGui::Combo("Style##fhVisualStyle", &settings->barVisualStyle, visualStyles, 5);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Flat: Solid color\nGradient: Left-to-right fade\nGlass: adds a top sheen\nSegmented: divider lines by stat points\nNone: Text and borders only\nAll styles support end shapes and the depth passes");
                    if (settings->barVisualStyle == 3) {
                        ImGui::SliderInt("Points per Segment##hudBarSegPts", &settings->hudBarSegmentPoints, 25, 500);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Stat points per segment cell; segment count = max stat / this value");
                        ImGui::SliderInt("Segment Gap##hudBarSegGap", &settings->hudBarSegmentGap, 0, 10);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Gap carved between segments (px); 0 = hairline notch lines");
                    }
                    ImGui::SliderFloat("Radius##Bars", &settings->barCornerRadius, 0.0f, 15.0f, "%.1f");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Depth & Light");
                    ImGui::Checkbox("Track##Bars", &settings->barTrackEnabled);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Dark recessed well behind the fill (empty part of the bar)");
                    ImGui::SameLine();
                    ImGui::Checkbox("Depth##Bars", &settings->barDepthShading);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Vertical shading on the fill: lit top edge, deep bottom");
                    ImGui::SameLine();
                    ImGui::Checkbox("Edge Light##Bars", &settings->barEdgeLightEnabled);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Brightness that follows the current value's edge");
                    ImGui::SameLine();
                    ImGui::Checkbox("Tip##Bars", &settings->barTipEnabled);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hot bright strip right at the fill edge");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Shape");
                    ImGui::Combo("Right End##Bars", &settings->barEndStyle, rightEndStyles, 5);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Shape of right bar ending:\nNormal: Standard rectangle\nSlant Forward/Back: Diagonal cut\nArrow: Pointed chevron\nDiamond: Diamond point");
                    ImGui::Combo("Left End##Bars", &settings->barEndStyleLeft, leftEndStyles, 5);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Shape of left bar ending:\nNormal: Standard rectangle\nSlant Forward/Back: Diagonal cut\nArrow: Pointed chevron\nDiamond: Diamond point");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Motion");
                    ImGui::Checkbox("Smooth Drain##Bars", &settings->barSmoothDrain);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Damage drains the bar smoothly (~250ms) so the lit edge travels;\nheals still fill near-instantly");
                    ImGui::SameLine();
                    ImGui::Checkbox("Damage Ghost##Bars", &settings->barDamageGhost);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Pale segment lingers at the pre-hit health, then drains away\n(health bar only, same as the Player HUD ghost)");

                    ImGui::Spacing();
                    ImGui::TextDisabled("Border");
                    const float barRightPad = 8.0f;
                    const float borderSwatchW = ImGui::GetFrameHeight();
                    const float borderRowSliderW = (ImGui::GetContentRegionAvail().x - borderSwatchW - barRightPad - ImGui::GetStyle().ItemSpacing.x * 2.0f) * 0.5f;

                    if (ImGui::ColorEdit3("##OuterBorderColor", (float*)&borderCol, ImGuiColorEditFlags_NoInputs))
                        settings->barBorderColor = ColorUtils::Vec4ToRgb(borderCol);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(borderRowSliderW);
                    ImGui::SliderInt("##OuterWidth", &settings->barOuterBorderWidth, 0, 10, "Outer Width %d px");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(borderRowSliderW);
                    if (ImGui::SliderInt("##OuterOpacity", &outerOpacity, 0, 100, "Outer Opacity %d%%"))
                        settings->barOuterBorderOpacity = static_cast<float>(outerOpacity) / 100.0f;

                    if (ImGui::ColorEdit3("##InnerBorderColor", (float*)&innerBorderCol, ImGuiColorEditFlags_NoInputs))
                        settings->fhBarInnerBorderColor = ColorUtils::Vec4ToRgb(innerBorderCol);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(borderRowSliderW);
                    ImGui::SliderInt("##InnerWidth", &settings->barInnerBorderWidth, 0, 10, "Inner Width %d px");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(borderRowSliderW);
                    if (ImGui::SliderInt("##InnerOpacity", &innerOpacity, 0, 100, "Inner Opacity %d%%"))
                        settings->barInnerBorderOpacity = static_cast<float>(innerOpacity) / 100.0f;
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Text("Halo");
                if (settings->hudHaloEnable) {
                    ImGui::Indent(20);
                    ImGui::TextDisabled("Follows the Show Health/Magicka/Stamina toggles and bar colors above");
                    ImGui::SliderInt("Health Share##hudHaloHealthShare", &settings->hudHaloHealthShare, 50, 90, "%d%%");
                    ImGui::SliderInt("Thickness##hudHaloThickness", &settings->hudHaloThickness, 0, 12,
                                     settings->hudHaloThickness == 0 ? "Auto" : "%d px");
                    ImGui::SliderInt("Segment Gap##hudHaloGap", &settings->hudHaloGap, 0, 10, "%d deg");
                    ImGui::SliderInt("Ring Offset##hudHaloOffset", &settings->hudHaloOffset, 0, 12, "%d px");
                    ImGui::Unindent(20);
                }
                ImGui::Unindent(20);
            }

            if (ImGui::CollapsingHeader("Portrait")) {
                ImGui::Indent(20);
                if (ImGui::BeginTable("PortraitShowHide", 1, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show/Hide");
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Portrait", &settings->showPortrait);
                    ImGui::SameLine();
                    ImGui::Checkbox("Light FX##Portrait", &settings->hudPortraitFx);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Lighting on the portrait: cinematic shading, top light,\nbottom shade, edge vignette, rim light");
                    ImGui::SameLine();
                    ImGui::Checkbox("Backlight##Portrait", &settings->hudPortraitBacklight);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Glow behind the portrait: halo past the frame edges plus\nan inner light that halos the character on dark art");
                    if (settings->hudPortraitBacklight) {
                        ImGui::SameLine();
                        ImVec4 blCol = ColorUtils::RgbToVec4(settings->hudPortraitBacklightColor);
                        if (ImGui::ColorEdit3("Hue##PortraitBacklight", (float*)&blCol, ImGuiColorEditFlags_NoInputs))
                            settings->hudPortraitBacklightColor = ColorUtils::Vec4ToRgb(blCol);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Portrait Fills Ribbon", &settings->hudPortraitFillsRibbon);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Portrait fills the whole ribbon as a background.\nName and bars draw on top. Shape no longer applies.");
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Hit Shake##FHPortrait", &settings->hudPortraitHitShake);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Briefly shakes the portrait horizontally when taking a hit");
                    ImGui::SameLine();
                    ImGui::Checkbox("Hit Flash/Pulse##FHPortrait", &settings->hudPortraitHitFlash);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Red flash on the portrait when taking an unblocked hit.\n"
                                          "Also controls the pulsing red low-health warning below 80%% HP\n"
                                          "(faster with a border below 25%%).");
                    ImGui::EndTable();
                }

                ImGui::Spacing();

                if (ImGui::BeginTable("PortraitStyles", 1, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Styles");
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    ImGui::SliderInt("Portrait Size", &settings->portraitSize, 48, 256);

                    const char* alignments[] = { "Left", "Top (Card Style)" };
                    ImGui::Combo("Alignment", &settings->portraitAlignment, alignments, 2);

                    if (settings->portraitAlignment == 0) {
                        ImGui::SliderInt("Right Margin", &settings->portraitSpacing, 0, 24);

                        const char* vAligns[] = { "Top", "Center", "Bottom" };
                        ImGui::Combo("Content V-Align", &settings->contentVAlign, vAligns, 3);
                    }

                    if (settings->portraitAlignment == 1) {
                        const char* hAligns[] = { "Left", "Center", "Right" };
                        ImGui::Combo("Horizontal Align", &settings->portraitHAlign, hAligns, 3);
                        ImGui::SliderInt("Bottom Margin", &settings->portraitBottomMargin, 0, 24);
                    }

                    const char* shapeOptions[] = { "Square", "Rounded", "Circle", "Diamond" };
                    ImGui::BeginDisabled(settings->hudPortraitFillsRibbon);
                    ImGui::Combo("Shape##Portrait", &settings->portraitShape, shapeOptions, 4);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Square: Uses Corner Radius\nRounded: Auto-rounded corners\nCircle: Fully circular\nDiamond: Rotated square");
                    }

                    if (settings->portraitShape == 0) {
                        ImGui::SliderFloat("Corner Radius", &settings->portraitCornerRadius, 0.0f, 48.0f, "%.0f");
                    }
                    ImGui::EndDisabled();

                    if (settings->hudPortraitFillsRibbon) {
                        ImGui::SliderFloat("Zoom##PortraitFill", &settings->hudPortraitZoom, 1.0f, 3.0f, "%.2f");
                        ImGui::SliderFloat("Focus X##PortraitFill", &settings->hudPortraitFocusU, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Focus Y##PortraitFill", &settings->hudPortraitFocusV, 0.0f, 1.0f, "%.2f");
                    } else {
                        ImGui::SliderFloat("Zoom##PortraitShape", &settings->hudPortraitShapeZoom, 1.0f, 3.0f, "%.2f");
                        ImGui::SliderFloat("Focus X##PortraitShape", &settings->hudPortraitShapeFocusU, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Focus Y##PortraitShape", &settings->hudPortraitShapeFocusV, 0.0f, 1.0f, "%.2f");
                    }

                    ImGui::Spacing();
                    ImGui::Text("Portrait Borders");
                    ImGui::SliderInt("Outer Width##Portrait", &settings->fhPortraitOuterBorderWidth, 0, 10);
                    ImGui::SliderInt("Inner Width##Portrait", &settings->fhPortraitInnerBorderWidth, 0, 10);
                    if (settings->fhPortraitOuterBorderWidth > 0 || settings->fhPortraitInnerBorderWidth > 0) {
                        ImVec4 pOuterCol = ColorUtils::RgbToVec4(settings->fhPortraitOuterBorderColor);
                        if (ImGui::ColorEdit3("Outer##PortraitBorder", (float*)&pOuterCol, ImGuiColorEditFlags_NoInputs)) {
                            settings->fhPortraitOuterBorderColor = ColorUtils::Vec4ToRgb(pOuterCol);
                        }
                        ImGui::SameLine();
                        ImVec4 pInnerCol = ColorUtils::RgbToVec4(settings->fhPortraitInnerBorderColor);
                        if (ImGui::ColorEdit3("Inner##PortraitBorder", (float*)&pInnerCol, ImGuiColorEditFlags_NoInputs)) {
                            settings->fhPortraitInnerBorderColor = ColorUtils::Vec4ToRgb(pInnerCol);
                        }
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent(20);
            }

            // ═══ ACTIVE EFFECTS ═══
            static bool loggedEffectHeaderID = false;
            if (!loggedEffectHeaderID) {
                logger::debug("[SettingsIDs] Follower effects header={:08X}, unscoped effects label={:08X}",
                    ImGui::GetID("Active Effects##FollowerHUDEffects"), ImGui::GetID("Active Effects"));
                loggedEffectHeaderID = true;
            }
            if (ImGui::CollapsingHeader("Active Effects##FollowerHUDEffects")) {
                ImGui::Indent(20);
                if (ImGui::BeginTable("ActiveEffectsShowHide", 3, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Show/Hide");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Debug Effect List", &settings->showActiveEffects);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Active Effect Icons", &settings->showEffectIcons);
                    ImGui::TableSetColumnIndex(2); ImGui::Checkbox("Show Timer##Effects", &settings->showEffectTimer);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Show Magnitude##Effects", &settings->showEffectMagnitude);
                    ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Show Source##Effects", &settings->showEffectSourceBadge);
                    ImGui::EndTable();
                }

                ImGui::Spacing();

                if (ImGui::BeginTable("ActiveEffectsStyles", 1, ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Styles");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    ImGui::InputInt("Max Effects Shown", &settings->maxEffectsShown);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Max no of active effects displayed on screen at once.");
                    }
                    if (settings->maxEffectsShown < 1) settings->maxEffectsShown = 1;
                    if (settings->maxEffectsShown > 16) settings->maxEffectsShown = 16;

                    ImGui::SliderInt("Icon Size", &settings->effectIconSize, 24, 64);
                    ImGui::SliderInt("Icon Spacing", &settings->effectIconSpacing, 0, 20);
                    ImGui::SliderInt("Icon Vert Spacing", &settings->effectIconVertSpacing, -10, 20);
                    ImGui::SliderInt("Max Per Column", &settings->maxEffectIconsPerColumn, 1, 10);

                    ImGui::SliderInt("Min Effect Duration (s)", &settings->effectMinDuration, 0, 60);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Hide effects shorter than this (seconds)\n0 = disabled");
                    }

                    ImGui::SliderInt("Max Effect Duration (m)", &settings->effectMaxDuration, 0, 600);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Hide effects longer than this (minutes)\n0 = disabled");
                    }

                    bool isHorizontalLayout = (settings->hudLayout == 1);
                    if (isHorizontalLayout) {
                        const char* placementModes[] = { "Right", "Bottom", "Top", "Overlay" };
                        int displayIndex = 0;
                        switch (settings->effectIconPlacement) {
                            case 0: displayIndex = 0; break;
                            case 1: displayIndex = 1; break;
                            case 3: displayIndex = 2; break;
                            case 2: displayIndex = 3; break;
                            default: displayIndex = 0; break;
                        }
                        if (ImGui::Combo("Icon Placement", &displayIndex, placementModes, 4)) {
                            switch (displayIndex) {
                                case 0: settings->effectIconPlacement = 0; break;
                                case 1: settings->effectIconPlacement = 1; break;
                                case 2: settings->effectIconPlacement = 3; break;
                                case 3: settings->effectIconPlacement = 2; break;
                            }
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Right: Icons to the right of ribbon\nBottom: Icons below ribbon (horizontal only)\nTop: Icons above ribbon (horizontal only)\nOverlay: Icons on portrait");
                        }
                    } else {
                        if (settings->effectIconPlacement == 1 || settings->effectIconPlacement == 3) {
                            settings->effectIconPlacement = 0;
                        }
                        const char* placementModes[] = { "Right", "Overlay" };
                        int adjustedPlacement = (settings->effectIconPlacement == 2) ? 1 : 0;
                        if (ImGui::Combo("Icon Placement", &adjustedPlacement, placementModes, 2)) {
                            settings->effectIconPlacement = (adjustedPlacement == 1) ? 2 : 0;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Right: Icons to the right of ribbon\nOverlay: Icons on portrait\n(Bottom/Top only in horizontal layout)");
                        }
                    }

                    if (settings->effectIconPlacement == 2) {
                        const char* anchors[] = { "Left", "Right" };
                        ImGui::Combo("Overlay Anchor", &settings->overlayAnchor, anchors, 2);
                        ImGui::SliderInt("Offset X", &settings->overlayOffsetX, -100, 100);
                        ImGui::SliderInt("Offset Y", &settings->overlayOffsetY, -100, 100);
                    }

                    if (settings->showEffectTimer) {
                        const char* timerStyles[] = { "Round", "Vertical", "Background Fill" };
                        ImGui::SetNextItemWidth(140.0f);
                        ImGui::Combo("Timer Style", &settings->effectTimerStyle, timerStyles, 3);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80.0f);
                        ImGui::SliderInt("Timer Border", &settings->effectTimerBorderWidth, 1, 6);

                        static constexpr ImGuiColorEditFlags kArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                        ImVec4 bc = ColorUtils::ArgbToVec4(settings->effectBuffColor);
                        if (ImGui::ColorEdit4("Buff##fhEffBuffCol", reinterpret_cast<float*>(&bc), kArgb))
                            settings->effectBuffColor = ColorUtils::Vec4ToArgb(bc);
                        ImGui::SameLine();
                        ImVec4 dc = ColorUtils::ArgbToVec4(settings->effectDebuffColor);
                        if (ImGui::ColorEdit4("Debuff##fhEffDebuffCol", reinterpret_cast<float*>(&dc), kArgb))
                            settings->effectDebuffColor = ColorUtils::Vec4ToArgb(dc);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80.0f);
                        ImGui::SliderInt("Bg Radius##fhEffBgRad", &settings->effectIconBorderRadius, 0, 100);
                    }

                    int effectTextPercent = static_cast<int>(settings->effectTextScale * 100);
                    if (ImGui::SliderInt("Effect List Text", &effectTextPercent, 50, 150)) {
                        settings->effectTextScale = static_cast<float>(effectTextPercent) / 100.0f;
                    }

                    if (settings->showEffectMagnitude) {
                        int effectMagPercent = static_cast<int>(settings->effectMagnitudeScale * 100);
                        if (ImGui::SliderInt("Magnitude Text", &effectMagPercent, 50, 150)) {
                            settings->effectMagnitudeScale = static_cast<float>(effectMagPercent) / 100.0f;
                        }
                    }

                    ImGui::SliderInt("Icon Bg Padding##fhEffBgPad", &settings->effectIconBgPadding, 0, 16);

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Icon Depth");
                ImGui::Checkbox("Duotone##fhEffIconDuo", &settings->hudEffectIconDuotone);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Shade each icon from a lit top to a darker bottom\ninstead of one flat color.");
                ImGui::SameLine();
                ImGui::Checkbox("Shadow##fhEffIconSh", &settings->hudEffectIconShadow);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Drop shadow + rim light behind each icon.");
                ImGui::SameLine();
                ImGui::Checkbox("Outline##fhEffIconOl", &settings->hudEffectIconOutline);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Dark contour ring hugging the icon shape.");

                ImGui::Checkbox("Glow##fhEffIconGlow", &settings->hudEffectIconGlow);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Soft white bloom behind the icon, adding depth.");
                ImGui::SameLine();
                ImGui::Checkbox("Gloss##fhEffIconGloss", &settings->hudEffectIconGloss);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("White sheen across the icon's top half,\nfor a polished look.");

                if (settings->hudEffectIconOutline) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::SliderInt("##fhEffIconOlW", &settings->hudEffectIconOutlineWidth, 1, 3, "Outline Width %d px");
                    static constexpr ImGuiColorEditFlags kOlArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                    ImVec4 oc = ColorUtils::ArgbToVec4(settings->hudEffectIconOutlineColor);
                    if (ImGui::ColorEdit4("Outline Color##fhEffIconOlC", reinterpret_cast<float*>(&oc), kOlArgb))
                        settings->hudEffectIconOutlineColor = ColorUtils::Vec4ToArgb(oc);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Alpha channel = outline opacity.");
                }
                ImGui::Unindent(20);
            }

            if (ImGui::CollapsingHeader("Ribbon")) {
                ImVec4 ribbonBgCol = ColorUtils::RgbToVec4(settings->ribbonBgColor);
                if (ImGui::ColorEdit3("Background##Ribbon", (float*)&ribbonBgCol, ImGuiColorEditFlags_NoInputs)) {
                    settings->ribbonBgColor = ColorUtils::Vec4ToRgb(ribbonBgCol);
                }
                ImGui::SameLine();
                int opacityPercent = static_cast<int>(settings->ribbonBgOpacity * 100);
                ImGui::SetNextItemWidth(100);
                if (ImGui::SliderInt("Opacity %##Ribbon", &opacityPercent, 0, 100)) {
                    settings->ribbonBgOpacity = static_cast<float>(opacityPercent) / 100.0f;
                }

                ImGui::SliderFloat("Border Radius##Ribbon", &settings->ribbonCornerRadius, 0.0f, 50.0f, "%.1f");
                ImGui::SliderInt("Border Width##Ribbon", &settings->ribbonBorderWidth, 0, 10);

                ImVec4 borderCol = ColorUtils::RgbToVec4(settings->ribbonBorderColor);
                if (ImGui::ColorEdit3("Border Color##Ribbon", (float*)&borderCol, ImGuiColorEditFlags_NoInputs)) {
                    settings->ribbonBorderColor = ColorUtils::Vec4ToRgb(borderCol);
                }

                ImGui::SliderInt("Padding Top", &settings->ribbonPaddingTop, 0, 80);
                ImGui::SliderInt("Padding Right", &settings->ribbonPaddingRight, 0, 150);
                ImGui::SliderInt("Padding Bottom", &settings->ribbonPaddingBottom, 0, 80);
                ImGui::SliderInt("Padding Left", &settings->ribbonPaddingLeft, 0, 150);
                ImGui::SliderInt("Ribbon Spacing", &settings->ribbonMargin, -40, 40);
                ImGui::SliderInt("Effect Clearance", &settings->ribbonEffectClearance, 0, 90);
            }

            if (ImGui::CollapsingHeader("Quick Heal")) {
                ImGui::Checkbox("Enable Quick Heal", &settings->qhEnabled);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Press the Quick Heal hotkey to cast your assigned heal spell\n"
                                      "on the lowest-HP follower below the threshold.");

                if (!settings->qhEnabled) ImGui::BeginDisabled();

                const auto& allSpells = QuickHeal::GetAllSpells();

                std::vector<const QuickHeal::HealSpellEntry*> visible;
                for (const auto& e : allSpells) {
                    if (!e.enabled) continue;
                    if (QuickHeal::PlayerKnowsSpell(e)) visible.push_back(&e);
                }

                int selectedIdx = -1;
                {
                    std::string assignedMod;
                    if (!settings->qhSpellMod.empty()) {
                        const int needed = WideCharToMultiByte(CP_UTF8, 0,
                            settings->qhSpellMod.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (needed > 1) {
                            assignedMod.resize(needed - 1);
                            WideCharToMultiByte(CP_UTF8, 0, settings->qhSpellMod.c_str(), -1,
                                assignedMod.data(), needed, nullptr, nullptr);
                        }
                    }
                    for (size_t i = 0; i < visible.size(); ++i) {
                        if (visible[i]->mod == assignedMod &&
                            visible[i]->localFormID == settings->qhSpellFormID) {
                            selectedIdx = static_cast<int>(i);
                            break;
                        }
                    }
                }

                const char* preview = (selectedIdx >= 0)
                    ? visible[selectedIdx]->label.c_str()
                    : (visible.empty() ? "(no compatible spells learned)" : "(none selected)");

                ImGui::SetNextItemWidth(220);
                if (ImGui::BeginCombo("Assigned Heal Spell", preview)) {
                    for (size_t i = 0; i < visible.size(); ++i) {
                        const auto* e = visible[i];
                        const bool selected = (static_cast<int>(i) == selectedIdx);
                        std::string label = e->label;
                        if (e->castType == QuickHeal::CastType::Concentration)
                            label += "  [Channel — not yet supported]";
                        else if (e->castType == QuickHeal::CastType::AoE)
                            label += "  [AoE — heals self + nearby allies]";
                        if (ImGui::Selectable(label.c_str(), selected)) {
                            int wlen = MultiByteToWideChar(CP_UTF8, 0, e->mod.c_str(), -1, nullptr, 0);
                            if (wlen > 0) {
                                settings->qhSpellMod.assign(wlen - 1, L'\0');
                                MultiByteToWideChar(CP_UTF8, 0, e->mod.c_str(), -1,
                                    settings->qhSpellMod.data(), wlen);
                            }
                            settings->qhSpellFormID = e->localFormID;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Reload##qhJson")) {
                    QuickHeal::ReloadFromJson();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Re-read PartySheet_HealSpells.json without restarting.");

                ImGui::TextDisabled("%zu of %zu allowlisted spells learned",
                                    visible.size(), allSpells.size());

                ImGui::SliderInt("Min HP % to Trigger", &settings->qhMinHpPercent, 1, 100);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Only target allies whose HP%% is below this value.");

                ImGui::SliderInt("Max Range (units)", &settings->qhMaxRange, 100, 4096);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Skyrim units. ~70 units = 1 metre.");

                ImGui::SliderInt("Magicka Floor %%", &settings->qhMagickaFloorPercent, 0, 90);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Refuse to cast if your magicka %% is below this.\n0 = always allow.");

                ImGui::Checkbox("Show Heal Banner on Focused Target", &settings->qhShowBadgeOnFocus);
                if (settings->qhShowBadgeOnFocus) {
                    ImGui::Checkbox("Hide if Cast Unavailable##banner", &settings->qhHideBadgeIfUnavailable);
                    ImGui::SliderFloat("Font Scale##banner", &settings->qhBannerFontScale, 0.5f, 2.0f, "%.2f");
                    ImGui::SliderInt("Offset X##banner",    &settings->qhBannerOffsetX, -50, 50);
                    ImGui::SliderInt("Offset Y##banner",    &settings->qhBannerOffsetY, -50, 50);
                }

                if (!settings->qhEnabled) ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::TextDisabled("Portrait Flash");
                ImGui::Checkbox("Heal Flash##qhHealFlash", &settings->qhHealFlash);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Green flash and small pulse on a follower's portrait when they are healed.\n"
                                      "Turn off if an aura or regen mod keeps healing followers.");
                ImGui::SameLine();
                ImGui::Checkbox("Buff Flash##qhBuffFlash", &settings->qhBuffFlash);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Orange flash and small pulse on a follower's portrait when they receive\n"
                                      "a non-hostile spell that is not a heal (e.g. Bestow Gift).");
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::Unindent(10);
        }
    }

}
