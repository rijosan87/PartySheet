#include "../../includes/Utils/EffectAttribution.h"
#include "../../includes/EffectCategories.h"
#include "../../includes/HUDSymptoms.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/TextureManager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace EffectAttribution {

    namespace {

        void ReassignPerkGrantedAbilities(std::vector<Record>& recs) {
            std::unordered_map<RE::SpellItem*, std::pair<std::size_t, RE::BGSPerk*>> granted;
            for (std::size_t i = 0; i < recs.size(); ++i) {
                auto* perk = recs[i].mgef->data.perk;
                if (!perk) continue;
                for (auto* entry : perk->perkEntries) {
                    if (!entry || entry->GetType() != RE::PERK_ENTRY_TYPE::kAbility) continue;
                    if (auto* ab = static_cast<RE::BGSAbilityPerkEntry*>(entry)->ability)
                        granted.try_emplace(ab, i, perk);
                }
            }
            if (granted.empty()) return;

            for (auto& r : recs) {
                if (r.kind != OwnerKind::Passive && r.kind != OwnerKind::Spell) continue;
                auto it = r.spellItem ? granted.find(r.spellItem) : granted.end();
                if (it == granted.end()) continue;
                const Record& g = recs[it->second.first];
                if (&g == &r) continue;
                r.kind       = g.kind;
                r.spell      = g.spell;
                r.spellItem  = g.spellItem;
                r.item       = g.item;
                r.sourceName = g.sourceName;
                r.duration   = g.duration;
                r.timeLeft   = g.timeLeft;
                r.grantedByPerk = it->second.second;
            }
        }

        EffectClass ClassFor(const Record& r) {
            switch (r.kind) {
            case OwnerKind::Equipment: return EffectClass::Gear;
            case OwnerKind::Disease:
            case OwnerKind::Need:      return EffectClass::Condition;
            case OwnerKind::Race:
            case OwnerKind::Stone:     return EffectClass::Permanent;
            default: break;
            }
            if (r.duration > 0.0f) return EffectClass::Timed;
            return r.hostile ? EffectClass::Condition : EffectClass::Permanent;
        }

        void Walk(RE::Actor* actor, std::vector<Record>& out) {
            auto* mt   = actor ? actor->AsMagicTarget() : nullptr;
            auto* list = mt ? mt->GetActiveEffectList() : nullptr;
            if (!list) return;

            std::unordered_set<RE::SpellItem*> raceSpells;
            if (auto* race = actor->GetRace(); race && race->actorEffects && race->actorEffects->spells)
                for (std::uint32_t i = 0; i < race->actorEffects->numSpells; ++i)
                    if (auto* sp = race->actorEffects->spells[i]) raceSpells.insert(sp);

            using Src = EffectCategories::EffectSource;
            for (auto* ae : *list) {
                if (!ae) continue;
                if (ae->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                    ae->flags.any(RE::ActiveEffect::Flag::kInactive)) continue;
                auto* base = ae->GetBaseObject();
                if (!base) continue;

                Record r;
                r.effect    = ae;
                r.mgef      = base;
                r.spell     = ae->spell;
                r.spellItem = ae->spell ? ae->spell->As<RE::SpellItem>() : nullptr;
                r.item      = ae->source;
                r.mechanism = EffectCategories::GetEffectSource(ae);
                r.kye2Armor = IsKye2ArmorSource(ae->spell);
                r.primaryAV = base->data.primaryAV;
                if (base->GetArchetype() == RE::EffectSetting::Archetype::kDualValueModifier)
                    r.secondaryAV = base->data.secondaryAV;
                r.rawMagnitude = ae->magnitude;
                r.magnitude    = std::abs(ae->magnitude);
                if (r.magnitude <= 0.0f && ae->effect) r.magnitude = std::abs(ae->effect->GetMagnitude());
                r.duration = ae->duration;
                r.timeLeft = ae->duration > 0.0f ? ae->duration - ae->elapsedSeconds : -1.0f;
                r.hostile    = EffectCategories::IsDebuff(base, ae->magnitude);
                r.hiddenInUI = base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
                if (const char* n = base->GetFullName()) r.effectName = n;
                if (ae->spell)
                    if (const char* n = ae->spell->GetFullName()) r.sourceName = n;

                auto* sp = r.spellItem;
                if (r.mechanism == Src::Enchantment || r.kye2Armor)                 r.kind = OwnerKind::Equipment;
                else if (r.mechanism == Src::Blessing)                              r.kind = OwnerKind::Blessing;
                else if (r.mechanism == Src::Potion || r.mechanism == Src::Food)    r.kind = OwnerKind::Potion;
                else if (sp && sp->GetSpellType() == RE::MagicSystem::SpellType::kDisease) r.kind = OwnerKind::Disease;
                else if (sp && (PlayerHUDSurvival::IsNeedSpell(sp) || PlayerHUDSurvival::IsVitalitySpell(sp)))
                                                                                    r.kind = OwnerKind::Need;
                else if (sp && raceSpells.count(sp))                                r.kind = OwnerKind::Race;
                else if (sp && IsStandingStoneSource(sp))                           r.kind = OwnerKind::Stone;
                else if (r.mechanism == Src::Ability)                               r.kind = OwnerKind::Passive;
                else                                                                r.kind = OwnerKind::Spell;
                out.push_back(r);
            }

            ReassignPerkGrantedAbilities(out);
            for (auto& r : out) r.cls = ClassFor(r);
        }
    }

    bool ShowsOnHud(EffectClass cls) {
        return cls == EffectClass::Timed || cls == EffectClass::Condition;
    }

    const Record* Find(RE::Actor* actor, const RE::ActiveEffect* effect) {
        for (const auto& r : Get(actor))
            if (r.effect == effect) return &r;
        return nullptr;
    }

    GroupKey GroupKeyOf(const RE::ActiveEffect* effect) {
        GroupKey key;
        if (!effect) return key;
        key.mgef = effect->GetBaseObject();
        key.source = effect->spell ? static_cast<const void*>(effect->spell) : static_cast<const void*>(effect->source);
        return key;
    }

    EffectClass ClassOf(RE::Actor* actor, const RE::ActiveEffect* effect) {
        const Record* r = Find(actor, effect);
        return r ? r->cls : EffectClass::Timed;
    }

    const std::vector<Record>& Get(RE::Actor* actor) {
        static const std::vector<Record> kEmpty;
        if (!actor) return kEmpty;

        static int s_frame = -1;
        static std::unordered_map<RE::FormID, std::vector<Record>> s_cache;
        const int frame = ImGui::GetFrameCount();
        if (frame != s_frame) {
            s_frame = frame;
            s_cache.clear();
        }

        auto [it, inserted] = s_cache.try_emplace(actor->GetFormID());
        if (inserted) Walk(actor, it->second);
        return it->second;
    }

    std::string OwnerName(const Record& r) {
        if (r.kind == OwnerKind::Equipment && r.item) {
            const char* n = r.item->GetName();
            if (n && n[0]) return n;
        }
        if (r.sourceName && r.sourceName[0]) return r.sourceName;
        return r.effectName ? r.effectName : "";
    }

    ID3D11ShaderResourceView* OwnerIcon(const Record& r) {
        ID3D11ShaderResourceView* icon = nullptr;
        switch (r.kind) {
            case OwnerKind::Equipment:
                if (r.item && r.item->GetName() && r.item->GetName()[0])
                    icon = TextureManager::GetItemIcon(r.item, "");
                if (!icon) icon = TextureManager::GetPartyIcon("armor");
                break;
            case OwnerKind::Race:
                icon = TextureManager::GetPartyIcon("race");
                break;
            case OwnerKind::Stone:
                icon = TextureManager::GetPartyIcon("standing_stone");
                break;
            case OwnerKind::Disease:
                icon = TextureManager::GetPartyIcon("disease");
                break;
            case OwnerKind::Potion: {
                RE::TESForm* item = r.item ? static_cast<RE::TESForm*>(r.item)
                                           : static_cast<RE::TESForm*>(r.spell);
                icon = TextureManager::GetItemIcon(item, "potion");
                break;
            }
            case OwnerKind::Blessing:
                icon = ResolveBlessingIcon(r.spellItem);
                break;
            case OwnerKind::Need:
                icon = PlayerHUDSurvival::GetNeedSpellIcon(r.spellItem);
                break;
            default:
                break;
        }
        return icon ? icon : ResolveMagicIcon(r.spell);
    }

    const std::string& KindLabel(OwnerKind kind) {
        static std::string s_labels[9];
        if (s_labels[0].empty()) {
            using K = OwnerKind;
            s_labels[static_cast<int>(K::Equipment)] = LocalizationManager::Get("PartySheetV2.SourceEquipment");
            s_labels[static_cast<int>(K::Blessing)]  = LocalizationManager::Get("PartySheetV2.SourceBlessing");
            s_labels[static_cast<int>(K::Potion)]    = LocalizationManager::Get("PartySheetV2.SourcePotion");
            s_labels[static_cast<int>(K::Disease)]   = LocalizationManager::Get("Symptom.KindDisease");
            s_labels[static_cast<int>(K::Need)]      = LocalizationManager::Get("Symptom.KindNeed");
            s_labels[static_cast<int>(K::Race)]      = LocalizationManager::Get("PartySheetV2.SourceRace");
            s_labels[static_cast<int>(K::Stone)]     = LocalizationManager::Get("PartySheetV2.SourcePassive");
            s_labels[static_cast<int>(K::Passive)]   = LocalizationManager::Get("PartySheetV2.SourcePassive");
            s_labels[static_cast<int>(K::Spell)]     = LocalizationManager::Get("PartySheetV2.SpellTypeSpell");
        }
        return s_labels[static_cast<int>(kind)];
    }

    std::string OwnerOrigin(const Record& r) {
        if (r.kind != OwnerKind::Passive && r.kind != OwnerKind::Spell) return {};
        auto* file = r.spell ? r.spell->GetFile(0) : nullptr;
        if (!file) return {};
        std::string fn(file->GetFilename());
        std::string lower = fn;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        static constexpr std::string_view kBase[] = {
            "skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm", "dragonborn.esm" };
        for (auto b : kBase)
            if (lower == b) return {};
        if (lower.starts_with("cc")) return {};
        if (auto dot = fn.find_last_of('.'); dot != std::string::npos) fn.resize(dot);
        return fn;
    }

    float DisplayMagnitude(const Record& r, RE::ActorValue av) {
        float mag = r.magnitude;
        if (EffectCategories::IsPercentageMagnitude(av) && mag > 0.0f && mag < 1.0f)
            mag *= 100.0f;
        return mag;
    }

    bool IsKye2ArmorSource(RE::MagicItem* spell) {
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

    bool IsStandingStoneSource(RE::SpellItem* sp) {
        if (!sp) return false;
        switch (sp->GetFormID()) {
            case 0x000E5F4C:
            case 0x000E5F47:
            case 0x000E5F45:
            case 0x000E5F58:
            case 0x000E5F54:
            case 0x000E5F4E:
            case 0x0010E439:
            case 0x00103A94:
            case 0x000E5F51:
            case 0x000E5F5A:
            case 0x000E7328:
            case 0x000E7329:
            case 0x000E5F61:
            case 0x000E732A:
                return true;
            default:
                break;
        }
        if (auto* dh = RE::TESDataHandler::GetSingleton()) {
            if (auto* steed = dh->LookupForm(0x000E5F5E, "ccQDRSSE001-SurvivalMode.esl"))
                if (sp == steed) return true;
        }
        return false;
    }

    RE::SpellItem* GetStandingStone(RE::Actor* actor) {
        if (!actor) return nullptr;
        static constexpr RE::FormID kStones[] = {
            0x000E5F4C, 0x000E5F47, 0x000E5F45, 0x000E5F58, 0x000E5F54, 0x000E5F4E,
            0x000E5F51, 0x000E5F5A, 0x000E7328, 0x000E7329, 0x000E5F61, 0x000E732A };
        for (auto id : kStones)
            if (auto* sp = RE::TESForm::LookupByID<RE::SpellItem>(id); sp && actor->HasSpell(sp))
                return sp;
        if (auto* dh = RE::TESDataHandler::GetSingleton())
            if (auto* steed = dh->LookupForm<RE::SpellItem>(0x000E5F5E, "ccQDRSSE001-SurvivalMode.esl");
                steed && actor->HasSpell(steed))
                return steed;
        return nullptr;
    }

    ID3D11ShaderResourceView* ResolveMagicIcon(RE::MagicItem* item) {
        if (!item) return nullptr;
        RE::EffectSetting* base = item->GetAVEffect();
        if (!base) {
            if (item->effects.empty() || !item->effects[0]) return nullptr;
            base = item->effects[0]->baseEffect;
        }
        if (!base) return nullptr;

        std::string iconId;
        auto effectType = EffectCategories::GetEffectType(base->GetArchetype());
        if (effectType == EffectCategories::EffectType::Standalone)
            iconId = EffectCategories::GetStandaloneIconId(base->GetArchetype());
        else if (effectType == EffectCategories::EffectType::Modifier)
            iconId = EffectCategories::GetActorValueIconId(base->data.primaryAV);
        else {
            const char* nm = base->GetFullName();
            iconId = EffectCategories::GetScriptEffectIconId(nm ? nm : "", base->data.associatedSkill);
        }
        if (iconId.empty()) return nullptr;
        return TextureManager::GetEffectIcon(iconId);
    }

    ID3D11ShaderResourceView* ResolveBlessingIcon(RE::SpellItem* spell) {
        if (spell) {
            static const std::unordered_map<std::string, const char*> kOverrides = {
            };
            if (const char* ed = spell->GetFormEditorID(); ed && ed[0]) {
                auto it = kOverrides.find(ed);
                if (it != kOverrides.end()) {
                    if (auto* tex = TextureManager::GetPartyIcon(it->second))
                        return tex;
                }
            }
        }
        return TextureManager::GetPartyIcon("blessing");
    }

    static bool IsTimedBuffSource(const Record& rec) {
        return rec.kind == OwnerKind::Potion || (rec.kind == OwnerKind::Spell && rec.duration > 0.0f);
    }

    static bool StatEffectMatches(RE::Actor* actor, RE::ActorValue stat, bool wantDebuff, bool timedOnly) {
        const bool skill = EffectCategories::GetSkillTarget(stat) == stat;
        auto Hits = [&](RE::ActorValue av) { return av != RE::ActorValue::kNone && (av == stat || (skill && EffectCategories::GetSkillTarget(av) == stat)); };
        for (const auto& rec : Get(actor)) {
            if (rec.hostile != wantDebuff) continue;
            if (timedOnly && !IsTimedBuffSource(rec)) continue;
            const RE::ActorValue av = Hits(rec.primaryAV) ? rec.primaryAV : (Hits(rec.secondaryAV) ? rec.secondaryAV : RE::ActorValue::kNone);
            if (av == RE::ActorValue::kNone) continue;
            if (DisplayMagnitude(rec, av) >= 0.5f) return true;
}
        return false;
    }

    bool HasTimedBuff(RE::Actor* actor, RE::ActorValue stat) { return StatEffectMatches(actor, stat, false, true); }
    bool HasStatDebuff(RE::Actor* actor, RE::ActorValue stat) { return StatEffectMatches(actor, stat, true, false); }

    namespace {
        enum class CutNeed : std::uint8_t { kHunger, kFatigue, kCold };
        struct CutEffect { RE::FormID localId; CutNeed need; };

        constexpr CutEffect kCutEffects[] = {
            { 0x95B, CutNeed::kHunger },  { 0x95C, CutNeed::kHunger },  { 0x95D, CutNeed::kHunger },
            { 0x957, CutNeed::kFatigue }, { 0x958, CutNeed::kFatigue }, { 0x959, CutNeed::kFatigue },
            { 0x956, CutNeed::kCold },    { 0x955, CutNeed::kCold },
            { 0x954, CutNeed::kCold },    { 0x95F, CutNeed::kCold },
        };

        const std::unordered_map<const RE::EffectSetting*, CutNeed>& CutEffects() {
            static const auto s_map = [] {
                std::unordered_map<const RE::EffectSetting*, CutNeed> map;
                if (auto* dh = RE::TESDataHandler::GetSingleton())
                    for (const auto& c : kCutEffects)
                        if (auto* m = dh->LookupForm<RE::EffectSetting>(c.localId, "ccQDRSSE001-SurvivalMode.esl"))
                            map.emplace(m, c.need);
                return map;
            }();
            return s_map;
}

        bool HasStarfrost() {
            static const bool s_loaded = [] {
                auto* dh = RE::TESDataHandler::GetSingleton();
                return dh && dh->LookupModByName("Starfrost.esp") != nullptr;
            }();
            return s_loaded;
        }

        float DescriptionNumber(const RE::EffectSetting* mgef) {
            const char* text = mgef->magicItemDescription.c_str();
            for (const char* p = text; p && *p; ++p) {
                if (*p != '<') continue;
                char* end = nullptr;
                const float v = std::strtof(p + 1, &end);
                if (end != p + 1 && *end == '>') return v;
            }
            return 0.0f;
        }

        float CapPoints(RE::Actor* actor, RE::ActorValue pool) {
            const auto& bars = HUDSymptoms::GetBars(actor);
            switch (pool) {
            case RE::ActorValue::kHealth:  return bars.health.capPoints;
            case RE::ActorValue::kMagicka: return bars.magicka.capPoints;
            default:                       return bars.stamina.capPoints;
            }
        }

        std::vector<RE::ActorValue> PoolsCutBy(CutNeed need) {
            using AV = RE::ActorValue;
            if (need == CutNeed::kCold) return { AV::kHealth };
            if (HasStarfrost())         return { AV::kMagicka, AV::kStamina };
            return { need == CutNeed::kHunger ? AV::kStamina : AV::kMagicka };
        }
    }

    std::vector<NeedPoolCut> NeedPoolCuts(RE::Actor* actor) {
        std::vector<NeedPoolCut> out;
        if (!actor || !actor->IsPlayerRef()) return out;
        const auto& effects   = CutEffects();
        const bool  starfrost = HasStarfrost();
        for (const auto& r : Get(actor)) {
            const auto it = effects.find(r.mgef);
            if (it == effects.end()) continue;
            for (auto pool : PoolsCutBy(it->second)) {
                const float amount = starfrost ? DescriptionNumber(r.mgef) : CapPoints(actor, pool);
                if (amount >= 0.5f) out.push_back({ &r, { pool, -amount, starfrost } });
            }
        }
        return out;
    }

    std::vector<StatChange> SpellStatChanges(RE::Actor* actor, const RE::MagicItem* spell, bool hiddenOnly) {
        using Arch = RE::EffectSetting::Archetype;
        std::vector<StatChange> out;
        if (!spell) return out;
        for (const auto& r : Get(actor)) {
            if (r.spell != spell || r.primaryAV == RE::ActorValue::kNone) continue;
            if (hiddenOnly && !r.hiddenInUI) continue;
            const auto arch = r.mgef->GetArchetype();
            if (arch != Arch::kValueModifier && arch != Arch::kPeakValueModifier &&
                arch != Arch::kDualValueModifier) continue;
            for (auto av : { r.primaryAV, r.secondaryAV }) {
                if (!EffectCategories::IsGameplayAV(av)) continue;
                const float mag = DisplayMagnitude(r, av);
                if (mag <= 0.0f) continue;
                out.push_back({ av, r.hostile ? -mag : mag });
            }
        }
        for (const auto& cut : NeedPoolCuts(actor))
            if (cut.record->spell == spell) out.push_back(cut.change);
        return out;
    }

    std::vector<const Record*> NeedStages(RE::Actor* actor) {
        std::vector<const Record*> out;
        for (const auto& r : Get(actor)) {
            if (r.kind != OwnerKind::Need || r.timeLeft >= 0.0f || !r.spell) continue;
            if (std::none_of(out.begin(), out.end(), [&](const Record* o) { return o->spell == r.spell; }))
                out.push_back(&r);
        }
        return out;
    }

    namespace {
        constexpr int kSkillsPerTree = 6;

        std::string JoinStatNames(const std::vector<RE::ActorValue>& avs) {
            static const char* kTreeKeys[3] = { "Skills.CombatSkills", "Skills.MagicSkills", "Skills.StealthSkills" };
            static const char* kTreeEng[3]  = { "Combat skills", "Magic skills", "Stealth skills" };
            auto Label = [](const char* key, const char* eng) {
                std::string v = LocalizationManager::GetSingleton()->FindLabel(key);
                return v.empty() ? std::string(eng) : v;
            };

            std::unordered_set<std::string> seen;
            int perTree[3] = {};
            for (auto av : avs) {
                const int g = EffectCategories::SkillGroup(av);
                if (g >= 0 && seen.insert(EffectCategories::LocalizedAVName(av)).second) ++perTree[g];
            }
            bool full[3];
            for (int g = 0; g < 3; ++g) full[g] = perTree[g] == kSkillsPerTree;

            std::vector<std::string> names;
            if (full[0] && full[1] && full[2]) names.push_back(Label("Skills.AllSkills", "All skills"));
            else for (int g = 0; g < 3; ++g) if (full[g]) names.push_back(Label(kTreeKeys[g], kTreeEng[g]));

            for (auto av : avs) {
                const int g = EffectCategories::SkillGroup(av);
                if (g >= 0 && full[g]) continue;
                auto n = EffectCategories::LocalizedAVName(av);
                if (std::find(names.begin(), names.end(), n) == names.end()) names.push_back(std::move(n));
            }

            std::string out;
            for (const auto& n : names) out += (out.empty() ? "" : ", ") + n;
            return out;
        }
    }

    std::vector<StatLine> FormatStatChanges(const std::vector<StatChange>& changes) {
        struct Bucket { int value; bool pct; std::vector<RE::ActorValue> avs; };
        std::vector<Bucket> buckets;
        for (const auto& c : changes) {
            const int  v   = static_cast<int>(std::lround(c.value));
            const bool pct = c.percent || EffectCategories::IsPercentageMagnitude(c.av);
            auto it = std::find_if(buckets.begin(), buckets.end(),
                [&](const Bucket& b) { return b.value == v && b.pct == pct; });
            if (it == buckets.end()) buckets.push_back({ v, pct, { c.av } });
            else it->avs.push_back(c.av);
        }

        std::vector<StatLine> lines;
        for (const auto& b : buckets) {
            if (b.value == 0) continue;
            char num[24];
            snprintf(num, sizeof(num), "%+d%s", b.value, b.pct ? "%" : "");
            lines.push_back({ JoinStatNames(b.avs) + " " + num, b.value < 0 });
        }
        return lines;
    }
}
