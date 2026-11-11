#pragma once

#include "../Settings.h"
#include "../EffectCategories.h"
#include "../Utils/HUDDraw.h"

#include <imgui.h>
#include <RE/Skyrim.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

struct ID3D11ShaderResourceView;

namespace PartySheetV2::Tip {

    inline constexpr ImU32 kAccent    = IM_COL32(212, 175,  55, 255);
    inline constexpr ImU32 kTextMain  = IM_COL32(240, 240, 240, 255);
    inline constexpr ImU32 kTextMuted = IM_COL32(136, 136, 136, 255);
    inline constexpr ImU32 kBorderDim = IM_COL32( 50,  50,  60, 255);
    inline constexpr ImU32 kBuff      = IM_COL32(100, 200, 120, 255);
    inline constexpr ImU32 kDebuff    = IM_COL32(220,  80,  80, 255);

    inline HUDDraw::IconFx IconFx() {
        auto* s = Settings::GetSingleton();
        HUDDraw::IconFx fx{};
        fx.duotone      = s->psV2IconDuotone;
        fx.shadow       = s->psV2IconShadow;
        fx.outline      = s->psV2IconOutline;
        fx.outlineWidth = s->psV2IconOutlineWidth;
        fx.outlineColor = s->psV2IconOutlineColor;
        fx.glow         = false;
        fx.gloss        = false;
        return fx;
    }

    struct TipLine { std::string text; ImU32 col; };
    struct TipRow  {
        ID3D11ShaderResourceView* icon = nullptr;
        std::string name, type;
        std::vector<TipLine> lines;
        int tone = 0;
    };

    inline void DrawCompactTooltip(const std::string& title, const std::vector<TipRow>& rows,
                                   const std::string& emptyText, float sc) {
        auto* s = Settings::GetSingleton();
        const float fBody  = s->psV2FontBody  * sc;
        const float fSmall = s->psV2FontSmall * sc;
        const float iconSz  = 32.0f * sc;
        const float textGap = 10.0f * sc;
        const float rowGap  = 8.0f  * sc;
        const float lineGap = 2.0f  * sc;
        const float wrapW   = 300.0f * sc;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(20, 23, 29, 245));
        ImGui::PushStyleColor(ImGuiCol_Border,  kBorderDim);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * sc, 10.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.0f, 0.0f));
        ImGui::BeginTooltip();
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float baseSz = ImGui::GetFontSize();
        auto Measure = [&](float scale, const std::string& t, float wrap = 0.0f) {
            return font->CalcTextSizeA(baseSz * scale, FLT_MAX, wrap, t.c_str());
        };

        float rowW = Measure(fSmall, title).x;
        for (const auto& r : rows) {
            float w = Measure(fBody, r.name).x;
            if (!r.type.empty()) w += textGap * 0.5f + Measure(fSmall, r.type).x;
            for (const auto& l : r.lines) w = (std::max)(w, (std::min)(Measure(fSmall, l.text).x + 4.0f * sc, wrapW));
            rowW = (std::max)(rowW, iconSz + textGap + w);
        }
        if (rows.empty()) rowW = (std::max)(rowW, Measure(fSmall, emptyText).x);
        const float textW = rowW - iconSz - textGap;

        ImVec2 p = ImGui::GetCursorScreenPos();
        const float subH = Measure(fSmall, "A").y;
        tdl->AddText(font, baseSz * fSmall, p, kAccent, title.c_str());
        float ruleY = p.y + subH + 4.0f * sc;
        tdl->AddLine(ImVec2(p.x, ruleY), ImVec2(p.x + rowW, ruleY), IM_COL32(212, 175, 55, 70), 1.0f);
        ImGui::Dummy(ImVec2(rowW, subH + 10.0f * sc));

        if (rows.empty()) {
            ImVec2 ep = ImGui::GetCursorScreenPos();
            tdl->AddText(font, baseSz * fSmall, ep, kTextMuted, emptyText.c_str());
            ImGui::Dummy(ImVec2(rowW, subH));
        }

        const float titleH = Measure(fBody, "A").y;
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            float textH = titleH;
            for (const auto& l : r.lines) textH += lineGap + Measure(fSmall, l.text, textW).y;
            const float rowH = (std::max)(iconSz, textH);
            ImVec2 rp = ImGui::GetCursorScreenPos();
            if (r.tone != 0)
                tdl->AddRectFilled(ImVec2(rp.x - 8.0f * sc, rp.y + 2.0f * sc),
                    ImVec2(rp.x - 5.0f * sc, rp.y + rowH - 2.0f * sc),
                    r.tone > 0 ? kBuff : kDebuff, 1.0f * sc);
            if (r.icon)
                HUDDraw::DrawIconStyled(tdl, ImVec2(rp.x, rp.y + (rowH - iconSz) * 0.5f), iconSz,
                    reinterpret_cast<ImTextureID>(r.icon), 0xFFFFFFFF, IconFx());

            float tx = rp.x + iconSz + textGap;
            float ty = rp.y + (rowH - textH) * 0.5f;
            tdl->AddText(font, baseSz * fBody, ImVec2(tx, ty), kTextMain, r.name.c_str());
            if (!r.type.empty()) {
                float nameW = Measure(fBody, r.name).x;
                tdl->AddText(font, baseSz * fSmall, ImVec2(tx + nameW + textGap * 0.5f, ty + titleH - subH),
                    kTextMuted, r.type.c_str());
            }
            ty += titleH;
            for (const auto& l : r.lines) {
                ty += lineGap;
                tdl->AddText(font, baseSz * fSmall, ImVec2(tx, ty), l.col, l.text.c_str(), nullptr, textW);
                ty += Measure(fSmall, l.text, textW).y;
            }
            ImGui::Dummy(ImVec2(rowW, rowH + (i + 1 < rows.size() ? rowGap : 0.0f)));
        }

        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    inline bool IsKye2ArmorSource(RE::MagicItem* spell) {
        if (!spell) return false;
        auto* file = spell->GetFile(0);
        if (!file) return false;
        std::string_view fn = file->GetFilename();
        constexpr std::string_view kKye2Armors = "know_your_enemy_2_armors.esp";
        if (fn.size() != kKye2Armors.size()) return false;
        for (size_t i = 0; i < fn.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(fn[i])) !=
                std::tolower(static_cast<unsigned char>(kKye2Armors[i])))
                return false;
        return true;
    }

    inline float DisplayMagnitude(const RE::EffectSetting* base, float mag) {
        mag = std::abs(mag);
        if (base && EffectCategories::IsPercentageMagnitude(base->data.primaryAV) && mag > 0.0f && mag < 1.0f)
            mag *= 100.0f;
        return mag;
    }

    inline std::string DescribeEffect(const RE::EffectSetting* base, float mag, float dur, bool hostile) {
        if (!base) return {};
        auto Num = [](float v) {
            char b[16];
            if (std::abs(v - std::round(v)) < 0.05f) snprintf(b, sizeof(b), "%.0f", v);
            else                                      snprintf(b, sizeof(b), "%.1f", v);
            return std::string(b);
        };
        const char* raw = base->magicItemDescription.c_str();
        if (raw && raw[0]) {
            std::string d = raw;
            auto Sub = [&](std::string_view tag, const std::string& val) {
                for (size_t p = d.find(tag); p != std::string::npos; p = d.find(tag, p + val.size()))
                    d.replace(p, tag.size(), val);
            };
            Sub("<mag>", Num(mag));
            Sub("<dur>", Num(dur));
            d.erase(std::remove(d.begin(), d.end(), '<'), d.end());
            d.erase(std::remove(d.begin(), d.end(), '>'), d.end());
            return d;
        }
        const char* nm = base->GetFullName();
        if (!nm || !nm[0] || mag < 0.5f) return {};
        const bool pct = EffectCategories::IsPercentageMagnitude(base->data.primaryAV);
        char buf[160];
        snprintf(buf, sizeof(buf), "%s %s%s%s", nm, hostile ? "-" : "+", Num(mag).c_str(), pct ? "%" : "");
        return buf;
    }

}
