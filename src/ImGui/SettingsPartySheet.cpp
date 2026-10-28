#include "../../includes/ImGui/RendererSections.h"

#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/Utils/ColorUtils.h"
#include <imgui.h>

namespace ImGuiRenderer::SettingsUI {
    void RenderPartySheetSection(Settings* settings)
    {
        if (!settings) {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(48, 42, 18, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(66, 58, 26, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(84, 74, 34, 255));
        bool psOpen = ImGui::CollapsingHeader("Party Sheet");
        ImGui::PopStyleColor(3);
        if (!psOpen) return;

        ImGui::Indent(10);

        ImGui::Spacing();
        ImGui::Checkbox("Fullscreen Mode (V2)##PSV2Mode", &settings->psUseFullscreenMode);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("When enabled, the hotkey opens the new fullscreen PartySheetV2 instead of the classic sheet.");
        ImGui::Spacing();
        ImGui::Separator();

        if (settings->psUseFullscreenMode) {
            ImGui::Spacing();
            ImGui::TextDisabled("V2 Font Scales");
            ImGui::Spacing();

            if (ImGui::BeginTable("##PSV2FontTable", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                {
                    int v = static_cast<int>(settings->psV2FontDisplay * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2FontDisplay", &v, 50, 200, "Display %d%%"))
                        settings->psV2FontDisplay = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hero name (large heading)");
                }
                {
                    int v = static_cast<int>(settings->psV2FontHeading * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2FontHeading", &v, 30, 150, "Heading %d%%"))
                        settings->psV2FontHeading = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Section titles: SKILLS, DEFENSE, EQUIPPED, ARMOR");
                }
                ImGui::TableSetColumnIndex(1);
                {
                    int v = static_cast<int>(settings->psV2FontBody * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2FontBody", &v, 50, 150, "Body %d%%"))
                        settings->psV2FontBody = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Item names, stat labels, skill names");
                }
                {
                    int v = static_cast<int>(settings->psV2FontSmall * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2FontSmall", &v, 30, 120, "Small %d%%"))
                        settings->psV2FontSmall = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Badges, slot labels, fine print");
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("V2 Icon Sizes");
            ImGui::Spacing();

            if (ImGui::BeginTable("##PSV2IconTable", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                {
                    int v = static_cast<int>(settings->psV2IconSkill);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconSkill", &v, 12, 48, "Skill %dpx"))
                        settings->psV2IconSkill = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Skill icons in the Skills section");
                }
                {
                    int v = static_cast<int>(settings->psV2IconStat);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconStat", &v, 12, 48, "Stat %dpx"))
                        settings->psV2IconStat = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Defense/Combat stat icons");
                }
                {
                    int v = static_cast<int>(settings->psV2IconCard);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconCard", &v, 16, 64, "Card %dpx"))
                        settings->psV2IconCard = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Central card icons (race, class, faction)");
                }
                ImGui::TableSetColumnIndex(1);
                {
                    int v = static_cast<int>(settings->psV2IconTrait);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconTrait", &v, 16, 64, "Trait %dpx"))
                        settings->psV2IconTrait = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Trait/perk icons in the central panel");
                }
                {
                    int v = static_cast<int>(settings->psV2IconBadge);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconBadge", &v, 12, 48, "Badge %dpx"))
                        settings->psV2IconBadge = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Equipped weapon / armor badge icons");
                }
                {
                    int v = static_cast<int>(settings->psV2IconFooter);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconFooter", &v, 12, 48, "Footer %dpx"))
                        settings->psV2IconFooter = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Footer row icons (gold, carry weight, etc.)");
                }
                {
                    int v = static_cast<int>(settings->psV2IconEquip);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconEquip", &v, 64, 240, "Equip %dpx"))
                        settings->psV2IconEquip = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Weapon card icon width (height scales to match 216x300 ratio)");
                }
                {
                    int v = static_cast<int>(settings->psV2IconArmor);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSV2IconArmor", &v, 64, 200, "Armor %dpx"))
                        settings->psV2IconArmor = static_cast<float>(v);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Armor slot icon size (square)");
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Icon Design");
            ImGui::Spacing();

            ImGui::Checkbox("Duotone##PSV2IconDuo", &settings->psV2IconDuotone);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shade each icon from a lit top to a darker bottom\ninstead of one flat color.");
            ImGui::SameLine();
            ImGui::Checkbox("Shadow##PSV2IconSh", &settings->psV2IconShadow);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Drop shadow + rim light behind each icon.");
            ImGui::SameLine();
            ImGui::Checkbox("Outline##PSV2IconOl", &settings->psV2IconOutline);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Dark contour ring hugging the icon shape.");

            if (settings->psV2IconOutline) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSV2IconOlW", &settings->psV2IconOutlineWidth, 1, 3, "Outline Width %d px");
                static constexpr ImGuiColorEditFlags kOlArgb = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                ImVec4 oc = ColorUtils::ArgbToVec4(settings->psV2IconOutlineColor);
                if (ImGui::ColorEdit4("Outline Color##PSV2IconOlC", reinterpret_cast<float*>(&oc), kOlArgb))
                    settings->psV2IconOutlineColor = ColorUtils::Vec4ToArgb(oc);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Alpha channel = outline opacity.");
            }
        } else {

            ImGui::Spacing();
            ImGui::TextDisabled("Window Size");
            ImGui::Spacing();

            if (ImGui::BeginTable("##PSWindowTable", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSWidth", &settings->partySheetWidth, 800, 2560, "Width %d");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSHeight", &settings->partySheetHeight, 600, 1600, "Height %d");
                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##PSWindowTable2", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSBottomPadding", &settings->psBottomPadding, 0, 120, "Bottom Padding %d");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Extra space at bottom if content clips");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(80);
                if (ImGui::InputInt("Cards/Page##PS", &settings->psCardsPerPage, 1, 1)) {
                    if (settings->psCardsPerPage < 1)  settings->psCardsPerPage = 1;
                    if (settings->psCardsPerPage > 10) settings->psCardsPerPage = 10;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Number of party members shown before pagination (1-10)");
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Backgrounds");
            ImGui::Spacing();

            if (ImGui::BeginTable("##PSBgTable", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Window BG");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Card BG");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (!settings->psUseCustomBackground) {
                    ImVec4 sheetBgCol = ColorUtils::RgbToVec4(settings->partySheetBgColor);
                    if (ImGui::ColorEdit3("##PSWindowBgColor", (float*)&sheetBgCol, ImGuiColorEditFlags_NoInputs))
                        settings->partySheetBgColor = ColorUtils::Vec4ToRgb(sheetBgCol);
                    ImGui::SameLine();
                    int sheetOpacity = static_cast<int>(settings->partySheetBgOpacity * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSWindowBgOpacity", &sheetOpacity, 0, 100, "Opacity %d%%"))
                        settings->partySheetBgOpacity = static_cast<float>(sheetOpacity) / 100.0f;
                }
                ImGui::Checkbox("Use Custom##PSWindow", &settings->psUseCustomBackground);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Use custom window background (PartySheet-X.png)");
                if (settings->psUseCustomBackground) {
                    ImGui::SameLine();
                    RenderBackgroundPicker("PSPickerPopup", settings->psBackgroundIndex, BackgroundManager::Type::Partysheet);
                }

                ImGui::TableSetColumnIndex(1);
                if (!settings->pcUseCustomBackground) {
                    ImVec4 cardBgCol = ColorUtils::RgbToVec4(settings->partyCardBgColor);
                    if (ImGui::ColorEdit3("##PSCardBgColor", (float*)&cardBgCol, ImGuiColorEditFlags_NoInputs))
                        settings->partyCardBgColor = ColorUtils::Vec4ToRgb(cardBgCol);
                    ImGui::SameLine();
                    int cardOpacity = static_cast<int>(settings->partyCardBgOpacity * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSCardBgOpacity", &cardOpacity, 0, 100, "Opacity %d%%"))
                        settings->partyCardBgOpacity = static_cast<float>(cardOpacity) / 100.0f;
                }
                ImGui::Checkbox("Use Custom##PSCard", &settings->pcUseCustomBackground);
                if (settings->pcUseCustomBackground) {
                    ImGui::SameLine();
                    RenderBackgroundPicker("PCPickerPopup", settings->pcBackgroundIndex, BackgroundManager::Type::Partycard);
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Portrait & Bars");
            ImGui::Spacing();

            if (ImGui::BeginTable("##PSBarsTable", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Fill Colors");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Border Colors");
                ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("Size");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->partySheetHealthColor);
                    if (ImGui::ColorEdit3("##PSHealthColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->partySheetHealthColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Health");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->partySheetStaminaColor);
                    if (ImGui::ColorEdit3("##PSStaminaColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->partySheetStaminaColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Stamina");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->partySheetMagickaColor);
                    if (ImGui::ColorEdit3("##PSMagickaColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->partySheetMagickaColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Magicka");
                }

                ImGui::TableSetColumnIndex(1);
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->psBarOuterBorderColor);
                    if (ImGui::ColorEdit3("##PSBarBorderOuter", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->psBarOuterBorderColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Bar Outer");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->psBarInnerBorderColor);
                    if (ImGui::ColorEdit3("##PSBarBorderInner", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->psBarInnerBorderColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Bar Inner");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->psPortraitOuterBorderColor);
                    if (ImGui::ColorEdit3("##PSPortraitOuter", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->psPortraitOuterBorderColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Portrait Outer");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->psPortraitInnerBorderColor);
                    if (ImGui::ColorEdit3("##PSPortraitInner", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->psPortraitInnerBorderColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Portrait Inner");
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSPortraitSize", &settings->psPortraitSize, 64, 256, "Portrait %dpx");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSBarWidth", &settings->psBarWidth, 100, 300, "Bar Width %d");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Controls overall card width");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSBarHeight", &settings->psBarHeight, 12, 40, "Bar Height %d");

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Text & Icons");
            ImGui::Spacing();

            if (ImGui::BeginTable("##PSTextTable", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 12));
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colors");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Font Scales");
                ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("Icon Sizes");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->partySheetNameColor);
                    if (ImGui::ColorEdit3("##PSNameColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->partySheetNameColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Name");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->psLabelColor);
                    if (ImGui::ColorEdit3("##PSLabelColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->psLabelColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine(); ImGui::TextUnformatted("Label");
                }
                {
                    ImVec4 col = ColorUtils::RgbToVec4(settings->psIconBgColor);
                    if (ImGui::ColorEdit3("##PSIconBgColor", (float*)&col, ImGuiColorEditFlags_NoInputs))
                        settings->psIconBgColor = ColorUtils::Vec4ToRgb(col);
                    ImGui::SameLine();
                    int iconOpacity = static_cast<int>(settings->psIconBgOpacity * 100);
                    ImGui::SetNextItemWidth(80.0f);
                    if (ImGui::SliderInt("##PSIconBgOpacity", &iconOpacity, 0, 100, "Icon BG %d%%"))
                        settings->psIconBgOpacity = static_cast<float>(iconOpacity) / 100.0f;
                }

                ImGui::TableSetColumnIndex(1);
                {
                    int v = static_cast<int>(settings->partySheetNameFontScale * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSNameScale", &v, 50, 150, "Name %d%%"))
                        settings->partySheetNameFontScale = static_cast<float>(v) / 100.0f;
                }
                {
                    int v = static_cast<int>(settings->psLabelFontScale * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSLabelScale", &v, 50, 150, "Label %d%%"))
                        settings->psLabelFontScale = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Section headers: Armor/Damage/Move and vertical Equipment/Skills/Resistances text");
                }
                {
                    int v = static_cast<int>(settings->psBodyFontScale * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSBodyScale", &v, 50, 150, "Body %d%%"))
                        settings->psBodyFontScale = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Body text: level/race, stat values, resistance %%, skill values");
                }
                {
                    int v = static_cast<int>(settings->psEquipTextScale * 100);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##PSEquipScale", &v, 50, 150, "Equip %d%%"))
                        settings->psEquipTextScale = static_cast<float>(v) / 100.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Text scale for item names in equipment slots");
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSIconSize", &settings->psStatIconSize, 16, 48, "Icon %dpx");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("All icons: equipment, stats, skills, resistances");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt("##PSIconBgSize", &settings->psStatIconBgSize, 24, 64, "Icon BG %dpx");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Background box behind icons");

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            {
                float portraitSize = static_cast<float>(settings->psPortraitSize);
                float barHeight    = static_cast<float>(settings->psBarHeight);
                float iconSize     = static_cast<float>(settings->psStatIconSize);
                float iconBgSize   = static_cast<float>(settings->psStatIconBgSize);
                float padding      = 26.0f;
                float fontScale    = static_cast<float>(settings->fontSize) / 18.0f;
                float labelHeight  = 16.0f * fontScale;
                float bottomPad    = static_cast<float>(settings->psBottomPadding);
                float barWidthVal  = static_cast<float>(settings->psBarWidth);

                float cardWidth  = padding * 2 + portraitSize + 10.0f + barWidthVal;
                float cardHeight = padding + 12.0f + bottomPad
                    + portraitSize + 8.0f
                    + barHeight * 3 + 12.0f
                    + labelHeight + (iconSize + 6.0f) * 4 + 8.0f
                    + labelHeight + iconBgSize * 2 + 12.0f
                    + labelHeight + (iconSize + 6.0f) * 2 + 8.0f
                    + labelHeight + (iconBgSize + 8.0f) * 3;

                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Card Size: %.0fx%.0f", cardWidth, cardHeight);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Use this resolution for custom bg-party.png");
            }
        }

        ImGui::Unindent(10);
    }
}
