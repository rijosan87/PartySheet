#include "../../includes/ImGui/CharacterSheet.h"
#include "../../includes/PerfStats.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/ImGui/PartySheet.h"
#include "../../includes/ImGui/PartySheetV2.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/ActorStats.h"
#include "../../includes/PlayerStatus.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/BadgeStyle.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/SpellIconManager.h"
#include "../../includes/Utils/HUDDraw.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>

namespace logger = SKSE::log;

static int s_statusCarouselPage = 0;
static float s_statusCarouselTimer = 0.0f;
static constexpr int CAROUSEL_ITEMS_PER_PAGE = 3;
static constexpr int CAROUSEL_ITEMS_FACTION = 2;
static constexpr int CAROUSEL_ITEMS_CHAMPION = 3;
static constexpr int CAROUSEL_ITEMS_THANE = 2;

static HUDDraw::IconFx IconFxDefault() {
    HUDDraw::IconFx fx{};
    fx.duotone      = true;
    fx.shadow       = true;
    fx.outline      = true;
    fx.outlineWidth = 1;
    fx.glow         = false;
    fx.gloss        = false;
    return fx;
}

static void DrawTextWithShadow(ImDrawList* drawList, ImVec2 pos, ImU32 textColor, const char* text,
                                ImU32 shadowColor = IM_COL32(0, 0, 0, 120), float shadowOffset = 1.0f) {
    if (Settings::GetSingleton()->csTextShadowEnabled) {
        drawList->AddText(ImVec2(pos.x + shadowOffset, pos.y + shadowOffset), shadowColor, text);
        if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
    }
    drawList->AddText(pos, textColor, text);
}

static void DrawNeonRect(ImDrawList* drawList, ImVec2 min, ImVec2 max, 
                         ImU32 glowColor, float borderThickness = 1.5f, 
                         int glowLayers = 3) {
    int r = (glowColor >> 0) & 0xFF;
    int g = (glowColor >> 8) & 0xFF;
    int b = (glowColor >> 16) & 0xFF;
    
    for (int i = glowLayers; i >= 1; i--) {
        float padding = i * 2.5f;
        int alpha = 25 + (glowLayers - i) * 30;
        ImU32 layerColor = IM_COL32(r, g, b, alpha);
        drawList->AddRect(
            ImVec2(min.x - padding, min.y - padding),
            ImVec2(max.x + padding, max.y + padding),
            layerColor, 0.0f, 0, borderThickness
        );
    }
    
    drawList->AddRect(min, max, IM_COL32(r, g, b, 200), 0.0f, 0, borderThickness);
}

static void DrawArrowBorderRect(ImDrawList* drawList, ImVec2 min, ImVec2 max, 
                                 float cornerNotch, ImU32 color, float thickness = 1.5f) {
    
    ImVec2 points[] = {
        ImVec2(min.x + cornerNotch, min.y),
        ImVec2(max.x - cornerNotch, min.y),
        ImVec2(max.x, min.y + cornerNotch),
        ImVec2(max.x, max.y - cornerNotch),
        ImVec2(max.x - cornerNotch, max.y),
        ImVec2(min.x + cornerNotch, max.y),
        ImVec2(min.x, max.y - cornerNotch),
        ImVec2(min.x, min.y + cornerNotch),
    };
    
    drawList->AddPolyline(points, 8, color, true, thickness);
}

static void DrawNeonArrowRect(ImDrawList* drawList, ImVec2 min, ImVec2 max, 
                               float cornerNotch, ImU32 glowColor, 
                               float borderThickness = 1.5f, int glowLayers = 3) {
    int r = (glowColor >> 0) & 0xFF;
    int g = (glowColor >> 8) & 0xFF;
    int b = (glowColor >> 16) & 0xFF;
    
    for (int i = glowLayers; i >= 1; i--) {
        float padding = i * 2.5f;
        int alpha = 25 + (glowLayers - i) * 30;
        ImU32 layerColor = IM_COL32(r, g, b, alpha);
        DrawArrowBorderRect(drawList, 
                           ImVec2(min.x - padding, min.y - padding),
                           ImVec2(max.x + padding, max.y + padding),
                           cornerNotch + padding * 0.5f, layerColor, borderThickness);
    }
    
    DrawArrowBorderRect(drawList, min, max, cornerNotch, IM_COL32(r, g, b, 200), borderThickness);
}

static void DrawDiamondFrame(ImDrawList* drawList, ImVec2 center, float size, ImU32 borderColor, ImU32 fillColor, float borderThickness = 2.0f)
{
    ImVec2 top(center.x, center.y - size);
    ImVec2 right(center.x + size, center.y);
    ImVec2 bottom(center.x, center.y + size);
    ImVec2 left(center.x - size, center.y);
    
    drawList->AddQuadFilled(top, right, bottom, left, fillColor);
    
    drawList->AddQuad(top, right, bottom, left, borderColor, borderThickness);
}

static void DrawDiamondWithItem(ImDrawList* drawList, ImVec2 center, float size,
                                 ID3D11ShaderResourceView* icon, const char* label,
                                 bool hasItem,
                                 float wrapWidth = 150.0f,
                                 float baseValue = 0.0f,
                                 float skillBonus = 0.0f, float fortifyBonus = 0.0f,
                                 float enchantDamage = 0.0f, std::string enchantType = "",
                                 std::string skillIconName = "none",
                                 std::string animKey = "",
                                 float scale = 1.0f,
                                 bool isSquareIcon = false)
{
    float animAlpha = 1.0f;
    float animOffsetY = 0.0f;
    if (!animKey.empty()) {
        UIAnimation::CheckAndAnimateChange(animKey, label ? label : "");
        animAlpha = UIAnimation::GetAlpha(animKey);
        ImVec2 offset = UIAnimation::GetOffset(animKey);
        animOffsetY = offset.y;
    }
    
    auto* slotSettings = Settings::GetSingleton();
    uint8_t br  = (slotSettings->csDiamondBorderColor >> 16) & 0xFF;
    uint8_t bg_ = (slotSettings->csDiamondBorderColor >> 8)  & 0xFF;
    uint8_t bb  =  slotSettings->csDiamondBorderColor        & 0xFF;
    uint8_t ba  = (slotSettings->csDiamondBorderColor >> 24) & 0xFF;
    uint8_t fr  = (slotSettings->csDiamondFillColor >> 16) & 0xFF;
    uint8_t fg  = (slotSettings->csDiamondFillColor >> 8)  & 0xFF;
    uint8_t fb  =  slotSettings->csDiamondFillColor        & 0xFF;
    uint8_t fa  = (slotSettings->csDiamondFillColor >> 24) & 0xFF;
    uint8_t er  = (slotSettings->csDiamondEmptyColor >> 16) & 0xFF;
    uint8_t eg  = (slotSettings->csDiamondEmptyColor >> 8)  & 0xFF;
    uint8_t eb  =  slotSettings->csDiamondEmptyColor        & 0xFF;
    uint8_t ea  = (slotSettings->csDiamondEmptyColor >> 24) & 0xFF;
    uint8_t gr = (slotSettings->csGlowColor >> 16) & 0xFF;
    uint8_t gg = (slotSettings->csGlowColor >> 8) & 0xFF;
    uint8_t gb = slotSettings->csGlowColor & 0xFF;
    float glowScale = 1.0f;

    ImU32 borderColor = hasItem ? IM_COL32(br, bg_, bb, ba) : IM_COL32(er, eg, eb, ea);
    ImU32 fillColor = IM_COL32(fr, fg, fb, fa);

    if (hasItem && slotSettings->csGlowEnabled) {
        DrawDiamondFrame(drawList, center, size + 8.0f * scale, IM_COL32(gr, gg, gb, static_cast<int>(30 * glowScale)), IM_COL32(0, 0, 0, 0), 2.0f * scale);
        DrawDiamondFrame(drawList, center, size + 5.0f * scale, IM_COL32(gr, gg, gb, static_cast<int>(60 * glowScale)), IM_COL32(0, 0, 0, 0), 2.0f * scale);
        DrawDiamondFrame(drawList, center, size + 2.0f * scale, IM_COL32(gr, gg, gb, static_cast<int>(100 * glowScale)), IM_COL32(0, 0, 0, 0), 2.0f * scale);
    }
    
    DrawDiamondFrame(drawList, center, size, borderColor, fillColor, 2.0f * scale);
    
    if (icon) {
        float iconWidth, iconHeight;
        if (isSquareIcon) {
            iconWidth  = size * 1.8f;
            iconHeight = iconWidth;
        } else {
            iconHeight = size * 2.5f;
            iconWidth  = iconHeight * (216.0f / 300.0f);
        }
        float diamondBottom = center.y + size;
        ImVec2 iconMin(center.x - iconWidth / 2, diamondBottom - iconHeight + animOffsetY);
        ImVec2 iconMax(center.x + iconWidth / 2, diamondBottom + animOffsetY);
        ImU32 iconTint = IM_COL32(255, 255, 255, static_cast<int>(255 * animAlpha));
        drawList->AddImage((ImTextureID)icon, iconMin, iconMax, ImVec2(0, 0), ImVec2(1, 1), iconTint);
    }
    
        if (hasItem && slotSettings->csShowBadges) {
        ImFont* badgeFont = ImGui::GetFont();
        float badgeFontSize = ImGui::GetFontSize() * slotSettings->csBadgeTextScale;

        auto DrawArrowBadge = [&](ImVec2 attachPoint, const char* valueText, ImU32 borderColor, ImU32 fillColor, bool isBottomRight = false) {
            ImVec2 textSize = badgeFont->CalcTextSizeA(badgeFontSize, FLT_MAX, 0.0f, valueText);
            float width = textSize.x + 11.0f * scale;
            float height = 18.0f * scale;
            float slant = 18.0f * scale;
            
            ImVec2 p1, p2, p3, p4;
            
            if (isBottomRight) {
                // Bottom badges: / slant on both sides ┌────────┐
                p1 = ImVec2(attachPoint.x + slant, attachPoint.y);
                p2 = ImVec2(attachPoint.x + width + slant, attachPoint.y);
                p3 = ImVec2(attachPoint.x + width, attachPoint.y + height);
                p4 = ImVec2(attachPoint.x, attachPoint.y + height);
            } else {
                // Top badges: \ slant on both sides ┌────────┐
                p1 = ImVec2(attachPoint.x, attachPoint.y);
                p2 = ImVec2(attachPoint.x + width, attachPoint.y);
                p3 = ImVec2(attachPoint.x + width + slant, attachPoint.y + height);
                p4 = ImVec2(attachPoint.x + slant, attachPoint.y + height);
            }
            
            ImVec2 points[] = {p1, p2, p3, p4};
            drawList->AddConvexPolyFilled(points, 4, fillColor);
            
            drawList->AddPolyline(points, 4, borderColor, true, 1.5f * scale);
            
            float leftPadding = -3.0f * scale;
            
            float textX = attachPoint.x + slant + leftPadding;
            
            ImVec2 textPos(textX, attachPoint.y + (height - textSize.y) / 2.0f - 1.0f);
            if (Settings::GetSingleton()->csTextShadowEnabled) {
                drawList->AddText(badgeFont, badgeFontSize, ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 120), valueText);
                if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
            }
            drawList->AddText(badgeFont, badgeFontSize, textPos, IM_COL32(255, 255, 255, 255), valueText);
        };
        
        
        float blueX = center.x + 14.0f * scale;
        float blueY = center.y - 40.0f * scale;
        
        float greenX = center.x + 33.0f * scale;
        float greenY = center.y - 20.0f * scale;
        
        float baseX = center.x + 16.0f * scale;
        float baseY = center.y + 20.0f * scale;
        
        float orangeX = center.x + 35.0f * scale;
        float orangeY = center.y;
        
        if (skillBonus > 0.9f) {
            char valText[16];
            snprintf(valText, sizeof(valText), "%.1f", skillBonus);
            
            DrawArrowBadge(ImVec2(blueX, blueY), valText,
                          IM_COL32(100, 180, 255, 200),
                          IM_COL32(40, 70, 100, 220),
                          false);
        }
        
        if (fortifyBonus > 0.9f) {
            char valText[16];
            snprintf(valText, sizeof(valText), "%.0f%%", fortifyBonus);
            
            DrawArrowBadge(ImVec2(greenX, greenY), valText,
                          IM_COL32(80, 200, 80, 200),
                          IM_COL32(30, 80, 30, 220),
                          false);
        }
        
        if (baseValue > 0.1f) {
            char valText[16];
            snprintf(valText, sizeof(valText), "%.0f", baseValue);
            
            bool isWeaponSkill = (skillIconName == "onehanded" || skillIconName == "twohanded" || 
                                  skillIconName == "archery" || skillIconName == "bow" ||
                                  skillIconName == "destruction" || skillIconName == "magic");
            bool isWeapon = isWeaponSkill;
            auto baseColors = BadgeStyle::GetBaseValueColors(isWeapon);
            
            DrawArrowBadge(ImVec2(baseX, baseY), valText, baseColors.border, baseColors.fill, true);
        }
        
        if (enchantDamage > 0.5f) {
            char valText[16];
            
            auto enchantCategory = BadgeStyle::ClassifyEnchant(enchantType);
            auto enchantColors = BadgeStyle::GetEnchantColors(enchantCategory);
            
            switch (enchantCategory) {
                case BadgeStyle::EnchantCategory::SkillPoints:
                case BadgeStyle::EnchantCategory::FortifyStats:
                case BadgeStyle::EnchantCategory::DamageFire:
                case BadgeStyle::EnchantCategory::DamageFrost:
                case BadgeStyle::EnchantCategory::DamageShock:
                case BadgeStyle::EnchantCategory::DamagePoison:
                    snprintf(valText, sizeof(valText), "+%.0f", enchantDamage);
                    break;
                case BadgeStyle::EnchantCategory::FortifySkill:
                case BadgeStyle::EnchantCategory::Regen:
                case BadgeStyle::EnchantCategory::Resist:
                    snprintf(valText, sizeof(valText), "%.0f%%", enchantDamage);
                    break;
                default:
                    snprintf(valText, sizeof(valText), "%.0f", enchantDamage);
                    break;
            }
            
            
            if (enchantCategory == BadgeStyle::EnchantCategory::SkillPoints) {
                if (skillBonus < 0.9f) {
                    DrawArrowBadge(ImVec2(blueX, blueY), valText, enchantColors.border, enchantColors.fill, false);
                }
            } else if (enchantCategory == BadgeStyle::EnchantCategory::FortifyStats ||
                       enchantCategory == BadgeStyle::EnchantCategory::FortifySkill ||
                       enchantCategory == BadgeStyle::EnchantCategory::Regen) {
                if (fortifyBonus < 0.9f) {
                    DrawArrowBadge(ImVec2(greenX, greenY), valText, enchantColors.border, enchantColors.fill, false);
                }
            } else {
                DrawArrowBadge(ImVec2(orangeX, orangeY), valText, enchantColors.border, enchantColors.fill, true);
            }
        }
    }
    
    if (label) {
        ImFont* font = ImGui::GetFont();
        float baseFontSize = ImGui::GetFontSize();
        float fontSize = baseFontSize * slotSettings->csSlotFontScale;

        auto CalcSize = [&](const char* s, const char* e = nullptr) {
            return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, s, e);
        };
        auto AddTextScaled = [&](ImVec2 pos, ImU32 col, const char* s, const char* e = nullptr) {
            drawList->AddText(font, fontSize, pos, col, s, e);
        };

        ImVec2 const textSize = CalcSize(label);
        uint8_t lr = (slotSettings->csSlotLabelColor >> 16) & 0xFF;
        uint8_t lg_ = (slotSettings->csSlotLabelColor >> 8) & 0xFF;
        uint8_t lb = slotSettings->csSlotLabelColor & 0xFF;
        ImU32 textColor = hasItem ? IM_COL32(lr, lg_, lb, 255) : IM_COL32(100, 100, 100, 200);
        float textStartY = center.y + size + 8.0f * scale;

        if (textSize.x <= wrapWidth) {
            ImVec2 textPos(center.x - textSize.x / 2, textStartY);
            if (Settings::GetSingleton()->csTextShadowEnabled) {
                AddTextScaled(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 120), label);
                if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
            }
            AddTextScaled(textPos, textColor, label);
        } else {
            const char* textEnd = label + strlen(label);
            const char* lineStart = label;
            float y = textStartY;
            while (lineStart < textEnd) {
                const char* lineEnd = lineStart;
                const char* nextLineStart = lineStart;

                while (true) {
                    const char* nextSpace = strchr(lineEnd, ' ');
                    if (!nextSpace) nextSpace = textEnd;

                    float width = CalcSize(lineStart, nextSpace).x;
                    if (width > wrapWidth && lineEnd > lineStart) {
                        nextLineStart = lineEnd;
                        break;
                    }

                    lineEnd = nextSpace;
                    if (lineEnd == textEnd) {
                        nextLineStart = textEnd;
                        break;
                    }
                    lineEnd++;
                }

                if (lineEnd == lineStart) {
                     const char* s = lineStart;
                     while (s < textEnd) {
                         s++;
                         if (CalcSize(lineStart, s).x > wrapWidth) {
                             s--;
                             break;
                         }
                     }
                     lineEnd = s;
                     nextLineStart = s;
                }

                ImVec2 lineSize = CalcSize(lineStart, lineEnd);
                ImVec2 linePos(center.x - lineSize.x / 2, y);
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    AddTextScaled(ImVec2(linePos.x + 1.0f, linePos.y + 1.0f), IM_COL32(0, 0, 0, 120), lineStart, lineEnd);
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                AddTextScaled(linePos, textColor, lineStart, lineEnd);

                y += lineSize.y + 0.6f;
                lineStart = nextLineStart;
                while (lineStart < textEnd && *lineStart == ' ') lineStart++;
            }
        }
    }
}

static void DrawDarkOverlay(ImDrawList* drawList, float screenWidth, float screenHeight, float scale = 1.0f, float panelContentHeight = 0.0f)
{
    auto settings = Settings::GetSingleton();
    
    float panelWidth = 580.0f * scale;
    float panelHeight = (panelContentHeight > 0.0f) ? panelContentHeight : 1080.0f * scale;
    float offsetX = screenWidth * (settings->csOffsetX / 100.0f);
    float offsetY = screenHeight * (settings->csOffsetY / 100.0f);
    float padding = settings->csPadding;
    
    float panelStartX = screenWidth - panelWidth + offsetX - padding;
    float panelEndX = screenWidth + offsetX + padding - 10.0f;
    float panelStartY = offsetY;
    float panelEndY = panelHeight + offsetY + 2.0f;
    
    if (!settings->csUseCustomBackground) {
        uint8_t r = (settings->csBackgroundColor >> 16) & 0xFF;
        uint8_t g = (settings->csBackgroundColor >> 8) & 0xFF;
        uint8_t b = settings->csBackgroundColor & 0xFF;
        uint8_t a = static_cast<uint8_t>(settings->csBackgroundOpacity * 255);
        
        ImU32 solidColor = IM_COL32(r, g, b, a);
        drawList->AddRectFilled(
            ImVec2(panelStartX, panelStartY), 
            ImVec2(panelEndX, panelEndY),
            solidColor
        );
        
        return;
    }
    
    ID3D11ShaderResourceView* bgTexture = BackgroundManager::GetBackground(
        BackgroundManager::Type::CharacterSheet, settings->csBackgroundIndex);
    
    if (bgTexture) {
        drawList->AddImage(
            reinterpret_cast<ImTextureID>(bgTexture),
            ImVec2(panelStartX, panelStartY),
            ImVec2(panelEndX, panelEndY)
        );
    } else {
        uint8_t r = (settings->csBackgroundColor >> 16) & 0xFF;
        uint8_t g = (settings->csBackgroundColor >> 8) & 0xFF;
        uint8_t b = settings->csBackgroundColor & 0xFF;
        uint8_t a = static_cast<uint8_t>(settings->csBackgroundOpacity * 255);
        
        drawList->AddRectFilled(
            ImVec2(panelStartX, panelStartY), 
            ImVec2(panelEndX, panelEndY),
            IM_COL32(r, g, b, a)
        );
    }
}

void CharacterSheet::SaveCameraState()
{
    auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera) return;
    
    _wasThirdPerson = camera->IsInThirdPerson();
    
    auto state = camera->currentState.get();
    if (!state) return;
    
    if (auto tps = skyrim_cast<RE::ThirdPersonState*>(state)) {
        _savedZoomOffset = tps->currentZoomOffset;
        _savedPosOffsetX = tps->posOffsetExpected.x;
        _savedPosOffsetZ = tps->posOffsetExpected.z;
    }
    
    logger::debug("CharacterSheet: Camera saved (zoom={}, posX={}, posZ={}, thirdPerson={})", 
        _savedZoomOffset, _savedPosOffsetX, _savedPosOffsetZ, _wasThirdPerson);
}

void CharacterSheet::RestoreCameraState()
{
    auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera) return;
    
    if (_wasThirdPerson && !camera->IsInThirdPerson()) {
        camera->ForceThirdPerson();
    } else if (!_wasThirdPerson && camera->IsInThirdPerson()) {
        camera->ForceFirstPerson();
    }
    
    auto state = camera->currentState.get();
    if (state) {
        if (auto tps = skyrim_cast<RE::ThirdPersonState*>(state)) {
            tps->targetZoomOffset = _savedZoomOffset;
            tps->currentZoomOffset = _savedZoomOffset;
            tps->posOffsetExpected.x = _savedPosOffsetX;
            tps->posOffsetExpected.z = _savedPosOffsetZ;
        }
    }
    
    logger::debug("CharacterSheet: Camera restored (zoom={}, posX={}, posZ={})", 
        _savedZoomOffset, _savedPosOffsetX, _savedPosOffsetZ);
}

void CharacterSheet::SetupPlayerView()
{
    auto camera = RE::PlayerCamera::GetSingleton();
    if (!camera) return;
    
    if (!camera->IsInThirdPerson()) {
        camera->ForceThirdPerson();
    }
    
    auto state = camera->currentState.get();
    if (state) {
        if (auto tps = skyrim_cast<RE::ThirdPersonState*>(state)) {
            const float zoom = std::clamp(Settings::GetSingleton()->csCameraZoom / 100.0f, 0.0f, 1.0f);
            tps->targetZoomOffset = zoom;
            tps->currentZoomOffset = zoom;
            
            // Move camera position Based on testing: negative X moves player RIGHT
            tps->translation.x = -25.0f;
            tps->translation.z = -30.0f;
            
            tps->posOffsetExpected.x = 0.0f;
            tps->posOffsetActual.x = 0.0f;
            tps->posOffsetExpected.z = -50.0f;
            tps->posOffsetActual.z = -50.0f;
        }
    }
    
    logger::debug("CharacterSheet: Camera setup for closeup view");
}

void CharacterSheet::Toggle()
{
    auto ui = RE::UI::GetSingleton();
    if (ui && !_visible) {
        if (MenuUtils::ShouldBlockCharacterSheet()) {
            if (Settings::GetSingleton()->debugEnableCSLogging)
                logger::info("[DEBUG-CS] Toggle() blocked by ShouldBlockCharacterSheet()");
            return;
        }
        if (Settings::GetSingleton()->debugEnableCSLogging)
            logger::info("[DEBUG-CS] Toggle() not blocked, proceeding to open");
        
        if (PartySheet::IsVisible()) {
            PartySheet::Toggle();
            logger::debug("CharacterSheet: Auto-closed PartySheet to open CharacterSheet");
        }
        if (PartySheetV2::IsVisible()) {
            PartySheetV2::Toggle();
            logger::debug("CharacterSheet: Auto-closed PartySheetV2 to open CharacterSheet");
        }
    }
    
    _visible = !_visible;
    
    if (ui) {
        if (!ui->IsMenuOpen("InventoryMenu") && !ui->IsMenuOpen("MagicMenu")) {
            ui->ShowMenus(!_visible);
        }
    }

    if (_visible) {
        auto settings = Settings::GetSingleton();
        if (settings->csEnableCameraControl) {
            SaveCameraState();
            SetupPlayerView();
        }
    } else {
        auto settings = Settings::GetSingleton();
        if (settings->csEnableCameraControl) {
            RestoreCameraState();
        }
    }

}

void CharacterSheet::Open()
{
    if (_visible) return;
    
    if (PartySheet::IsVisible()) {
        PartySheet::Toggle();
        logger::debug("CharacterSheet: Auto-closed PartySheet to open CharacterSheet");
    }
    if (PartySheetV2::IsVisible()) {
        PartySheetV2::Toggle();
        logger::debug("CharacterSheet: Auto-closed PartySheetV2 to open CharacterSheet");
    }

    _visible = true;

    auto ui = RE::UI::GetSingleton();
    if (ui) {
        if (!ui->IsMenuOpen("InventoryMenu") && !ui->IsMenuOpen("MagicMenu")) {
            ui->ShowMenus(false);
        }
    }
    
    auto settings = Settings::GetSingleton();
    if (settings->csEnableCameraControl) {
        SaveCameraState();
        SetupPlayerView();
    }

}

void CharacterSheet::Close()
{
    if (!_visible) return;
    
    _visible = false;

    auto ui = RE::UI::GetSingleton();
    if (ui) {
        ui->ShowMenus(true);
    }

    auto settings = Settings::GetSingleton();
    if (settings->csEnableCameraControl) {
        RestoreCameraState();
    }

}

std::vector<EquipSlot> CharacterSheet::GetPlayerEquipment()
{
    std::vector<EquipSlot> equipment;
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return equipment;
    
    bool rightHandIs2H = false;
    
    {
        EquipSlot slot = {LocalizationManager::Get("slot_right_hand").c_str(), LocalizationManager::Get("none").c_str(), "weapon", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "", "none", 0.0f, 0.0f, false, false, nullptr, nullptr};
        auto rightHand = player->GetEquippedObject(false);
        if (rightHand) {
            slot.itemName = rightHand->GetName() ? rightHand->GetName() : LocalizationManager::Get("Unknown").c_str();
            slot.hasItem = true;
            slot.itemForm = rightHand;
            
            if (auto weapon = rightHand->As<RE::TESObjectWEAP>()) {
                float baseDamage = static_cast<float>(weapon->GetAttackDamage());
                
                auto weaponType = weapon->GetWeaponType();

                auto avo = player->AsActorValueOwner();
                float totalDamage = baseDamage;
                float fortifyMod = 0.0f;
                
                if (avo) {
                    float skillLevel = 0.0f;
                    
                    switch (weaponType) {
                        case RE::WEAPON_TYPE::kOneHandSword:
                        case RE::WEAPON_TYPE::kOneHandDagger:
                        case RE::WEAPON_TYPE::kOneHandAxe:
                        case RE::WEAPON_TYPE::kOneHandMace:
                            skillLevel = avo->GetActorValue(RE::ActorValue::kOneHanded);
                            fortifyMod = avo->GetActorValue(RE::ActorValue::kOneHandedModifier);
                            slot.skillIconName = "onehanded";
                            break;
                        case RE::WEAPON_TYPE::kTwoHandSword:
                        case RE::WEAPON_TYPE::kTwoHandAxe:
                            skillLevel = avo->GetActorValue(RE::ActorValue::kTwoHanded);
                            fortifyMod = avo->GetActorValue(RE::ActorValue::kTwoHandedModifier);
                            slot.skillIconName = "twohanded";
                            break;
                        case RE::WEAPON_TYPE::kBow:
                        case RE::WEAPON_TYPE::kCrossbow:
                            skillLevel = avo->GetActorValue(RE::ActorValue::kArchery);
                            fortifyMod = avo->GetActorValue(RE::ActorValue::kMarksmanModifier);
                            slot.skillIconName = "archery";
                            break;
                        default:
                            break;
                    }
                    
                    // Use the game's internal damage calculation for accurate display
                    auto* entryData = player->GetEquippedEntryData(false);
                    if (entryData) {
                        totalDamage = player->GetDamage(entryData);
                    } else {
                        float skillMult = 1.0f + (skillLevel / 200.0f);
                        float fortifyMult = 1.0f + (fortifyMod / 100.0f);
                        totalDamage = baseDamage * skillMult * fortifyMult;
                    }
                    
                    
                    slot.skillBonus = baseDamage * (skillLevel / 200.0f);
                }
                
                slot.baseValue = std::round(totalDamage);
                
                slot.fortifyBonus = fortifyMod;
                
                try {
                    if (auto enchantable = weapon->As<RE::TESEnchantableForm>()) {
                        if (enchantable && enchantable->formEnchanting) {
                            auto* enchant = enchantable->formEnchanting;
                            
                            for (std::uint32_t i = 0; i < enchant->effects.size(); i++) {
                                auto* effect = enchant->effects[i];
                                if (!effect || !effect->baseEffect) continue;
                                
                                auto* baseEffect = effect->baseEffect;
                                auto resistType = baseEffect->data.resistVariable;
                                auto primaryAV = baseEffect->data.primaryAV;
                                float magnitude = effect->effectItem.magnitude;
                                
                                if (magnitude > 0 && primaryAV == RE::ActorValue::kHealth) {
                                    if (resistType == RE::ActorValue::kResistFire) {
                                        slot.enchantDamage = magnitude;
                                        slot.enchantType = "fire_resist";
                                        break;
                                    } else if (resistType == RE::ActorValue::kResistFrost) {
                                        slot.enchantDamage = magnitude;
                                        slot.enchantType = "frost_resist";
                                        break;
                                    } else if (resistType == RE::ActorValue::kResistShock) {
                                        slot.enchantDamage = magnitude;
                                        slot.enchantType = "shock_resist";
                                        break;
                                    } else if (resistType == RE::ActorValue::kPoisonResist) {
                                        slot.enchantDamage = magnitude;
                                        slot.enchantType = "poison_resist";
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } catch (...) {
                }
                
                slot.weaponSpeed = weapon->GetSpeed();
                
                switch (weaponType) {
                    case RE::WEAPON_TYPE::kTwoHandSword:
                    case RE::WEAPON_TYPE::kTwoHandAxe:
                        slot.iconName = "greatsword";
                        slot.is2H = true;
                        slot.stagger = weapon->GetStagger();
                        rightHandIs2H = true;
                        break;
                    case RE::WEAPON_TYPE::kBow:
                        slot.iconName = "bow";
                        slot.is2H = true;
                        slot.stagger = weapon->GetStagger();
                        rightHandIs2H = true;
                        break;
                    case RE::WEAPON_TYPE::kCrossbow:
                        slot.iconName = "crossbow";
                        slot.is2H = true;
                        slot.stagger = weapon->GetStagger();
                        rightHandIs2H = true;
                        break;
                    case RE::WEAPON_TYPE::kStaff:
                        slot.iconName = "staff";
                        break;
                    case RE::WEAPON_TYPE::kOneHandDagger:
                        slot.iconName = "dagger";
                        break;
                    default:
                        slot.iconName = "weapon";
                        break;
                }
            } else if (auto spell = rightHand->As<RE::SpellItem>()) {
                slot.spellItem = spell;
                
                auto* avEffect = spell->GetAVEffect();
                if (avEffect) {
                    auto school = avEffect->data.associatedSkill;
                    switch (school) {
                        case RE::ActorValue::kDestruction:
                            slot.iconName = "destruction";
                            break;
                        case RE::ActorValue::kAlteration:
                            slot.iconName = "alteration";
                            break;
                        case RE::ActorValue::kIllusion:
                            slot.iconName = "illusion";
                            break;
                        case RE::ActorValue::kConjuration:
                            slot.iconName = "conjuration";
                            break;
                        case RE::ActorValue::kRestoration:
                            slot.iconName = "restoration";
                            break;
                        default:
                            slot.iconName = "magic";
                            break;
                    }
                    
                    if (avEffect->IsHostile()) {
                        for (auto* effect : spell->effects) {
                            if (effect && effect->baseEffect == avEffect) {
                                slot.baseValue = effect->effectItem.magnitude;
                                break;
                            }
                        }
                    }
                } else {
                    slot.iconName = "magic";
                }
            }
        }
        equipment.push_back(slot);
    }

    
    {
        EquipSlot slot = {LocalizationManager::Get("slot_left_hand").c_str(), LocalizationManager::Get("empty").c_str(), "empty", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "", "none", 0.0f, 0.0f, false, false, nullptr, nullptr};

        // Check left hand first: dual-equip mods may still place a real offhand item here
        auto leftHand = player->GetEquippedObject(true);
        bool leftHandOccupied = (player->GetEquippedEntryData(true) != nullptr);

        bool rightHandIsBow = false;
        if (rightHandIs2H) {
            auto rightHand = player->GetEquippedObject(false);
            if (rightHand) {
                if (auto rightWeapon = rightHand->As<RE::TESObjectWEAP>()) {
                    auto weaponType = rightWeapon->GetWeaponType();
                    if (weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow) {
                        rightHandIsBow = true;
                    }
                }
            }
        }

        if (rightHandIsBow || (rightHandIs2H && !leftHandOccupied)) {
            auto rightHand = player->GetEquippedObject(false);
            if (rightHand) {
                if (auto weapon = rightHand->As<RE::TESObjectWEAP>()) {
                    auto weaponType = weapon->GetWeaponType();
                    if (weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow) {
                        slot.iconName = (weaponType == RE::WEAPON_TYPE::kCrossbow) ? "type_bolt" : "type_quiver";

                        auto ammo = player->GetCurrentAmmo();
                        if (ammo) {
                            int32_t arrowCount = player->GetItemCount(ammo);
                            float arrowDamage = ammo->GetRuntimeData().data.damage;
                            const char* arrowName = ammo->GetName() ? ammo->GetName() : LocalizationManager::Get("Arrows").c_str();

                            char buf[64];
                            snprintf(buf, sizeof(buf), "%s x%d", arrowName, arrowCount);
                            slot.itemName = buf;
                            slot.baseValue = std::round(arrowDamage);
                            slot.hasItem = true;
                            slot.itemForm = ammo;
                        } else {
                            slot.itemName = LocalizationManager::Get("no_arrows").c_str();
                        }
                    }
                }
            }
        } else {

            if (leftHand) {
                slot.itemName = leftHand->GetName() ? leftHand->GetName() : LocalizationManager::Get("Unknown").c_str();
                slot.hasItem = true;
                slot.itemForm = leftHand;
                
                if (auto armor = leftHand->As<RE::TESObjectARMO>()) {
                    slot.iconName = "shield";
                    
                    float displayedAR = 0.0f;
                    
                    auto inventory = player->GetInventory([](const RE::TESBoundObject& a_object) { return a_object.IsArmor(); });
                    for (auto& [item, inventoryData] : inventory) {
                        auto& [count, entryData] = inventoryData;
                        if (count > 0 && entryData && entryData->IsWorn()) {
                            if (item == armor) {
                                displayedAR = player->GetArmorValue(entryData.get());
                                break;
                            }
                        }
                    }
                    
                    if (displayedAR <= 0) {
                        displayedAR = armor->GetArmorRating();
                    }
                    
                    auto avo = player->AsActorValueOwner();
                    if (avo) {
                        bool isHeavyArmor = armor->IsHeavyArmor();
                        if (isHeavyArmor) {
                            slot.skillIconName = "heavyarmor";
                        } else {
                            slot.skillIconName = "lightarmor";
                        }
                    }
                    
                    slot.baseValue = std::round(displayedAR);
                } else if (auto weapon = leftHand->As<RE::TESObjectWEAP>()) {
                    float baseDamage = static_cast<float>(weapon->GetAttackDamage());
                    
                    slot.skillIconName = "onehanded";
                    
                    auto avo = player->AsActorValueOwner();
                    float totalDamage = baseDamage;
                    float fortifyMod = 0.0f;
                    
                    if (avo) {
                        float skillLevel = avo->GetActorValue(RE::ActorValue::kOneHanded);
                        fortifyMod = avo->GetActorValue(RE::ActorValue::kOneHandedModifier);
                        
                        // Use the game's internal damage calculation for accurate display
                        auto* entryData = player->GetEquippedEntryData(true);
                        if (entryData) {
                            totalDamage = player->GetDamage(entryData);
                        } else {
                            float skillMult = 1.0f + (skillLevel / 200.0f);
                            float fortifyMult = 1.0f + (fortifyMod / 100.0f);
                            totalDamage = baseDamage * skillMult * fortifyMult;
                        }
                        
                        slot.skillBonus = baseDamage * (skillLevel / 200.0f);
                    }
                    
                    slot.baseValue = std::round(totalDamage);
                    
                    slot.fortifyBonus = fortifyMod;
                    
                    try {
                        if (auto enchantable = weapon->As<RE::TESEnchantableForm>()) {
                            if (enchantable && enchantable->formEnchanting) {
                                auto* enchant = enchantable->formEnchanting;
                                
                                for (std::uint32_t i = 0; i < enchant->effects.size(); i++) {
                                    auto* effect = enchant->effects[i];
                                    if (!effect || !effect->baseEffect) continue;
                                    
                                    auto* baseEffect = effect->baseEffect;
                                    auto resistType = baseEffect->data.resistVariable;
                                    auto primaryAV = baseEffect->data.primaryAV;
                                    float magnitude = effect->effectItem.magnitude;
                                    
                                    if (magnitude > 0 && primaryAV == RE::ActorValue::kHealth) {
                                        if (resistType == RE::ActorValue::kResistFire) {
                                            slot.enchantDamage = magnitude;
                                            slot.enchantType = "fire_resist";
                                            break;
                                        } else if (resistType == RE::ActorValue::kResistFrost) {
                                            slot.enchantDamage = magnitude;
                                            slot.enchantType = "frost_resist";
                                            break;
                                        } else if (resistType == RE::ActorValue::kResistShock) {
                                            slot.enchantDamage = magnitude;
                                            slot.enchantType = "shock_resist";
                                            break;
                                        } else if (resistType == RE::ActorValue::kPoisonResist) {
                                            slot.enchantDamage = magnitude;
                                            slot.enchantType = "poison_resist";
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    } catch (...) {
                    }
                    
                    slot.weaponSpeed = weapon->GetSpeed();
                    slot.iconName = "weapon";
                } else if (auto spell = leftHand->As<RE::SpellItem>()) {
                    slot.spellItem = spell;
                    auto* avEffect = spell->GetAVEffect();
                    if (avEffect) {
                        auto school = avEffect->data.associatedSkill;
                        switch (school) {
                            case RE::ActorValue::kDestruction:
                                slot.iconName = "destruction";
                                break;
                            case RE::ActorValue::kAlteration:
                                slot.iconName = "alteration";
                                break;
                            case RE::ActorValue::kIllusion:
                                slot.iconName = "illusion";
                                break;
                            case RE::ActorValue::kConjuration:
                                slot.iconName = "conjuration";
                                break;
                            case RE::ActorValue::kRestoration:
                                slot.iconName = "restoration";
                                break;
                            default:
                                slot.iconName = "magic";
                                break;
                        }
                        
                        if (avEffect->IsHostile()) {
                            for (auto* effect : spell->effects) {
                                if (effect && effect->baseEffect == avEffect) {
                                    slot.baseValue = effect->effectItem.magnitude;
                                    break;
                                }
                            }
                        }
                    } else {
                        slot.iconName = "magic";
                    }
                    slot.hasItem = true;
                    slot.itemName = spell->GetName() ? spell->GetName() : LocalizationManager::Get("spell").c_str();
                }
            }
        }
        equipment.push_back(slot);
    }

    {
        EquipSlot slot = {LocalizationManager::Get("slot_head").c_str(), LocalizationManager::Get("none").c_str(), "helmet", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "", "none", 0.0f, 0.0f, false, false, nullptr, nullptr};
        
        auto armor = player->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHead);
        if (!armor) {
            armor = player->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHair);
        }
        if (!armor) {
            armor = player->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kCirclet);
        }
        
        if (armor) {
            slot.itemName = armor->GetName() ? armor->GetName() : LocalizationManager::Get("Unknown").c_str();
            slot.hasItem = true;
            slot.itemForm = armor;
            
            float displayedAR = 0.0f;
            
            auto inventory = player->GetInventory([](const RE::TESBoundObject& a_object) { return a_object.IsArmor(); });
            for (auto& [item, inventoryData] : inventory) {
                auto& [count, entryData] = inventoryData;
                if (count > 0 && entryData && entryData->IsWorn()) {
                    if (item == armor) {
                        displayedAR = player->GetArmorValue(entryData.get());
                        break;
                    }
                }
            }
            
            if (displayedAR <= 0) {
                displayedAR = armor->GetArmorRating();
            }
            
            slot.baseValue = std::round(displayedAR);
            
            auto avo = player->AsActorValueOwner();
            if (avo) {
                bool isHeavyArmor = armor->IsHeavyArmor();
                if (isHeavyArmor) {
                    slot.skillIconName = "heavyarmor";
                } else {
                    slot.skillIconName = "lightarmor";
                }
            }
        }
        equipment.push_back(slot);
    }
    
    static const std::pair<RE::BIPED_MODEL::BipedObjectSlot, std::pair<const char*, const char*>> armorSlots[] = {
        {RE::BIPED_MODEL::BipedObjectSlot::kBody,    {LocalizationManager::Get("slot_chest").c_str(),   "armor"}},
        {RE::BIPED_MODEL::BipedObjectSlot::kHands,   {LocalizationManager::Get("slot_hands").c_str(),   "gauntlets"}},
        {RE::BIPED_MODEL::BipedObjectSlot::kFeet,    {LocalizationManager::Get("slot_feet").c_str(),    "boots"}},
        {RE::BIPED_MODEL::BipedObjectSlot::kAmulet,  {LocalizationManager::Get("slot_amulet").c_str(),  "amulet"}},
        {RE::BIPED_MODEL::BipedObjectSlot::kRing,    {LocalizationManager::Get("slot_ring").c_str(),    "ring"}},
    };

    static RE::BGSKeyword* jewelryKeyword = nullptr;
    static RE::BGSKeyword* clothingNecklaceKeyword = nullptr;
    static RE::BGSKeyword* clothingCircletKeyword = nullptr;
    static RE::BGSKeyword* clothingRingKeyword = nullptr;
    static bool keywordsInitialized = false;
    if (!keywordsInitialized) {
        jewelryKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("VendorItemJewelry");
        clothingNecklaceKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingNecklace");
        clothingCircletKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingCirclet");
        clothingRingKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ClothingRing");
        keywordsInitialized = true;
    }

    for (const auto& [bipedSlot, names] : armorSlots) {
        EquipSlot slot = {names.first, LocalizationManager::Get("none").c_str(), names.second, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "", "none", 0.0f, 0.0f, false, false, nullptr};
        
        auto armor = player->GetWornArmor(bipedSlot);
        
        if (!armor && bipedSlot == RE::BIPED_MODEL::BipedObjectSlot::kAmulet) {
            auto inv = player->GetInventory();
            for (const auto& [item, invData] : inv) {
                if (!item || !invData.second || !invData.second->IsWorn()) continue;
                
                auto armorItem = item->As<RE::TESObjectARMO>();
                if (!armorItem) continue;
                
                bool isNecklace = false;
                if (clothingNecklaceKeyword && armorItem->HasKeyword(clothingNecklaceKeyword)) {
                    isNecklace = true;
                }
                if (jewelryKeyword && armorItem->HasKeyword(jewelryKeyword)) {
                    isNecklace = true;
                }
                
                bool isExcluded = false;
                
                if (clothingRingKeyword && armorItem->HasKeyword(clothingRingKeyword)) {
                    isExcluded = true;
                }
                if (clothingCircletKeyword && armorItem->HasKeyword(clothingCircletKeyword)) {
                    isExcluded = true;
                }
                
                auto slotMask = armorItem->GetSlotMask();
                auto ringSlot = RE::BIPED_MODEL::BipedObjectSlot::kRing;
                auto headSlot = RE::BIPED_MODEL::BipedObjectSlot::kHead;
                auto hairSlot = RE::BIPED_MODEL::BipedObjectSlot::kHair;
                auto circletSlot = RE::BIPED_MODEL::BipedObjectSlot::kCirclet;
                std::uint32_t mask = slotMask.underlying();
                if (mask & static_cast<std::uint32_t>(ringSlot) ||
                    mask & static_cast<std::uint32_t>(headSlot) ||
                    mask & static_cast<std::uint32_t>(hairSlot) ||
                    mask & static_cast<std::uint32_t>(circletSlot)) {
                    isExcluded = true;
                }
                
                if (isNecklace && !isExcluded) {
                    armor = armorItem;
                    break;
                }
            }
        }
        
        if (!armor && bipedSlot == RE::BIPED_MODEL::BipedObjectSlot::kRing) {
            auto inv = player->GetInventory();
            for (const auto& [item, invData] : inv) {
                if (!item || !invData.second || !invData.second->IsWorn()) continue;
                
                auto armorItem = item->As<RE::TESObjectARMO>();
                if (!armorItem) continue;
                
                bool isRing = false;
                if (clothingRingKeyword && armorItem->HasKeyword(clothingRingKeyword)) {
                    isRing = true;
                }
                
                bool isExcluded = false;
                
                if (clothingCircletKeyword && armorItem->HasKeyword(clothingCircletKeyword)) {
                    isExcluded = true;
                }
                if (clothingNecklaceKeyword && armorItem->HasKeyword(clothingNecklaceKeyword)) {
                    isExcluded = true;
                }
                
                auto slotMask = armorItem->GetSlotMask();
                auto headSlot = RE::BIPED_MODEL::BipedObjectSlot::kHead;
                auto hairSlot = RE::BIPED_MODEL::BipedObjectSlot::kHair;
                auto circletSlot = RE::BIPED_MODEL::BipedObjectSlot::kCirclet;
                auto amuletSlot = RE::BIPED_MODEL::BipedObjectSlot::kAmulet;
                std::uint32_t mask = slotMask.underlying();
                if (mask & static_cast<std::uint32_t>(headSlot) ||
                    mask & static_cast<std::uint32_t>(hairSlot) ||
                    mask & static_cast<std::uint32_t>(circletSlot) ||
                    mask & static_cast<std::uint32_t>(amuletSlot)) {
                    isExcluded = true;
                }
                
                if (isRing && !isExcluded) {
                    armor = armorItem;
                    break;
                }
            }
        }
        
        if (armor) {
            slot.itemName = armor->GetName() ? armor->GetName() : LocalizationManager::Get("Unknown").c_str();
            slot.hasItem = true;
            slot.itemForm = armor;
            
            float displayedAR = 0.0f;
            
            auto inventory = player->GetInventory([](const RE::TESBoundObject& a_object) { return a_object.IsArmor(); });
            for (auto& [item, inventoryData] : inventory) {
                auto& [count, entryData] = inventoryData;
                if (count > 0 && entryData && entryData->IsWorn()) {
                    if (item == armor) {
                        displayedAR = player->GetArmorValue(entryData.get());
                        break;
                    }
                }
            }
            
            if (displayedAR <= 0) {
                displayedAR = armor->GetArmorRating();
            }
            
            auto avo = player->AsActorValueOwner();
            if (avo) {
                bool isHeavyArmor = armor->IsHeavyArmor();
                if (isHeavyArmor) {
                    slot.skillIconName = "heavyarmor";
                } else {
                    slot.skillIconName = "lightarmor";
                }
            }
            
            slot.baseValue = std::round(displayedAR);
            
            try {
                if (auto enchantable = armor->As<RE::TESEnchantableForm>()) {
                    if (enchantable && enchantable->formEnchanting) {
                        auto* enchant = enchantable->formEnchanting;
                        
                        for (std::uint32_t i = 0; i < enchant->effects.size(); i++) {
                            auto* effect = enchant->effects[i];
                            if (!effect || !effect->baseEffect) continue;
                            
                            auto* baseEffect = effect->baseEffect;
                            auto primaryAV = baseEffect->data.primaryAV;
                            float magnitude = effect->effectItem.magnitude;
                            
                            if (magnitude > 0) {
                                slot.enchantDamage = magnitude;
                                
                                if (primaryAV == RE::ActorValue::kHealth) {
                                    slot.enchantType = "fortify_health";
                                } else if (primaryAV == RE::ActorValue::kMagicka) {
                                    slot.enchantType = "fortify_magicka";
                                } else if (primaryAV == RE::ActorValue::kStamina) {
                                    slot.enchantType = "fortify_stamina";
                                } else if (primaryAV == RE::ActorValue::kCarryWeight) {
                                    slot.enchantType = "fortify_carry";
                                } else if (primaryAV == RE::ActorValue::kArchery) {
                                    slot.enchantType = "skill_archery";
                                } else if (primaryAV == RE::ActorValue::kMarksmanModifier) {
                                    slot.enchantType = "fortify_archery";
                                } else if (primaryAV == RE::ActorValue::kOneHanded) {
                                    slot.enchantType = "skill_onehanded";
                                } else if (primaryAV == RE::ActorValue::kOneHandedModifier) {
                                    slot.enchantType = "fortify_onehanded";
                                } else if (primaryAV == RE::ActorValue::kTwoHanded) {
                                    slot.enchantType = "skill_twohanded";
                                } else if (primaryAV == RE::ActorValue::kTwoHandedModifier) {
                                    slot.enchantType = "fortify_twohanded";
                                } else if (primaryAV == RE::ActorValue::kBlock) {
                                    slot.enchantType = "skill_block";
                                } else if (primaryAV == RE::ActorValue::kBlockModifier) {
                                    slot.enchantType = "fortify_block";
                                } else if (primaryAV == RE::ActorValue::kDestruction) {
                                    slot.enchantType = "skill_destruction";
                                } else if (primaryAV == RE::ActorValue::kDestructionModifier) {
                                    slot.enchantType = "fortify_destruction";
                                } else if (primaryAV == RE::ActorValue::kRestoration) {
                                    slot.enchantType = "skill_restoration";
                                } else if (primaryAV == RE::ActorValue::kRestorationModifier) {
                                    slot.enchantType = "fortify_restoration";
                                } else if (primaryAV == RE::ActorValue::kConjuration) {
                                    slot.enchantType = "skill_conjuration";
                                } else if (primaryAV == RE::ActorValue::kConjurationModifier) {
                                    slot.enchantType = "fortify_conjuration";
                                } else if (primaryAV == RE::ActorValue::kAlteration) {
                                    slot.enchantType = "skill_alteration";
                                } else if (primaryAV == RE::ActorValue::kAlterationModifier) {
                                    slot.enchantType = "fortify_alteration";
                                } else if (primaryAV == RE::ActorValue::kIllusion) {
                                    slot.enchantType = "skill_illusion";
                                } else if (primaryAV == RE::ActorValue::kIllusionModifier) {
                                    slot.enchantType = "fortify_illusion";
                                } else if (primaryAV == RE::ActorValue::kSneak) {
                                    slot.enchantType = "skill_sneak";
                                } else if (primaryAV == RE::ActorValue::kSneakingModifier) {
                                    slot.enchantType = "fortify_sneak";
                                } else if (primaryAV == RE::ActorValue::kLockpicking) {
                                    slot.enchantType = "skill_lockpicking";
                                } else if (primaryAV == RE::ActorValue::kLockpickingModifier) {
                                    slot.enchantType = "fortify_lockpicking";
                                } else if (primaryAV == RE::ActorValue::kPickpocket) {
                                    slot.enchantType = "skill_pickpocket";
                                } else if (primaryAV == RE::ActorValue::kPickpocketModifier) {
                                    slot.enchantType = "fortify_pickpocket";
                                } else if (primaryAV == RE::ActorValue::kAlchemy) {
                                    slot.enchantType = "skill_alchemy";
                                } else if (primaryAV == RE::ActorValue::kAlchemyModifier) {
                                    slot.enchantType = "fortify_alchemy";
                                } else if (primaryAV == RE::ActorValue::kSpeech) {
                                    slot.enchantType = "skill_speech";
                                } else if (primaryAV == RE::ActorValue::kSpeechcraftModifier) {
                                    slot.enchantType = "fortify_speech";
                                } else if (primaryAV == RE::ActorValue::kSmithing) {
                                    slot.enchantType = "skill_smithing";
                                } else if (primaryAV == RE::ActorValue::kSmithingModifier) {
                                    slot.enchantType = "fortify_smithing";
                                } else if (primaryAV == RE::ActorValue::kResistFire) {
                                    slot.enchantType = "resist_fire";
                                } else if (primaryAV == RE::ActorValue::kResistFrost) {
                                    slot.enchantType = "resist_frost";
                                } else if (primaryAV == RE::ActorValue::kResistShock) {
                                    slot.enchantType = "resist_shock";
                                } else if (primaryAV == RE::ActorValue::kResistMagic) {
                                    slot.enchantType = "resist_magic";
                                } else if (primaryAV == RE::ActorValue::kPoisonResist) {
                                    slot.enchantType = "resist_poison";
                                } else if (primaryAV == RE::ActorValue::kHealRate) {
                                    slot.enchantType = "regen_health";
                                } else if (primaryAV == RE::ActorValue::kMagickaRate) {
                                    slot.enchantType = "regen_magicka";
                                } else if (primaryAV == RE::ActorValue::kStaminaRate) {
                                    slot.enchantType = "regen_stamina";
                                } else {
                                    slot.enchantType = "enchant";
                                }
                                break;
                            }
                        }
                    }
                }
            } catch (...) {
            }
        }
        equipment.push_back(slot);
    }

    
    logger::debug("CharacterSheet: Retrieved {} equipment slots", equipment.size());
    return equipment;
}

PlayerCombatStats CharacterSheet::GetPlayerCombatStats()
{
    PlayerCombatStats stats = {};
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return stats;
    
    ActorStats actorStats = GetActorStats(player);
    
    stats.armorRating = actorStats.armorRating;
    stats.weaponDamage = actorStats.weaponDamage;
    stats.baseWeaponDamage = actorStats.baseWeaponDamage;
    stats.weaponSpeed = actorStats.weaponSpeed;
    stats.weaponReach = actorStats.weaponReach;
    stats.unarmedDamage = actorStats.unarmedDamage;
    stats.speedMult = actorStats.speedMult;
    stats.damageResistPct = actorStats.damageResistPct;
    stats.skills = actorStats.skills;
    
    stats.inventoryWeight = player->GetWeightInContainer();
    stats.carryWeightMax = actorStats.carryWeightMax;
    
    stats.gold = 0;
    if (auto goldForm = RE::TESForm::LookupByID(0x0000000F)) {
        if (auto goldObj = goldForm->As<RE::TESBoundObject>()) {
            stats.gold = player->GetItemCount(goldObj);
        }
    }
    stats.dragonSouls = static_cast<int>(player->AsActorValueOwner()->GetActorValue(static_cast<RE::ActorValue>(133)));
    
    stats.healthRegen = actorStats.healthRegen;
    stats.magickaRegen = actorStats.magickaRegen;
    stats.staminaRegen = actorStats.staminaRegen;
    
    stats.shoutRecovery = actorStats.shoutRecovery;
    stats.perkPoints = static_cast<int>(player->GetGameStatsData().perkCount);
    
    return stats;
}

PlayerResistances CharacterSheet::GetPlayerResistances()
{
    PlayerResistances resists = {};
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return resists;
    
    auto actorResists = GetActorResistances(player);
    resists.fire = actorResists.fire;
    resists.frost = actorResists.frost;
    resists.shock = actorResists.shock;
    resists.poison = actorResists.poison;
    resists.magic = actorResists.magic;
    resists.disease = actorResists.disease;
    
    return resists;
}

void CharacterSheet::Render()
{
    if (!_visible) return;

    if (Settings::GetSingleton()->debugEnableCSLogging)
        logger::info("[DEBUG-CS] Render() entered — checking guards");

    if (auto ui = RE::UI::GetSingleton()) {
        bool blocked = MenuUtils::ShouldBlockCharacterSheet();
        bool invOpen = ui->IsMenuOpen("InventoryMenu");
        bool magOpen = ui->IsMenuOpen("MagicMenu");
        if (Settings::GetSingleton()->debugEnableCSLogging)
            logger::info("[DEBUG-CS] Render() guards — blocked={} invOpen={} magOpen={}", blocked, invOpen, magOpen);

        if (blocked) {
            if (Settings::GetSingleton()->debugEnableCSLogging)
                logger::info("[DEBUG-CS] Render() auto-closing — blocked by menu");
            _visible = false;
            RestoreCameraState();
            ui->ShowMenus(true);
            return;
        }

        if (invOpen || magOpen) {
            ui->ShowMenus(true);
        }
    }
    
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    DrawUI();

    ImGui::Render();
    ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
}

void CharacterSheet::DrawUI()
{
    PerfStats::BeginCSFrame();

    auto& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screenWidth, screenHeight));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (ImGui::Begin("##CharacterSheetOverlay", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        bool isGameMenuOpen = false;
        if (auto ui = RE::UI::GetSingleton()) {
            if (ui->IsMenuOpen("InventoryMenu") || ui->IsMenuOpen("MagicMenu")) {
                isGameMenuOpen = true;
            }
        }

        auto settings = Settings::GetSingleton();

        bool hideNameRow = isGameMenuOpen && settings->csHideNameOnMenus;
        bool hideEquipment = isGameMenuOpen && settings->csHideEquipmentOnMenus;
        bool hideSkills = isGameMenuOpen && settings->csHideSkillsOnMenus;
        bool hideStatus = isGameMenuOpen && settings->csHideStatusOnMenus;

        // Compute unscaled base content height to ensure UI fits within screen
        float baseContentHeight = 20.0f + 10.0f;
        if (!hideNameRow) baseContentHeight += 60.0f;
        baseContentHeight += hideEquipment ? 65.0f : 120.0f;
        if (!hideEquipment) baseContentHeight += 300.0f;
        baseContentHeight += hideSkills ? (hideStatus ? 240.0f : 280.0f) : 280.0f;
        if (!hideSkills) baseContentHeight += hideStatus ? 195.0f : 210.0f;
        if (!hideStatus) baseContentHeight += 80.0f;

        // Global scale factor: use menu-specific scale when Inventory/Magic is open,
        const float requestedScale = isGameMenuOpen ? settings->csMenuScale : settings->csScale;
        const float verticalFit = screenHeight / baseContentHeight;
        const float horizontalFit = screenWidth / 580.0f;
        const float s = (std::min)(requestedScale, (std::min)(verticalFit, horizontalFit));
        ImGui::SetWindowFontScale(s);
        
        
        bool debugGrid = false;
        if (debugGrid) {
            float gridSpacing = 20.0f;
            ImU32 gridColor = IM_COL32(100, 100, 100, 80);
            
            for (float x = 0; x < screenWidth; x += gridSpacing) {
                drawList->AddLine(ImVec2(x, 0), ImVec2(x, screenHeight), gridColor);
            }
            
            for (float y = 0; y < screenHeight; y += gridSpacing) {
                drawList->AddLine(ImVec2(0, y), ImVec2(screenWidth, y), gridColor);
            }
            
        }
        
        auto equipment = GetPlayerEquipment();  if (PerfStats::g_Enabled) ++PerfStats::g_Current.csEquipmentQueries;
        auto stats = GetPlayerCombatStats();    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csStatsQueries;
        auto resists = GetPlayerResistances();  if (PerfStats::g_Enabled) ++PerfStats::g_Current.csResistQueries;
        auto player = RE::PlayerCharacter::GetSingleton();
        
        float panelWidth = 580.0f * s;
        float offsetX = screenWidth * (settings->csOffsetX / 100.0f);
        float offsetY = screenHeight * (settings->csOffsetY / 100.0f);
        float panelStartX = screenWidth - panelWidth + offsetX;
        float panelPadding = 15.0f * s;
        
        float cursorY = 20.0f * s + offsetY;
        float row1Y = cursorY;
        if (!hideNameRow) cursorY += 60.0f * s;
        float row2Y = cursorY;
        cursorY += hideEquipment ? 65.0f * s : 120.0f * s;
        float row3Y = cursorY;
        if (!hideEquipment) cursorY += 300.0f * s;
        float row4Y = cursorY;
        cursorY += hideSkills ? (hideStatus ? 240.0f : 280.0f) * s : 280.0f * s;
        float row5Y = cursorY;
        if (!hideSkills) cursorY += hideStatus ? 195.0f * s : 210.0f * s;
        float row6Y = cursorY;
        if (!hideStatus) cursorY += 80.0f * s;
        float panelContentHeight = cursorY - offsetY + 10.0f * s;
        
        DrawDarkOverlay(drawList, screenWidth, screenHeight, s, panelContentHeight);
        
        // Column X positions within panel (4 columns for flexibility)
        float contentWidth = panelWidth - panelPadding * 2 - 80.0f * s;
        float col1X = panelStartX + panelPadding;
        float col2X = panelStartX + panelPadding + contentWidth * 0.33f;
        
        float diamondSize = settings->csDiamondSize * s;
        float diamondGap = 120.0f * s;
        float equipTextWidth = 130.0f * s;
        float textYOffset = -2.0f * s;
        
        if (!hideNameRow && player) {
            const char* playerName = player->GetName();
            
            uint8_t ncR = (settings->csNameColor >> 16) & 0xFF;
            uint8_t ncG = (settings->csNameColor >> 8)  & 0xFF;
            uint8_t ncB =  settings->csNameColor        & 0xFF;
            uint8_t rcR = (settings->csRaceColor >> 16) & 0xFF;
            uint8_t rcG = (settings->csRaceColor >> 8)  & 0xFF;
            uint8_t rcB =  settings->csRaceColor        & 0xFF;

            ImFont* headingFont = ImGuiRenderer::GetHeadingFont();
            if (headingFont) {
                drawList->AddText(headingFont, headingFont->FontSize * s * settings->csNameFontScale,
                                  ImVec2(col1X, row1Y - 6.0f * s),
                                  IM_COL32(ncR, ncG, ncB, 255),
                                  playerName ? playerName : LocalizationManager::Get("dragonborn").c_str());
            } else {
                ImFont* baseFont = ImGui::GetFont();
                float baseFontSz = ImGui::GetFontSize() * settings->csNameFontScale;
                drawList->AddText(baseFont, baseFontSz,
                                  ImVec2(col1X, row1Y - 6.0f * s),
                                  IM_COL32(ncR, ncG, ncB, 255),
                                  playerName ? playerName : LocalizationManager::Get("dragonborn").c_str());
            }

            if (auto race = player->GetRace()) {
                const char* raceName = race->GetName();
                ImFont* baseFont = ImGui::GetFont();
                float raceFontSz = ImGui::GetFontSize() * settings->csBarTextScale;
                drawList->AddText(baseFont, raceFontSz,
                                  ImVec2(col1X, row1Y + 28.0f * s + textYOffset),
                                  IM_COL32(rcR, rcG, rcB, 255),
                                  raceName ? raceName : LocalizationManager::Get("Unknown").c_str());
            }
            
            float xpBarX = col2X + 40.0f * s;
            float xpBarWidth = 190.0f * s;
            float xpBarHeight = 28.0f * s;
            
            float currentXP = 0.0f;
            float xpNeeded = 1.0f;
            float xpProgress = 0.0f;
            
            if (auto skills = player->GetInfoRuntimeData().skills) {
                if (skills->data) {
                    currentXP = skills->data->xp;
                    xpNeeded = skills->data->levelThreshold;
                    if (xpNeeded > 0) {
                        xpProgress = currentXP / xpNeeded;
                    }
                }
            }
            
            ImU32 xpFillColor = IM_COL32((settings->csXpBarColor >> 16) & 0xFF, (settings->csXpBarColor >> 8) & 0xFF, settings->csXpBarColor & 0xFF, 255);
            
            ImFont* barFont = ImGui::GetFont();
            float barFontSz = ImGui::GetFontSize() * settings->csBarTextScale;
            ImU32 barTextCol = IM_COL32((settings->csBarTextColor >> 16) & 0xFF, (settings->csBarTextColor >> 8) & 0xFF, settings->csBarTextColor & 0xFF, 255);
            drawList->AddRectFilled(ImVec2(xpBarX, row1Y), ImVec2(xpBarX + xpBarWidth, row1Y + xpBarHeight), IM_COL32(20, 25, 40, 220));
            drawList->AddRectFilled(ImVec2(xpBarX, row1Y), ImVec2(xpBarX + xpBarWidth * xpProgress, row1Y + xpBarHeight), xpFillColor);
            DrawNeonRect(drawList, ImVec2(xpBarX, row1Y), ImVec2(xpBarX + xpBarWidth, row1Y + xpBarHeight), xpFillColor, 1.5f * s, 3);

            char lvlText[32];
            snprintf(lvlText, sizeof(lvlText), LocalizationManager::Get("LevelShort").c_str(), player->GetLevel());
            if (Settings::GetSingleton()->csTextShadowEnabled) {
                drawList->AddText(barFont, barFontSz, ImVec2(xpBarX + 8.0f * s + 1, row1Y + 4.0f * s + 1), IM_COL32(0, 0, 0, 120), lvlText);
                if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
            }
            drawList->AddText(barFont, barFontSz, ImVec2(xpBarX + 8.0f * s, row1Y + 4.0f * s), barTextCol, lvlText);

            char xpText[32];
            snprintf(xpText, sizeof(xpText), "%.0f/%.0f XP", currentXP, xpNeeded);
            ImVec2 xpTextSize = barFont->CalcTextSizeA(barFontSz, FLT_MAX, 0.0f, xpText);
            if (Settings::GetSingleton()->csTextShadowEnabled) {
                drawList->AddText(barFont, barFontSz, ImVec2(xpBarX + xpBarWidth - xpTextSize.x - 8.0f * s + 1, row1Y + 4.0f * s + 1), IM_COL32(0, 0, 0, 120), xpText);
                if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
            }
            drawList->AddText(barFont, barFontSz, ImVec2(xpBarX + xpBarWidth - xpTextSize.x - 8.0f * s, row1Y + 4.0f * s), barTextCol, xpText);

            float shoutMod = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kShoutRecoveryMult)
                           + player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kShoutRecoveryMult);
            float shoutTotal = (1.0f + shoutMod) * 100.0f;
            float shoutPct = std::clamp(shoutTotal / 100.0f, 0.0f, 1.0f);
            
            float shoutX = xpBarX + xpBarWidth + 20.0f * s;
            float shoutWidth = 140.0f * s;
            float shoutHeight = 28.0f * s;
            
            ImU32 shoutFillColor = IM_COL32((settings->csShoutBarColor >> 16) & 0xFF, (settings->csShoutBarColor >> 8) & 0xFF, settings->csShoutBarColor & 0xFF, 255);

            drawList->AddRectFilled(ImVec2(shoutX, row1Y), ImVec2(shoutX + shoutWidth, row1Y + shoutHeight), IM_COL32(20, 25, 40, 220));
            drawList->AddRectFilled(ImVec2(shoutX, row1Y), ImVec2(shoutX + shoutWidth * shoutPct, row1Y + shoutHeight), shoutFillColor);
            DrawNeonRect(drawList, ImVec2(shoutX, row1Y), ImVec2(shoutX + shoutWidth, row1Y + shoutHeight), shoutFillColor, 1.5f * s, 3);
            
            char shoutText[32];
            snprintf(shoutText, sizeof(shoutText), LocalizationManager::Get("shout_format").c_str(), shoutTotal);
            
            UIAnimation::CheckNumericChange("shout_recovery", shoutTotal);
            ImU32 normalShoutColor = IM_COL32(220, 220, 220, 255);
            ImU32 shoutTextColor = UIAnimation::GetFlashedColor("shout_recovery", 
                normalShoutColor,
                normalShoutColor,
                IM_COL32(100, 255, 100, 255));
            
            if (Settings::GetSingleton()->csTextShadowEnabled) {
                drawList->AddText(barFont, barFontSz, ImVec2(shoutX + 8.0f * s + 1, row1Y + 4.0f * s + 1), IM_COL32(0, 0, 0, 120), shoutText);
                if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
            }
            drawList->AddText(barFont, barFontSz, ImVec2(shoutX + 8.0f * s, row1Y + 4.0f * s), shoutTextColor, shoutText);

            if (shoutMod != 0.0f) {
                char modText[16];
                snprintf(modText, sizeof(modText), "%+.0f%%", shoutMod * 100.0f);
                ImVec2 modSize = barFont->CalcTextSizeA(barFontSz, FLT_MAX, 0.0f, modText);
                drawList->AddText(barFont, barFontSz, ImVec2(shoutX + shoutWidth - modSize.x - 8.0f * s, row1Y + 4.0f * s), IM_COL32(180, 180, 180, 200), modText);
            }
        }
        
        if (player) {
            ActorStats aStats = GetActorStats(player);
            
            float barWidth = 172.0f * s;
            float barHeight = 20.0f * s;
            float barGapX = 16.0f * s;
            float barsStartX = col1X;
            
            ImU32 regenCol  = IM_COL32((settings->csRegenColor >> 16) & 0xFF, (settings->csRegenColor >> 8) & 0xFF, settings->csRegenColor & 0xFF, 220);
            ImU32 row2TextCol = IM_COL32((settings->csBarTextColor >> 16) & 0xFF, (settings->csBarTextColor >> 8) & 0xFF, settings->csBarTextColor & 0xFF, 255);
            ImFont* row2Font = ImGui::GetFont();
            float row2FontSz = ImGui::GetFontSize() * settings->csBarTextScale;

            auto DrawBar = [&](float x, float y, float pct, float value, float maxVal,
                              float regen, ImU32 fillCol, const char* animKey) {
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth, y + barHeight), IM_COL32(20, 25, 40, 220));
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth * pct, y + barHeight), fillCol);
                DrawNeonRect(drawList, ImVec2(x, y), ImVec2(x + barWidth, y + barHeight), fillCol, 1.5f * s, 3);

                UIAnimation::CheckNumericChange(animKey, value);
                ImU32 flashedTextCol = UIAnimation::GetFlashedColor(animKey, row2TextCol,
                    IM_COL32(100, 255, 100, 255),
                    row2TextCol);

                char text[32];
                snprintf(text, sizeof(text), "%.0f/%.0f", value, maxVal);
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    drawList->AddText(row2Font, row2FontSz, ImVec2(x + 6.0f * s + 1, y + 1.0f * s + 1), IM_COL32(0, 0, 0, 120), text);
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                drawList->AddText(row2Font, row2FontSz, ImVec2(x + 6.0f * s, y + 1.0f * s), flashedTextCol, text);

                if (regen != 0.0f) {
                    float actualRegen = maxVal * (regen / 100.0f);
                    char regText[16];
                    snprintf(regText, sizeof(regText), "+%.1f/s", actualRegen);
                    ImVec2 regSize = row2Font->CalcTextSizeA(row2FontSz, FLT_MAX, 0.0f, regText);
                    drawList->AddText(row2Font, row2FontSz, ImVec2(x + barWidth - regSize.x + 1, y + barHeight + 2.0f * s + 1), IM_COL32(0, 0, 0, 100), regText);
                    drawList->AddText(row2Font, row2FontSz, ImVec2(x + barWidth - regSize.x, y + barHeight + 2.0f * s), regenCol, regText);
                }
            };

            ImU32 hpFill = IM_COL32((settings->csHealthColor >> 16) & 0xFF, (settings->csHealthColor >> 8) & 0xFF, settings->csHealthColor & 0xFF, 255);
            DrawBar(barsStartX, row2Y, aStats.health.GetPercent(), aStats.health.current, aStats.health.GetMax(),
                   aStats.healthRegen, hpFill, "stat_health");

            float spX = barsStartX + barWidth + barGapX;
            ImU32 spFill = IM_COL32((settings->csStaminaColor >> 16) & 0xFF, (settings->csStaminaColor >> 8) & 0xFF, settings->csStaminaColor & 0xFF, 255);
            DrawBar(spX, row2Y, aStats.stamina.GetPercent(), aStats.stamina.current, aStats.stamina.GetMax(),
                   aStats.staminaRegen, spFill, "stat_stamina");

            float mpX = spX + barWidth + barGapX;
            ImU32 mpFill = IM_COL32((settings->csMagickaColor >> 16) & 0xFF, (settings->csMagickaColor >> 8) & 0xFF, settings->csMagickaColor & 0xFF, 255);
            DrawBar(mpX, row2Y, aStats.magicka.GetPercent(), aStats.magicka.current, aStats.magicka.GetMax(),
                   aStats.magickaRegen, mpFill, "stat_magicka");
        }
        
        
        if (!hideEquipment) 
        {
            float gap = diamondGap + 18.0f * s;
            float startX = col1X + 60.0f * s;
            
            if (equipment.size() > 2) {
                auto& slot = equipment[2];
                auto icon = TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                DrawDiamondWithItem(drawList, ImVec2(startX, row3Y), diamondSize,
                                   icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_head").c_str(), slot.hasItem,
                                   equipTextWidth,
                                   slot.baseValue, slot.skillBonus, slot.fortifyBonus,
                                   slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_head", s, true);
            }

            if (equipment.size() > 3) {
                auto& slot = equipment[3];
                auto icon = TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                DrawDiamondWithItem(drawList, ImVec2(startX + gap, row3Y), diamondSize,
                                   icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_chest").c_str(), slot.hasItem,
                                   equipTextWidth,
                                   slot.baseValue, slot.skillBonus, slot.fortifyBonus,
                                   slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_chest", s, true);
            }

            if (equipment.size() > 4) {
                auto& slot = equipment[4];
                auto icon = TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                DrawDiamondWithItem(drawList, ImVec2(startX + gap * 2, row3Y), diamondSize,
                                   icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_hands").c_str(), slot.hasItem,
                                   equipTextWidth,
                                   slot.baseValue, slot.skillBonus, slot.fortifyBonus,
                                   slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_hands", s, true);
            }

            if (equipment.size() > 5) {
                auto& slot = equipment[5];
                auto icon = TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                DrawDiamondWithItem(drawList, ImVec2(startX + gap * 3, row3Y), diamondSize,
                                   icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_feet").c_str(), slot.hasItem,
                                   equipTextWidth,
                                   slot.baseValue, slot.skillBonus, slot.fortifyBonus,
                                   slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_feet", s, true);
            }
        }

        if (!hideEquipment) 
        {
            float row3_5_Y = row3Y + 185.0f * s;
            
            {
                float equipX = col1X + 60.0f * s;
                float gap = diamondGap + 18.0f * s;
                
                if (equipment.size() > 0) {
                    auto& slot = equipment[0];
                    auto icon = slot.spellItem ? SpellIconManager::GetSpellIcon(slot.spellItem)
                                               : TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                    DrawDiamondWithItem(drawList, ImVec2(equipX, row3_5_Y), diamondSize,
                                       icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_main").c_str(), slot.hasItem,
                                       equipTextWidth,
                                       slot.baseValue, 0.0f, 0.0f,
                                       slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_mainhand", s);
                }

                if (equipment.size() > 1) {
                    auto& slot = equipment[1];
                    auto icon = slot.spellItem ? SpellIconManager::GetSpellIcon(slot.spellItem)
                                               : TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                    DrawDiamondWithItem(drawList, ImVec2(equipX + gap, row3_5_Y), diamondSize,
                                       icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_off").c_str(), slot.hasItem,
                                       equipTextWidth,
                                       slot.baseValue, 0.0f, 0.0f,
                                       slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_offhand", s);
                }
            }

            {
                float accX = col1X + 335.0f * s;
                float gap = diamondGap + 18.0f * s;

                if (equipment.size() > 6) {
                    auto& slot = equipment[6];
                    auto icon = TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                    DrawDiamondWithItem(drawList, ImVec2(accX, row3_5_Y), diamondSize,
                                       icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_amulet").c_str(), slot.hasItem,
                                       equipTextWidth,
                                       slot.baseValue, slot.skillBonus, slot.fortifyBonus,
                                       slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_amulet", s, true);
                }

                if (equipment.size() > 7) {
                    auto& slot = equipment[7];
                    auto icon = TextureManager::GetItemIcon(slot.itemForm, slot.iconName);
                    DrawDiamondWithItem(drawList, ImVec2(accX + gap, row3_5_Y), diamondSize,
                                       icon, slot.hasItem ? slot.itemName.c_str() : LocalizationManager::Get("slot_ring").c_str(), slot.hasItem,
                                       equipTextWidth,
                                       slot.baseValue, slot.skillBonus, slot.fortifyBonus,
                                       slot.enchantDamage, slot.enchantType, slot.skillIconName, "equip_ring", s, true);
                }
            }
        }
            
        if (ImGuiRenderer::g_csFocusRow == ImGuiRenderer::CSFocusRow::Slots) {
            float p = 8.0f * s;
            float topY = row3Y - diamondSize * 0.5f - p;
            drawList->AddRect(
                ImVec2(panelStartX - p, topY),
                ImVec2(panelStartX + panelWidth + p, row4Y - p),
                IM_COL32(255, 200, 80, 180), 4.0f, 0, 2.0f
            );
        }

        {
            float iconSize = settings->csStatIconSize * s;
            float rowHeight = 28.0f * s;
            float statFontSz = ImGui::GetFontSize() * settings->csStatFontScale;
            uint8_t slR = (settings->csStatLabelColor >> 16) & 0xFF;
            uint8_t slG = (settings->csStatLabelColor >> 8)  & 0xFF;
            uint8_t slB =  settings->csStatLabelColor        & 0xFF;
            ImU32 statLabelCol = IM_COL32(slR, slG, slB, 255);
            float statsX = col1X + 5.0f * s; 
            float statRowWidth = 200.0f * s;
            
            // Stats box dimensions
            float statsResY = row4Y + 5.0f * s;
            float statsBoxHeight = rowHeight * 8;
            float statsPadding = 8.0f * s;
            uint8_t sgR = (settings->csStatGlowColor >> 16) & 0xFF;
            uint8_t sgG = (settings->csStatGlowColor >> 8)  & 0xFF;
            uint8_t sgB =  settings->csStatGlowColor        & 0xFF;
            float statGlowScale = 1.0f;

            ImVec2 statsBoxMin(statsX - statsPadding, statsResY - statsPadding);
            ImVec2 statsBoxMax(statsX + statRowWidth + statsPadding, statsResY + statsBoxHeight + statsPadding);

            drawList->AddRectFilled(statsBoxMin, statsBoxMax, IM_COL32(
                (settings->csStatBgColor >> 16) & 0xFF,
                (settings->csStatBgColor >> 8)  & 0xFF,
                 settings->csStatBgColor        & 0xFF,
                (settings->csStatBgColor >> 24) & 0xFF));

            if (settings->csStatGlowEnabled) {
                drawList->AddRect(ImVec2(statsBoxMin.x - 4.0f * s, statsBoxMin.y - 4.0f * s), ImVec2(statsBoxMax.x + 4.0f * s, statsBoxMax.y + 4.0f * s), IM_COL32(sgR, sgG, sgB, static_cast<int>(30 * statGlowScale)));
                drawList->AddRect(ImVec2(statsBoxMin.x - 2.0f * s, statsBoxMin.y - 2.0f * s), ImVec2(statsBoxMax.x + 2.0f * s, statsBoxMax.y + 2.0f * s), IM_COL32(sgR, sgG, sgB, static_cast<int>(60 * statGlowScale)));
                drawList->AddRect(statsBoxMin, statsBoxMax, IM_COL32(sgR, sgG, sgB, static_cast<int>(120 * statGlowScale)));
            }
            
            auto iconDamage = TextureManager::GetPartyIcon("weapon");
            auto iconSpeed = TextureManager::GetPartyIcon("attackspeed");
            auto iconReach = TextureManager::GetPartyIcon("reach");
            auto iconMove = TextureManager::GetPartyIcon("movespeed");
            auto iconGold = TextureManager::GetPartyIcon("gold");
            auto iconWeight = TextureManager::GetPartyIcon("weight");
            auto iconSouls = TextureManager::GetPartyIcon("soul");
            auto iconPerks = TextureManager::GetPartyIcon("perk");
            
            auto DrawStatRow = [&](float x, float y, int rowIndex, void* icon, const char* label, const char* value, ImU32 valCol, 
                                   const char* animKey = nullptr, float numValue = 0.0f) {
                uint8_t sbR = (settings->csStatBgColor >> 16) & 0xFF;
                uint8_t sbG = (settings->csStatBgColor >> 8)  & 0xFF;
                uint8_t sbB =  settings->csStatBgColor        & 0xFF;
                uint8_t sbA = (settings->csStatBgColor >> 24) & 0xFF;
                ImU32 bgCol = (rowIndex % 2 == 0)
                    ? IM_COL32(std::min((int)sbR + 20, 255), std::min((int)sbG + 20, 255), std::min((int)sbB + 20, 255), sbA)
                    : IM_COL32(sbR, sbG, sbB, sbA);
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + statRowWidth, y + rowHeight), bgCol);
                
                if (icon) HUDDraw::DrawIconStyled(drawList, ImVec2(x + 4.0f * s, y + 2.0f * s), iconSize,
                    (ImTextureID)icon, 0xFFFFFFFF, IconFxDefault());
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(x + iconSize + 10.0f * s + 1.0f, y + 5.0f * s + 1.0f), IM_COL32(0, 0, 0, 120), label);
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(x + iconSize + 10.0f * s, y + 5.0f * s), statLabelCol, label);
                
                ImU32 flashedValCol = valCol;
                if (animKey) {
                    UIAnimation::CheckNumericChange(animKey, numValue);
                    flashedValCol = UIAnimation::GetFlashedColor(animKey, valCol,
                        IM_COL32(100, 255, 100, 255),
                        valCol);
                }
                
                ImVec2 valSize = ImGui::GetFont()->CalcTextSizeA(statFontSz, FLT_MAX, 0.0f, value);
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(x + statRowWidth - valSize.x - 8.0f * s + 1.0f, y + 5.0f * s + 1.0f), IM_COL32(0, 0, 0, 120), value);
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(x + statRowWidth - valSize.x - 8.0f * s, y + 5.0f * s), flashedValCol, value);
            };
            
            {
                float resStartX = col1X + 235.0f * s;
                float resGap = 72.0f * s;
                float resIconSize = settings->csResistIconSize * s;
                float resY = statsResY;
                
                // Resistances container dimensions: match stats box height/padding
                float resBoxWidth = resGap * 2 + resIconSize + 140.0f * s;
                float resBoxHeight = rowHeight * 8;
                float resPadding = statsPadding;
                float resIconPaddingLeft = 50.0f * s;
                uint8_t rgR = (settings->csResistGlowColor >> 16) & 0xFF;
                uint8_t rgG = (settings->csResistGlowColor >> 8)  & 0xFF;
                uint8_t rgB =  settings->csResistGlowColor        & 0xFF;
                float resistGlowScale = 1.0f;
                ImU32 resBoxColor = IM_COL32(rgR, rgG, rgB, static_cast<int>(200 * resistGlowScale));
                
                ImVec2 resBoxMin(resStartX - resPadding, resY - resPadding);
                ImVec2 resBoxMax(resStartX + resBoxWidth, resY + resBoxHeight + resPadding);
                
                drawList->AddRectFilled(resBoxMin, resBoxMax, IM_COL32(
                    (settings->csResistBgColor >> 16) & 0xFF,
                    (settings->csResistBgColor >> 8)  & 0xFF,
                     settings->csResistBgColor        & 0xFF,
                    (settings->csResistBgColor >> 24) & 0xFF));
                
                auto iconArmor = TextureManager::GetPartyIcon("block");
                auto iconFire = TextureManager::GetPartyIcon("fire");
                auto iconFrost = TextureManager::GetPartyIcon("frost");
                auto iconShock = TextureManager::GetPartyIcon("shock");
                auto iconMagic = TextureManager::GetPartyIcon("magic");
                auto iconPoison = TextureManager::GetPartyIcon("poison");
                auto iconDisease = TextureManager::GetPartyIcon("disease");
                

                auto DrawResistWithBorder = [&](float x, float y, void* icon, float value, const char* fmt, ImU32 color, const char* label, const char* animKey = nullptr) {
                    float circleRadius = resIconSize * 0.7f;

                    int r = (color >> 0) & 0xFF;
                    int g = (color >> 8) & 0xFF;
                    int b = (color >> 16) & 0xFF;

                    
                    char valText[32];
                    snprintf(valText, sizeof(valText), fmt, value);
                    ImVec2 valSize = ImGui::GetFont()->CalcTextSizeA(statFontSz, FLT_MAX, 0.0f, valText);

                    float gap = 14.0f * s;
                    float totalWidth = resIconSize + gap + valSize.x;

                    ImVec2 labelSize = ImGui::GetFont()->CalcTextSizeA(statFontSz, FLT_MAX, 0.0f, label);
                    float labelX = x + (totalWidth - labelSize.x) / 2.0f;

                    if (Settings::GetSingleton()->csTextShadowEnabled) {
                        drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(labelX + 1.0f, y - 8.0f * s + 1.0f), IM_COL32(0, 0, 0, 120), label);
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                    }
                    drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(labelX, y - 8.0f * s), color, label);
                    
                    // 2. Icon in MIDDLE (below label)
                    float iconY = y + 20.0f * s;
                    ImVec2 center(x + resIconSize / 2.0f, iconY + resIconSize / 2.0f);
                    
                    drawList->AddCircleFilled(center, circleRadius + 6.0f * s, IM_COL32(r, g, b, 20), 24);
                    drawList->AddCircleFilled(center, circleRadius + 4.0f * s, IM_COL32(r, g, b, 35), 24);
                    drawList->AddCircleFilled(center, circleRadius + 2.0f * s, IM_COL32(r, g, b, 50), 24);
                    
                    drawList->AddCircleFilled(center, circleRadius, IM_COL32(20, 25, 45, 230), 24);
                    
                    drawList->AddCircle(center, circleRadius, IM_COL32(r, g, b, 200), 24, 2.0f * s);
                    
                    if (icon) HUDDraw::DrawIconStyled(drawList, ImVec2(x, iconY), resIconSize,
                        (ImTextureID)icon, 0xFFFFFFFF, IconFxDefault());
                    
                    ImU32 valColor = IM_COL32(220, 220, 220, 255);
                    if (animKey) {
                        UIAnimation::CheckNumericChange(animKey, value);
                        valColor = UIAnimation::GetFlashedColor(animKey, valColor,
                            IM_COL32(100, 255, 100, 255),
                            valColor);
                    }
                    
                    float valX = x + resIconSize + gap;
                    float valY = iconY + (resIconSize - statFontSz) / 2.0f;
                    if (Settings::GetSingleton()->csTextShadowEnabled) {
                        drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(valX + 1.0f, valY + 1.0f), IM_COL32(0, 0, 0, 120), valText);
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                    }
                    drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(valX, valY), valColor, valText);
                };
                
                // Row Y positions with spacing
                float rrow1Y = resY + 10.0f * s;
                float rrow2Y = resY + 85.0f * s;
                float rrow3Y = resY + 160.0f * s;

                // --- ROW: Armor & Damage Resist ---
                float iconX = resStartX + resIconPaddingLeft + 5.0f * s;
                float col1Offset = -38.0f * s;
                float col2Offset = -12.0f * s;
                float col3Offset = 20.0f * s;
                
                DrawResistWithBorder(iconX + col1Offset, rrow1Y, iconArmor, stats.armorRating, "%.0f", IM_COL32(180, 180, 180, 255), LocalizationManager::Get("Armor").c_str(), "resist_armor");

                auto iconDmgResist = TextureManager::GetPartyIcon("damage_resist");
                DrawResistWithBorder(iconX + resGap + col2Offset, rrow1Y, iconDmgResist, stats.damageResistPct, "%.1f%%", IM_COL32(200, 200, 220, 255), LocalizationManager::Get("dmg_resist").c_str(), "resist_damage_pct");

                DrawResistWithBorder(iconX + col1Offset, rrow2Y, iconFire, resists.fire, "%.0f%%", IM_COL32(255, 100, 50, 255), LocalizationManager::Get("Fire").c_str(), "resist_fire");
                DrawResistWithBorder(iconX + resGap + col2Offset, rrow2Y, iconFrost, resists.frost, "%.0f%%", IM_COL32(80, 180, 255, 255), LocalizationManager::Get("Frost").c_str(), "resist_frost");
                DrawResistWithBorder(iconX + resGap * 2 + col3Offset, rrow2Y, iconShock, resists.shock, "%.0f%%", IM_COL32(255, 230, 80, 255), LocalizationManager::Get("Shock").c_str(), "resist_shock");

                DrawResistWithBorder(iconX + col1Offset, rrow3Y, iconMagic, resists.magic, "%.0f%%", IM_COL32(180, 100, 255, 255), LocalizationManager::Get("Magic").c_str(), "resist_magic");
                DrawResistWithBorder(iconX + resGap + col2Offset, rrow3Y, iconPoison, resists.poison, "%.0f%%", IM_COL32(100, 220, 100, 255), LocalizationManager::Get("Poison").c_str(), "resist_poison");
                DrawResistWithBorder(iconX + resGap * 2 + col3Offset, rrow3Y, iconDisease, resists.disease, "%.0f%%", IM_COL32(160, 140, 120, 255), LocalizationManager::Get("Disease").c_str(), "resist_disease");
                
                if (settings->csResistGlowEnabled)
                    DrawNeonRect(drawList, resBoxMin, resBoxMax, resBoxColor, 1.5f * s, 3);
            }
            
            bool mainHandIsStaff = false;
            bool mainHandIsBow = false;
            if (equipment.size() > 0) {
                if (equipment[0].iconName == "staff") {
                    mainHandIsStaff = true;
                } else if (equipment[0].iconName == "bow" || equipment[0].iconName == "crossbow") {
                    mainHandIsBow = true;
                }
            }
            
            float damage = 0.0f;
            float arrowDamage = 0.0f;
            if (!mainHandIsStaff) {
                if (equipment.size() > 0 && equipment[0].baseValue > 0) {
                    damage = equipment[0].baseValue;

                    if (mainHandIsBow && player) {
                        auto ammo = player->GetCurrentAmmo();
                        if (ammo) {
                            arrowDamage = ammo->GetRuntimeData().data.damage;
                            damage += arrowDamage;
                        }
                    }
                }
                if (damage < 0.1f && equipment.size() > 1 && equipment[1].baseValue > 0) {
                    damage = equipment[1].baseValue;
                }
                if (damage < 0.1f) {
                    damage = stats.unarmedDamage;
                }
            }
            char spdBuf[32]; snprintf(spdBuf, sizeof(spdBuf), "%.2f", stats.weaponSpeed);
            char rchBuf[32]; snprintf(rchBuf, sizeof(rchBuf), "%.1f", stats.weaponReach);
            char movBuf[32]; snprintf(movBuf, sizeof(movBuf), "%.0f%%", stats.speedMult);
            char goldBuf[32]; snprintf(goldBuf, sizeof(goldBuf), "%d", stats.gold);
            char wgtBuf[32]; snprintf(wgtBuf, sizeof(wgtBuf), "%.0f/%.0f", stats.inventoryWeight, stats.carryWeightMax);
            char soulBuf[32]; snprintf(soulBuf, sizeof(soulBuf), "%d", stats.dragonSouls);
            char perkBuf[32]; snprintf(perkBuf, sizeof(perkBuf), "%d", stats.perkPoints);
            
            float enchDmg = 0.0f;
            std::string enchType = "";
            if (equipment.size() > 0 && equipment[0].enchantDamage > 0.0f) {
                enchDmg = equipment[0].enchantDamage;
                enchType = equipment[0].enchantType;
            } else if (equipment.size() > 1 && equipment[1].enchantDamage > 0.0f) {
                enchDmg = equipment[1].enchantDamage;
                enchType = equipment[1].enchantType;
            }
            
            {
                float y = statsResY;
                ImU32 bgCol = IM_COL32(
                    std::min((int)((settings->csStatBgColor >> 16) & 0xFF) + 20, 255),
                    std::min((int)((settings->csStatBgColor >> 8)  & 0xFF) + 20, 255),
                    std::min((int)( settings->csStatBgColor        & 0xFF) + 20, 255),
                    (settings->csStatBgColor >> 24) & 0xFF);
                drawList->AddRectFilled(ImVec2(statsX, y), ImVec2(statsX + statRowWidth, y + rowHeight), bgCol);
                
                if (iconDamage) HUDDraw::DrawIconStyled(drawList, ImVec2(statsX + 4.0f * s, y + 2.0f * s), iconSize,
                    (ImTextureID)iconDamage, 0xFFFFFFFF, IconFxDefault());
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(statsX + iconSize + 10.0f * s + 1.0f, y + 5.0f * s + 1.0f), IM_COL32(0, 0, 0, 120), LocalizationManager::Get("Damage").c_str());
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                drawList->AddText(ImGui::GetFont(), statFontSz, ImVec2(statsX + iconSize + 10.0f * s, y + 5.0f * s), statLabelCol, LocalizationManager::Get("Damage").c_str());
                
                ImU32 enchColor = BadgeStyle::GetEnchantTextColor(enchType);
                
                float totalDamage = damage + enchDmg;
                char dmgValStr[64];
                snprintf(dmgValStr, sizeof(dmgValStr), "%.0f", totalDamage);

                UIAnimation::CheckNumericChange("stat_damage", totalDamage);
                
                auto* dmgFont = ImGui::GetFont();
                float textOffY = y + 5.0f * s;
                ImU32 valColor = (enchDmg > 0.0f) ? enchColor : IM_COL32(255, 100, 100, 255);
                ImU32 flashedValColor = UIAnimation::GetFlashedColor("stat_damage",
                    valColor, IM_COL32(100, 255, 100, 255), valColor);
                float valWidth = dmgFont->CalcTextSizeA(statFontSz, FLT_MAX, 0.0f, dmgValStr).x;
                float startX = statsX + statRowWidth - valWidth - 8.0f * s;
                drawList->AddText(dmgFont, statFontSz, ImVec2(startX + 1.0f, textOffY + 1.0f), IM_COL32(0,0,0,120), dmgValStr);
                drawList->AddText(dmgFont, statFontSz, ImVec2(startX, textOffY), flashedValColor, dmgValStr);
            }
            
            DrawStatRow(statsX, statsResY + rowHeight * 1, 1, iconSpeed, LocalizationManager::Get("Speed").c_str(), spdBuf, IM_COL32(200, 200, 200, 255), "stat_speed", stats.weaponSpeed);
            DrawStatRow(statsX, statsResY + rowHeight * 2, 2, iconReach, LocalizationManager::Get("Reach").c_str(), rchBuf, IM_COL32(200, 200, 200, 255), "stat_reach", stats.weaponReach);
            DrawStatRow(statsX, statsResY + rowHeight * 3, 3, iconMove, LocalizationManager::Get("Move").c_str(), movBuf, IM_COL32(200, 200, 200, 255), "stat_move", stats.speedMult);
            DrawStatRow(statsX, statsResY + rowHeight * 4, 4, iconGold, LocalizationManager::Get("gold").c_str(), goldBuf, IM_COL32(230, 200, 80, 255));
            DrawStatRow(statsX, statsResY + rowHeight * 5, 5, iconWeight, LocalizationManager::Get("weight").c_str(), wgtBuf, IM_COL32(200, 180, 140, 255), "stat_weight", stats.inventoryWeight);
            DrawStatRow(statsX, statsResY + rowHeight * 6, 6, iconSouls, LocalizationManager::Get("souls").c_str(), soulBuf, IM_COL32(180, 130, 220, 255));
            DrawStatRow(statsX, statsResY + rowHeight * 7, 7, iconPerks, LocalizationManager::Get("perks").c_str(), perkBuf, IM_COL32(230, 230, 130, 255));
            
            if (settings->csStatGlowEnabled) {
                ImU32 statsBoxBorderColor = IM_COL32(sgR, sgG, sgB, static_cast<int>(200 * statGlowScale));
                DrawNeonRect(drawList, statsBoxMin, statsBoxMax, statsBoxBorderColor, 1.5f * s, 3);
            }
        }

        
        if (!hideSkills)
        {
            float skillIconSize = 22.0f * s;
            float skillRowHeight = 26.0f * s;
            float skillColWidth = 170.0f * s;
            float skillColGap = 8.0f * s;
            float skillsOffsetX = 5.0f * s;
            
            float warriorX = col1X + skillsOffsetX;
            float thiefX = col1X + skillsOffsetX + skillColWidth + skillColGap;
            float mageX = col1X + skillsOffsetX + (skillColWidth + skillColGap) * 2;
            
            float skillsBoxWidth = skillColWidth * 3 + skillColGap * 2 + 16.0f * s;
            float skillsPadding = 8.0f * s;
            ImVec2 skillsBoxMin(col1X + skillsOffsetX - skillsPadding, row5Y - 30.0f * s);
            ImVec2 skillsBoxMax(col1X + skillsOffsetX + skillsBoxWidth, row5Y + skillRowHeight * 6 + skillsPadding);
            uint8_t skR = (settings->csSkillGlowColor >> 16) & 0xFF;
            uint8_t skG = (settings->csSkillGlowColor >> 8)  & 0xFF;
            uint8_t skB =  settings->csSkillGlowColor        & 0xFF;
            float skillGlowScale = 1.0f;
            ImU32 skillsBoxColor = IM_COL32(skR, skG, skB, static_cast<int>(200 * skillGlowScale));
            uint8_t stR = (settings->csSkillTextColor >> 16) & 0xFF;
            uint8_t stG = (settings->csSkillTextColor >> 8)  & 0xFF;
            uint8_t stB =  settings->csSkillTextColor        & 0xFF;
            float skillFontSz = ImGui::GetFontSize() * settings->csSkillFontScale;

            drawList->AddRectFilled(skillsBoxMin, skillsBoxMax, IM_COL32(
                (settings->csSkillBgColor >> 16) & 0xFF,
                (settings->csSkillBgColor >> 8)  & 0xFF,
                 settings->csSkillBgColor        & 0xFF,
                (settings->csSkillBgColor >> 24) & 0xFF));

            if (settings->csSkillGlowEnabled) {
                drawList->AddRect(ImVec2(skillsBoxMin.x - 4.0f*s, skillsBoxMin.y - 4.0f*s), ImVec2(skillsBoxMax.x + 4.0f*s, skillsBoxMax.y + 4.0f*s), IM_COL32(skR, skG, skB, static_cast<int>(30 * skillGlowScale)));
                drawList->AddRect(ImVec2(skillsBoxMin.x - 2.0f*s, skillsBoxMin.y - 2.0f*s), ImVec2(skillsBoxMax.x + 2.0f*s, skillsBoxMax.y + 2.0f*s), IM_COL32(skR, skG, skB, static_cast<int>(60 * skillGlowScale)));
            }

            ImU32 headerWarrior = IM_COL32(200, 100, 100, 255);
            ImU32 headerThief = IM_COL32(100, 200, 100, 255);
            ImU32 headerMage = IM_COL32(100, 150, 255, 255);
            ImU32 skillNameColor = IM_COL32(stR, stG, stB, 255);
            ImU32 skillValColor  = IM_COL32(
                static_cast<int>(std::min(255, (int)stR + 40)),
                static_cast<int>(std::min(255, (int)stG + 35)),
                static_cast<int>(std::min(255, (int)stB + 20)),
                255);
            uint8_t skbR = (settings->csSkillBgColor >> 16) & 0xFF;
            uint8_t skbG = (settings->csSkillBgColor >> 8)  & 0xFF;
            uint8_t skbB =  settings->csSkillBgColor        & 0xFF;
            uint8_t skbA = (settings->csSkillBgColor >> 24) & 0xFF;
            ImU32 rowBgEven = IM_COL32(std::min((int)skbR + 20, 255), std::min((int)skbG + 20, 255), std::min((int)skbB + 20, 255), skbA);
            ImU32 rowBgOdd  = IM_COL32(skbR, skbG, skbB, skbA);
            
            auto DrawSkillRow = [&](float x, float y, int rowIndex, const char* name, float level, const char* iconName, RE::ActorValue av) {
                float permMod = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, av);
                float tempMod = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, av);
                float totalMod = permMod + tempMod;
                bool hasModifier = std::abs(totalMod) > 0.01f;
                
                ImU32 bgCol;
                if (hasModifier) {
                    bgCol = IM_COL32(45, 60, 100, 200);
                } else {
                    bgCol = (rowIndex % 2 == 0) ? rowBgEven : rowBgOdd;
                }
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + skillColWidth, y + skillRowHeight), bgCol);
                
                auto icon = TextureManager::GetPartyIcon(iconName);
                if (icon) {
                    HUDDraw::DrawIconStyled(drawList, ImVec2(x + 2.0f * s, y + 1.0f * s), skillIconSize,
                        (ImTextureID)icon, 0xFFFFFFFF, IconFxDefault());
                }
                
                ImU32 nameCol = hasModifier ? IM_COL32(100, 180, 255, 255) : skillNameColor;
                ImU32 valCol = hasModifier ? IM_COL32(130, 200, 255, 255) : skillValColor;
                
                float textY = y + (skillRowHeight - skillFontSz) / 2.0f + textYOffset;
                auto* font = ImGui::GetFont();
                drawList->AddText(font, skillFontSz, ImVec2(x + skillIconSize + 6.0f * s + 1.0f, textY + 1.0f), IM_COL32(0,0,0,120), name);
                drawList->AddText(font, skillFontSz, ImVec2(x + skillIconSize + 6.0f * s, textY), nameCol, name);

                float rightEdge = x + skillColWidth - 6.0f * s;
                char valText[16];
                snprintf(valText, sizeof(valText), "%.0f", level);
                ImVec2 valSize = font->CalcTextSizeA(skillFontSz, FLT_MAX, 0.0f, valText);
                drawList->AddText(font, skillFontSz, ImVec2(rightEdge - valSize.x + 1.0f, textY + 1.0f), IM_COL32(0,0,0,120), valText);
                drawList->AddText(font, skillFontSz, ImVec2(rightEdge - valSize.x, textY), valCol, valText);
            };
            
            auto* font = ImGui::GetFont();
            float headerY = row5Y - 22.0f * s + textYOffset;
            auto DrawHeader = [&](float x, ImU32 color, const char* text) {
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    drawList->AddText(font, skillFontSz, ImVec2(x + 1.0f, headerY + 1.0f), IM_COL32(0, 0, 0, 120), text);
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                drawList->AddText(font, skillFontSz, ImVec2(x, headerY), color, text);
            };
            DrawHeader(warriorX, headerWarrior, LocalizationManager::Get("header_warrior").c_str());
            DrawHeader(thiefX,   headerThief,   LocalizationManager::Get("header_thief").c_str());
            DrawHeader(mageX,    headerMage,    LocalizationManager::Get("header_mage").c_str());

            DrawSkillRow(warriorX, row5Y, 0, LocalizationManager::Get("Smithing").c_str(), stats.skills.smithing.value, "smithing", RE::ActorValue::kSmithing);
            DrawSkillRow(warriorX, row5Y + skillRowHeight, 1, LocalizationManager::Get("HeavyArmor").c_str(), stats.skills.heavyArmor.value, "heavyarmor", RE::ActorValue::kHeavyArmor);
            DrawSkillRow(warriorX, row5Y + skillRowHeight * 2, 2, LocalizationManager::Get("Block").c_str(), stats.skills.block.value, "block", RE::ActorValue::kBlock);
            DrawSkillRow(warriorX, row5Y + skillRowHeight * 3, 3, LocalizationManager::Get("TwoHanded").c_str(), stats.skills.twoHanded.value, "twohanded", RE::ActorValue::kTwoHanded);
            DrawSkillRow(warriorX, row5Y + skillRowHeight * 4, 4, LocalizationManager::Get("OneHanded").c_str(), stats.skills.oneHanded.value, "onehanded", RE::ActorValue::kOneHanded);
            DrawSkillRow(warriorX, row5Y + skillRowHeight * 5, 5, LocalizationManager::Get("LightArmor").c_str(), stats.skills.lightArmor.value, "lightarmor", RE::ActorValue::kLightArmor);

            DrawSkillRow(thiefX, row5Y, 0, LocalizationManager::Get("Sneak").c_str(), stats.skills.sneak.value, "sneak", RE::ActorValue::kSneak);
            DrawSkillRow(thiefX, row5Y + skillRowHeight, 1, LocalizationManager::Get("Lockpicking").c_str(), stats.skills.lockpicking.value, "lockpicking", RE::ActorValue::kLockpicking);
            DrawSkillRow(thiefX, row5Y + skillRowHeight * 2, 2, LocalizationManager::Get("Pickpocket").c_str(), stats.skills.pickpocket.value, "pickpocket", RE::ActorValue::kPickpocket);
            DrawSkillRow(thiefX, row5Y + skillRowHeight * 3, 3, LocalizationManager::Get("Archery").c_str(), stats.skills.archery.value, "archery", RE::ActorValue::kArchery);
            DrawSkillRow(thiefX, row5Y + skillRowHeight * 4, 4, LocalizationManager::Get("Alchemy").c_str(), stats.skills.alchemy.value, "alchemy", RE::ActorValue::kAlchemy);
            DrawSkillRow(thiefX, row5Y + skillRowHeight * 5, 5, LocalizationManager::Get("Speech").c_str(), stats.skills.speech.value, "speech", RE::ActorValue::kSpeech);

            DrawSkillRow(mageX, row5Y, 0, LocalizationManager::Get("Destruction").c_str(), stats.skills.destruction.value, "destruction", RE::ActorValue::kDestruction);
            DrawSkillRow(mageX, row5Y + skillRowHeight, 1, LocalizationManager::Get("Restoration").c_str(), stats.skills.restoration.value, "restoration", RE::ActorValue::kRestoration);
            DrawSkillRow(mageX, row5Y + skillRowHeight * 2, 2, LocalizationManager::Get("Conjuration").c_str(), stats.skills.conjuration.value, "conjuration", RE::ActorValue::kConjuration);
            DrawSkillRow(mageX, row5Y + skillRowHeight * 3, 3, LocalizationManager::Get("Alteration").c_str(), stats.skills.alteration.value, "alteration", RE::ActorValue::kAlteration);
            DrawSkillRow(mageX, row5Y + skillRowHeight * 4, 4, LocalizationManager::Get("Illusion").c_str(), stats.skills.illusion.value, "illusion", RE::ActorValue::kIllusion);
            DrawSkillRow(mageX, row5Y + skillRowHeight * 5, 5, LocalizationManager::Get("Enchanting").c_str(), stats.skills.enchanting.value, "enchanting", RE::ActorValue::kEnchanting);
            
            if (settings->csSkillGlowEnabled)
                DrawNeonRect(drawList, skillsBoxMin, skillsBoxMax, skillsBoxColor, 1.5f * s, 3);
        }
        
        
        if (!hideStatus)
        {
            // --- STATUS (Faction Memberships, Champions, Thane) - Carousel with Icons ---
            float statusX = col1X + 5.0f * s;
            float statusPadding = 8.0f * s;
            ImU32 statusBoxColor = IM_COL32(
                (settings->csStatusGlowColor >> 16) & 0xFF,
                (settings->csStatusGlowColor >> 8)  & 0xFF,
                 settings->csStatusGlowColor        & 0xFF, 200);
            
            auto statusData = PlayerStatus::GetPlayerStatus(player);
            
            enum class StatusType { Faction, Champion, Thane, Empty };
            struct StatusItem {
                StatusType type;
                std::string iconKey;
                std::string title;
                std::string subtitle;
                ImU32 glowColor;
                ImU32 titleColor;
            };
            std::vector<StatusItem> allItems;
            std::vector<int> pageStarts;
            
            auto GetItemsPerPage = [](StatusType type) -> int {
                if (type == StatusType::Faction) return CAROUSEL_ITEMS_FACTION;
                if (type == StatusType::Champion) return CAROUSEL_ITEMS_CHAMPION;
                if (type == StatusType::Thane) return CAROUSEL_ITEMS_THANE;
                return CAROUSEL_ITEMS_PER_PAGE;
            };
            
            auto AddCategory = [&](const auto& items, StatusType type, auto itemBuilder) {
                if (items.empty()) return;
                int itemsPerPage = GetItemsPerPage(type);
                int count = 0;
                for (const auto& data : items) {
                    if (count % itemsPerPage == 0) {
                        pageStarts.push_back(static_cast<int>(allItems.size()));
                    }
                    allItems.push_back(itemBuilder(data, type));
                    count++;
                }
            };

            AddCategory(statusData.factions, StatusType::Faction, [](const auto& faction, StatusType) {
                StatusItem item;
                item.type = StatusType::Faction;
                item.iconKey = faction.key;
                item.title = faction.displayName;
                item.subtitle = faction.isLeader ? faction.leaderTitle : LocalizationManager::Get("member").c_str();
                item.glowColor = faction.isLeader ? IM_COL32(255, 215, 100, 255) : IM_COL32(100, 150, 200, 255);
                item.titleColor = faction.isLeader ? IM_COL32(255, 215, 100, 255) : IM_COL32(220, 220, 220, 255);
                return item;
            });

            AddCategory(statusData.champions, StatusType::Champion, [](const auto& champion, StatusType) {
                StatusItem item;
                item.type = StatusType::Champion;
                item.iconKey = champion.key;
                item.title = champion.displayName;
                item.subtitle = !champion.leaderTitle.empty() ? champion.leaderTitle : LocalizationManager::Get("champion").c_str();
                item.glowColor = IM_COL32(200, 100, 255, 255);
                item.titleColor = IM_COL32(200, 100, 255, 255);
                return item;
            });

            AddCategory(statusData.thaneHolds, StatusType::Thane, [](const auto& thane, StatusType) {
                StatusItem item;
                item.type = StatusType::Thane;
                item.iconKey = thane.key;
                item.title = thane.displayName;
                item.subtitle = "";
                item.glowColor = IM_COL32(100, 200, 150, 255);
                item.titleColor = IM_COL32(100, 200, 150, 255);
                return item;
            });
            
            int totalItems = static_cast<int>(allItems.size());
            int totalPages = static_cast<int>(pageStarts.size());
            if (totalPages == 0) totalPages = 1;
            
            if (totalPages > 1) {
                float deltaTime = UIAnimation::AnimationManager::Get().GetDeltaTime();
                s_statusCarouselTimer += deltaTime;
                float carouselInterval = Settings::GetSingleton()->csCarouselInterval;
                if (s_statusCarouselTimer >= carouselInterval) {
                    s_statusCarouselTimer = 0.0f;
                    s_statusCarouselPage = (s_statusCarouselPage + 1) % totalPages;
                    // Trigger slide animation for smooth transition
                    UIAnimation::TriggerSlideIn("status_carousel", 1, 30.0f);
                }
            }
            
            if (s_statusCarouselPage >= totalPages) s_statusCarouselPage = 0;
            
            float statusBoxWidth = 540.0f * s;
            float statusBoxHeight = 74.0f * s;
            ImVec2 statusBoxMin(statusX - statusPadding, row6Y - 30.0f * s);
            ImVec2 statusBoxMax(statusX + statusBoxWidth, row6Y + statusBoxHeight);
            
            drawList->AddRectFilled(statusBoxMin, statusBoxMax, IM_COL32(
                (settings->csStatusBgColor >> 16) & 0xFF,
                (settings->csStatusBgColor >> 8)  & 0xFF,
                 settings->csStatusBgColor        & 0xFF,
                (settings->csStatusBgColor >> 24) & 0xFF));
            
            std::string headerText = LocalizationManager::Get("header_status");
            if (!pageStarts.empty() && s_statusCarouselPage < static_cast<int>(pageStarts.size())) {
                int pageStartIdx = pageStarts[s_statusCarouselPage];
                if (pageStartIdx < static_cast<int>(allItems.size())) {
                    StatusType type = allItems[pageStartIdx].type;
                    if (type == StatusType::Faction) headerText = LocalizationManager::Get("header_faction_status");
                    else if (type == StatusType::Champion) headerText = LocalizationManager::Get("header_champion_status");
                    else if (type == StatusType::Thane) headerText = LocalizationManager::Get("header_thane_status");
                }
            }
            float statusFontSz = ImGui::GetFontSize() * settings->csStatusFontScale;
            auto* statusFont = ImGui::GetFont();
            auto DrawStatusText = [&](ImVec2 pos, ImU32 color, const char* text) {
                if (Settings::GetSingleton()->csTextShadowEnabled) {
                    drawList->AddText(statusFont, statusFontSz, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 120), text);
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.csShadowDrawCalls;
                }
                drawList->AddText(statusFont, statusFontSz, pos, color, text);
            };
            DrawStatusText(ImVec2(statusX, row6Y - 22.0f * s), IM_COL32(180, 160, 100, 255), headerText.c_str());
            
            if (totalPages > 1) {
                float dotRadius = 4.0f * s;
                float dotGap = 12.0f * s;
                float dotsWidth = totalPages * dotGap;
                float dotStartX = statusX + statusBoxWidth - dotsWidth - 20.0f * s;
                float dotY = row6Y - 18.0f * s;
                
                for (int i = 0; i < totalPages; i++) {
                    float dotX = dotStartX + i * dotGap;
                    ImU32 dotColor = (i == s_statusCarouselPage) 
                        ? IM_COL32(255, 215, 100, 255)
                        : IM_COL32(100, 100, 100, 150);
                    drawList->AddCircleFilled(ImVec2(dotX, dotY), dotRadius, dotColor, 12);
                }
            }
            
            ImVec2 slideOffset = UIAnimation::GetOffset("status_carousel");
            
            if (totalItems > 0 && !pageStarts.empty()) {
                int startIdx = (s_statusCarouselPage < static_cast<int>(pageStarts.size())) 
                               ? pageStarts[s_statusCarouselPage] : 0;
                
                int itemsPerPage = CAROUSEL_ITEMS_PER_PAGE;
                if (startIdx < totalItems) {
                    auto firstType = allItems[startIdx].type;
                    if (firstType == StatusType::Faction) itemsPerPage = CAROUSEL_ITEMS_FACTION;
                    else if (firstType == StatusType::Thane) itemsPerPage = CAROUSEL_ITEMS_THANE;
                    else if (firstType == StatusType::Champion) itemsPerPage = CAROUSEL_ITEMS_CHAMPION;
                }
                
                int endIdx = (s_statusCarouselPage + 1 < static_cast<int>(pageStarts.size()))
                             ? pageStarts[s_statusCarouselPage + 1]
                             : totalItems;
                
                float badgeWidth = (itemsPerPage == 2) ? 220.0f * s : 160.0f * s;
                float badgeGap = 16.0f * s;
                float iconSize = 58.0f * s;
                float badgeStartX = statusX + slideOffset.x;
                
                int validItemIndex = 0;

                for (int i = startIdx; i < endIdx; i++) {
                    const auto& item = allItems[i];
                    if (item.type == StatusType::Empty) continue;

                    float badgeX = badgeStartX + validItemIndex * (badgeWidth + badgeGap);
                    float badgeY = row6Y + 10.0f * s;
                    validItemIndex++;
                    
                    float iconDrawX = badgeX + 8.0f * s; 
                    float iconDrawY = badgeY - 5.0f * s;
                    
                    std::string iconPath = std::string("Data\\Interface\\PartySheet\\Status\\") + item.iconKey + ".png";
                    auto icon = TextureManager::GetTexture(iconPath);
                    if (icon) {
                        drawList->AddImage((ImTextureID)icon, 
                            ImVec2(iconDrawX, iconDrawY), 
                            ImVec2(iconDrawX + iconSize, iconDrawY + iconSize));
                    }
                    
                    // Title text (right of icon)
                    float textX = iconDrawX + iconSize + 10.0f * s;
                    float textY = badgeY + 4.0f * s;
                    DrawStatusText(ImVec2(textX, textY), item.titleColor, item.title.c_str());

                    if (!item.subtitle.empty()) {
                        DrawStatusText(ImVec2(textX, textY + statusFontSz + 2.0f * s), IM_COL32(150, 150, 150, 200), item.subtitle.c_str());
                    }
                }

            } else {
                DrawStatusText(ImVec2(statusX + 8.0f * s, row6Y + 20.0f * s), IM_COL32(120, 120, 120, 255), LocalizationManager::Get("no_major_factions").c_str());
            }
            
            if (settings->csStatusGlowEnabled)
                DrawNeonRect(drawList, statusBoxMin, statusBoxMax, statusBoxColor, 1.5f * s, 3);
        }

    }
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (dl && PerfStats::g_Enabled) PerfStats::g_Current.csDrawCalls = dl->CmdBuffer.Size;
    }
    ImGui::End();

    ImGui::PopStyleVar();

    PerfStats::EndCSFrame();

    ImGui::EndFrame();
    ImGui::Render();
    ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
}
