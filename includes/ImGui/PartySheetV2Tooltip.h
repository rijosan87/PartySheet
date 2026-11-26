#pragma once

#include "../Settings.h"
#include "../ActorStats.h"
#include "../EffectCategories.h"
#include "../LocalizationManager.h"
#include "../Utils/HUDDraw.h"
#include "../Utils/EquipmentStatResolver.h"
#include "../Utils/SpellDamageResolver.h"
#include "../BadgeStyle.h"
#include "../SpellIconManager.h"

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

    inline constexpr bool kChipTooltips = true;
    inline ImU32 g_slotChipBase = IM_COL32(20, 23, 29, 255);
    inline constexpr float kTipChipDarken = 0.45f;
    inline constexpr float kTileChipDarkenDim = 0.25f;
    inline constexpr float kPanelChipDarken   = 0.20f;

    inline void PushTipColors() {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, kChipTooltips ? IM_COL32(0, 0, 0, 0) : IM_COL32(20, 23, 29, 245));
        ImGui::PushStyleColor(ImGuiCol_Border,  kChipTooltips ? IM_COL32(0, 0, 0, 0) : kBorderDim);
    }

    inline ImDrawList* g_hoverList = nullptr;
    inline std::size_t g_hoverId   = 0;

    inline void NoteHover(const std::string& id) {
        g_hoverList = ImGui::GetWindowDrawList();
        g_hoverId   = std::hash<std::string>{}(id);
    }

    inline void BeginTipChip() {
        if (kChipTooltips) {
            ImGui::GetWindowDrawList()->ChannelsSplit(2);
            ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
        }
    }

    inline void EndTipChip(float sc) {
        if (!kChipTooltips) return;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->ChannelsSetCurrent(0);
        const ImVec2 tl = ImGui::GetWindowPos();
        const ImVec2 br(tl.x + ImGui::GetWindowWidth(), tl.y + ImGui::GetWindowHeight());
        auto chip = HUDDraw::PanelChipFromColor(HUDDraw::PackedToArgb(g_slotChipBase), kTipChipDarken, 0.0f, 3.0f * sc);
        chip.topColor |= 0xFF000000u;
        HUDDraw::DrawChip(dl, tl, br, chip);
        HUDDraw::DrawOrnateFrame(dl, tl, br, sc);
        dl->ChannelsMerge();
    }

    struct TipLine { std::string text; ImU32 col; };
    struct TipRow  {
        ID3D11ShaderResourceView* icon = nullptr;
        std::string name, type;
        std::vector<TipLine> lines;
        int tone = 0;
        std::string section;
        std::uint32_t iconTint = 0xFFFFFFFF;
    };

    inline constexpr float kTipWidth = 320.0f;

    struct TipHero {
        std::string category;
        ID3D11ShaderResourceView* icon = nullptr;
        std::uint32_t iconTint = 0xFFFFFFFF;
        std::string name, subtitle;
        std::string hint;
    };

    inline std::string Upper(std::string t) {
        for (char& c : t) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        return t;
    }

    inline void DrawTipHero(const TipHero& hero, float rowW, float sc, bool hasBody) {
        auto* s = Settings::GetSingleton();
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float baseSz = ImGui::GetFontSize();
        const float fBody  = s->psV2FontBody  * sc;
        const float fSmall = s->psV2FontSmall * sc;
        const float iconSz = 40.0f * sc;
        const float gap    = 10.0f * sc;
        const float smallH = font->CalcTextSizeA(baseSz * fSmall, FLT_MAX, 0.0f, "A").y;

        ImVec2 p = ImGui::GetCursorScreenPos();
        float y = p.y;
        if (!hero.category.empty()) {
            tdl->AddText(font, baseSz * fSmall, p, kAccent, Upper(hero.category).c_str());
            if (!hero.hint.empty()) {
                const float hw = font->CalcTextSizeA(baseSz * fSmall, FLT_MAX, 0.0f, hero.hint.c_str()).x;
                tdl->AddText(font, baseSz * fSmall, ImVec2(p.x + rowW - hw, p.y), kTextMuted, hero.hint.c_str());
            }
            y += smallH + 6.0f * sc;
        }

        const float textX = p.x + (hero.icon ? iconSz + gap : 0.0f);
        const float textW = p.x + rowW - textX;
        const float nameH = font->CalcTextSizeA(baseSz * fBody, FLT_MAX, textW, hero.name.c_str()).y;
        const float subH  = hero.subtitle.empty() ? 0.0f
            : 2.0f * sc + font->CalcTextSizeA(baseSz * fSmall, FLT_MAX, textW, hero.subtitle.c_str()).y;
        const float blockH = (std::max)(hero.icon ? iconSz : 0.0f, nameH + subH);
        if (hero.icon)
            HUDDraw::DrawIconStyled(tdl, ImVec2(p.x, y + (blockH - iconSz) * 0.5f), iconSz,
                reinterpret_cast<ImTextureID>(hero.icon), hero.iconTint, IconFx());
        float ty = y + (blockH - nameH - subH) * 0.5f;
        tdl->AddText(font, baseSz * fBody, ImVec2(textX, ty), kTextMain, hero.name.c_str(), nullptr, textW);
        if (!hero.subtitle.empty())
            tdl->AddText(font, baseSz * fSmall, ImVec2(textX, ty + nameH + 2.0f * sc), kTextMuted,
                hero.subtitle.c_str(), nullptr, textW);
        y += blockH;

        if (hasBody) HUDDraw::DrawRuleH(tdl, p.x, p.x + rowW, y + 6.0f * sc, IM_COL32(212, 175, 55, 70));
        ImGui::Dummy(ImVec2(rowW, y - p.y + (hasBody ? 12.0f * sc : 0.0f)));
    }

    inline constexpr bool kSplitColumns = true;

    inline constexpr float kColumnShare    = 0.60f;
    inline constexpr float kMaxColumnShare = 0.88f;

    inline bool StartsSection(const std::vector<TipRow>& rows, size_t i) {
        return i == 0 || rows[i - 1].section != rows[i].section;
    }

    inline std::vector<size_t> ColumnStarts(const std::vector<TipRow>& rows, const std::vector<float>& heights,
                                            float maxH, float headingH) {
        std::vector<size_t> starts = { 0 };
        float used = 0.0f;
        for (size_t i = 0; i < rows.size(); ++i) {
            float sectionH = 0.0f;
            if (StartsSection(rows, i))
                for (size_t j = i; j < rows.size() && rows[j].section == rows[i].section; ++j) sectionH += heights[j];
            const bool moveSection = used + sectionH > maxH && sectionH <= maxH && sectionH > 0.0f;
            const bool rowOverflows = used + heights[i] > maxH;
            if (used > 0.0f && (moveSection || rowOverflows)) {
                starts.push_back(i);
                used = StartsSection(rows, i) ? 0.0f : headingH;
            }
            used += heights[i];
        }
        return starts;
    }

    inline void DrawCompactTooltip(const std::string& title, const std::vector<TipRow>& rows,
                                   const std::string& emptyText, float sc,
                                   const std::string& subtitle = {}, const TipHero& hero = {},
                                   bool splitColumns = false) {
        auto* s = Settings::GetSingleton();
        const float fBody  = s->psV2FontBody  * sc;
        const float fSmall = s->psV2FontSmall * sc;
        const float iconSz  = 32.0f * sc;
        const float textGap = 10.0f * sc;
        const float rowGap  = 8.0f  * sc;
        const float lineGap = 2.0f  * sc;

        PushTipColors();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * sc, 10.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.0f, 0.0f));
        ImGui::BeginTooltip();
        BeginTipChip();
        NoteHover(title + "|" + hero.category + "|" + hero.name);
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float baseSz = ImGui::GetFontSize();
        auto Measure = [&](float scale, const std::string& t, float wrap = 0.0f) {
            return font->CalcTextSizeA(baseSz * scale, FLT_MAX, wrap, t.c_str());
        };

        float rowW = kTipWidth * sc;
        for (const auto& r : rows) {
            float w = Measure(fBody, r.name).x;
            if (!r.type.empty()) w += textGap * 0.5f + Measure(fSmall, r.type).x;
            rowW = (std::max)(rowW, iconSz + textGap + w);
        }

        const float subH = Measure(fSmall, "A").y;
        const float titleH = Measure(fBody, "A").y;
        const float headingH   = subH + 6.0f * sc;
        const float sectionGap = 8.0f * sc;
        const float columnGap  = 32.0f * sc;

        auto TextHeight = [&](const TipRow& r) {
            const float textW = rowW - (r.icon ? iconSz + textGap : 0.0f);
            float h = !r.icon && r.name.empty() ? -lineGap : titleH;
            for (const auto& l : r.lines) h += lineGap + Measure(fSmall, l.text, textW).y;
            return h;
        };
        auto RowHeight = [&](const TipRow& r) {
            return r.icon ? (std::max)(iconSz, TextHeight(r)) : TextHeight(r);
        };

        std::vector<size_t> columns = { 0 };
        if (splitColumns) {
            std::vector<float> heights;
            for (size_t i = 0; i < rows.size(); ++i) {
                float h = RowHeight(rows[i]) + rowGap;
                if (!rows[i].section.empty() && StartsSection(rows, i)) h += headingH + sectionGap;
                heights.push_back(h);
            }
            const float screenH = ImGui::GetIO().DisplaySize.y;
            columns = ColumnStarts(rows, heights, screenH * kColumnShare, headingH);
            if (columns.size() > 2) columns = ColumnStarts(rows, heights, screenH * kMaxColumnShare, headingH);
        }
        const float totalW = rowW * columns.size() + columnGap * (columns.size() - 1);

        const bool headerOnly = rows.empty() && emptyText.empty();
        if (!hero.name.empty()) {
            DrawTipHero(hero, totalW, sc, !headerOnly);
        } else {

        ImVec2 p = ImGui::GetCursorScreenPos();
        tdl->AddText(font, baseSz * fSmall, p, kAccent, title.c_str());
            float headH = subH;
            if (!subtitle.empty()) {
                tdl->AddText(font, baseSz * fSmall, ImVec2(p.x, p.y + subH + 2.0f * sc), kTextMuted, subtitle.c_str());
                headH += subH + 2.0f * sc;
            }
            if (!headerOnly)
                HUDDraw::DrawRuleH(tdl, p.x, p.x + totalW, p.y + headH + 4.0f * sc, IM_COL32(212, 175, 55, 70));
            ImGui::Dummy(ImVec2(totalW, headH + (headerOnly ? 0.0f : 10.0f * sc)));
        }

        if (rows.empty() && !emptyText.empty()) {
            ImVec2 ep = ImGui::GetCursorScreenPos();
            tdl->AddText(font, baseSz * fSmall, ep, kTextMuted, emptyText.c_str());
            ImGui::Dummy(ImVec2(rowW, subH));
        }

        const float bodyTop = ImGui::GetCursorScreenPos().y;
        size_t column = 0;
        std::vector<float> dividers;
        ImGui::BeginGroup();
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];

            const bool startsColumn = column + 1 < columns.size() && columns[column + 1] == i;
            if (startsColumn) {
                ++column;
                ImGui::EndGroup();
                ImGui::SameLine(0.0f, columnGap);
                dividers.push_back(ImGui::GetCursorScreenPos().x - columnGap * 0.6f);
                ImGui::BeginGroup();
            }

            if (!r.section.empty() && (startsColumn || StartsSection(rows, i))) {
                if (i > 0 && !startsColumn) {
                    ImVec2 sp = ImGui::GetCursorScreenPos();
                    HUDDraw::DrawRuleH(tdl, sp.x, sp.x + rowW, sp.y + 2.0f * sc, IM_COL32(255, 255, 255, 40));
                    ImGui::Dummy(ImVec2(rowW, sectionGap));
                }
                ImVec2 hp = ImGui::GetCursorScreenPos();
                tdl->AddText(font, baseSz * fSmall, hp, kTextMuted, r.section.c_str());
                ImGui::Dummy(ImVec2(rowW, headingH));
            }

            const bool bare = !r.icon && r.name.empty();
            const float indent = r.icon ? iconSz + textGap : 0.0f;
            const float textW = rowW - indent;
            const float textH = TextHeight(r);
            const float rowH = RowHeight(r);
            ImVec2 rp = ImGui::GetCursorScreenPos();
            if (r.tone != 0)
                tdl->AddRectFilled(ImVec2(rp.x - 8.0f * sc, rp.y + 2.0f * sc),
                    ImVec2(rp.x - 5.0f * sc, rp.y + rowH - 2.0f * sc),
                    r.tone > 0 ? kBuff : kDebuff, 1.0f * sc);
            if (r.icon)
                HUDDraw::DrawIconStyled(tdl, ImVec2(rp.x, rp.y + (rowH - iconSz) * 0.5f), iconSz,
                    reinterpret_cast<ImTextureID>(r.icon), r.iconTint, IconFx());

            float tx = rp.x + indent;
            float ty = rp.y + (rowH - textH) * 0.5f;
            if (!bare) tdl->AddText(font, baseSz * fBody, ImVec2(tx, ty), kTextMain, r.name.c_str());
            if (!r.type.empty()) {
                float nameW = Measure(fBody, r.name).x;
                tdl->AddText(font, baseSz * fSmall, ImVec2(tx + nameW + textGap * 0.5f, ty + titleH - subH),
                    kTextMuted, r.type.c_str());
            }
            ty += bare ? -lineGap : titleH;
            for (const auto& l : r.lines) {
                ty += lineGap;
                tdl->AddText(font, baseSz * fSmall, ImVec2(tx, ty), l.col, l.text.c_str(), nullptr, textW);
                ty += Measure(fSmall, l.text, textW).y;
            }
            ImGui::Dummy(ImVec2(rowW, rowH + (i + 1 < rows.size() ? rowGap : 0.0f)));
        }
        ImGui::EndGroup();
        const float bodyBottom = ImGui::GetCursorScreenPos().y;
        for (float x : dividers) HUDDraw::DrawRuleV(tdl, x, bodyTop, bodyBottom, IM_COL32(255, 255, 255, 40));

        EndTipChip(sc);

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
        if (raw && raw[0]) return EffectCategories::FillDescriptionTags(raw, mag, dur);
        const char* nm = base->GetFullName();
        if (!nm || !nm[0] || mag < 0.5f) return {};
        const bool pct = EffectCategories::IsPercentageMagnitude(base->data.primaryAV);
        char buf[160];
        snprintf(buf, sizeof(buf), "%s %s%s%s", nm, hostile ? "-" : "+", Num(mag).c_str(), pct ? "%" : "");
        return buf;
    }

    struct SumLine { std::string label, note, value; ImU32 col; bool ruleAbove = false; };

    inline void DrawSumTooltip(const std::string& title, const std::vector<SumLine>& lines, float sc,
                               const std::vector<std::string>& footer = {}, const TipHero& hero = {}) {
        auto* s = Settings::GetSingleton();
        const float fBody  = s->psV2FontBody  * sc;
        const float fSmall = s->psV2FontSmall * sc;
        const float noteGap  = 6.0f  * sc;
        const float valueGap = 24.0f * sc;
        const float lineGap  = 4.0f  * sc;
        const float ruleGap  = 6.0f  * sc;

        PushTipColors();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * sc, 10.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.0f, 0.0f));
        ImGui::BeginTooltip();
        BeginTipChip();
        NoteHover(title + "|" + hero.category + "|" + hero.name);
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float baseSz = ImGui::GetFontSize();
        auto Measure = [&](float scale, const std::string& t) { return font->CalcTextSizeA(baseSz * scale, FLT_MAX, 0.0f, t.c_str()); };

        const std::string t = Upper(title);

        float leftW = 0.0f, valueW = 0.0f;
        for (const auto& l : lines) {
            float w = Measure(fBody, l.label).x;
            if (!l.note.empty()) w += noteGap + Measure(fSmall, l.note).x;
            leftW  = (std::max)(leftW, w);
            valueW = (std::max)(valueW, Measure(fBody, l.value).x);
        }
        const bool titleOnly = lines.empty() && footer.empty();
        float rowW = (std::max)(Measure(fSmall, t).x, leftW + valueGap + valueW);
        if (!titleOnly || !hero.name.empty()) rowW = (std::max)(rowW, kTipWidth * sc);
        auto MeasureWrapped = [&](const std::string& f) { return font->CalcTextSizeA(baseSz * fSmall, FLT_MAX, rowW, f.c_str()).y; };
        const float subH  = Measure(fSmall, "A").y;
        const float bodyH = Measure(fBody, "A").y;
        if (!hero.name.empty()) {
            DrawTipHero(hero, rowW, sc, !titleOnly);
        } else {
            ImVec2 p = ImGui::GetCursorScreenPos();
        tdl->AddText(font, baseSz * fSmall, p, kAccent, t.c_str());
            if (!titleOnly) HUDDraw::DrawRuleH(tdl, p.x, p.x + rowW, p.y + subH + 4.0f * sc, IM_COL32(212, 175, 55, 70));
            ImGui::Dummy(ImVec2(rowW, titleOnly ? subH : subH + 10.0f * sc));
        }

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

        EndTipChip(sc);

        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    inline void AddPoisonLines(std::vector<SumLine>& lines, std::vector<std::string>& footer,
                               const RE::AlchemyItem* poison, int doses) {
        if (!poison || doses <= 0) return;
        const char* name = poison->GetName();
        std::string dosesText = LocalizationManager::Get("PartySheetV2.PoisonDoses");
        if (auto at = dosesText.find("%1"); at != std::string::npos) dosesText.replace(at, 2, std::to_string(doses));
        lines.push_back({ name ? name : "", std::move(dosesText), {}, IM_COL32(125, 220, 122, 255), true });
        for (auto* eff : poison->effects) {
            auto* base = eff ? eff->baseEffect : nullptr;
            if (!base || base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
            std::string text = DescribeEffect(base, DisplayMagnitude(base, eff->effectItem.magnitude),
                                              static_cast<float>(eff->effectItem.duration), false);
            if (!text.empty()) footer.push_back(std::move(text));
        }
    }

    inline std::vector<std::string> SpellDescriptions(const RE::MagicItem* spell) {
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

    inline void DrawWeaponTooltip(const std::string& title,
                                  const EquipmentStatResolver::WeaponDamage& wd, float sc,
                                  const RE::AlchemyItem* poison = nullptr, int poisonDoses = 0,
                                  const RE::MagicItem* enchant = nullptr, const TipHero& hero = {}) {
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

        std::vector<std::string> footer = SpellDescriptions(enchant);
        AddPoisonLines(lines, footer, poison, poisonDoses);
        DrawSumTooltip(title, lines, sc, footer, hero);
    }

    inline const char* SpellRankKey(const RE::SpellItem* spell) {
        auto* effect = const_cast<RE::SpellItem*>(spell)->GetCostliestEffectItem();
        auto* mgef = effect ? effect->baseEffect : nullptr;
        const auto level = mgef ? mgef->GetMinimumSkillLevel() : 0;
        if (level >= 100) return "Stats.SpellRankMaster";
        if (level >= 75)  return "Stats.SpellRankExpert";
        if (level >= 50)  return "Stats.SpellRankAdept";
        if (level >= 25)  return "Stats.SpellRankApprentice";
        return "Stats.SpellRankNovice";
        }

    inline SumLine SpellHeadLine(RE::Actor* actor, const RE::SpellItem* spell) {
        using ST = RE::MagicSystem::SpellType;
        const auto type = spell->GetSpellType();
        if (type == ST::kPower || type == ST::kLesserPower || type == ST::kVoicePower)
            return { LocalizationManager::Get(type == ST::kLesserPower ? "PartySheetV2.SpellTypeLesserPower"
                                                                       : "PartySheetV2.SpellTypePower"),
                     {}, {}, kTextMuted };

        const auto school = spell->GetAssociatedSkill();
        std::string label = school != RE::ActorValue::kNone ? EffectCategories::LocalizedAVName(school) : std::string{};
        std::string cost;
        if (actor) {
            const bool perSecond = spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
            char buf[48];
            snprintf(buf, sizeof(buf), "%.0f %s%s", spell->CalculateMagickaCost(actor),
                     LocalizationManager::Get("Stats.Magicka").c_str(), perSecond ? "/s" : "");
            cost = buf;
        }
        const std::string& rank = LocalizationManager::Get(SpellRankKey(spell));
        if (!rank.empty()) label += (label.empty() ? "" : " \xC2\xB7 ") + rank;
        return { std::move(label), {}, std::move(cost), kTextMain };
    }

    inline void DrawSpellTooltip(const std::string& title, const RE::SpellItem* spell,
                                 const SpellDamageResolver::SpellDamage& sd, float sc,
                                 std::vector<SumLine> head = {}, const TipHero& hero = {}) {
        const auto* p = sd.Primary();
        if (!p) return;
        std::vector<SumLine> lines = std::move(head);
        const bool schoolShown = !lines.empty();
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
            school != RE::ActorValue::kNone && !schoolShown ? EffectCategories::LocalizedAVName(school) : std::string{},
            "%.0f", p->baseMagnitude, kTextMain, schoolShown);
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
        DrawSumTooltip(title, lines, sc, SpellDescriptions(spell), hero);
    }

    inline void DrawSpellHover(const std::string& category, RE::Actor* actor, const RE::SpellItem* spell, float sc,
                               const std::vector<SumLine>& extra = {}) {
        std::vector<SumLine> lines{ SpellHeadLine(actor, spell) };
        lines.insert(lines.end(), extra.begin(), extra.end());
        TipHero hero;
        hero.category = category;
        hero.name     = spell->GetName() ? spell->GetName() : "";
        hero.icon     = SpellIconManager::GetSpellIcon(const_cast<RE::SpellItem*>(spell));
        const auto sd = SpellDamageResolver::ResolveSpellDamage(actor, const_cast<RE::SpellItem*>(spell));
        if (sd.HasDamage()) DrawSpellTooltip(category, spell, sd, sc, std::move(lines), hero);
        else                DrawSumTooltip(category, lines, sc, SpellDescriptions(spell), hero);
    }
}
