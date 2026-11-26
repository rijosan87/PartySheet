#include "../../includes/ImGui/PartySheetV2Condition.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/HUDSymptoms.h"
#include "../../includes/Utils/SurvivalState.h"
#include "../../includes/Utils/SurvivalSupplies.h"

#include <RE/Skyrim.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace PartySheetV2::Condition {

    static float s_scale = 1.0f;
    static float FontBody()  { return Settings::GetSingleton()->psV2FontBody  * s_scale; }
    static float FontSmall() { return Settings::GetSingleton()->psV2FontSmall * s_scale; }

    static constexpr ImU32 kTextMain  = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 kTextMuted = IM_COL32(136, 136, 136, 255);
    static constexpr ImU32 kBuff      = IM_COL32(100, 200, 120, 255);
    static constexpr ImU32 kDebuff    = IM_COL32(220,  80,  80, 255);

    static std::string Loc(const char* key, const char* english) {
        std::string t = LocalizationManager::Get(key);
        return (t.empty() || t == key) ? std::string(english) : t;
    }

    static constexpr ImU32 kWarn     = IM_COL32(222, 160,  60, 255);
    static constexpr ImU32 kHeading  = IM_COL32(217, 164,  65, 255);

    static constexpr bool kTintIcons = true;

    struct Tone { ImU32 edge, fill, icon; };
    static constexpr Tone kBorder   { IM_COL32( 58,  62,  72, 255), IM_COL32(255, 255, 255,   8), IM_COL32(200, 205, 215, 255) };
    static constexpr Tone kUrgent   { IM_COL32(150,  60,  56, 255), IM_COL32(200,  70,  60,  34), IM_COL32(235,  95,  85, 255) };
    static constexpr Tone kAmber    { IM_COL32(150, 110,  40, 255), IM_COL32(220, 160,  60,  32), IM_COL32(235, 175,  70, 255) };

    static ImU32 IconTint(ImU32 col) { return kTintIcons ? col : IM_COL32_WHITE; }

    static ImU32 SeverityColor(float r) {
        if (r >= 1.0f) return kDebuff;
        if (r >= 0.5f) return kWarn;
        if (r <= 0.0f) return kBuff;
        return kTextMuted;
    }

    static ID3D11ShaderResourceView* PartyIcon(const char* name)  { return TextureManager::GetPartyIcon(name); }
    static ID3D11ShaderResourceView* WidgetIcon(const char* name) {
        return TextureManager::GetTexture(std::string("Data\\Interface\\PartySheet\\Widgets\\") + name + ".png");
    }

    static std::string HoursText(float h) {
        char buf[32];
        if (h < 1.0f) snprintf(buf, sizeof(buf), "<1h");
        else          snprintf(buf, sizeof(buf), "%.0fh", h);
        return buf;
    }

    static ImU32 NeedColor(const char* key) {
        if (!strcmp(key, "hunger"))  return IM_COL32(212, 160,  60, 255);
        if (!strcmp(key, "thirst"))  return IM_COL32( 80, 150, 220, 255);
        if (!strcmp(key, "fatigue")) return IM_COL32(160, 110, 210, 255);
        if (!strcmp(key, "wetness")) return IM_COL32( 80, 150, 220, 255);
        return IM_COL32(110, 190, 220, 255);
    }

    static void DrawIcon(ImDrawList* dl, ID3D11ShaderResourceView* icon, ImVec2 p, float size, ImU32 tint = IM_COL32_WHITE) {
        if (!icon) return;
        const auto* s = Settings::GetSingleton();
        HUDDraw::IconFx fx{};
        fx.duotone      = s->psV2IconDuotone;
        fx.shadow       = s->psV2IconShadow;
        fx.outline      = s->psV2IconOutline;
        fx.outlineWidth = s->psV2IconOutlineWidth;
        fx.outlineColor = s->psV2IconOutlineColor;
        fx.glow         = false;
        fx.gloss        = false;
        HUDDraw::DrawIconStyled(dl, p, size, reinterpret_cast<ImTextureID>(icon), HUDDraw::PackedToArgb(IconTint(tint)), fx);
    }

    static void DrawWarnMark(ImDrawList* dl, ImVec2 p, float size, ImU32 col) {
        dl->AddTriangleFilled(ImVec2(p.x + size * 0.5f, p.y), ImVec2(p.x + size, p.y + size), ImVec2(p.x, p.y + size), col);
        const float cx = p.x + size * 0.5f, w = (std::max)(1.0f, size * 0.12f);
        const ImU32 ink = IM_COL32(20, 20, 24, 255);
        dl->AddRectFilled(ImVec2(cx - w * 0.5f, p.y + size * 0.32f), ImVec2(cx + w * 0.5f, p.y + size * 0.68f), ink);
        dl->AddRectFilled(ImVec2(cx - w * 0.5f, p.y + size * 0.76f), ImVec2(cx + w * 0.5f, p.y + size * 0.88f), ink);
    }

    static void Text(ImDrawList* dl, ImVec2 p, ImU32 col, const std::string& s) {
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), p, col, s.c_str());
    }

    static void SubHeading(ImDrawList* dl, const std::string& label, float bodyW, float sc) {
        ImGui::Dummy(ImVec2(bodyW, 8.0f * sc));
        ImGui::SetWindowFontScale(FontSmall());
        ImVec2 o = ImGui::GetCursorScreenPos();
        Text(dl, o, kHeading, label);
        const float h = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(bodyW, h + 5.0f * sc));
    }

    struct Box {
        ID3D11ShaderResourceView* icon = nullptr;
        std::string title, sub;
        ImU32 subCol = kTextMuted;
        Tone  tone   = kBorder;
    };

    static void DrawBoxes(ImDrawList* dl, const std::vector<Box>& boxes, int perRow, float iconSize, float bodyW, float sc) {
        if (boxes.empty()) return;
        const float gap = 8.0f * sc, pad = 8.0f * sc;
        const float w = (bodyW - gap * (perRow - 1)) / perRow;
        ImGui::SetWindowFontScale(FontBody());
        const float titleH = ImGui::GetTextLineHeight();
        ImGui::SetWindowFontScale(FontSmall());
        const float subH = ImGui::GetTextLineHeight();
        const float h = (std::max)(iconSize, titleH + subH + 2.0f * sc) + pad * 2.0f;
        ImVec2 o = ImGui::GetCursorScreenPos();
        for (size_t i = 0; i < boxes.size(); ++i) {
            const auto& b = boxes[i];
            const float x = o.x + (i % perRow) * (w + gap);
            const float y = o.y + (i / perRow) * (h + gap);
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), b.tone.fill, 4.0f * sc);
            dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), b.tone.edge, 4.0f * sc, 0, 1.0f);
            DrawIcon(dl, b.icon, ImVec2(x + pad, y + (h - iconSize) * 0.5f), iconSize, b.tone.icon);
            const float tx = x + pad + iconSize + pad;
            const float block = b.sub.empty() ? titleH : titleH + subH + 2.0f * sc;
            const float ty = y + (h - block) * 0.5f;
            dl->PushClipRect(ImVec2(tx, y), ImVec2(x + w - pad, y + h), true);
            ImGui::SetWindowFontScale(FontBody());
            Text(dl, ImVec2(tx, ty), kTextMain, b.title);
            ImGui::SetWindowFontScale(FontSmall());
            if (!b.sub.empty()) Text(dl, ImVec2(tx, ty + titleH + 2.0f * sc), b.subCol, b.sub);
            dl->PopClipRect();
        }
        ImGui::SetWindowFontScale(1.0f);
        const size_t rows = (boxes.size() + perRow - 1) / perRow;
        ImGui::Dummy(ImVec2(bodyW, rows * h + (rows - 1) * gap));
    }

    static void SmallLine(const std::string& text, ImU32 col) {
        ImGui::SetWindowFontScale(FontSmall());
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    static void DrawNeeds(ImDrawList* dl, const SurvivalState::Snapshot& c, float bodyW, float sc) {
        struct Cell { ID3D11ShaderResourceView* icon; std::string name, word, pct; float fill; ImU32 bar, wordCol; bool worst; ImU32 iconCol = 0; };
        auto pctText = [](float f) { char b[8]; snprintf(b, sizeof(b), "%d%%", static_cast<int>(std::lround(f * 100.0f))); return std::string(b); };
        std::vector<Cell> cells;
        for (const auto& n : c.needs) {
            const auto& st = n.status;
            const bool exact = st.fill >= 0.0f;
            const float fill = exact ? st.fill : (st.maxStage > 0 ? static_cast<float>(st.stage) / st.maxStage : 0.0f);
            const float sev = st.maxStage > 0 ? static_cast<float>(st.stage) / st.maxStage : 0.0f;
            cells.push_back({ st.icon, st.needName, st.stageName, exact ? pctText(fill) : std::string(), fill, NeedColor(st.key),
                              SeverityColor(sev), st.maxStage > 0 && st.stage >= st.maxStage });
        }
        if (c.vitality) {
            const auto& v = *c.vitality;
            std::string word = v.tierName;
            if (v.trend > 0)      word += ", " + Loc("Condition.Improving", "improving");
            else if (v.trend < 0) word += ", " + Loc("Condition.Declining", "declining");
            const ImU32 wc = v.tier <= 0 ? kDebuff : (v.tier <= 2 ? kWarn : (v.tier > 4 ? kBuff : kTextMuted));
            const float health = v.fill >= 0.0f ? v.fill : (v.maxTier > 0 ? static_cast<float>(v.tier) / v.maxTier : 1.0f);
            const float decline = 1.0f - health;
            cells.push_back({ v.icon, v.name, word, v.fill >= 0.0f ? pctText(decline) : std::string(), decline, kDebuff, wc, v.tier <= 0,
                              IM_COL32(100, 200, 120, 255) });
        }

        ImGui::SetWindowFontScale(FontBody());
        const float textH = ImGui::GetTextLineHeight();
        const float iconSize = 26.0f * sc;
        const float rowH = (std::max)(iconSize, textH + 10.0f * sc) + 8.0f * sc;
        const float colGap = 28.0f * sc;
        const float colW = (bodyW - colGap) * 0.5f;
        ImVec2 o = ImGui::GetCursorScreenPos();
        for (size_t i = 0; i < cells.size(); ++i) {
            const auto& cell = cells[i];
            const float x = o.x + (i % 2) * (colW + colGap);
            const float y = o.y + (i / 2) * rowH;
            DrawIcon(dl, cell.icon, ImVec2(x, y), iconSize, (cell.iconCol && !cell.worst) ? cell.iconCol : cell.bar);
            const float tx = x + iconSize + 8.0f * sc, right = x + colW;
            Text(dl, ImVec2(tx, y), kTextMain, cell.name);
            const std::string val = cell.pct.empty() ? cell.word : cell.pct + "  \xC2\xB7  " + cell.word;
            const float vw = ImGui::CalcTextSize(val.c_str()).x;
            Text(dl, ImVec2(right - vw, y), cell.wordCol, val);
            if (cell.worst) {
                const float m = textH * 0.8f;
                DrawWarnMark(dl, ImVec2(right - vw - m - 6.0f * sc, y + (textH - m) * 0.5f), m, kDebuff);
            }
            const float by = y + textH + 4.0f * sc, bh = 5.0f * sc;
            dl->AddRectFilled(ImVec2(tx, by), ImVec2(right, by + bh), IM_COL32(34, 37, 44, 255), bh * 0.5f);
            dl->AddRectFilled(ImVec2(tx, by), ImVec2(tx + (right - tx) * std::clamp(cell.fill, 0.0f, 1.0f), by + bh), cell.bar, bh * 0.5f);
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(bodyW, ((cells.size() + 1) / 2) * rowH));
    }

    struct Chip { ID3D11ShaderResourceView* icon; std::string text; };
    static void DrawChips(ImDrawList* dl, const std::vector<Chip>& chips, float bodyW, float sc) {
        ImGui::SetWindowFontScale(FontSmall());
        const float th = ImGui::GetTextLineHeight();
        const float icon = th + 2.0f * sc, padX = 7.0f * sc, padY = 4.0f * sc, gap = 6.0f * sc;
        const float h = icon + padY * 2.0f;
        ImVec2 o = ImGui::GetCursorScreenPos();
        float x = o.x, y = o.y;
        for (const auto& ch : chips) {
            const float w = padX + icon + 5.0f * sc + ImGui::CalcTextSize(ch.text.c_str()).x + padX;
            if (x > o.x && x + w > o.x + bodyW) { x = o.x; y += h + gap; }
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), kUrgent.fill, 4.0f * sc);
            dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), kUrgent.edge, 4.0f * sc, 0, 1.0f);
            DrawIcon(dl, ch.icon, ImVec2(x + padX, y + padY), icon, kUrgent.icon);
            Text(dl, ImVec2(x + padX + icon + 5.0f * sc, y + (h - th) * 0.5f), kDebuff, ch.text);
            x += w + gap;
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(bodyW, y - o.y + h));
    }

    static std::vector<Chip> CostChips() {
        std::vector<Chip> cost;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return cost;
        const auto& bars = HUDSymptoms::GetBars(player);
        auto addBar = [&](const HUDSymptoms::Bar& b, const char* icon, const char* key, const char* english) {
            if (b.Lost() < 1.0f) return;
            char buf[96];
            snprintf(buf, sizeof(buf), "%s -%.0f", Loc(key, english).c_str(), b.Lost());
            cost.push_back({ PartyIcon(icon), buf });
        };
        addBar(bars.health,  "health",  "Condition.MaxHealth",  "Max health");
        addBar(bars.stamina, "stamina", "Condition.MaxStamina", "Max stamina");
        addBar(bars.magicka, "magicka", "Condition.MaxMagicka", "Max magicka");
        if (bars.magicka.regen != HUDSymptoms::Regen::kNormal)
            cost.push_back({ PartyIcon("magicka_regen"), Loc("Condition.MagickaRegenSlowed", "Magicka regen slowed") });
        if (bars.stamina.regen != HUDSymptoms::Regen::kNormal)
            cost.push_back({ PartyIcon("stamina_regen"), Loc("Condition.StaminaRegenSlowed", "Stamina regen slowed") });
        if (bars.health.regen != HUDSymptoms::Regen::kNormal)
            cost.push_back({ PartyIcon("health_regen"), Loc("Condition.HealthRegenSlowed", "Health regen slowed") });
        return cost;
    }

    static std::vector<Box> SupplyCards(const SurvivalState::Snapshot& c) {
        const auto t = SurvivalSupplies::Count();
        std::vector<Box> sup;
        auto needStage = [&](const char* key) {
            for (const auto& n : c.needs) if (!strcmp(n.status.key, key)) return n.status.stage;
            return -1;
        };
        auto count = [](int n, const std::string& word) { return std::to_string(n) + "x " + word; };

        Box food{ PartyIcon("source_food"), count(t.food, Loc("Condition.Food", "food")) };
        if (t.food == 0 && needStage("hunger") >= 2) {
            food.tone = kUrgent; food.sub = Loc("Condition.HungerGettingBad", "hunger is getting bad"); food.subCol = kDebuff;
        }
        sup.push_back(food);
        const int thirstStage = needStage("thirst");
        if (thirstStage >= 0) {
            Box drink{ WidgetIcon("survival_drink"), count(t.drinks, Loc("Condition.Drinks", "drinks")) };
            if (t.drinks == 0 && thirstStage >= 2) {
                drink.tone = kUrgent; drink.sub = Loc("Condition.ThirstGettingBad", "thirst is getting bad"); drink.subCol = kDebuff;
            }
            sup.push_back(drink);
        }
        if (t.waterskinSips > 0)
            sup.push_back({ WidgetIcon("survival_thirst"), std::to_string(t.waterskinSips) + " " + Loc("Condition.WaterskinSips", "waterskin sips") });
        if (t.dirtyWater > 0)
            sup.push_back({ WidgetIcon("survival_dirtywater"), count(t.dirtyWater, Loc("Condition.DirtyWaterShort", "dirty water")),
                            Loc("Condition.BoilFirst", "boil first"), kWarn, kAmber });
        if (t.firewood > 0)
            sup.push_back({ PartyIcon("fire"), count(t.firewood, Loc("Condition.Firewood", "firewood")) });
        if (t.tents > 0)
            sup.push_back({ WidgetIcon("survival_tent"),
                            count(t.tents, t.tents == 1 ? Loc("Condition.Tent", "tent") : Loc("Condition.Tents", "tents")) });
        if (t.torches > 0)
            sup.push_back({ PartyIcon("light"),
                            count(t.torches, t.torches == 1 ? Loc("Condition.Torch", "torch") : Loc("Condition.Torches", "torches")) });
        return sup;
    }

    static void DrawRows(ImDrawList* dl, const SurvivalState::Snapshot& c, float bodyW, float sc) {
        if (!c.stackWarning.empty()) {
            ImGui::SetWindowFontScale(FontSmall());
            ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bodyW);
            ImGui::TextUnformatted(c.stackWarning.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
        }

        const float cardIcon = 26.0f * sc;

        SubHeading(dl, Loc("Condition.Needs", "NEEDS"), bodyW, sc);
        DrawNeeds(dl, c, bodyW, sc);

        std::vector<std::pair<float, std::string>> soon;
        for (const auto& n : c.needs)
            if (n.hoursToNextStage >= 0.0f && n.hoursToNextStage <= 12.0f)
                soon.push_back({ n.hoursToNextStage,
                    n.status.needName + " " + Loc("Condition.WorseIn", "worse in") + " ~" + HoursText(n.hoursToNextStage) });
        std::sort(soon.begin(), soon.end());
        if (!soon.empty()) {
            std::string line;
            for (size_t i = 0; i < soon.size() && i < 3; ++i) line += (i ? "  \xC2\xB7  " : "") + soon[i].second;
            SmallLine(line, soon.front().first < 2.0f ? kWarn : kTextMuted);
        }

        auto cost = CostChips();
        if (!cost.empty()) {
            SubHeading(dl, Loc("Condition.CostHeading", "COST"), bodyW, sc);
            DrawChips(dl, cost, bodyW, sc);
        }

        SubHeading(dl, Loc("Condition.SuppliesHeading", "SUPPLIES"), bodyW, sc);
        DrawBoxes(dl, SupplyCards(c), 3, cardIcon, bodyW, sc);

        std::string rest;
        ImU32 restCol = kTextMuted;
        if (c.hoursSinceSleep >= 0.0f) {
            rest = Loc("Condition.Slept", "Slept") + " " + HoursText(c.hoursSinceSleep) + " " + Loc("Condition.Ago", "ago");
            if (c.hoursSinceSleep > 24.0f) restCol = kWarn;
        }
        if (c.wellRested) {
            if (!rest.empty()) rest += "  \xC2\xB7  ";
            rest += Loc("Condition.Rested", "Rested");
            if (c.restedHoursLeft >= 0.0f) rest += " " + HoursText(c.restedHoursLeft) + " " + Loc("Condition.Left", "left");
        }
        if (!rest.empty()) {
            ImGui::Dummy(ImVec2(bodyW, 8.0f * sc));
            ImVec2 o = ImGui::GetCursorScreenPos();
            dl->AddLine(o, ImVec2(o.x + bodyW, o.y), IM_COL32(42, 45, 53, 255), 1.0f);
            ImGui::Dummy(ImVec2(bodyW, 6.0f * sc));
            SmallLine(rest, restCol);
        }
    }

    const char* Title() {
        static std::string title;
        if (title.empty()) {
            title = Loc("PartySheetV2.SectionCondition", "Survival & Needs");
            for (char& ch : title) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
        }
        return title.c_str();
    }

    void Draw(ImDrawList* dl, float bodyW, float sc) {
        s_scale = sc;
        const auto& c = SurvivalState::Get();
        if (!c.available) {
            ImGui::SetWindowFontScale(FontSmall());
            ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
            ImGui::TextUnformatted(Loc("Condition.NoSurvival", "No survival mod running").c_str());
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            return;
        }
        DrawRows(dl, c, bodyW, sc);
    }
}
