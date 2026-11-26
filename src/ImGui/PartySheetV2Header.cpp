
#include "../../includes/ImGui/PartySheetV2Modal.h"
#include "../../includes/ImGui/PartySheetV2Tooltip.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/Utils/EffectAttribution.h"
#include "../../includes/Utils/EffectIconRenderer.h"
#include "../../includes/TextureManager.h"
#include "../../includes/LocalizationManager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

namespace PartySheetV2 {

    namespace {
        constexpr int kMaxEffects = 64;

        std::vector<EffectIconRenderer::EffectDisplayInfo> ActiveEffects(RE::Actor* actor) {
            return EffectIconRenderer::GetActiveEffectsInfo(actor, kMaxEffects, 0, 0);
        }

        std::vector<const EffectAttribution::Record*> NeedStages(RE::Actor* actor) {
            std::vector<const EffectAttribution::Record*> out;
            for (auto* r : EffectAttribution::NeedStages(actor))
                if (!EffectAttribution::SpellStatChanges(actor, r->spell).empty()) out.push_back(r);
            return out;
        }

        constexpr bool kHiddenOnly = true;

        void AddStatLines(Tip::TipRow& r, RE::Actor* actor, const RE::MagicItem* spell, bool hiddenOnly = false) {
            auto changes = EffectAttribution::SpellStatChanges(actor, spell, hiddenOnly);
            for (auto& l : EffectAttribution::FormatStatChanges(changes))
                r.lines.push_back({ std::move(l.text), l.cut ? Tip::kDebuff : Tip::kBuff });
        }

        std::vector<PlayerHUDDisease::ActiveDisease> Diseases(RE::Actor* actor) {
            if (!actor || !actor->IsPlayerRef()) return {};
            return PlayerHUDDisease::GetActiveDiseases();
        }

        std::string Upper(std::string s) {
            for (char& c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            return s;
        }

        std::string TimeLeft(float t) {
            if (t < 0.0f) return {};
            char b[16];
            if (t == 0.0f)         snprintf(b, sizeof(b), "\xe2\x88\x9e");
            else if (t < 60.0f)    snprintf(b, sizeof(b), "%.0fs", t);
            else if (t < 3600.0f)  snprintf(b, sizeof(b), "%.0fm", t / 60.0f);
            else                   snprintf(b, sizeof(b), "%.0fh", t / 3600.0f);
            return b;
        }

        std::string Magnitude(const EffectIconRenderer::EffectDisplayInfo& e) {
            if (e.magnitude <= 0.0f) return {};
            if (e.primaryAV != RE::ActorValue::kNone && !EffectCategories::IsGameplayAV(e.primaryAV)) return {};
            char b[32];
            snprintf(b, sizeof(b), "%s%.0f%s", e.isHostile ? "-" : "+", e.magnitude,
                EffectCategories::IsPercentageMagnitude(e.primaryAV) ? "%" : "");
            return b;
        }

        const char* SeverityKey(const PlayerHUDDisease::ActiveDisease& d) {
            if (d.maxStage > 1) {
                if (d.stage >= 3) return "disease_stage_crippling";
                if (d.stage == 2) return "disease_stage_severe";
            }
            return "disease_stage_infected";
        }
    }

    bool HasActiveEffects(RE::Actor* actor) {
        return !ActiveEffects(actor).empty() || !NeedStages(actor).empty() || !Diseases(actor).empty();
    }

    void DrawActiveEffectsTooltip(RE::Actor* actor, const std::string& title, float scale) {
        static std::string s_buffs, s_debuffs, s_needs, s_diseases;
        if (s_buffs.empty()) {
            s_buffs    = Upper(LocalizationManager::Get("PartySheetV2.SectionBuffs"));
            s_debuffs  = Upper(LocalizationManager::Get("PartySheetV2.SectionDebuffs"));
            s_needs    = Upper(LocalizationManager::Get("PartySheetV2.SectionNeeds"));
            s_diseases = Upper(LocalizationManager::Get("header_disease_status"));
        }

        std::vector<Tip::TipRow> buffs, debuffs, needs, diseases;
        std::unordered_set<const RE::MagicItem*> summarized;

        for (const auto& e : ActiveEffects(actor)) {
            Tip::TipRow r;
            r.icon = e.needIcon ? e.needIcon : TextureManager::GetEffectIcon(e.iconId);
            const bool hasSource = !e.sourceName.empty() && e.sourceName != e.name;
            r.name = hasSource ? e.sourceName : e.name;
            r.type = TimeLeft(e.timeRemaining);
            std::string line = hasSource ? e.name : std::string{};
            if (const auto mag = Magnitude(e); !mag.empty()) line += line.empty() ? mag : "  " + mag;
            if (!line.empty()) r.lines.push_back({ line, e.isHostile ? Tip::kDebuff : Tip::kBuff });
            if (e.spell && summarized.insert(e.spell).second) AddStatLines(r, actor, e.spell, kHiddenOnly);
            r.tone    = e.isHostile ? -1 : 1;
            r.section = e.isHostile ? s_debuffs : s_buffs;
            (e.isHostile ? debuffs : buffs).push_back(std::move(r));
        }

        const auto poolCuts = EffectAttribution::NeedPoolCuts(actor);
        for (auto* n : NeedStages(actor)) {
            const bool hasPoolCut = std::any_of(poolCuts.begin(), poolCuts.end(),
                [&](const auto& c) { return c.record->spell == n->spell; });
            if (hasPoolCut && summarized.contains(n->spell)) continue;
            Tip::TipRow r;
            r.icon = EffectAttribution::OwnerIcon(*n);
            r.name = EffectAttribution::OwnerName(*n);
            AddStatLines(r, actor, n->spell);
            r.tone    = n->hostile ? -1 : 1;
            r.section = s_needs;
            needs.push_back(std::move(r));
        }

        for (const auto& d : Diseases(actor)) {
            Tip::TipRow r;
            r.icon = d.icon;
            r.name = d.name;
            r.type = LocalizationManager::Get(SeverityKey(d));
            if (!d.penalty.empty()) r.lines.push_back({ d.penalty, Tip::kDebuff });
            r.tone    = -1;
            r.section = s_diseases;
            diseases.push_back(std::move(r));
        }

        std::vector<Tip::TipRow> rows;
        for (auto* run : { &buffs, &debuffs, &needs, &diseases })
            for (auto& r : *run) rows.push_back(std::move(r));
        Tip::DrawCompactTooltip(title, rows, LocalizationManager::Get("Common.None"), scale, {}, {}, Tip::kSplitColumns);
    }

    void DrawBehaviorTooltip(RE::Actor* actor, const std::string& title, float scale) {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return;
        static constexpr const char* kAggression[] = { "AI.Aggression.Unaggressive", "AI.Aggression.Aggressive",
                                                       "AI.Aggression.VeryAggressive", "AI.Aggression.Frenzied" };
        static constexpr const char* kConfidence[] = { "AI.Confidence.Cowardly", "AI.Confidence.Cautious",
                                                       "AI.Confidence.Average", "AI.Confidence.Brave",
                                                       "AI.Confidence.Foolhardy" };
        static constexpr const char* kAssistance[] = { "AI.Assistance.Nobody", "AI.Assistance.Allies",
                                                       "AI.Assistance.FriendsAndAllies" };
        static constexpr const char* kMorality[]   = { "AI.Morality.AnyCrime", "AI.Morality.ViolenceAgainstEnemy",
                                                       "AI.Morality.PropertyCrimeOnly", "AI.Morality.NoCrime" };
        auto Pick = [](const auto& keys, auto level) -> std::string {
            const auto i = static_cast<std::size_t>(level);
            return i < std::size(keys) ? LocalizationManager::Get(keys[i]) : std::string("?");
        };
        std::vector<Tip::SumLine> lines = {
            { LocalizationManager::Get("AI.Aggression.Label"), {}, Pick(kAggression, base->GetAggressionLevel()), IM_COL32(255, 120,  80, 255) },
            { LocalizationManager::Get("AI.Confidence.Label"), {}, Pick(kConfidence, base->GetConfidenceLevel()), IM_COL32(100, 200, 255, 255) },
            { LocalizationManager::Get("AI.Assistance.Label"), {}, Pick(kAssistance, base->GetAssistanceLevel()), IM_COL32(100, 220, 120, 255) },
            { LocalizationManager::Get("AI.Morality.Label"),   {}, Pick(kMorality,   base->GetMoralityLevel()),   IM_COL32(210, 180, 120, 255) },
        };
        Tip::DrawSumTooltip(title, lines, scale);
    }

}
