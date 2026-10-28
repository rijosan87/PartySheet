#include "../includes/QuickHeal.h"

#include "../includes/PlayerStatus.h"
#include "../includes/Settings.h"
#include "../includes/Utils/WidgetJsonParser.h"
#include "../includes/ImGui/FollowerHUD.h"
#include "../includes/UIAnimation.h"

#include <fstream>
#include <sstream>
#include <Windows.h>

namespace QuickHeal {

    static std::vector<HealSpellEntry> g_Spells;
    static bool                        g_Loaded = false;

    static CastType ParseCastType(const std::string& s)
    {
        if (s == "instant")       return CastType::Instant;
        if (s == "concentration") return CastType::Concentration;
        if (s == "aoe" || s == "aoe_self_centered") return CastType::AoE;
        return CastType::Unknown;
    }

    static void LoadJson()
    {
        g_Spells.clear();

        const char* jsonPath = "Data/SKSE/Plugins/PartySheet_HealSpells.json";
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            logger::warn("QuickHeal: Could not open {}", jsonPath);
            return;
        }

        std::stringstream buf;
        buf << file.rdbuf();
        const std::string content = buf.str();
        file.close();

        const std::string arr = WidgetJsonParser::ExtractArray(content, "healSpells");
        if (arr.empty()) {
            logger::warn("QuickHeal: No 'healSpells' array in JSON");
            return;
        }

        for (const auto& obj : WidgetJsonParser::SplitObjects(arr)) {
            HealSpellEntry e;
            e.id          = WidgetJsonParser::ExtractValue(obj, "id");
            e.label       = WidgetJsonParser::ExtractValue(obj, "label");
            e.mod         = WidgetJsonParser::ExtractValue(obj, "mod");
            e.localFormID = WidgetJsonParser::ParseHexFormID(WidgetJsonParser::ExtractValue(obj, "formId"));
            e.castType    = ParseCastType(WidgetJsonParser::ExtractValue(obj, "castType"));
            e.enabled     = WidgetJsonParser::ParseBool(WidgetJsonParser::ExtractValue(obj, "enabled"), true);

            if (e.id.empty() || e.mod.empty() || e.localFormID == 0) {
                logger::warn("QuickHeal: Skipping invalid entry id='{}' mod='{}' formId={:X}",
                             e.id, e.mod, e.localFormID);
                continue;
            }

            e.resolvedFormID = PlayerStatus::ResolveFormID(e.mod, e.localFormID);
            if (e.resolvedFormID != 0) {
                auto* form = RE::TESForm::LookupByID(e.resolvedFormID);
                e.spell = form ? form->As<RE::SpellItem>() : nullptr;
                if (!e.spell) {
                    logger::warn("QuickHeal: Form {:X} in '{}' is not a SpellItem", e.localFormID, e.mod);
                }
            } else {
                auto* dh = RE::TESDataHandler::GetSingleton();
                bool dataReady = dh && dh->GetLoadedModCount() > 0;
                if (dataReady)
                    logger::warn("QuickHeal: Could not resolve {:X} in '{}'", e.localFormID, e.mod);
                else
                    logger::info("QuickHeal: Deferred {:X} in '{}' (pre-load)", e.localFormID, e.mod);
            }

            g_Spells.push_back(std::move(e));
        }

        logger::info("QuickHeal: Loaded {} heal spell entries", g_Spells.size());
    }

    void Init()
    {
        if (g_Loaded) return;
        LoadJson();
        g_Loaded = true;
    }

    void ReloadFromJson()
    {
        LoadJson();
        g_Loaded = true;
    }

    const std::vector<HealSpellEntry>& GetAllSpells()
    {
        return g_Spells;
    }

    const HealSpellEntry* FindByMod(const std::string& mod, RE::FormID localFormID)
    {
        for (const auto& e : g_Spells) {
            if (e.mod == mod && e.localFormID == localFormID) return &e;
        }
        return nullptr;
    }

    bool PlayerKnowsSpell(const HealSpellEntry& e)
    {
        if (!e.spell) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;
        return player->HasSpell(e.spell);
    }

    RE::Actor* PickHealTarget()
    {
        const auto* settings = Settings::GetSingleton();
        if (!settings || !settings->qhEnabled) return nullptr;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return nullptr;

        const auto allies = FollowerHUD::GetCachedAllies();
        if (allies.empty()) return nullptr;

        const float minHpFrac = static_cast<float>(settings->qhMinHpPercent) / 100.0f;
        const float maxRangeSq = static_cast<float>(settings->qhMaxRange) *
                                 static_cast<float>(settings->qhMaxRange);

        RE::Actor* best = nullptr;
        float      bestFrac = minHpFrac;

        for (auto* a : allies) {
            if (!a || a->IsDead() || a->IsDisabled()) continue;
            if (a->IsSummoned() || a->IsCommandedActor()) continue;

            auto* avo = a->AsActorValueOwner();
            const float hpBase = avo->GetBaseActorValue(RE::ActorValue::kHealth);
            const float hpPerm = a->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth);
            const float hpTemp = a->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
            const float hpMax  = hpBase + hpPerm + hpTemp;
            if (hpMax <= 0.0f) continue;
            const float frac = avo->GetActorValue(RE::ActorValue::kHealth) / hpMax;
            if (frac >= minHpFrac) continue;

            const RE::NiPoint3 pp = player->GetPosition();
            const RE::NiPoint3 ap = a->GetPosition();
            const float dx = pp.x - ap.x, dy = pp.y - ap.y, dz = pp.z - ap.z;
            if ((dx * dx + dy * dy + dz * dz) > maxRangeSq) continue;

            if (frac < bestFrac) {
                bestFrac = frac;
                best = a;
            }
        }
        return best;
    }

    void TryCast()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings || !settings->qhEnabled) return;

        std::string assignedMod;
        if (!settings->qhSpellMod.empty()) {
            const int needed = WideCharToMultiByte(CP_UTF8, 0,
                settings->qhSpellMod.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (needed > 1) {
                assignedMod.resize(needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, settings->qhSpellMod.c_str(), -1,
                    assignedMod.data(), needed, nullptr, nullptr);
            }
        }
        const HealSpellEntry* entry = FindByMod(assignedMod, settings->qhSpellFormID);
        if (!entry || !entry->spell) return;
        if (entry->castType == CastType::Concentration) return;
        if (entry->castType == CastType::Unknown) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        if (!player->HasSpell(entry->spell)) return;

        const float mpPerm = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka);
        const float mpTemp = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka);
        const float mpMax  = mpPerm + mpTemp;
        const float mpCur  = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
        if (mpMax > 0.0f) {
            const float frac = mpCur / mpMax;
            if (frac < (settings->qhMagickaFloorPercent / 100.0f)) return;
        }

        RE::MagicSystem::CannotCastReason reason{};
        if (!static_cast<RE::Actor*>(player)->CheckCast(entry->spell, false, &reason)) return;

        RE::Actor* target = nullptr;
        if (entry->castType == CastType::AoE) {
            if (!PickHealTarget()) return;
            target = player;
        } else {
            target = PickHealTarget();
            if (!target) return;
        }

        auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) return;

        const float cost = entry->spell->CalculateMagickaCost(player);
        player->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kMagicka, cost);

        caster->CastSpellImmediate(entry->spell, false, target, 1.0f, false, 0.0f, player);
        if (target && !target->IsPlayerRef() && settings->qhHealFlash) {
            std::string key = "portrait_follower_" + std::to_string(target->GetFormID());
            UIAnimation::TriggerHealFeedback(key);
        }
        logger::debug("QuickHeal: Cast '{}' on '{}'", entry->label,
                      target->GetDisplayFullName() ? target->GetDisplayFullName() : "?");
    }

}
