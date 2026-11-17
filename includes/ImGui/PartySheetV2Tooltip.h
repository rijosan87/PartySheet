#pragma once

#include "../Settings.h"
#include "../ActorStats.h"
#include "../EffectCategories.h"
#include "../LocalizationManager.h"
#include "../Utils/HUDDraw.h"
#include "../Utils/EquipmentStatResolver.h"
#include "../Utils/SpellDamageResolver.h"
#include "../BadgeStyle.h"

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
        std::string section;
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
            if (!r.section.empty()) rowW = (std::max)(rowW, Measure(fSmall, r.section).x);
        }
        if (rows.empty()) rowW = (std::max)(rowW, Measure(fSmall, emptyText).x);
        const float textW = rowW - iconSz - textGap;

        ImVec2 p = ImGui::GetCursorScreenPos();
        const float subH = Measure(fSmall, "A").y;
        tdl->AddText(font, baseSz * fSmall, p, kAccent, title.c_str());
        float ruleY = p.y + subH + 4.0f * sc;
        HUDDraw::DrawRuleH(tdl, p.x, p.x + rowW, ruleY, IM_COL32(212, 175, 55, 70));
        ImGui::Dummy(ImVec2(rowW, subH + 10.0f * sc));

        if (rows.empty()) {
            ImVec2 ep = ImGui::GetCursorScreenPos();
            tdl->AddText(font, baseSz * fSmall, ep, kTextMuted, emptyText.c_str());
            ImGui::Dummy(ImVec2(rowW, subH));
        }

        const float titleH = Measure(fBody, "A").y;
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];

            if (!r.section.empty() && (i == 0 || rows[i - 1].section != r.section)) {
                if (i > 0) {
                    ImVec2 sp = ImGui::GetCursorScreenPos();
                    HUDDraw::DrawRuleH(tdl, sp.x, sp.x + rowW, sp.y + 2.0f * sc, IM_COL32(255, 255, 255, 40));
                    ImGui::Dummy(ImVec2(rowW, 8.0f * sc));
                }
                ImVec2 hp = ImGui::GetCursorScreenPos();
                tdl->AddText(font, baseSz * fSmall, hp, kTextMuted, r.section.c_str());
                ImGui::Dummy(ImVec2(rowW, subH + 6.0f * sc));
            }
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

    struct SumLine { std::string label, note, value; ImU32 col; bool ruleAbove = false; };

    inline void DrawSumTooltip(const std::string& title, const std::vector<SumLine>& lines, float sc,
                               const std::vector<std::string>& footer = {}) {
        auto* s = Settings::GetSingleton();
        const float fBody  = s->psV2FontBody  * sc;
        const float fSmall = s->psV2FontSmall * sc;
        const float noteGap  = 6.0f  * sc;
        const float valueGap = 24.0f * sc;
        const float lineGap  = 4.0f  * sc;
        const float ruleGap  = 6.0f  * sc;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(20, 23, 29, 245));
        ImGui::PushStyleColor(ImGuiCol_Border,  kBorderDim);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * sc, 10.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.0f, 0.0f));
        ImGui::BeginTooltip();
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float baseSz = ImGui::GetFontSize();
        auto Measure = [&](float scale, const std::string& t) { return font->CalcTextSizeA(baseSz * scale, FLT_MAX, 0.0f, t.c_str()); };

        std::string t = title;
        for (char& c : t) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

        float leftW = 0.0f, valueW = 0.0f;
        for (const auto& l : lines) {
            float w = Measure(fBody, l.label).x;
            if (!l.note.empty()) w += noteGap + Measure(fSmall, l.note).x;
            leftW  = (std::max)(leftW, w);
            valueW = (std::max)(valueW, Measure(fBody, l.value).x);
        }
        float rowW = (std::max)(Measure(fSmall, t).x, leftW + valueGap + valueW);
        const float wrapW = 300.0f * sc;
        for (const auto& f : footer) rowW = (std::max)(rowW, (std::min)(Measure(fSmall, f).x + 4.0f * sc, wrapW));
        auto MeasureWrapped = [&](const std::string& f) { return font->CalcTextSizeA(baseSz * fSmall, FLT_MAX, rowW, f.c_str()).y; };

        ImVec2 p = ImGui::GetCursorScreenPos();
        const float subH  = Measure(fSmall, "A").y;
        const float bodyH = Measure(fBody, "A").y;
        tdl->AddText(font, baseSz * fSmall, p, kAccent, t.c_str());
        HUDDraw::DrawRuleH(tdl, p.x, p.x + rowW, p.y + subH + 4.0f * sc, IM_COL32(212, 175, 55, 70));
        ImGui::Dummy(ImVec2(rowW, subH + 10.0f * sc));

        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& l = lines[i];
            if (l.ruleAbove) {
                ImVec2 rp = ImGui::GetCursorScreenPos();
                HUDDraw::DrawRuleH(tdl, rp.x, rp.x + rowW, rp.y + ruleGap * 0.5f, IM_COL32(255, 255, 255, 40));
                ImGui::Dummy(ImVec2(rowW, ruleGap));
            }
            ImVec2 lp = ImGui::GetCursorScreenPos();
            tdl->AddText(font, baseSz * fBody, lp, l.ruleAbove ? kTextMain : kTextMuted, l.label.c_str());
            if (!l.note.empty())
                tdl->AddText(font, baseSz * fSmall,
                    ImVec2(lp.x + Measure(fBody, l.label).x + noteGap, lp.y + bodyH - subH), kTextMuted, l.note.c_str());
            const float vw = Measure(fBody, l.value).x;
            tdl->AddText(font, baseSz * fBody, ImVec2(lp.x + rowW - vw, lp.y), l.col, l.value.c_str());
            ImGui::Dummy(ImVec2(rowW, bodyH + (i + 1 < lines.size() ? lineGap : 0.0f)));
        }

        if (!footer.empty()) {
            if (!lines.empty()) {
                ImVec2 rp = ImGui::GetCursorScreenPos();
                HUDDraw::DrawRuleH(tdl, rp.x, rp.x + rowW, rp.y + ruleGap * 0.5f + lineGap, IM_COL32(255, 255, 255, 40));
                ImGui::Dummy(ImVec2(rowW, ruleGap + lineGap));
            }
            for (size_t i = 0; i < footer.size(); ++i) {
                ImVec2 fp = ImGui::GetCursorScreenPos();
                tdl->AddText(font, baseSz * fSmall, fp, kTextMuted, footer[i].c_str(), nullptr, rowW);
                ImGui::Dummy(ImVec2(rowW, MeasureWrapped(footer[i]) + (i + 1 < footer.size() ? lineGap : 0.0f)));
    }

}

        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    inline void DrawWeaponTooltip(const std::string& title,
                                  const EquipmentStatResolver::WeaponDamage& wd, float sc) {
        if (!wd.valid) return;
        std::vector<SumLine> lines;
        char buf[48];
        auto Add = [&](std::string label, std::string note, const char* fmt, float v, ImU32 col, bool rule = false) {
            snprintf(buf, sizeof(buf), fmt, v);
            lines.push_back({ std::move(label), std::move(note), buf, col, rule });
        };
        auto Mult = [&](const char* key, std::string note, float m) {
            if (std::abs(m - 1.0f) < 0.005f) return;
            Add(LocalizationManager::Get(key), std::move(note), "x%.2f", m, m > 1.0f ? kBuff : kDebuff);
        };

        Add(LocalizationManager::Get("Stats.DmgBase"), {}, "%.0f", wd.weaponBase, kTextMain);
        if (wd.ammoBase > 0.0f) Add(LocalizationManager::Get("Stats.DmgAmmo"), {}, "+%.0f", wd.ammoBase, kTextMain);
        snprintf(buf, sizeof(buf), "%.0f", wd.skillLevel);
        std::string skillNote = buf;
        if (wd.skill != RE::ActorValue::kNone)
            skillNote = EffectCategories::LocalizedAVName(wd.skill) + " " + skillNote;
        Mult("Stats.DmgSkill", std::move(skillNote), wd.skillMult);
        Mult("Stats.DmgFortify", {}, wd.fortifyMult);
        Mult("Stats.DmgPerks", {}, wd.perkMult);
        Mult("Stats.DmgSetting", {}, wd.settingMult);
        if (wd.tempered) Mult("Stats.DmgTempering", {}, wd.otherMult);
        else             Mult("Stats.DmgOther", LocalizationManager::Get("Stats.DmgOtherNote"), wd.otherMult);
        Add(LocalizationManager::Get("Stats.DmgPhysical"), {}, "%.0f", wd.physical, kTextMain, true);
        for (const auto& e : wd.enchants)
            Add(e.name.empty() ? LocalizationManager::Get("Stats.DmgEnchant") : e.name, {}, "+%.0f",
                e.magnitude, BadgeStyle::GetEnchantColors(e.type).border);

        DrawSumTooltip(title, lines, sc);
    }

    inline std::vector<std::string> SpellDescriptions(const RE::SpellItem* spell) {
        std::vector<std::string> out;
        if (!spell) return out;
        for (auto* eff : spell->effects) {
            auto* base = eff ? eff->baseEffect : nullptr;
            if (!base || base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
            auto d = DescribeEffect(base, DisplayMagnitude(base, eff->effectItem.magnitude),
                                    static_cast<float>(eff->effectItem.duration), false);
            if (!d.empty() && std::find(out.begin(), out.end(), d) == out.end()) out.push_back(std::move(d));
        }
        return out;
    }

    inline void DrawSpellTooltip(const std::string& title, const RE::SpellItem* spell,
                                 const SpellDamageResolver::SpellDamage& sd, float sc) {
        const auto* p = sd.Primary();
        if (!p) return;
        std::vector<SumLine> lines;
        char buf[48];
        auto Add = [&](std::string label, std::string note, const char* fmt, float v, ImU32 col, bool rule = false) {
            snprintf(buf, sizeof(buf), fmt, v);
            lines.push_back({ std::move(label), std::move(note), buf, col, rule });
        };
        auto Mult = [&](const char* key, std::string note, float m) {
            if (std::abs(m - 1.0f) < 0.005f) return;
            Add(LocalizationManager::Get(key), std::move(note), "x%.2f", m, m > 1.0f ? kBuff : kDebuff);
        };

        const auto school = p->school != RE::ActorValue::kNone ? p->school : sd.school;
        Add(LocalizationManager::Get("Stats.DmgSpellBase"),
            school != RE::ActorValue::kNone ? EffectCategories::LocalizedAVName(school) : std::string{},
            "%.0f", p->baseMagnitude, kTextMain);
        Mult("Stats.DmgPerks", {}, p->perkMult);
        Mult("Stats.DmgFortify", {}, p->fortifyMult);

        bool first = true;
        for (const auto& e : sd.effects) {
            std::string note;
            if (sd.isConcentration) {
                note = LocalizationManager::Get("Stats.DmgPerSecond");
            } else if (e.duration > 1.0f) {
                char nb[48];
                snprintf(nb, sizeof(nb), "%.0f/s x %.0fs", e.magnitude, e.duration);
                note = nb;
            }
            if (e.conditional) {
                if (!note.empty()) note += ", ";
                note += LocalizationManager::Get("Stats.DmgConditional");
            }
            const ImU32 col = e.conditional ? kTextMuted : BadgeStyle::GetEnchantColors(e.type).border;
            Add(e.name.empty() ? LocalizationManager::Get("Stats.DmgEnchant") : e.name, std::move(note), "%.0f",
                e.Total(sd.isConcentration), col, first);
            first = false;
        }
        DrawSumTooltip(title, lines, sc, SpellDescriptions(spell));
    }
}
