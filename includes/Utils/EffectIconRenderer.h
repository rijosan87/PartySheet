#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

#include "../../includes/EffectCategories.h"
#include "../../includes/TextureManager.h"
#include "../../includes/EffectBlacklist.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/ColorUtils.h"

namespace EffectIconRenderer {

    struct EffectDisplayInfo {
        std::string name;
        std::string iconId;
        float       magnitude     = 0.0f;
        float       duration      = 0.0f;
        float       timeRemaining = 0.0f;
        bool        isHostile     = false;
        RE::EffectArchetypes::ArchetypeID archetype = RE::EffectArchetypes::ArchetypeID::kNone;
        RE::ActorValue                    primaryAV = RE::ActorValue::kNone;
        EffectCategories::EffectSource    source       = EffectCategories::EffectSource::Unknown;
        std::string                       sourceIconId;
        std::string                       sourceName;
    };

    // Query an actor's active effect list and return displayable entries.
    inline std::vector<EffectDisplayInfo> GetActiveEffectsInfo(RE::Actor* actor, int maxEffects, int minDuration = 0, int maxDuration = 0, bool hideManagedSummons = false) {
        std::vector<EffectDisplayInfo> results;
        if (!actor) return results;

        auto magicTarget = actor->AsMagicTarget();
        if (!magicTarget) return results;

        auto effectList = magicTarget->GetActiveEffectList();
        if (!effectList) return results;

        for (auto* effect : *effectList) {
            if (!effect) continue;

            if (effect->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                effect->flags.any(RE::ActiveEffect::Flag::kInactive))
                continue;

            auto baseEffect = effect->GetBaseObject();
            if (!baseEffect) continue;

            if (baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI))
                continue;

            const char* effectName = baseEffect->GetFullName();
            if (!effectName || strlen(effectName) == 0) continue;

            if (EffectBlacklist::IsBlacklisted(effectName)) continue;

            if (effect->duration <= 0) continue;

            float timeRemaining = effect->duration - effect->elapsedSeconds;
            if (timeRemaining <= 0) continue;

            if (minDuration > 0 && effect->duration < static_cast<float>(minDuration))
                continue;

            if (maxDuration > 0 && effect->duration > static_cast<float>(maxDuration * 60))
                continue;

            // Display filtering only. Apply before the cap so other effects can fill the freed slots.
            if (hideManagedSummons && FollowerHUD::ShouldHideSummonEffect(actor->GetFormID(),
                    reinterpret_cast<std::uintptr_t>(effect), effect->usUniqueID,
                    baseEffect->GetFormID(), effect->spell ? effect->spell->GetFormID() : 0))
                continue;

            EffectDisplayInfo info;
            info.name          = effectName;
            info.magnitude     = std::abs(effect->magnitude);
            info.duration      = effect->duration;
            info.timeRemaining = timeRemaining;
            info.isHostile     = baseEffect->IsHostile() || baseEffect->IsDetrimental();
            info.archetype     = baseEffect->GetArchetype();
            info.primaryAV     = baseEffect->data.primaryAV;
            info.source       = EffectCategories::GetEffectSource(effect);
            info.sourceIconId = EffectCategories::GetSourceIconId(info.source);
            if (effect->spell) {
                const char* sn = effect->spell->GetFullName();
                if (sn && sn[0]) info.sourceName = sn;
            }
            if (info.source == EffectCategories::EffectSource::Spell) {
                const char* schoolIcon = EffectCategories::GetSpellSchoolIconId(effect->spell);
                if (schoolIcon && schoolIcon[0] != '\0')
                    info.sourceIconId = schoolIcon;
            }

            const char* frostbiteIcon = EffectCategories::GetFrostbiteIconId(info.name);
            if (frostbiteIcon) {
                info.iconId = frostbiteIcon;
                info.sourceIconId.clear();
            } else {
                auto effectType = EffectCategories::GetEffectType(info.archetype);
                if (effectType == EffectCategories::EffectType::Standalone)
                    info.iconId = EffectCategories::GetStandaloneIconId(info.archetype);
                else if (effectType == EffectCategories::EffectType::Modifier)
                    info.iconId = EffectCategories::GetActorValueIconId(info.primaryAV);
                else
                    info.iconId = EffectCategories::GetScriptEffectIconId(info.name, baseEffect->data.associatedSkill);
            }

            results.push_back(info);

            if (static_cast<int>(results.size()) >= maxEffects) break;
        }

        return results;
    }

    struct DrawOptions {
        float iconSize           = 32.0f;
        float borderThickness    = 2.0f;
        bool  showTimer          = true;
        int   timerStyle         = 0;
        bool  showMagnitude      = true;
        float magnitudeScale     = 0.8f;
        float bgPadding          = 0.0f;
        float zoom               = 1.0f;
        float focusU             = 0.5f;
        float focusV             = 0.5f;
        bool  showTimeText       = false;
        float timeTextScale      = 0.75f;
        ImU32 timeTextColor      = IM_COL32(255, 255, 255, 255);
        ImU32 buffColor          = IM_COL32(68, 255, 68, 255);
        ImU32 debuffColor        = IM_COL32(255, 68, 68, 255);
        float bgFillRadius      = 0.0f;
        bool  showSourceBadge      = true;
        float sourceBadgeScale     = 0.35f;
        ImU32 sourceBadgeTint      = IM_COL32(255, 255, 255, 255);
        int   sourceBadgeOffsetX   = 0;
        int   sourceBadgeOffsetY   = 0;
        int   magnitudeOffsetX     = 0;
        int   magnitudeOffsetY     = 0;
        HUDDraw::IconFx iconFx = { false, false, false, false, false, 2, 0xC80A080C };
    };

    inline bool DrawEffectIcon(
        const EffectDisplayInfo& effect,
        ImVec2                   iconPos,
        ImDrawList*              drawList,
        const DrawOptions&       opt)
    {
        auto effectIcon = TextureManager::GetEffectIcon(effect.iconId);
        if (!effectIcon) return false;

        bool  isNegative  = effect.isHostile || EffectCategories::IsNegativeArchetype(effect.archetype);
        ImU32 effectColor = isNegative ? opt.debuffColor : opt.buffColor;

        ImVec2 center       = ImVec2(iconPos.x + opt.iconSize / 2.0f, iconPos.y + opt.iconSize / 2.0f);
        float  borderRadius = opt.iconSize / 2.0f + opt.borderThickness + 1.0f;

        if (opt.showTimer && opt.timerStyle == 1) {
            float barW = (std::max)(opt.borderThickness + 2.0f, 5.0f);
            float barX = iconPos.x - barW - 7.0f;

            HUDDraw::TimerBarStyle tb;
            tb.width     = barW;
            tb.height    = opt.iconSize;
            tb.fraction  = effect.duration > 0
                           ? std::clamp(effect.timeRemaining / effect.duration, 0.0f, 1.0f) : 0.0f;
            tb.fillColor = ((effectColor >> IM_COL32_A_SHIFT & 0xFF) << 24) |
                           ((effectColor >> IM_COL32_R_SHIFT & 0xFF) << 16) |
                           ((effectColor >> IM_COL32_G_SHIFT & 0xFF) << 8)  |
                            (effectColor >> IM_COL32_B_SHIFT & 0xFF);
            tb.edgeLightSize = opt.iconSize * 0.4f;
            HUDDraw::DrawTimerBarV(drawList, ImVec2(barX, iconPos.y), tb);
        } else if (opt.showTimer && opt.timerStyle == 2) {
            HUDDraw::TimerBarStyle tb;
            tb.width       = opt.iconSize;
            tb.height      = opt.iconSize;
            tb.rounding    = opt.bgFillRadius;
            tb.fraction    = effect.duration > 0
                             ? std::clamp(effect.timeRemaining / effect.duration, 0.0f, 1.0f) : 0.0f;
            tb.fillColor   = 0x90000000 |
                             ((effectColor >> IM_COL32_R_SHIFT & 0xFF) << 16) |
                             ((effectColor >> IM_COL32_G_SHIFT & 0xFF) << 8)  |
                              (effectColor >> IM_COL32_B_SHIFT & 0xFF);
            tb.trackTop    = 0x6008070A;
            tb.trackBottom = 0x60181519;
            tb.shadow      = false;
            tb.glow        = false;
            tb.edgeLightSize = opt.iconSize * 0.35f;
            HUDDraw::DrawTimerBarV(drawList, iconPos, tb);
        } else {
            if (opt.showTimer) {
                HUDDraw::TimerArcStyle ta;
                ta.radius    = borderRadius;
                ta.thickness = opt.borderThickness;
                ta.fraction  = effect.duration > 0
                               ? std::clamp(effect.timeRemaining / effect.duration, 0.0f, 1.0f) : 0.0f;
                ta.fillColor = ((effectColor >> IM_COL32_A_SHIFT & 0xFF) << 24) |
                               ((effectColor >> IM_COL32_R_SHIFT & 0xFF) << 16) |
                               ((effectColor >> IM_COL32_G_SHIFT & 0xFF) << 8)  |
                                (effectColor >> IM_COL32_B_SHIFT & 0xFF);
                HUDDraw::DrawTimerArc(drawList, center, ta);
            }
        }

        {
            float baseW = opt.iconSize - opt.bgPadding * 2.0f;
            float baseH = baseW;
            float z     = (std::max)(opt.zoom, 1.0f);
            float imgW  = baseW * z;
            float imgH  = baseH * z;
            float anchorX = iconPos.x + opt.iconSize * opt.focusU;
            float anchorY = iconPos.y + opt.iconSize * opt.focusV;
            ImVec2 imgMin = ImVec2(anchorX - imgW * opt.focusU, anchorY - imgH * opt.focusV);
            ImVec2 imgMax = ImVec2(imgMin.x + imgW, imgMin.y + imgH);

            const HUDDraw::IconFx& fx = opt.iconFx;
            bool anyDepth = fx.duotone || fx.shadow || fx.outline || fx.glow || fx.gloss;
            if (anyDepth) {
                // The plain path zooms by OVERFLOWING the slot (image grows past baseW,
                float span = 1.0f / z;
                float u0   = opt.focusU * (1.0f - span);
                float v0   = opt.focusV * (1.0f - span);
                HUDDraw::DrawIconStyled(drawList, ImVec2(iconPos.x + opt.bgPadding, iconPos.y + opt.bgPadding),
                                        baseW, reinterpret_cast<ImTextureID>(effectIcon),
                                        0xFFFFFFFF, fx,
                                        ImVec2(u0, v0), ImVec2(u0 + span, v0 + span));
            } else {
                drawList->AddImage(reinterpret_cast<ImTextureID>(effectIcon), imgMin, imgMax);
            }
        }

        if (opt.showSourceBadge && !effect.sourceIconId.empty()) {
            auto sourceIcon = TextureManager::GetEffectIcon(effect.sourceIconId);
            if (sourceIcon) {
                float badgeSize = (std::max)(opt.iconSize * opt.sourceBadgeScale, 10.0f);
                float badgeX    = iconPos.x + static_cast<float>(opt.sourceBadgeOffsetX);
                float badgeY    = iconPos.y + opt.iconSize - badgeSize + static_cast<float>(opt.sourceBadgeOffsetY);
                ImVec2 badgeMin = ImVec2(badgeX,             badgeY);
                ImVec2 badgeMax = ImVec2(badgeX + badgeSize, badgeY + badgeSize);
                std::uint32_t badgeArgb =
                    ((opt.sourceBadgeTint >> IM_COL32_A_SHIFT & 0xFF) << 24) |
                    ((opt.sourceBadgeTint >> IM_COL32_R_SHIFT & 0xFF) << 16) |
                    ((opt.sourceBadgeTint >> IM_COL32_G_SHIFT & 0xFF) << 8)  |
                    ((opt.sourceBadgeTint >> IM_COL32_B_SHIFT & 0xFF));
                HUDDraw::IconFx badgeFx = { true, true, false, false, false, 2, 0xC80A080C };
                HUDDraw::DrawIconStyled(drawList, badgeMin, badgeSize,
                                        reinterpret_cast<ImTextureID>(sourceIcon), badgeArgb, badgeFx);
            }
        }

        if (opt.showMagnitude && effect.magnitude >= 1.0f) {
            char  magText[16];
            int   magInt = static_cast<int>(effect.magnitude);
            bool  isPct = EffectCategories::IsPercentageMagnitude(effect.primaryAV);
            snprintf(magText, sizeof(magText), isNegative ? (isPct ? "-%d%%" : "-%d") : (isPct ? "+%d%%" : "+%d"), magInt);

            ImFont* font     = ImGui::GetFont();
            float   fontSize = font->FontSize * opt.magnitudeScale;
            ImVec2  textSize = ImGui::CalcTextSize(magText);
            textSize.x *= opt.magnitudeScale;
            textSize.y *= opt.magnitudeScale;

            float  totalRadius = opt.iconSize / 2.0f + opt.borderThickness + 1.0f;
            ImVec2 textPos     = ImVec2(
                iconPos.x + opt.iconSize / 2.0f + totalRadius - textSize.x + static_cast<float>(opt.magnitudeOffsetX),
                iconPos.y + opt.iconSize / 2.0f + totalRadius - textSize.y + static_cast<float>(opt.magnitudeOffsetY));

            ImU32 strokeColor = IM_COL32(0, 0, 0, 255);
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    drawList->AddText(font, fontSize,
                        ImVec2(textPos.x + dx, textPos.y + dy), strokeColor, magText);
                }
            drawList->AddText(font, fontSize, textPos, effectColor, magText);
        }

        if (opt.showTimeText && effect.timeRemaining > 0.0f) {
            int totalSec = static_cast<int>(effect.timeRemaining);
            int hh       = totalSec / 3600;
            int mm       = (totalSec % 3600) / 60;
            int ss       = totalSec % 60;
            char timeStr[16];
            if (hh > 0)
                snprintf(timeStr, sizeof(timeStr), "%d:%02d:%02d", hh, mm, ss);
            else
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", mm, ss);

            ImFont* font     = ImGui::GetFont();
            float   fontSize = font->FontSize * opt.timeTextScale;
            ImVec2  textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, timeStr);
            ImVec2  textPos  = ImVec2(
                iconPos.x + (opt.iconSize - textSize.x) * 0.5f,
                iconPos.y + opt.iconSize + 8.0f);

            ImU32 strokeColor = IM_COL32(0, 0, 0, 200);
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    drawList->AddText(font, fontSize,
                        ImVec2(textPos.x + dx, textPos.y + dy), strokeColor, timeStr);
                }
            drawList->AddText(font, fontSize, textPos, opt.timeTextColor, timeStr);
        }

        return true;
    }

}
