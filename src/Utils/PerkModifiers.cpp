#include "../../includes/Utils/PerkModifiers.h"

#include <algorithm>

#include <chrono>
#include <string>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace PerkModifiers {

    float AttackPerkMult(RE::Actor* actor, RE::TESObjectWEAP* weapon) {
        float perkMult = 1.0f;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || !weapon || !player || !actor->Is3DLoaded()) return perkMult;
        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModAttackDamage,
            actor, weapon, static_cast<RE::Actor*>(player), &perkMult);
        return perkMult;
    }

    float ArmorRatingPerk(RE::Actor* actor, RE::TESObjectARMO* armor, float value) {
        if (!actor || !armor || !actor->Is3DLoaded()) return value;
        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModArmorRating, actor, armor, &value);
        return value;
    }

    namespace {
        using Clock = std::chrono::steady_clock;

        constexpr auto kRefreshInterval = std::chrono::milliseconds(1000);

        struct Entry {
            HitEstimate       est{};
            Clock::time_point requested{};
            bool              pending = false;
        };

        std::mutex                               g_lock;
        std::unordered_map<std::uint64_t, Entry> g_cache;

        struct SpellEntry {
            SpellEstimate     est{};
            Clock::time_point requested{};
            bool              pending = false;
        };

        std::unordered_map<std::uint64_t, SpellEntry> g_spellCache;

        std::uint64_t MakeKey(RE::FormID actorID, RE::FormID weaponID, bool leftHand) {
            return (static_cast<std::uint64_t>(actorID) << 32) | weaponID | (leftHand ? (1ull << 63) : 0);
        }

        void Refresh(RE::ActorHandle handle, RE::FormID weaponID, bool leftHand, std::uint64_t key) {
            HitEstimate est{};
            auto  actorPtr = handle.get();
            auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(weaponID);
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (actorPtr && weapon && player && actorPtr->Is3DLoaded()) {
                est.perkMult = AttackPerkMult(actorPtr.get(), weapon);

                auto* entry = actorPtr->GetEquippedEntryData(leftHand);
                if (!actorPtr->IsPlayerRef() && weapon->IsMelee() && entry && entry->object == weapon) {
                    if (auto* hit = RE::HitData::Create(actorPtr.get(), player, entry, leftHand)) {
                        est.physicalDamage = hit->physicalDamage;
                        hit->attackData.reset();
                        RE::free(hit);
                    }
                }
            }

            std::lock_guard lk(g_lock);
            auto& e = g_cache[key];
            e.est = est;
            e.pending = false;
        }

        void RefreshSpell(RE::ActorHandle handle, RE::FormID spellID, std::uint64_t key) {
            SpellEstimate est{};
            auto  actorPtr = handle.get();
            auto* spell  = RE::TESForm::LookupByID<RE::SpellItem>(spellID);
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (actorPtr && spell && player && actorPtr->Is3DLoaded()) {
                for (auto* effect : spell->effects) {
                    float mag = effect ? effect->effectItem.magnitude : 0.0f;
                    float dur = effect ? static_cast<float>(effect->effectItem.duration) : 0.0f;
                    if (effect) {
                        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModSpellMagnitude,
                            actorPtr.get(), spell, static_cast<RE::Actor*>(player), &mag);
                        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModSpellDuration,
                            actorPtr.get(), spell, static_cast<RE::Actor*>(player), &dur);
    }
                    est.magnitudes.push_back(mag);
                    est.durations.push_back(dur);
                }
                est.known = true;
            }

            std::lock_guard lk(g_lock);
            auto& e = g_spellCache[key];
            e.est = std::move(est);
            e.pending = false;
        }

        struct SkillPerkEntry {
            SkillPerks        est{};
            Clock::time_point requested{};
            bool              pending = false;
        };

        std::unordered_map<std::uint64_t, SkillPerkEntry> g_skillPerkCache;

        std::vector<std::vector<RE::BGSPerk*>> TreeChains(RE::ActorValue skill) {
            std::vector<std::vector<RE::BGSPerk*>> chains;
            auto* info = RE::ActorValueList::GetActorValueInfo(skill);
            if (!info || !info->perkTree) return chains;

            std::unordered_set<RE::BGSSkillPerkTreeNode*> seenNodes;
            std::unordered_set<RE::BGSPerk*>              seenPerks;
            std::vector<RE::BGSSkillPerkTreeNode*>        queue{ info->perkTree };
            for (size_t i = 0; i < queue.size(); ++i) {
                auto* node = queue[i];
                if (!node || !seenNodes.insert(node).second) continue;
                for (auto* child : node->children) queue.push_back(child);

                if (!node->perk || seenPerks.contains(node->perk)) continue;
                constexpr size_t kMaxRanks = 10;
                std::vector<RE::BGSPerk*> chain;
                for (auto* p = node->perk; p && chain.size() < kMaxRanks && seenPerks.insert(p).second; p = p->nextPerk)
                    chain.push_back(p);
                chains.push_back(std::move(chain));
            }
            return chains;
        }

        const std::unordered_set<RE::BGSPerk*>& AllTreePerks() {
            static std::unordered_set<RE::BGSPerk*> s_all;
            static bool s_built = false;
            if (!s_built) {
                for (int av = static_cast<int>(RE::ActorValue::kOneHanded); av <= static_cast<int>(RE::ActorValue::kEnchanting); ++av)
                    for (auto& chain : TreeChains(static_cast<RE::ActorValue>(av)))
                        s_all.insert(chain.begin(), chain.end());
                s_built = true;
            }
            return s_all;
        }

        std::unordered_set<RE::BGSPerk*> RecordPerks(RE::Actor* actor) {
            std::unordered_set<RE::BGSPerk*> out;
            if (auto* npc = actor->GetActorBase(); npc && npc->perks)
                for (std::uint32_t i = 0; i < npc->perkCount; ++i)
                    if (npc->perks[i].perk) out.insert(npc->perks[i].perk);
            return out;
        }

        std::string PerkDescription(RE::BGSPerk* perk) {
            RE::BSString buf;
            perk->GetDescription(buf, perk);
            std::string out = buf.c_str() ? buf.c_str() : "";
            out.erase(std::remove_if(out.begin(), out.end(), [](char c) { return c == '<' || c == '>'; }), out.end());
            return out;
        }

        void StoreSkillPerks(std::uint64_t key, SkillPerks est) {
            std::lock_guard lk(g_lock);
            auto& e = g_skillPerkCache[key];
            e.est = std::move(est);
            e.pending = false;
        }

        void RefreshSkillPerks(RE::ActorHandle handle, RE::ActorValue skill, std::uint64_t key) {
            SkillPerks est{};
            auto actorPtr = handle.get();
            if (actorPtr) {
                const auto recordPerks = RecordPerks(actorPtr.get());
                for (const auto& chain : TreeChains(skill)) {
                    int rank = 0;
                    for (size_t r = 0; r < chain.size(); ++r)
                        if (actorPtr->HasPerk(chain[r]) || recordPerks.contains(chain[r])) rank = static_cast<int>(r) + 1;
                    if (rank == 0) continue;

                    const char* nm = chain.front()->GetFullName();
                    if (!nm || !nm[0]) continue;
                    est.perks.push_back({ nm, PerkDescription(chain[rank - 1]), rank, static_cast<int>(chain.size()) });
                }
                est.known = true;
            }
            StoreSkillPerks(key, std::move(est));
        }

        void RefreshOtherPerks(RE::ActorHandle handle, std::uint64_t key) {
            SkillPerks est{};
            auto actorPtr = handle.get();
            if (actorPtr) {
                const auto& treePerks = AllTreePerks();
                std::unordered_set<std::string> seenNames;
                if (auto* npc = actorPtr->GetActorBase(); npc && npc->perks) {
                    for (std::uint32_t i = 0; i < npc->perkCount; ++i) {
                        auto* perk = npc->perks[i].perk;
                        if (!perk || perk->data.hidden || treePerks.contains(perk)) continue;
                        const char* nm = perk->GetFullName();
                        if (!nm || !nm[0] || !seenNames.insert(nm).second) continue;
                        auto desc = PerkDescription(perk);
                        if (desc.empty()) continue;
                        est.perks.push_back({ nm, std::move(desc) });
                    }
                }
                est.known = true;
            }
            StoreSkillPerks(key, std::move(est));
        }

        bool RaisesArmorRating(RE::BGSPerk* perk, RE::Actor* actor, RE::TESObjectARMO* armor) {
            for (auto* entry : perk->perkEntries) {
                if (!entry || entry->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) continue;
                auto* ep = static_cast<RE::BGSEntryPointPerkEntry*>(entry);
                if (!ep->IsEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModArmorRating)) continue;
                RE::TESForm* args[2] = { actor, armor };
                const bool pass = ep->CheckConditionFilters(2, args);
                logger::debug("[ArmorPerk] {} on {}: conditions {}", perk->GetFullName(), armor->GetName(), pass);
                if (pass) return true;
            }
            return false;
        }

        void RefreshArmorPerks(RE::ActorHandle handle, RE::FormID armorID, std::uint64_t key) {
            SkillPerks est{};
            auto  actorPtr = handle.get();
            auto* armor    = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorID);
            const bool rated = armor && (armor->IsHeavyArmor() || armor->IsLightArmor());
            if (actorPtr && rated) {
                auto* actor = actorPtr.get();
                const auto recordPerks = RecordPerks(actor);
                auto Owned = [&](RE::BGSPerk* p) { return actor->HasPerk(p) || recordPerks.contains(p); };
                for (int av = static_cast<int>(RE::ActorValue::kOneHanded); av <= static_cast<int>(RE::ActorValue::kEnchanting); ++av) {
                    for (const auto& chain : TreeChains(static_cast<RE::ActorValue>(av))) {
                        RE::BGSPerk* top = nullptr;
                        for (auto* p : chain) if (Owned(p)) top = p;
                        if (!top || !RaisesArmorRating(top, actor, armor)) continue;
                        const char* nm = top->GetFullName();
                        if (nm && nm[0]) est.perks.push_back({ nm, PerkDescription(top) });
                    }
                }
                const auto& treePerks = AllTreePerks();
                for (auto* p : recordPerks) {
                    if (p->data.hidden || treePerks.contains(p) || !RaisesArmorRating(p, actor, armor)) continue;
                    const char* nm = p->GetFullName();
                    if (nm && nm[0]) est.perks.push_back({ nm, PerkDescription(p) });
                }
                est.known = true;
            }
            StoreSkillPerks(key, std::move(est));
        }

        template <class RefreshFn>
        SkillPerks ReadPerkCache(RE::Actor* actor, std::uint64_t key, RefreshFn refresh) {
            const auto now = Clock::now();
            SkillPerks est{};
            bool       queue = false;
            {
                std::lock_guard lk(g_lock);
                auto& e = g_skillPerkCache[key];
                est = e.est;
                if (!e.pending && now - e.requested >= kRefreshInterval) {
                    e.pending = true;
                    e.requested = now;
                    queue = true;
                }
            }
            if (queue) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([handle = actor->GetHandle(), key, refresh]() { refresh(handle, key); });
                } else {
                    std::lock_guard lk(g_lock);
                    g_skillPerkCache[key].pending = false;
                }
            }
            return est;
        }
    }

    HitEstimate GetHitEstimate(RE::Actor* actor, RE::TESObjectWEAP* weapon, bool leftHand) {
        if (!actor || !weapon) return {};

        const auto key = MakeKey(actor->GetFormID(), weapon->GetFormID(), leftHand);
        const auto now = Clock::now();

        HitEstimate est{};
        bool        queue = false;
        {
            std::lock_guard lk(g_lock);
            auto& e = g_cache[key];
            est = e.est;
            if (!e.pending && now - e.requested >= kRefreshInterval) {
                e.pending = true;
                e.requested = now;
                queue = true;
            }
        }

        if (queue) {
            auto* tasks = SKSE::GetTaskInterface();
            if (tasks) {
                tasks->AddTask([handle = actor->GetHandle(), weaponID = weapon->GetFormID(), leftHand, key]() {
                    Refresh(handle, weaponID, leftHand, key);
                });
            } else {
                std::lock_guard lk(g_lock);
                g_cache[key].pending = false;
            }
        }
        return est;
    }

    SpellEstimate GetSpellEstimate(RE::Actor* actor, RE::SpellItem* spell) {
        if (!actor || !spell) return {};

        const auto key = (static_cast<std::uint64_t>(actor->GetFormID()) << 32) | spell->GetFormID();
        const auto now = Clock::now();

        SpellEstimate est{};
        bool          queue = false;
        {
            std::lock_guard lk(g_lock);
            auto& e = g_spellCache[key];
            est = e.est;
            if (!e.pending && now - e.requested >= kRefreshInterval) {
                e.pending = true;
                e.requested = now;
                queue = true;
}
        }

        if (queue) {
            auto* tasks = SKSE::GetTaskInterface();
            if (tasks) {
                tasks->AddTask([handle = actor->GetHandle(), spellID = spell->GetFormID(), key]() {
                    RefreshSpell(handle, spellID, key);
                });
            } else {
                std::lock_guard lk(g_lock);
                g_spellCache[key].pending = false;
            }
        }
        return est;
    }

    SkillPerks GetSkillPerks(RE::Actor* actor, RE::ActorValue skill) {
        if (!actor || skill == RE::ActorValue::kNone) return {};

        const auto key = (static_cast<std::uint64_t>(actor->GetFormID()) << 32) | static_cast<std::uint32_t>(skill);
        return ReadPerkCache(actor, key, [skill](RE::ActorHandle h, std::uint64_t k) { RefreshSkillPerks(h, skill, k); });
    }

    SkillPerks GetOtherPerks(RE::Actor* actor) {
        if (!actor) return {};
        constexpr std::uint32_t kOtherPerksSlot = 0xFFFFFFFF;
        const auto key = (static_cast<std::uint64_t>(actor->GetFormID()) << 32) | kOtherPerksSlot;
        return ReadPerkCache(actor, key, [](RE::ActorHandle h, std::uint64_t k) { RefreshOtherPerks(h, k); });
    }

    SkillPerks GetArmorPerks(RE::Actor* actor, RE::TESObjectARMO* armor) {
        if (!actor || !armor) return {};
        const auto key = (static_cast<std::uint64_t>(actor->GetFormID()) << 32) | armor->GetFormID();
        return ReadPerkCache(actor, key, [armorID = armor->GetFormID()](RE::ActorHandle h, std::uint64_t k) { RefreshArmorPerks(h, armorID, k); });
}
}
