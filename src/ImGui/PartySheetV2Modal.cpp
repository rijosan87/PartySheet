#include "../../includes/ImGui/PartySheetV2Modal.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/EffectCategories.h"
#include "../../includes/HUDSymptoms.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/Utils/EffectAttribution.h"
#include "../../includes/Utils/PerkModifiers.h"
#include "../../includes/ImGui/PartySheetV2Tooltip.h"
#include "../../includes/ImGui/PartySheetV2Condition.h"

#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cfloat>

namespace logger = SKSE::log;

namespace PartySheetV2 {

    static bool s_ModalOpen = false;

    void OpenEffectsModal() {
        s_ModalOpen = true;
        logger::info("EffectsModal: Opened");
    }
    void CloseEffectsModal() {
        if (s_ModalOpen)
            logger::info("EffectsModal: Closed");
        s_ModalOpen = false;
    }
    bool IsEffectsModalOpen() { return s_ModalOpen; }

    // ── Font scale helpers (mirrors PartySheetV2.cpp) ────────────────────────
    static float s_EffectiveScale = 1.0f;
    static float FontDisplay() { auto* s = Settings::GetSingleton(); return s->psV2FontDisplay * s_EffectiveScale; }
    static float FontHeading() { auto* s = Settings::GetSingleton(); return s->psV2FontHeading * s_EffectiveScale; }
    static float FontBody()    { auto* s = Settings::GetSingleton(); return s->psV2FontBody    * s_EffectiveScale; }
    static float FontSmall()   { auto* s = Settings::GetSingleton(); return s->psV2FontSmall   * s_EffectiveScale; }

    static constexpr ImU32 kAccent      = IM_COL32(212, 175,  55, 255);
    static constexpr ImU32 kTextMain    = IM_COL32(240, 240, 240, 255);
    static constexpr ImU32 kTextMuted   = IM_COL32(136, 136, 136, 255);
    static constexpr ImU32 kBorderDim   = IM_COL32( 50,  50,  60, 255);
    static constexpr ImU32 kBuff        = IM_COL32(100, 200, 120, 255);
    static constexpr ImU32 kDebuff      = IM_COL32(220,  80,  80, 255);
    static constexpr ImU32 kOverlayBg   = IM_COL32(  0,   0,   0, 180);

    static float DrawModalSectionTitle(ImDrawList* dl,
                                       float ox, float oy, float colW,
                                       const char* title, float sc) {
        ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
        ImGui::SetWindowFontScale(FontHeading());
        ImVec2 tsz = ImGui::CalcTextSize(title);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(ox, oy), kAccent, title);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        float sepY = oy + tsz.y + 3.0f * sc;
        HUDDraw::DrawFadeRule(dl, ox, ox + colW * 0.65f, sepY, kAccent, HUDDraw::RuleFade::Right);
        return sepY + 7.0f * sc;
    }

    struct EffectEntry {
        std::string source;
        std::string name;
        std::string valStr;
        bool        isHostile     = false;
        RE::SpellItem* sourceSpell = nullptr;
        RE::ActorValue primaryAV = RE::ActorValue::kNone;
        RE::EffectSetting* mgef = nullptr;
    };

    static std::string FormatMagnitude(float magnitude, RE::ActorValue primaryAV, bool isHostile) {
        if (magnitude <= 0.0f) return "";
        bool isPct = EffectCategories::IsPercentageMagnitude(primaryAV);
        char buf[32];
        if (isPct)
            snprintf(buf, sizeof(buf), "%s%.0f%%", isHostile ? "-" : "+", magnitude);
        else
            snprintf(buf, sizeof(buf), "%s%.0f", isHostile ? "-" : "+", magnitude);
        return buf;
    }

    enum class EffectBucket { Passive, Blessing };

    static std::vector<EffectEntry> GetPermanentEffects(RE::Actor* actor, EffectBucket bucket) {
        using Kind = EffectAttribution::OwnerKind;
        std::vector<EffectEntry> out;
        if (!actor) return out;

        for (const auto& rec : EffectAttribution::Get(actor)) {
            auto* base = rec.mgef;
            if (rec.hiddenInUI) continue;
            if (rec.effectName[0] == '\0') continue;
            bool match = false;
            switch (bucket) {
                case EffectBucket::Passive:
                    match = (rec.kind == Kind::Passive || rec.kind == Kind::Stone) &&
                            rec.mechanism == EffectCategories::EffectSource::Ability &&
                            rec.duration <= 0.0f;
                    if (match && rec.primaryAV == RE::ActorValue::kNone && rec.kind != Kind::Stone)
                        match = false;
                    break;
                case EffectBucket::Blessing:
                    match = (rec.kind == Kind::Blessing);
                    break;
            }
            if (!match) continue;

            const float mag = EffectAttribution::DisplayMagnitude(rec, base->data.primaryAV);

            EffectEntry entry;
            entry.source      = rec.sourceName;
            entry.name        = rec.effectName;
            entry.valStr      = FormatMagnitude(mag, base->data.primaryAV, rec.hostile);
            entry.isHostile   = rec.hostile;
            entry.sourceSpell = rec.spellItem;
            entry.primaryAV   = base->data.primaryAV;
            entry.mgef        = base;
            out.push_back(entry);
        }
        return out;
    }

    static std::string RaceMagnitudeLine(RE::MagicItem* item) {
        if (!item) return "";
        RE::EffectSetting* base = item->GetAVEffect();
        RE::Effect* eff = item->effects.empty() ? nullptr : item->effects[0];
        if (!base && eff) base = eff->baseEffect;
        if (!base) return "";

        RE::ActorValue av = base->data.primaryAV;
        if (av == RE::ActorValue::kNone) return "";

        float mag = eff ? eff->GetMagnitude() : 0.0f;
        if (mag <= 0.0f) return "";

        std::string label = EffectCategories::LocalizedAVName(av);
        if (label.empty()) return "";

        const bool isPct = EffectCategories::IsPercentageMagnitude(av);
        char buf[96];
        if (isPct) snprintf(buf, sizeof(buf), "%s +%.0f%%", label.c_str(), mag);
        else       snprintf(buf, sizeof(buf), "%s +%.0f",   label.c_str(), mag);
        return buf;
    }

    struct RaceEntry {
        std::string name;
        std::string typeLabel;
        std::string magLine;
        ID3D11ShaderResourceView* icon = nullptr;
    };

    static std::vector<RaceEntry> GetRaceAbilities(RE::Actor* actor) {
        std::vector<RaceEntry> out;
        if (!actor) return out;
        auto* race = actor->GetRace();
        if (!race) return out;
        auto* ae = race->actorEffects;
        if (!ae || !ae->spells || ae->numSpells == 0) return out;

        using ST = RE::MagicSystem::SpellType;
        for (std::uint32_t i = 0; i < ae->numSpells; ++i) {
            auto* sp = ae->spells[i];
            if (!sp) continue;
            const char* nm = sp->GetFullName();
            if (!nm || !nm[0]) continue;
            const char* label = nullptr;
            std::string typeStr;
            auto spellType = sp->GetSpellType();
            switch (spellType) {
                case ST::kAbility:     typeStr = LocalizationManager::Get("PartySheetV2.SpellTypeAbility");     break;
                case ST::kPower:       typeStr = LocalizationManager::Get("PartySheetV2.SpellTypePower");       break;
                case ST::kLesserPower: typeStr = LocalizationManager::Get("PartySheetV2.SpellTypeLesserPower"); break;
                case ST::kSpell:       typeStr = LocalizationManager::Get("PartySheetV2.SpellTypeSpell");       break;
                default:               break;
            }
            if (typeStr.empty()) continue;

            const bool isPower = (spellType == ST::kPower || spellType == ST::kLesserPower);
            ID3D11ShaderResourceView* icon = isPower
                ? TextureManager::GetPartyIcon("power")
                : EffectAttribution::ResolveMagicIcon(sp);

            std::string magLine = isPower ? std::string() : RaceMagnitudeLine(sp);

            out.push_back({ nm, typeStr, std::move(magLine), icon });
        }
        return out;
    }

    static std::string StoneLineText(const EffectEntry& e, const std::string& stoneTitle) {
        if (e.valStr.empty() && e.mgef) {
            const char* d = e.mgef->magicItemDescription.c_str();
            if (d && d[0]) return EffectCategories::FillDescriptionTags(d);
        }
        bool echoesTitle = !e.name.empty() &&
            (e.name == stoneTitle || e.name.rfind(stoneTitle, 0) == 0);
        if (echoesTitle && e.primaryAV != RE::ActorValue::kNone && !e.valStr.empty()) {
            std::string label = EffectCategories::LocalizedAVName(e.primaryAV);
            if (!label.empty()) return label;
        }
        return e.name;
    }

    static std::string ResolveStoneMessage(RE::SpellItem* stone) {
        if (!stone) return {};
        RE::FormID msgId = 0;
        switch (stone->GetFormID()) {
            case 0x000E5F4C: msgId = 0x016869; break;
            case 0x000E5F47: msgId = 0x01683A; break;
            case 0x000E5F45: msgId = 0x016867; break;
            case 0x000E5F58: msgId = 0x016838; break;
            case 0x000E5F54: msgId = 0x016837; break;
            case 0x000E5F4E:
            case 0x0010E439:
            case 0x00103A94: msgId = 0x016835; break;
            case 0x000E5F51: msgId = 0x016836; break;
            case 0x000E5F5A: msgId = 0x016839; break;
            case 0x000E7328: msgId = 0x016868; break;
            case 0x000E7329: msgId = 0x01683B; break;
            case 0x000E5F61: msgId = 0x016844; break;
            case 0x000E732A: msgId = 0x016865; break;
            default: break;
        }
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!msgId && dh) {
            if (auto* steed = dh->LookupForm(0x000E5F5E, "ccQDRSSE001-SurvivalMode.esl"); steed == stone)
                msgId = 0x016866;
        }
        if (!msgId || !dh) return {};
        auto* f = dh->LookupForm(msgId, "Skyrim.esm");
        auto* msg = f ? f->As<RE::BGSMessage>() : nullptr;
        if (!msg) return {};
        RE::BSString buf;
        msg->GetDescription(buf, msg);
        std::string out = EffectCategories::FillDescriptionTags(buf.c_str() ? buf.c_str() : "");
        for (size_t p = out.find("%%"); p != std::string::npos; p = out.find("%%", p + 1))
            out.erase(p, 1);
        return out;
    }

    static std::string ResolveStoneDescription(RE::SpellItem* stone) {
        if (!stone) return {};
        for (auto* eff : stone->effects) {
            if (!eff || !eff->baseEffect) continue;
            const char* d = eff->baseEffect->magicItemDescription.c_str();
            if (!d || !d[0]) continue;
            return EffectCategories::FillDescriptionTags(d);
        }
        {
            RE::BSString buf;
            stone->GetDescription(buf, stone);
            std::string out = EffectCategories::FillDescriptionTags(buf.c_str() ? buf.c_str() : "");
            if (!out.empty()) return out;
        }
        return ResolveStoneMessage(stone);
    }

    void RenderEffectsModal(RE::Actor* actor) {
        if (!s_ModalOpen) return;
        if (!actor) return;

        auto* settings = Settings::GetSingleton();
        auto& io       = ImGui::GetIO();
        const float fitScale = (std::min)(io.DisplaySize.x * 0.95f / 820.0f, io.DisplaySize.y * 0.90f / 620.0f);
        const float sc = (std::min)(settings->psV2Scale, fitScale);
        s_EffectiveScale = sc;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##psv2_modal_overlay", nullptr,
            ImGuiWindowFlags_NoTitleBar            | ImGuiWindowFlags_NoResize     |
            ImGuiWindowFlags_NoMove                | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoCollapse            | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav        |
            ImGuiWindowFlags_NoInputs);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(0, 0), io.DisplaySize, kOverlayBg);
        ImGui::End();
        ImGui::PopStyleVar();

        const float modalW = 820.0f * sc;
        const float modalH = 620.0f * sc;
        ImVec2 modalPos(
            (io.DisplaySize.x - modalW) * 0.5f,
            (io.DisplaySize.y - modalH) * 0.5f);

        ImGui::SetNextWindowPos(modalPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::SetNextWindowFocus();
        ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(9/255.f, 9/255.f, 11/255.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(212/255.f, 175/255.f, 55/255.f, 0.4f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(20.0f * sc, 16.0f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::Begin("##psv2_effects_modal", nullptr,
            ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoNav);

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImDrawList* dl    = ImGui::GetWindowDrawList();
        ImVec2      wPos  = ImGui::GetWindowPos();

        float curY = wPos.y + 16.0f * sc;
        float curX = wPos.x + 20.0f * sc;

        const char* actorName = actor->GetDisplayFullName();
        static std::string s_unknown;
        if (s_unknown.empty()) s_unknown = LocalizationManager::Get("Common.Unknown");
        if (!actorName || !actorName[0]) actorName = s_unknown.c_str();

        ImGui::PushFont(nullptr, ImGuiRenderer::GetHeadingFontSize());
        ImGui::SetWindowFontScale(FontDisplay());
        ImVec2 nameSz = ImGui::CalcTextSize(actorName);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(curX, curY), kAccent, actorName);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        const float btnSz  = 20.0f * sc;
        ImVec2 btnTL = ImVec2(wPos.x + modalW - btnSz - 12.0f * sc, wPos.y + 10.0f * sc);
        ImVec2 btnBR = ImVec2(btnTL.x + btnSz, btnTL.y + btnSz);
        bool closeBtnHovered = ImGui::IsMouseHoveringRect(btnTL, btnBR);
        dl->AddRectFilled(btnTL, btnBR, closeBtnHovered ? IM_COL32(80,30,30,200) : IM_COL32(30,30,40,180), 3.0f);
        ImGui::SetWindowFontScale(FontBody());
        ImVec2 xSz = ImGui::CalcTextSize("x");
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(btnTL.x + (btnSz - xSz.x) * 0.5f, btnTL.y + (btnSz - xSz.y) * 0.5f),
            kTextMuted, "x");
        ImGui::SetWindowFontScale(1.0f);
        if (closeBtnHovered && ImGui::IsMouseClicked(0))
            CloseEffectsModal();

        float sepY = wPos.y + 16.0f * sc + nameSz.y + 6.0f * sc;
        HUDDraw::DrawFadeRule(dl, curX, curX + (modalW - 40.0f * sc) * 0.85f, sepY, kAccent, HUDDraw::RuleFade::Right);

        float bodyY = sepY + 12.0f * sc;

        // ── Single-scroll layout ──────────────────────────────────────────────
        const float scrollbarW = 14.0f;
        const float bodyW = modalW - 40.0f * sc - scrollbarW;
        const float bodyH = (std::max)(1.0f, modalH - (bodyY - wPos.y) - 16.0f * sc);

        ImGui::SetWindowFontScale(FontBody());
        const float rowH = ImGui::GetTextLineHeight() + 5.0f * sc;
        ImGui::SetWindowFontScale(1.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg,               ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,           ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         ImVec4(212/255.f,175/255.f,55/255.f,0.4f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  ImVec4(212/255.f,175/255.f,55/255.f,0.7f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);

        ImGui::SetCursorScreenPos(ImVec2(curX, bodyY));
        ImGui::BeginChild("##modal_scroll", ImVec2(bodyW + scrollbarW, bodyH), false, ImGuiWindowFlags_NoNav);
        ImDrawList* cdl = ImGui::GetWindowDrawList();

        float sectionGap = 16.0f * sc;

        const float colSrcW  = bodyW * 0.35f;
        const float colValW  = bodyW * 0.20f;
        const float colNameW = bodyW - colSrcW - colValW;

        int tableRowIdx = 0;

        auto DrawEffectRow = [&](const EffectEntry& e) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(FontBody());

            if (tableRowIdx % 2 == 0)
                cdl->AddRectFilled(ImVec2(rp.x, rp.y), ImVec2(rp.x + bodyW, rp.y + rowH),
                    IM_COL32(255, 255, 255, 6));
            ++tableRowIdx;

            if (!e.source.empty()) {
                std::string src = e.source;
                while (src.size() > 1 && ImGui::CalcTextSize(src.c_str()).x > colSrcW - 6.0f * sc)
                    src.resize(src.size() - 1);
                if (src != e.source) src += "\xe2\x80\xa6";
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(rp.x + 4.0f * sc, rp.y), kTextMuted, src.c_str());
            }

            float divX = rp.x + colSrcW - 6.0f * sc;
            HUDDraw::DrawRuleV(cdl, divX, rp.y + 1.0f, rp.y + rowH - 3.0f * sc, IM_COL32(80, 80, 90, 120));

            float nameX = rp.x + colSrcW;
            cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(nameX, rp.y), kTextMain, e.name.c_str());

            if (!e.valStr.empty()) {
                ImU32 valCol = e.isHostile ? kDebuff : kBuff;
                float valW = ImGui::CalcTextSize(e.valStr.c_str()).x;
                float valX = rp.x + bodyW - 8.0f * sc - valW;
                cdl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(valX, rp.y), valCol, e.valStr.c_str());
            }

            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, rowH));
        };

        bool firstSection = true;
        auto DrawSectionHeader = [&](const char* title) {
            if (!firstSection) {
                ImGui::Dummy(ImVec2(bodyW, sectionGap));
            }
            firstSection = false;
            ImVec2 hp = ImGui::GetCursorScreenPos();
            float contentY = DrawModalSectionTitle(cdl, hp.x, hp.y, bodyW, title, sc);
            ImGui::SetWindowFontScale(FontHeading());
            float titleH = ImGui::GetTextLineHeight() + 10.0f * sc;
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(bodyW, titleH));
        };

        DrawSectionHeader(Condition::Title());
        if (actor->IsPlayerRef()) Condition::Draw(cdl, bodyW, sc);

        ImGui::Dummy(ImVec2(bodyW, 8.0f * sc));

        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::End();
    }

    using Tip::TipLine;
    using Tip::TipRow;
    using Tip::DrawCompactTooltip;

    void DrawRaceTooltip(RE::Actor* actor, float sc) {
        auto entries = GetRaceAbilities(actor);
        if (entries.empty()) return;
        auto* race = actor->GetRace();
        std::string title = (race && race->GetFullName()) ? race->GetFullName() : "";
        for (char& c : title) c = static_cast<char>(toupper(c));
        std::vector<TipRow> rows;
        for (auto& e : entries) {
            TipRow r{ e.icon, e.name, e.typeLabel, {} };
            if (!e.magLine.empty()) { r.lines.push_back({ e.magLine, kBuff }); r.tone = 1; }
            rows.push_back(std::move(r));
        }
        DrawCompactTooltip(title, rows, {}, sc);
    }

    static std::string FormatTimeLeft(float secs) {
        char buf[16];
        if (secs >= 3600.0f)    snprintf(buf, sizeof(buf), "%.0fh", secs / 3600.0f);
        else if (secs >= 60.0f) snprintf(buf, sizeof(buf), "%.0fm", secs / 60.0f);
        else                    snprintf(buf, sizeof(buf), "%.0fs", secs);
        return buf;
    }

    static void AddNeedCutRows(std::vector<TipRow>& rows, RE::Actor* actor, RE::ActorValue pool, const std::string& section) {
        for (const auto& cut : EffectAttribution::NeedPoolCuts(actor)) {
            if (cut.change.av != pool) continue;
            TipRow r;
            r.icon    = EffectAttribution::OwnerIcon(*cut.record);
            r.name    = EffectAttribution::OwnerName(*cut.record);
            r.type    = EffectAttribution::KindLabel(EffectAttribution::OwnerKind::Need);
            r.section = section;
            r.tone    = -1;
            for (auto& l : EffectAttribution::FormatStatChanges({ cut.change }))
                r.lines.push_back({ std::move(l.text), kDebuff });
            rows.push_back(std::move(r));
        }
    }

    static void AddUnknownCutRow(std::vector<TipRow>& rows, RE::Actor* actor, RE::ActorValue pool, const std::string& section) {
        const auto& bars = HUDSymptoms::GetBars(actor);
        const HUDSymptoms::Bar* bar = nullptr;
        switch (pool) {
            case RE::ActorValue::kHealth:  bar = &bars.health;  break;
            case RE::ActorValue::kMagicka: bar = &bars.magicka; break;
            case RE::ActorValue::kStamina: bar = &bars.stamina; break;
            default: return;
        }
        const float net = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, pool) +
                          actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, pool);
        const float unknown = -net - bar->Lost();
        constexpr float kSmallestShown = 1.0f;
        if (unknown < kSmallestShown) return;

        auto label = [](const char* key, const char* english) {
            std::string v = LocalizationManager::GetSingleton()->FindLabel(key);
            return v.empty() ? std::string(english) : v;
        };
        TipRow r;
        r.icon    = TextureManager::GetPartyIcon("default");
        r.name    = LocalizationManager::Get("Common.Unknown");
        r.section = section;
        r.tone    = -1;
        if (!bar->buffs.empty()) r.type = label("PartySheetV2.AtLeast", "at least");
        for (auto& l : EffectAttribution::FormatStatChanges({ { pool, -unknown } }))
            r.lines.push_back({ std::move(l.text), kDebuff });
        rows.push_back(std::move(r));
    }

    static std::vector<TipRow> StatSourceRows(RE::Actor* actor, RE::ActorValue pool) {
        using AV = RE::ActorValue;
        std::vector<TipRow> rows;
        struct Stack { EffectAttribution::GroupKey key; AV av = AV::kNone; float mag = 0.0f; float timeLeft = -1.0f; int count = 1; std::string kindText; };
        std::vector<Stack> stacks;

        auto Feeds = [pool](AV av) {
            if (av == AV::kNone) return false;
            using HT = EffectCategories::HudTarget;
            switch (pool) {
                case AV::kHealth: case AV::kStamina: case AV::kMagicka:
                case AV::kHealRate: case AV::kStaminaRate: case AV::kMagickaRate:
                    return EffectCategories::GetHudTarget(av) == EffectCategories::GetHudTarget(pool)
                        && EffectCategories::GetHudTarget(pool) != HT::None;
                default: return av == pool || EffectCategories::GetSkillTarget(av) == pool;
            }
        };

        const bool skillPool = EffectCategories::GetSkillTarget(pool) == pool;
        static std::string s_secEquip, s_secEffects;
        if (s_secEquip.empty()) {
            s_secEquip   = LocalizationManager::Get("PartySheetV2.SectionEquipment");
            s_secEffects = LocalizationManager::Get("PartySheetV2.SectionEffects");
        }

        for (const auto& rec : EffectAttribution::Get(actor)) {
            auto* base = rec.mgef;
            AV av = rec.primaryAV;
            if (!Feeds(av)) {
                if (!Feeds(rec.secondaryAV)) continue;
                av = rec.secondaryAV;
        }

            const float mag = EffectAttribution::DisplayMagnitude(rec, av);
            if (mag < 0.5f) continue;
            const bool hostile = rec.hostile;

            const auto key = EffectAttribution::GroupKeyOf(rec.effect);
            auto st = std::find_if(stacks.begin(), stacks.end(), [&](const Stack& x) { return x.key == key && x.av == av; });
            if (st != stacks.end()) {
                st->mag += mag;
                st->count++;
                if (rec.timeLeft > 0.0f && (st->timeLeft <= 0.0f || rec.timeLeft < st->timeLeft)) st->timeLeft = rec.timeLeft;
                auto& row = rows[st - stacks.begin()];
                if (st->timeLeft > 0.0f)
                    row.type = st->kindText + (st->kindText.empty() ? "" : " \xC2\xB7 ") + FormatTimeLeft(st->timeLeft);
                char buf[48];
                snprintf(buf, sizeof(buf), " x%d", st->count);
                row.lines.front().text = FormatMagnitude(st->mag, av, hostile) + buf;
                continue;
            }
            TipRow r;
            r.name = EffectAttribution::OwnerName(rec);
            r.icon = EffectAttribution::OwnerIcon(rec);
            const bool gear = rec.kind == EffectAttribution::OwnerKind::Equipment;
            r.section = gear ? s_secEquip : s_secEffects;
            if (!gear) r.type = EffectAttribution::KindLabel(rec.kind);
            if (auto origin = EffectAttribution::OwnerOrigin(rec); !origin.empty())
                r.type += (r.type.empty() ? "" : " \xC2\xB7 ") + origin;

            const std::string kindText = r.type;

            if (rec.duration > 0.0f && rec.timeLeft > 0.0f)
                r.type += (r.type.empty() ? "" : " \xC2\xB7 ") + FormatTimeLeft(rec.timeLeft);

            r.tone = hostile ? -1 : 1;
            std::string val;
            const bool ratePool = pool == AV::kHealRate || pool == AV::kStaminaRate || pool == AV::kMagickaRate;
            if (ratePool || (av != pool && EffectCategories::GetSkillTarget(pool) == pool))
                val = Tip::DescribeEffect(base, mag, rec.duration, hostile);
            if (val.empty()) val = FormatMagnitude(mag, av, hostile);
            r.lines.push_back({ std::move(val), hostile ? kDebuff : kBuff });

            auto it = std::find_if(rows.begin(), rows.end(), [&](const TipRow& x) {
                return x.name == r.name && x.tone == r.tone; });
            if (it == rows.end()) {
                rows.push_back(std::move(r));
                stacks.push_back({ key, av, mag, rec.timeLeft, 1, kindText });
        }
        }
        AddNeedCutRows(rows, actor, pool, s_secEffects);
        AddUnknownCutRow(rows, actor, pool, s_secEffects);
        std::stable_partition(rows.begin(), rows.end(), [](const TipRow& x) { return x.section == s_secEquip; });
        if (!skillPool) return rows;

        static std::string s_secPerks;
        if (s_secPerks.empty()) s_secPerks = LocalizationManager::Get("PartySheetV2.SectionPerks");
        auto* perkIcon = TextureManager::GetPartyIcon("perk");
        for (const auto& p : PerkModifiers::GetSkillPerks(actor, pool).perks) {
            TipRow r;
            r.icon    = perkIcon;
            r.name    = p.name;
            r.section = s_secPerks;
            if (p.maxRank > 1) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d/%d", p.rank, p.maxRank);
                r.type = buf;
            }
            if (!p.description.empty()) r.lines.push_back({ p.description, kTextMuted });
            rows.push_back(std::move(r));
        }
        return rows;
    }

    bool HasStatPassives(RE::Actor* actor, RE::ActorValue pool) {
        return !StatSourceRows(actor, pool).empty();
    }

    void DrawStatTooltip(RE::Actor* actor, const std::string& title, RE::ActorValue pool, float sc) {
        auto rows = StatSourceRows(actor, pool);
        if (rows.empty()) return;
        std::string t = title;
        for (char& c : t) c = static_cast<char>(toupper(c));
        DrawCompactTooltip(t, rows, {}, sc);
    }

    void DrawPassiveTooltip(RE::Actor* actor, const std::string& title, float sc, bool stoneOnly) {
        static std::string s_noneTxt;
        if (s_noneTxt.empty()) s_noneTxt = LocalizationManager::Get("Common.None");

        RE::SpellItem* stone = stoneOnly ? EffectAttribution::GetStandingStone(actor) : nullptr;
        TipRow stoneRow;
        if (stone) {
            using ST = RE::MagicSystem::SpellType;
            const auto t = stone->GetSpellType();
            stoneRow.icon = TextureManager::GetPartyIcon("standing_stone");
            stoneRow.name = stone->GetFullName() ? stone->GetFullName() : "";
            stoneRow.type = LocalizationManager::Get((t == ST::kPower || t == ST::kLesserPower)
                ? "PartySheetV2.SpellTypePower" : "PartySheetV2.SpellTypeAbility");
            std::string desc = ResolveStoneDescription(stone);
            if (!desc.empty()) stoneRow.lines.push_back({ std::move(desc), kTextMuted });
        }
        const bool stoneHasDesc = !stoneRow.lines.empty();

        std::vector<TipRow> rows;
        std::vector<TipRow> plain;
        for (auto& e : GetPermanentEffects(actor, EffectBucket::Passive)) {
            ImU32 col = e.isHostile ? kDebuff : kBuff;
            if (e.sourceSpell && EffectAttribution::IsStandingStoneSource(e.sourceSpell)) {
                if (stone && !stoneHasDesc) {
                    std::string txt = StoneLineText(e, stoneRow.name);
                    if (!e.valStr.empty()) { txt += " "; txt += e.valStr; }
                    bool dup = std::any_of(stoneRow.lines.begin(), stoneRow.lines.end(),
                        [&](const TipLine& l) { return l.text == txt; });
                    if (!dup) stoneRow.lines.push_back({ std::move(txt), col });
                }
                continue;
            }
            if (stoneOnly) continue;
            TipRow r{ EffectAttribution::ResolveMagicIcon(e.sourceSpell), e.name, {}, {} };
            r.tone = e.isHostile ? -1 : 1;
            if (!e.valStr.empty()) r.lines.push_back({ e.valStr, col });
            plain.push_back(std::move(r));
        }
        if (stone) rows.push_back(std::move(stoneRow));
        for (auto& r : plain) rows.push_back(std::move(r));

        if (!stoneOnly) {
            const auto other = PerkModifiers::GetOtherPerks(actor).perks;
            if (!other.empty()) {
                static std::string s_secPassive, s_secPerks;
                if (s_secPassive.empty()) {
                    s_secPassive = LocalizationManager::Get("PartySheetV2.SectionPassive");
                    s_secPerks   = LocalizationManager::Get("PartySheetV2.SectionPerks");
                }
                for (auto& r : rows) r.section = s_secPassive;
                auto* perkIcon = TextureManager::GetPartyIcon("perk");
                for (const auto& p : other) {
                    TipRow r;
                    r.icon    = perkIcon;
                    r.name    = p.name;
                    r.section = s_secPerks;
                    r.lines.push_back({ p.description, kTextMuted });
                    rows.push_back(std::move(r));
                }
            }
        }
        DrawCompactTooltip(title, rows, s_noneTxt, sc);
    }

    bool HasBlessings(RE::Actor* actor) {
        return !GetPermanentEffects(actor, EffectBucket::Blessing).empty();
    }

    void DrawBlessingsTooltip(RE::Actor* actor, const std::string& title, float sc) {
        auto entries = GetPermanentEffects(actor, EffectBucket::Blessing);
        if (entries.empty()) return;

        auto TimeLeft = [actor](RE::SpellItem* sp) -> float {
            float best = -1.0f;
            if (!sp) return best;
            for (const auto& rec : EffectAttribution::Get(actor))
                if (rec.spell == sp && rec.duration > 0.0f)
                    best = (std::max)(best, rec.timeLeft);
            return best;
        };

        std::vector<TipRow> rows;
        for (auto& e : entries) {
            const std::string name = e.source.empty() ? e.name : e.source;
            size_t k = 0;
            for (; k < rows.size(); ++k)
                if (rows[k].name == name) break;
            if (k == rows.size()) {
                TipRow r;
                r.icon = EffectAttribution::ResolveBlessingIcon(e.sourceSpell);
                r.name = name;
                r.tone = 1;
                float left = TimeLeft(e.sourceSpell);
                if (left > 0.0f) r.type = FormatTimeLeft(left);
                rows.push_back(std::move(r));
            }
            TipLine line;
            if (!e.valStr.empty() && !e.name.empty()) {
                line = { e.name + " " + e.valStr, e.isHostile ? kDebuff : kBuff };
            } else if (e.mgef && e.mgef->magicItemDescription.c_str() && e.mgef->magicItemDescription.c_str()[0]) {
                line = { EffectCategories::FillDescriptionTags(e.mgef->magicItemDescription.c_str()), kTextMuted };
            } else {
                continue;
            }
            auto& ls = rows[k].lines;
            if (std::none_of(ls.begin(), ls.end(), [&](const TipLine& l) { return l.text == line.text; }))
                ls.push_back(std::move(line));
        }
        DrawCompactTooltip(title, rows, {}, sc);
    }

    namespace {
        RE::TESForm* Home(RE::FormID id, const char* file) {
            auto* dh = RE::TESDataHandler::GetSingleton();
            return dh ? dh->LookupForm(id, file) : nullptr;
        }
        bool Has(RE::Actor* a, RE::FormID id, const char* file) {
            auto* sp = Home(id, file);
            return sp && sp->Is(RE::FormType::Spell) && a->HasSpell(sp->As<RE::SpellItem>());
        }
        bool GlobalOn(RE::FormID id) {
            auto* g = Home(id, "Skyrim.esm");
            return g && g->Is(RE::FormType::Global) && g->As<RE::TESGlobal>()->value > 0.0f;
        }
    }

    CurseInfo GetCurse(RE::Actor* actor) {
        CurseInfo c;
        if (!actor) return c;
        const bool isPlayer = actor->IsPlayerRef();
        auto* race = actor->GetRace();

        bool vamp = actor->HasKeywordString("Vampire")
                 || Has(actor, 0x00283B, "Dawnguard.esm")
                 || (race && race == Home(0x00283A, "Dawnguard.esm"))
                 || (isPlayer && GlobalOn(0x0ED06D));
        if (vamp) {
            c.kind = CurseInfo::Kind::Vampire;
            static constexpr RE::FormID kStage[4] = { 0x02E1C3, 0x0ED099, 0x0ED09D, 0x0ED09E };
            for (int i = 0; i < 4; ++i)
                if (Has(actor, kStage[i], "Skyrim.esm")) { c.stage = i + 1; break; }
            return c;
        }
        bool wolf = (race && race->GetFormID() == 0x000CDD84)
                 || Has(actor, 0x092C48, "Skyrim.esm")
                 || (isPlayer && GlobalOn(0x0ED06C));
        if (!wolf)
            if (auto* fac = Home(0x07C90A, "Skyrim.esm"); fac && fac->Is(RE::FormType::Faction))
                wolf = actor->IsInFaction(fac->As<RE::TESFaction>());
        if (wolf) c.kind = CurseInfo::Kind::Werewolf;
        return c;
    }

    void DrawCurseTooltip(RE::Actor* actor, const std::string& title, float sc) {
        struct Id { RE::FormID id; const char* file; };
        static constexpr Id kVampire[] = {
            { 0x00283B, "Dawnguard.esm" },
            { 0x0ED0A4, "Skyrim.esm" }, { 0x0ED0A5, "Skyrim.esm" }, { 0x0ED0A6, "Skyrim.esm" }, { 0x0ED0A7, "Skyrim.esm" },
            { 0x08D5BF, "Skyrim.esm" }, { 0x08D5C0, "Skyrim.esm" }, { 0x08D5C1, "Skyrim.esm" }, { 0x08D5C2, "Skyrim.esm" },
            { 0x0C4DE2, "Skyrim.esm" },
            { 0x088821, "Skyrim.esm" },
            { 0x0C4DE1, "Skyrim.esm" },
            { 0x0F5B54, "Skyrim.esm" }, { 0x10F1E7, "Skyrim.esm" },
            { 0x02E1C3, "Skyrim.esm" }, { 0x0ED099, "Skyrim.esm" }, { 0x0ED09D, "Skyrim.esm" }, { 0x0ED09E, "Skyrim.esm" },
            { 0x10F1EB, "Skyrim.esm" }, { 0x10F1EA, "Skyrim.esm" }, { 0x10F1E9, "Skyrim.esm" }, { 0x10F1E8, "Skyrim.esm" },
            { 0x0C1E8B, "Skyrim.esm" }, { 0x0ED09A, "Skyrim.esm" }, { 0x0ED09B, "Skyrim.esm" }, { 0x0ED09C, "Skyrim.esm" },
        };
        static constexpr Id kWerewolf[] = {
            { 0x092C48, "Skyrim.esm" },
            { 0x0F8306, "Skyrim.esm" },
        };
        const auto c = GetCurse(actor);
        if (c.kind == CurseInfo::Kind::None) return;
        const bool vamp = c.kind == CurseInfo::Kind::Vampire;
        const Id* ids = vamp ? kVampire : kWerewolf;
        const size_t n = vamp ? std::size(kVampire) : std::size(kWerewolf);

        using ST = RE::MagicSystem::SpellType;
        std::vector<TipRow> rows;
        for (size_t i = 0; i < n; ++i) {
            auto* f = Home(ids[i].id, ids[i].file);
            auto* sp = (f && f->Is(RE::FormType::Spell)) ? f->As<RE::SpellItem>() : nullptr;
            if (!sp || !actor->HasSpell(sp)) continue;
            const char* nm = sp->GetFullName();
            if (!nm || !nm[0]) continue;
            if (std::any_of(rows.begin(), rows.end(), [&](const TipRow& r) { return r.name == nm; })) continue;

            TipRow r;
            r.name = nm;
            const auto t = sp->GetSpellType();
            r.type = LocalizationManager::Get((t == ST::kPower || t == ST::kLesserPower)
                ? "PartySheetV2.SpellTypePower"
                : (t == ST::kSpell ? "PartySheetV2.SpellTypeSpell" : "PartySheetV2.SourcePassive"));
            r.icon = (t == ST::kPower || t == ST::kLesserPower) ? TextureManager::GetPartyIcon("power")
                                                               : EffectAttribution::ResolveMagicIcon(sp);
            bool bad = false;
            for (auto* eff : sp->effects) {
                if (!eff || !eff->baseEffect) continue;
                bad = bad || EffectCategories::IsDebuff(eff->baseEffect);
            }
            for (auto* eff : sp->effects) {
                if (!eff || !eff->baseEffect) continue;
                const char* d = eff->baseEffect->magicItemDescription.c_str();
                if (!d || !d[0]) continue;
                r.lines.push_back({ EffectCategories::FillDescriptionTags(d, eff->effectItem.magnitude,
                                        static_cast<float>(eff->effectItem.duration)), kTextMuted });
                break;
            }
            r.tone = (t == ST::kAbility) ? (bad ? -1 : 1) : 0;
            rows.push_back(std::move(r));
        }
        std::string none = LocalizationManager::Get("Common.None");
        DrawCompactTooltip(title, rows, none, sc);
    }

}
