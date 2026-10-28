#include "../../includes/ImGui/PlayerHUDResistances.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/Settings.h"
#include "../../includes/ActorStats.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/TextureManager.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/HUDDraw.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>
#include <array>
#include <vector>

namespace PlayerHUDResistances {

    static bool g_ForcePos = false;

    void ForceReposition() { g_ForcePos = true; }

    static float MasterScale()
    {
        const auto* settings = Settings::GetSingleton();
        return settings ? std::clamp(static_cast<float>(settings->prScale) / 100.0f, 0.5f, 2.0f) : 1.0f;
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

    enum class ResistState { Idle, Buff, Debuff };

    struct ResistReactiveStates {
        ResistState fire    = ResistState::Idle;
        ResistState frost   = ResistState::Idle;
        ResistState shock   = ResistState::Idle;
        ResistState magic   = ResistState::Idle;
        ResistState poison  = ResistState::Idle;
        ResistState disease = ResistState::Idle;
        ResistState armor   = ResistState::Idle;
        ResistState speed   = ResistState::Idle;
    };

    static ResistReactiveStates GetResistReactiveStates(RE::Actor* actor) {
        ResistReactiveStates states;
        if (!actor) return states;

        auto* magicTarget = actor->AsMagicTarget();
        if (!magicTarget) return states;

        auto* effectList = magicTarget->GetActiveEffectList();
        if (!effectList) return states;

        for (auto* effect : *effectList) {
            if (!effect) continue;
            if (effect->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                effect->flags.any(RE::ActiveEffect::Flag::kInactive))
                continue;
            if (effect->duration <= 0.0f) continue;
            if ((effect->duration - effect->elapsedSeconds) <= 0.0f) continue;

            auto* baseEffect = effect->GetBaseObject();
            if (!baseEffect) continue;

            RE::ActorValue av = baseEffect->data.primaryAV;

            ResistState* target = nullptr;
            switch (av) {
                case RE::ActorValue::kResistFire:    target = &states.fire;    break;
                case RE::ActorValue::kResistFrost:   target = &states.frost;   break;
                case RE::ActorValue::kResistShock:   target = &states.shock;   break;
                case RE::ActorValue::kResistMagic:   target = &states.magic;   break;
                case RE::ActorValue::kPoisonResist:  target = &states.poison;  break;
                case RE::ActorValue::kResistDisease: target = &states.disease; break;
                case RE::ActorValue::kDamageResist:  target = &states.armor;   break;
                case RE::ActorValue::kSpeedMult:     target = &states.speed;   break;
                default: break;
            }
            if (!target) continue;

            bool isNegative = baseEffect->IsHostile() || baseEffect->IsDetrimental() || (effect->magnitude < 0.0f);
            ResistState newState = isNegative ? ResistState::Debuff : ResistState::Buff;

            if (*target == ResistState::Debuff) continue;
            *target = newState;
        }

        return states;
    }

    static void RenderWindow(bool allowDrag) {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerResistWidget) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;
        if (MenuUtils::ShouldBlockOverlay()) return;

        auto res    = GetActorResistances(player);
        auto states = GetResistReactiveStates(player);

        if (!PlayerHUDDisease::GetActiveDiseases().empty())
            states.disease = ResistState::Debuff;

        auto* avo = player->AsActorValueOwner();
        float armorRating = avo ? avo->GetActorValue(RE::ActorValue::kDamageResist) : 0.0f;
        // Physical damage reduction % (0.12% per point of armor rating, capped at 80%).
        float dmgResistPct = std::clamp(armorRating * 0.12f, 0.0f, 80.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ScalePx(10.0f), ScalePx(10.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(ScalePx(4.0f), ScalePx(3.0f)));

        const ImVec2 targetPos(static_cast<float>(settings->prPosX), static_cast<float>(settings->prPosY));
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

        ImGui::Begin("##playerHUDresist", nullptr, flags);
        ImGui::SetWindowFontScale(settings->prFontScale);

        if (settings->prLayout != 2) {
            ImU32 ribbonCol = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->prRibbonColor));
            if (ribbonCol & IM_COL32_A_MASK) {
                ImDrawList* rdl  = ImGui::GetWindowDrawList();
                ImVec2      rMin = ImGui::GetWindowPos();
                ImVec2      rMax = ImVec2(rMin.x + ImGui::GetWindowWidth(), rMin.y + ImGui::GetWindowHeight());

                if (settings->prRibbonBgStyle == 0) {
                    rdl->AddRectFilled(rMin, rMax, ribbonCol);
                } else {
                    HUDDraw::FadeBgStyle fs;
                    fs.mode      = static_cast<HUDDraw::FadeBgMode>(settings->prRibbonFadeMode);
                    fs.color     = settings->prRibbonColor;
                    fs.solidFrac = settings->prRibbonFadeAnchor / 100.0f;
                    fs.fadeFrac  = settings->prRibbonFadeLength / 100.0f;
                    HUDDraw::DrawFadeBg(rdl, rMin, rMax, fs);
                }
            }
        }

        float iconSz = ScalePx(static_cast<float>(settings->prIconSize));

        auto* iconArmor     = TextureManager::GetPartyIcon("block");
        auto* iconDmgResist = TextureManager::GetPartyIcon("damage_resist");
        auto* iconFire      = TextureManager::GetPartyIcon("fire_resist");
        auto* iconFrost     = TextureManager::GetPartyIcon("frost_resist");
        auto* iconShock     = TextureManager::GetPartyIcon("shock_resist");
        auto* iconMagic     = TextureManager::GetPartyIcon("magic_resist");
        auto* iconPoison    = TextureManager::GetPartyIcon("poison_resist");
        auto* iconDisease   = TextureManager::GetPartyIcon("disease_resist");

        struct ResistEntry {
            const char*                  label;
            float                        value;
            const char*                  fmt;
            std::uint32_t                color;
            ID3D11ShaderResourceView*    icon;
            ResistState                  state;
            float                        rangeMax;
        };
        std::vector<ResistEntry> entries = {
            { "Armor",   armorRating,  "%.0f",   settings->prArmorColor,     iconArmor,     states.armor,      500.0f },
            { "Phys",    dmgResistPct, "%.0f%%", settings->prDmgResistColor, iconDmgResist, ResistState::Idle,  80.0f },
            { "Fire",    res.fire,     "%.0f%%", settings->prFireColor,      iconFire,      states.fire,       100.0f },
            { "Frost",   res.frost,    "%.0f%%", settings->prFrostColor,     iconFrost,     states.frost,      100.0f },
            { "Shock",   res.shock,    "%.0f%%", settings->prShockColor,     iconShock,     states.shock,      100.0f },
            { "Magic",   res.magic,    "%.0f%%", settings->prMagicColor,     iconMagic,     states.magic,       85.0f },
            { "Poison",  res.poison,   "%.0f%%", settings->prPoisonColor,    iconPoison,    states.poison,     100.0f },
            { "Disease", res.disease,  "%.0f%%", settings->prDiseaseColor,   iconDisease,   states.disease,    100.0f },
        };

        if (settings->prShowSpeed) {
            float speedMult = avo ? avo->GetActorValue(RE::ActorValue::kSpeedMult) : 100.0f;
            entries.push_back({ "Speed", speedMult, "%.0f%%", settings->prSpeedColor,
                                TextureManager::GetPartyIcon("movespeed"), states.speed, 200.0f });
        }

        float spacing = ScalePx(static_cast<float>(settings->prSpacing));
        bool  horiz   = (settings->prLayout == 1);
        float textOff = (iconSz - ImGui::GetTextLineHeight()) * 0.5f;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->PushClipRectFullScreen();

        const float pad      = ScalePx(6.0f);   // breathing room between content and border/bg
        const float rounding = ScalePx(static_cast<float>(settings->prCornerRadius));

        auto DimTint = [](std::uint32_t argb, float factor, uint8_t alpha) -> ImU32 {
            uint8_t r = static_cast<uint8_t>(((argb >> 16) & 0xFF) * factor);
            uint8_t g = static_cast<uint8_t>(((argb >>  8) & 0xFF) * factor);
            uint8_t b = static_cast<uint8_t>(( argb        & 0xFF) * factor);
            return IM_COL32(r, g, b, alpha);
        };

        auto DrawBgTint = [&](std::uint32_t resistColor, ResistState st, const ImVec2& itemMin, const ImVec2& itemMax) {
            ImVec2 bMin(itemMin.x - pad, itemMin.y - pad);
            ImVec2 bMax(itemMax.x + pad, itemMax.y + pad);

            if (st == ResistState::Idle) {
                uint8_t alpha = static_cast<uint8_t>(std::clamp(settings->prBgTintOpacity, 0, 255));
                dl->AddRectFilled(bMin, bMax, DimTint(resistColor, 0.35f, alpha), rounding);
                return;
            }

            bool  isDebuff = (st == ResistState::Debuff);
            ImU32 wash;
            ImU32 barCol;
            if (isDebuff) {
                float pulse = 0.5f + 0.5f * sinf(static_cast<float>(ImGui::GetTime()) * 3.5f);
                uint8_t a   = static_cast<uint8_t>(180.0f * (0.6f + 0.4f * pulse));
                wash   = IM_COL32(90, 20, 20, a);
                barCol = IM_COL32(220, 60, 60, static_cast<uint8_t>(160.0f + 95.0f * pulse));
            } else {
                wash   = IM_COL32(20, 80, 40, 180);
                barCol = IM_COL32(60, 210, 90, 220);
            }

            dl->AddRectFilled(bMin, bMax, wash, rounding);

            // Top accent bar: top corners follow card rounding, bottom edge is flat
            dl->AddRectFilled(ImVec2(bMin.x, bMin.y), ImVec2(bMax.x, bMin.y + ScalePx(3.0f)),
                              barCol, rounding, ImDrawFlags_RoundCornersTop);
        };

        auto DrawChip = [&](std::uint32_t resistColor, ResistState st, const ImVec2& itemMin, const ImVec2& itemMax) {
            HUDDraw::ChipStyle cs;
            cs.rounding    = rounding;
            cs.topColor    = settings->prChipTopColor;
            cs.bottomColor = settings->prChipBottomColor;
            cs.shadow      = settings->prChipShadow;
            cs.highlight   = settings->prChipHighlight;
            cs.innerBorder = settings->prChipInnerBorderColor;
            cs.outerBorder = settings->prChipOuterBorderColor;
            cs.grainAlpha  = settings->prChipGrain ? settings->prChipGrainOpacity : 0;

            cs.accentColor = settings->prChipAccent ? (0xC8000000u | (resistColor & 0x00FFFFFFu)) : 0;
            if (st == ResistState::Debuff) {
                float pulse = 0.5f + 0.5f * sinf(static_cast<float>(ImGui::GetTime()) * 3.5f);
                auto a8  = [](float a) { return static_cast<std::uint32_t>(a) << 24; };
                cs.accentColor = a8(160.0f + 95.0f * pulse) | 0x00DC3C3Cu;
                cs.glowColor   = a8(60.0f + 90.0f * pulse)  | 0x00DC3C3Cu;
            } else if (st == ResistState::Buff) {
                cs.accentColor = 0xDC3CD25Au;
                cs.glowColor   = 0x5A3CD25Au;
            }

            HUDDraw::DrawChip(dl,
                ImVec2(itemMin.x - pad, itemMin.y - pad),
                ImVec2(itemMax.x + pad, itemMax.y + pad), cs);
        };

        HUDDraw::IconFx iconFx;
        iconFx.duotone      = settings->prIconDuotone;
        iconFx.shadow       = settings->prIconShadow;
        iconFx.outline      = settings->prIconOutline;
        iconFx.glow         = settings->prIconGlow;
        iconFx.gloss        = settings->prIconGloss;
        iconFx.outlineWidth = ScalePxInt(settings->prIconOutlineWidth);
        iconFx.outlineColor = settings->prIconOutlineColor;

        auto DrawIconMeter = [&](const ImVec2& pos, ID3D11ShaderResourceView* icon,
                                 float size, std::uint32_t argb, float frac) {
            if (!settings->prIconFill) {
                HUDDraw::DrawIconStyled(dl, pos, size, reinterpret_cast<ImTextureID>(icon), argb, iconFx);
                return;
            }
            ImU32 ghostCol = IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 38);
            dl->AddImage(reinterpret_cast<ImTextureID>(icon),
                         pos, ImVec2(pos.x + size, pos.y + size), ImVec2(0, 0), ImVec2(1, 1), ghostCol);
            if (frac > 0.0f) {
                float clipTop = pos.y + size * (1.0f - frac);
                dl->PushClipRect(ImVec2(pos.x, clipTop), ImVec2(pos.x + size, pos.y + size), true);
                HUDDraw::DrawIconStyled(dl, pos, size, reinterpret_cast<ImTextureID>(icon), argb, iconFx);
                dl->PopClipRect();
            }
        };

        auto DrawIconDuotone = [&](ID3D11ShaderResourceView* icon, float size, std::uint32_t argb, float frac) {
            DrawIconMeter(ImGui::GetCursorScreenPos(), icon, size, argb, frac);
            ImGui::Dummy(ImVec2(size, size));
        };

        HUDDraw::TextFx textFx;
        textFx.shadow         = settings->prTextShadow;
        textFx.outline        = settings->prTextOutline;
        textFx.outlineOpacity = settings->prTextOutlineOpacity;
        textFx.duotone        = settings->prTextDuotone;

        auto DrawValueText = [&](const ImVec2& tp, ImU32 baseCol, const char* txt) {
            HUDDraw::DrawTextStyled(dl, tp, baseCol, txt, textFx);
        };

        if (settings->prLayout == 2) {
            const int   N          = static_cast<int>(entries.size());
            const float radius     = ScalePx(static_cast<float>(settings->prRadialRadius));
            const float startDeg   = static_cast<float>(settings->prRadialStartAngle);
            const float ringThick  = ScalePx(3.5f);
            const float ringRadius = iconSz * 0.5f + ScalePx(6.0f);   // arc sits just outside the icon
            const float pad2       = ScalePx(8.0f);

            const float D = 2.0f * (radius + ringRadius + ringThick + pad2);
            ImVec2 dummyStart = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(D, D));
            ImVec2 center(dummyStart.x + D * 0.5f, dummyStart.y + D * 0.5f);

            for (int i = 0; i < N; ++i) {
                auto& e = entries[i];

                float angDeg = startDeg + (N > 0 ? (360.0f / N) * i : 0.0f);
                float angRad = angDeg * 3.14159265358979323846f / 180.0f;
                ImVec2 slot(center.x + cosf(angRad) * radius,
                            center.y + sinf(angRad) * radius);

                float frac = (e.rangeMax > 0.0f)
                                 ? std::clamp(e.value / e.rangeMax, 0.0f, 1.0f)
                                 : 0.0f;

                std::uint32_t ringArgb = 0xFF000000u | (e.color & 0x00FFFFFFu);

                if (e.state == ResistState::Buff) {
                    ringArgb = 0xFF3CD25Au;
                } else if (e.state == ResistState::Debuff) {
                    float pulse = 0.5f + 0.5f * sinf(static_cast<float>(ImGui::GetTime()) * 3.5f);
                    std::uint32_t a = static_cast<std::uint32_t>(160.0f + 95.0f * pulse);
                    ringArgb = (a << 24) | 0x00DC3C3Cu;
                }

                std::uint32_t trackArgb = 0x40000000u | (e.color & 0x00FFFFFFu);

                if (settings->prRadialShowRing) {
                    HUDDraw::RingSegment seg{ frac, 1.0f, ringArgb };
                    HUDDraw::ResourceRingStyle ringStyle;
                    ringStyle.radius       = ringRadius;
                    ringStyle.thickness    = ringThick;
                    ringStyle.startAngleDeg = -90.0f;
                    ringStyle.gapDeg       = 0.0f;
                    ringStyle.trackColor   = trackArgb;
                    ringStyle.segments     = &seg;
                    ringStyle.segmentCount = 1;
                    HUDDraw::DrawResourceRing(dl, slot, ringStyle);
                }

                if (e.icon)
                    DrawIconMeter(ImVec2(slot.x - iconSz * 0.5f, slot.y - iconSz * 0.5f),
                                  e.icon, iconSz, e.color, frac);

                if (settings->prShowValue) {
                    ImU32  col2 = IM_COL32((e.color >> 16) & 0xFF, (e.color >> 8) & 0xFF,
                                            e.color & 0xFF, (e.color >> 24) & 0xFF);
                    char   valBuf[16];
                    snprintf(valBuf, sizeof(valBuf), e.fmt, e.value);
                    ImVec2 tsz = ImGui::CalcTextSize(valBuf);
                    ImVec2 tp(slot.x - tsz.x * 0.5f, slot.y + ringRadius + ringThick + ScalePx(1.0f));
                    DrawValueText(tp, col2, valBuf);
                }
            }

            dl->PopClipRect();
            ImGui::SetWindowFontScale(1.0f);

            if (allowDrag) {
                ImVec2 winPos = ImGui::GetWindowPos();
                settings->prPosX = static_cast<int>(winPos.x);
                settings->prPosY = static_cast<int>(winPos.y);
            }

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            return;
        }

        dl->ChannelsSplit(2);

        bool firstItem = true;
        for (auto& e : entries) {
            ImU32  col    = IM_COL32((e.color >> 16) & 0xFF, (e.color >> 8) & 0xFF,
                                      e.color & 0xFF, (e.color >> 24) & 0xFF);
            ImVec4 colV4  = ImGui::ColorConvertU32ToFloat4(col);
            char   valBuf[16];
            snprintf(valBuf, sizeof(valBuf), e.fmt, e.value);

            ImU32 valueTextCol = (settings->prBgTintEnabled && settings->prBgStyle == 1 && settings->prChipNeutralText)
                                     ? IM_COL32(235, 232, 222, 255)
                                     : ImGui::ColorConvertFloat4ToU32(colV4);

            dl->ChannelsSetCurrent(1);

            const bool showValue = settings->prShowValue;

            const float fillFrac = (e.rangeMax > 0.0f)
                                       ? std::clamp(e.value / e.rangeMax, 0.0f, 1.0f)
                                       : 0.0f;

            if (horiz) {
                if (!firstItem) ImGui::SameLine(0.0f, spacing);
                ImGui::BeginGroup();
                float textW     = showValue ? ImGui::CalcTextSize(valBuf).x : 0.0f;
                float cellW     = (std::max)(iconSz, textW);
                float cellStart = ImGui::GetCursorPosX();

                float iconIndent = (cellW - iconSz) * 0.5f;
                if (iconIndent > 0.0f) ImGui::SetCursorPosX(cellStart + iconIndent);
                if (e.icon)
                    DrawIconDuotone(e.icon, iconSz, e.color, fillFrac);
                else
                    ImGui::Dummy(ImVec2(iconSz, iconSz));

                if (showValue) {
                    float textIndent = (cellW - textW) * 0.5f;
                    ImGui::SetCursorPosX(cellStart + textIndent);
                    ImVec2 textPos = ImGui::GetCursorScreenPos();
                    DrawValueText(textPos, valueTextCol, valBuf);
                    ImGui::Dummy(ImVec2(textW, ImGui::GetTextLineHeight()));
                }
                ImGui::EndGroup();
            } else {
                if (!firstItem) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);
                ImGui::BeginGroup();
                if (e.icon)
                    DrawIconDuotone(e.icon, iconSz, e.color, fillFrac);
                else
                    ImGui::Dummy(ImVec2(iconSz, iconSz));
                if (showValue) {
                    ImGui::SameLine(0.0f, ScalePx(4.0f));
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOff);
                    ImVec2 textPos = ImGui::GetCursorScreenPos();
                    float textW2 = ImGui::CalcTextSize(valBuf).x;
                    DrawValueText(textPos, valueTextCol, valBuf);
                    ImGui::Dummy(ImVec2(textW2, ImGui::GetTextLineHeight()));
                }
                ImGui::EndGroup();
            }

            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            if (settings->prBgTintEnabled) {
                dl->ChannelsSetCurrent(0);
                if (settings->prBgStyle == 1)
                    DrawChip(e.color, e.state, itemMin, itemMax);
                else
                    DrawBgTint(e.color, e.state, itemMin, itemMax);
                dl->ChannelsSetCurrent(1);
            }

            firstItem = false;
        }

        dl->ChannelsMerge();
        dl->PopClipRect();

        ImGui::SetWindowFontScale(1.0f);

        if (allowDrag) {
            ImVec2 winPos = ImGui::GetWindowPos();
            settings->prPosX = static_cast<int>(winPos.x);
            settings->prPosY = static_cast<int>(winPos.y);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    void RenderHUD() {
        auto* settings = Settings::GetSingleton();
        if (!settings->enablePlayerHud || !settings->enablePlayerResistWidget) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) return;
        if (!HUDVisibility::IsVisible(HUDVisibility::WidgetID::PlayerResist)) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerResist));
        RenderWindow(false);
        ImGui::PopStyleVar();
        ImGui::EndFrame();
        ImGui::Render();
        HUDVisibility::ApplyAlphaToDrawData(HUDVisibility::GetAlpha(HUDVisibility::WidgetID::PlayerResist));
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
    }

    void RenderPreview() { RenderWindow(true); }

}
