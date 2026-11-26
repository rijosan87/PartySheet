#include "../../includes/ImGui/PlayerHUDResistances.h"
#include "../../includes/EffectCategories.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/Settings.h"
#include "../../includes/ActorStats.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/TextureManager.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/HudItem.h"
#include "../../includes/HUDSymptoms.h"
#include "../../includes/ImGui/WidgetDrawHelpers.h"
#include "../../includes/UIAnimation.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace PlayerHUDResistances {

    static bool g_ForcePos = false;

    static ImVec2 g_WindowSize(0.0f, 0.0f);

    void ForceReposition() { g_ForcePos = true; }

    static HUDFlow::Corner AnchorCorner() { return static_cast<HUDFlow::Corner>(Settings::GetSingleton()->prAnchor); }

    void KeepPlaceOnAnchorChange(int oldAnchor)
    {
        auto* settings = Settings::GetSingleton();
        const ImVec2 oldPos(static_cast<float>(settings->prPosX), static_cast<float>(settings->prPosY));
        const ImVec2 topLeft = HUDFlow::TopLeftFromAnchor(oldPos, g_WindowSize, static_cast<HUDFlow::Corner>(oldAnchor));
        const ImVec2 newPos = HUDFlow::AnchorFromTopLeft(topLeft, g_WindowSize, AnchorCorner());
        settings->prPosX = static_cast<int>(std::lround(newPos.x));
        settings->prPosY = static_cast<int>(std::lround(newPos.y));
        g_ForcePos = true;
    }

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

            bool isNegative = EffectCategories::IsDebuff(baseEffect, effect->magnitude);
            ResistState newState = isNegative ? ResistState::Debuff : ResistState::Buff;

            if (*target == ResistState::Debuff) continue;
            *target = newState;
        }

        return states;
    }

    struct ListRow {
        std::string   name;
        std::string   value;
        ImTextureID   icon = 0;
        std::uint32_t color = 0;
        ResistState   state = ResistState::Idle;
        bool          symptom = false;
    };

    static std::string RowName(const char* key, const char* english)
    {
        const std::string text = LocalizationManager::Get(key);
        return (text.empty() || text == key) ? std::string(english) : text;
    }

    static HUDFlow::Options FlowOptions()
    {
        const auto* settings = Settings::GetSingleton();
        HUDFlow::Options flow;
        flow.corner = AnchorCorner();
        flow.axis = settings->prLayout == 0 ? HUDFlow::Axis::Vertical : HUDFlow::Axis::Horizontal;
        flow.wrapCount = settings->prWrapAfter;
        flow.gapX = flow.gapY = ScalePx(static_cast<float>(settings->prSpacing));
        return flow;
    }

    static HudItem::Style ListStyle(const HUDDraw::IconFx& iconFx, const HUDDraw::TextFx& textFx)
    {
        const auto* settings = Settings::GetSingleton();
        const HUDFlow::Options flow = FlowOptions();
        HudItem::Style style;
        style.font = ImGui::GetFont();
        style.fontSize = ScalePx(style.font->LegacySize * settings->prFontScale);
        style.iconSize = ScalePx(static_cast<float>(settings->prIconSize));
        style.showCount = settings->prShowValue;
        style.pulseBad = false;
        style.padX = ScalePx(10.0f);
        style.padY = ScalePx(3.0f);
        style.discColor = settings->prDiscColor;
        style.stripColor = settings->prStripColor;
        style.discBorderColor = settings->prDiscBorderColor;
        style.discBorderWidth = settings->prDiscBorderWidth > 0
            ? (std::max)(1.0f, ScalePx(static_cast<float>(settings->prDiscBorderWidth))) : 0.0f;
        style.discRounding = static_cast<float>(settings->prDiscRadius) / 100.0f;
        style.iconFx = iconFx;
        style.textFx = textFx;

        constexpr std::uint32_t kNameColor = 0xFFF0F0F0;
        constexpr std::uint32_t kCountPlateTint = 0xE7444341;
        style.normalColor = kNameColor;
        style.countBadge = HUDDraw::BadgePresetStyle(HUDDraw::BadgePreset_Plain);
        HUDDraw::TintBadge(style.countBadge, kCountPlateTint);

        constexpr int kPennantStyle = 2;
        if (settings->prStyle == kPennantStyle) {
            style.pennant = true;
            style.clothDirection = settings->prClothDir > 0
                ? static_cast<HUDDraw::PennantDir>(settings->prClothDir - 1)
                : HudItem::AutoClothDirection(flow.corner, flow.axis);
            style.discSize = style.iconSize * (40.0f / 18.0f);
            style.clothWidth = style.iconSize * (26.0f / 18.0f);
            style.clothLength = style.iconSize * (50.0f / 18.0f);
        } else {
            style.discSize = style.iconSize * (46.0f / 30.0f);
            if (flow.axis == HUDFlow::Axis::Vertical) style.stripMinWidth = style.discSize * 2.5f;
        }
        return style;
    }

    static HudItem::Content ListItem(const ListRow& row)
    {
        HudItem::Content item;
        item.icon = row.icon;
        item.color = row.color;
        item.value = row.value.c_str();
        item.label = Settings::GetSingleton()->prShowNames ? row.name.c_str() : "";
        if (row.state == ResistState::Debuff) item.status = HudItem::Status::Bad;
        if (row.state == ResistState::Buff) item.status = HudItem::Status::Good;
        return item;
    }

    static void DrawList(ImDrawList* dl, const std::vector<ListRow>& rows, const HudItem::Style& style)
    {
        const HUDFlow::Options flow = FlowOptions();
        std::vector<HudItem::Content> items;
        std::vector<ImVec2> sizes;
        for (const ListRow& row : rows) {
            items.push_back(ListItem(row));
            sizes.push_back(HudItem::Measure(items.back(), style));
        }
        const ImVec2 size = HUDFlow::Arrange(sizes, flow).size;
        const ImVec2 topLeft = ImGui::GetCursorScreenPos();
        const HudItem::ListResult list = HudItem::DrawList(dl, HUDFlow::AnchorFromTopLeft(topLeft, size, flow.corner), items, style, flow);

        const float arrowH = style.iconSize * 0.7f;
        const float arrowW = arrowH * 0.55f;
        const float radius = style.discSize * 0.5f;
        const float side = HUDFlow::AwayFromAnchor(flow.corner).x;
        for (std::size_t i = 0; i < list.discCenters.size(); ++i) {
            if (!rows[i].symptom) continue;
            const ImVec2 center = list.discCenters[i];
            WidgetDraw::DrawSymptomArrow(dl, ImVec2(center.x + side * radius * 0.8f - arrowW * 0.5f, center.y - radius), arrowH);
        }
        ImGui::Dummy(size);
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
        const HUDFlow::Corner corner = AnchorCorner();
        const ImVec2 pivot(HUDFlow::IsLeft(corner) ? 0.0f : 1.0f, HUDFlow::IsTop(corner) ? 0.0f : 1.0f);
        const bool forcedThisFrame = g_ForcePos;
        ImGui::SetNextWindowPos(targetPos, (!allowDrag || forcedThisFrame) ? ImGuiCond_Always : ImGuiCond_FirstUseEver, pivot);
            g_ForcePos = false;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar  |
                                 ImGuiWindowFlags_NoResize     |
                                 ImGuiWindowFlags_NoScrollbar  |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoBackground;
        if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

        ImGui::Begin("##playerHUDresist", nullptr, flags);
        ImGui::SetWindowFontScale(settings->prFontScale);

        const ImVec2 windowSize = ImGui::GetWindowSize();
        const bool sizeChanged = windowSize.x != g_WindowSize.x || windowSize.y != g_WindowSize.y;
        g_WindowSize = windowSize;
        if (sizeChanged) g_ForcePos = true;

        const bool classic = settings->prStyle == 0;

        auto EndWindow = [&]() {
            ImGui::SetWindowFontScale(1.0f);
            if (allowDrag && !forcedThisFrame && !sizeChanged) {
                const ImVec2 dragged = HUDFlow::AnchorFromTopLeft(ImGui::GetWindowPos(), windowSize, corner);
                if (std::fabs(dragged.x - targetPos.x) > 1.5f || std::fabs(dragged.y - targetPos.y) > 1.5f) {
                    settings->prPosX = static_cast<int>(std::lround(dragged.x));
                    settings->prPosY = static_cast<int>(std::lround(dragged.y));
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        };

        if (classic && settings->prLayout != 2) {
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
        auto* iconFire      = TextureManager::GetPartyIcon("fire");
        auto* iconFrost     = TextureManager::GetPartyIcon("frost");
        auto* iconShock     = TextureManager::GetPartyIcon("shock");
        auto* iconMagic     = TextureManager::GetPartyIcon("magic");
        auto* iconPoison    = TextureManager::GetPartyIcon("poison");
        auto* iconDisease   = TextureManager::GetPartyIcon("disease");

        struct ResistEntry {
            const char*                  label;
            float                        value;
            const char*                  fmt;
            std::uint32_t                color;
            ID3D11ShaderResourceView*    icon;
            ResistState                  state;
            float                        rangeMax;
            bool                         symptom = false;
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

        {
            const auto& sr = HUDSymptoms::GetResist(player);
            const HUDSymptoms::Mark* marks[] = { &sr.armor, nullptr, &sr.fire, &sr.frost, &sr.shock,
                                                      &sr.magic, &sr.poison, &sr.disease, &sr.speed };
            for (std::size_t i = 0; i < entries.size() && i < std::size(marks); ++i) {
                if (!marks[i] || !marks[i]->on) continue;
                entries[i].symptom = true;
                entries[i].state   = ResistState::Debuff;
            }
        }
        const float symArrowH = iconSz * 0.7f;
        const float symArrowW = symArrowH * 0.55f;
        const ImU32 symRed    = IM_COL32(0xE0, 0x5A, 0x4A, 0xFF);
        bool  horiz   = (settings->prLayout == 1);

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
                float pulse = UIAnimation::Pulse(3.5f, 0.0f, 1.0f);
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
                cs.accentColor = 0xDCDC3C3Cu;
                cs.outerBorder = 0xFFFF5A5Au;
            } else if (st == ResistState::Buff) {
                cs.accentColor = 0xDC3CD25Au;
                cs.outerBorder = 0xFF5AFF78u;
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

        HUDDraw::TextFx textFx;
        textFx.shadow         = settings->prTextShadow;
        textFx.outline        = settings->prTextOutline;
        textFx.outlineOpacity = settings->prTextOutlineOpacity;
        textFx.duotone        = settings->prTextDuotone;

        auto DrawValueText = [&](const ImVec2& tp, ImU32 baseCol, const char* txt) {
            HUDDraw::DrawTextStyled(dl, tp, baseCol, txt, textFx);
        };

        if (!classic) {
            static constexpr const char* kNameKeys[] = { "Armor", "dmg_resist", "Fire", "Frost", "Shock",
                                                         "Magic", "Poison", "Disease", "Move" };
            std::vector<ListRow> rows;
            for (std::size_t i = 0; i < entries.size() && i < std::size(kNameKeys); ++i) {
                const ResistEntry& e = entries[i];
                char value[16];
                snprintf(value, sizeof(value), e.fmt, e.value);
                rows.push_back({ settings->prShowNames ? RowName(kNameKeys[i], e.label) : std::string(), value,
                                 reinterpret_cast<ImTextureID>(e.icon), e.color, e.state, e.symptom });
            }
            DrawList(dl, rows, ListStyle(iconFx, textFx));
            dl->PopClipRect();
            EndWindow();
            return;
        }

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
                    float pulse = UIAnimation::Pulse(3.5f, 0.0f, 1.0f);
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
                if (e.symptom)
                    WidgetDraw::DrawSymptomArrow(dl, ImVec2(slot.x + iconSz * 0.5f - symArrowW * 0.5f,
                                                            slot.y - iconSz * 0.5f - symArrowH * 0.5f), symArrowH);

                if (settings->prShowValue) {
                    ImU32  col2 = e.symptom ? symRed
                                            : IM_COL32((e.color >> 16) & 0xFF, (e.color >> 8) & 0xFF,
                                                       e.color & 0xFF, (e.color >> 24) & 0xFF);
                    char   valBuf[16];
                    snprintf(valBuf, sizeof(valBuf), e.fmt, e.value);
                    ImVec2 tsz = ImGui::CalcTextSize(valBuf);
                    ImVec2 tp(slot.x - tsz.x * 0.5f, slot.y + ringRadius + ringThick + ScalePx(1.0f));
                    DrawValueText(tp, col2, valBuf);
                }
            }

            dl->PopClipRect();
            EndWindow();
            return;
        }

        dl->ChannelsSplit(2);

        const bool  showValue = settings->prShowValue;
        const float lineH = ImGui::GetTextLineHeight();
        const float columnTextGap = ScalePx(4.0f);
        const float rowTextGap = ScalePx(3.0f);

        std::vector<ImVec2> cellSizes;
        for (const auto& e : entries) {
            char valBuf[16];
            snprintf(valBuf, sizeof(valBuf), e.fmt, e.value);
            const float textW = showValue ? ImGui::CalcTextSize(valBuf).x : 0.0f;
            if (horiz)
                cellSizes.emplace_back((std::max)(iconSz, textW), showValue ? iconSz + rowTextGap + lineH : iconSz);
            else
                cellSizes.emplace_back(showValue ? iconSz + columnTextGap + textW : iconSz,
                                       showValue ? (std::max)(iconSz, lineH) : iconSz);
        }
        HUDFlow::Options flow = FlowOptions();
        if (!horiz) flow.gapY += rowTextGap;
        const HUDFlow::Result layout = HUDFlow::Arrange(cellSizes, flow);
        const ImVec2 listPos = ImGui::GetCursorScreenPos();

        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            ImU32  col    = IM_COL32((e.color >> 16) & 0xFF, (e.color >> 8) & 0xFF,
                                      e.color & 0xFF, (e.color >> 24) & 0xFF);
            ImVec4 colV4  = ImGui::ColorConvertU32ToFloat4(col);
            char   valBuf[16];
            snprintf(valBuf, sizeof(valBuf), e.fmt, e.value);

            ImU32 valueTextCol = (settings->prBgTintEnabled && settings->prBgStyle == 1 && settings->prChipNeutralText)
                                     ? IM_COL32(235, 232, 222, 255)
                                     : ImGui::ColorConvertFloat4ToU32(colV4);
            if (e.symptom && !settings->prBgTintEnabled) valueTextCol = symRed;

            dl->ChannelsSetCurrent(1);

            const float fillFrac = (e.rangeMax > 0.0f)
                                       ? std::clamp(e.value / e.rangeMax, 0.0f, 1.0f)
                                       : 0.0f;

            const ImVec2 itemMin(listPos.x + layout.offsets[i].x, listPos.y + layout.offsets[i].y);
            const ImVec2 itemMax(itemMin.x + cellSizes[i].x, itemMin.y + cellSizes[i].y);
            const float textW = ImGui::CalcTextSize(valBuf).x;
            ImVec2 iconPos, textPos;

            if (horiz) {
                const float midX = (itemMin.x + itemMax.x) * 0.5f;
                iconPos = ImVec2(midX - iconSz * 0.5f, itemMin.y);
                textPos = ImVec2(midX - textW * 0.5f, itemMin.y + iconSz + rowTextGap);
            } else {
                const float midY = (itemMin.y + itemMax.y) * 0.5f;
                const bool  left = HUDFlow::IsLeft(corner);
                iconPos = ImVec2(left ? itemMin.x : itemMax.x - iconSz, midY - iconSz * 0.5f);
                textPos = ImVec2(left ? itemMin.x + iconSz + columnTextGap : itemMin.x, midY - lineH * 0.5f);
                }
            if (e.icon) DrawIconMeter(iconPos, e.icon, iconSz, e.color, fillFrac);
            if (showValue) DrawValueText(textPos, valueTextCol, valBuf);
            if (e.symptom) {
                const float inset = settings->prBgTintEnabled ? ScalePx(2.0f) : 0.0f;
                const float edge  = settings->prBgTintEnabled ? pad : 0.0f;
                WidgetDraw::DrawSymptomArrow(dl,
                    ImVec2(itemMax.x + edge - inset - symArrowW, itemMin.y - edge + inset + ScalePx(2.0f)), symArrowH);
            }
            if (settings->prBgTintEnabled) {
                dl->ChannelsSetCurrent(0);
                if (settings->prBgStyle == 1)
                    DrawChip(e.color, e.state, itemMin, itemMax);
                else
                    DrawBgTint(e.color, e.state, itemMin, itemMax);
                dl->ChannelsSetCurrent(1);
            }
        }
        ImGui::Dummy(layout.size);

        dl->ChannelsMerge();
        dl->PopClipRect();

        EndWindow();
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
