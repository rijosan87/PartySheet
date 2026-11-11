#include "../../includes/Utils/EffectAttribution.h"
#include "../../includes/EffectCategories.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/TextureManager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
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
                r.hostile    = base->IsHostile() || base->IsDetrimental() || ae->magnitude < 0.0f;
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
        }
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
}
