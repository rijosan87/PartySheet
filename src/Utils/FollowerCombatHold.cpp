#include "../../includes/Utils/FollowerCombatHold.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#undef GetObject

namespace FollowerCombatHold {

    namespace {
        const RE::BGSEquipSlot* HandSlot(bool leftHand) {
            auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
            if (!dom) return nullptr;
            return dom->GetObject<RE::BGSEquipSlot>(leftHand ? RE::DEFAULT_OBJECT::kLeftHandEquip
                                                             : RE::DEFAULT_OBJECT::kRightHandEquip);
        }

        struct HeldSpell {
            RE::FormID formID = 0;
            bool       leftHand = false;
            int        role = -1;
            bool       logged = false;
        };

        struct HeldGear {
            GearCatalog::InstanceKey              right;
            GearCatalog::InstanceKey              left;
            std::vector<GearCatalog::InstanceKey> armor;
            std::vector<HeldSpell>                spells;
            bool                                  logged = false;
            bool                                  restorePending = false;
            int                                   restoresInWindow = 0;
            std::chrono::steady_clock::time_point windowStart{};
        };

        std::mutex                               s_holdLock;
        std::unordered_map<RE::FormID, HeldGear> s_holds;
        std::atomic<bool>                        s_anyHold{ false };
        std::atomic<bool>                        s_holdInstalled{ false };

        const RE::BGSEquipSlot* s_rightSlot = nullptr;
        const RE::BGSEquipSlot* s_leftSlot = nullptr;

        constexpr float kHoldBonusRight = 600.0f;
        constexpr float kHoldBonusLeft = 400.0f;

        constexpr float kSpellHoldMult = 1000.0f;

        constexpr std::size_t kMaxHeldSpells = 8;

        constexpr auto kRestoreDelay = std::chrono::milliseconds(1000);
        constexpr int  kRestoreTries = 10;

        constexpr int  kMaxRestoresPerWindow = 3;
        constexpr auto kRestoreWindow = std::chrono::seconds(20);

        bool SlotsOverlap(RE::FormID heldArmorID, RE::TESObjectARMO* newArmor) {
            auto* heldArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(heldArmorID);
            if (!heldArmor || !newArmor) return false;
            const auto heldSlots = heldArmor->GetSlotMask().underlying();
            const auto newSlots = newArmor->GetSlotMask().underlying();
            return (heldSlots & newSlots) != 0;
        }

        float AddHoldBonus(RE::CombatInventoryItem* item, RE::CombatController* controller, float score) {
            if (!s_anyHold || !item || !controller || !item->item) return score;

            auto  actorPtr = controller->attackerHandle.get();
            auto* actor = actorPtr.get();
            if (!actor || actor->IsPlayerRef() || !actor->IsPlayerTeammate()) return score;

            const RE::FormID        id = item->item->GetFormID();
            const RE::BGSEquipSlot* slot = item->itemSlot.equipSlot;

            std::lock_guard lock(s_holdLock);
            auto it = s_holds.find(actor->GetFormID());
            if (it == s_holds.end()) return score;
            HeldGear& held = it->second;

            float bonus = 0.0f;
            if (id == held.right.baseID && slot != s_leftSlot) bonus = kHoldBonusRight;
            else if (id == held.left.baseID && slot != s_rightSlot) bonus = kHoldBonusLeft;
            if (bonus == 0.0f) return score;

            if (!held.logged) {
                held.logged = true;
                logger::info("[CombatHold] hold bonus {:08X} on {:08X}, score {:.1f} to {:.1f}",
                    id, actor->GetFormID(), score, score + bonus);
            }
            return score + bonus;
        }

        template <int N>
        struct ScoreHook {
            static float Thunk(RE::CombatInventoryItem* item, RE::CombatController* controller) {
                return AddHoldBonus(item, controller, original(item, controller));
            }
            static void Install(REL::VariantID vtableID) {
                constexpr std::size_t kCalculateScoreIndex = 0x0C;
                REL::Relocation<std::uintptr_t> vtable{ vtableID };
                original = vtable.write_vfunc(kCalculateScoreIndex, Thunk);
            }
            static inline REL::Relocation<decltype(Thunk)> original;
        };

        float AddSpellHold(RE::CombatInventoryItem* item, RE::CombatController* controller, float score) {
            if (!s_anyHold || score <= 0.0f || !item || !controller || !item->item) return score;

            auto  actorPtr = controller->attackerHandle.get();
            auto* actor = actorPtr.get();
            if (!actor || actor->IsPlayerRef() || !actor->IsPlayerTeammate()) return score;

            const RE::FormID id = item->item->GetFormID();
            std::lock_guard lock(s_holdLock);
            auto it = s_holds.find(actor->GetFormID());
            if (it == s_holds.end()) return score;
            auto& spells = it->second.spells;
            auto  mine = std::find_if(spells.begin(), spells.end(), [id](const HeldSpell& h) { return h.formID == id; });
            if (mine == spells.end()) return score;

            if (mine->role < 0) {
                const int role = static_cast<int>(item->GetCategory());
                const auto newer = std::find_if(mine + 1, spells.end(), [role](const HeldSpell& h) { return h.role == role; });
                if (newer != spells.end()) {
                    logger::info("[CombatHold] spell hold {:08X} on {:08X} dropped, newer pick for role {}",
                        id, actor->GetFormID(), role);
                    spells.erase(mine);
                    return score;
                }
                mine->role = role;
                const RE::FormID keep = id;
                std::erase_if(spells, [&](const HeldSpell& h) { return h.role == role && h.formID != keep; });
                mine = std::find_if(spells.begin(), spells.end(), [keep](const HeldSpell& h) { return h.formID == keep; });
            }

            if (!mine->logged) {
                mine->logged = true;
                logger::info("[CombatHold] spell hold {:08X} on {:08X}, role {}, score {:.1f} to {:.1f}",
                    id, actor->GetFormID(), mine->role, score, score * kSpellHoldMult);
            }
            return score * kSpellHoldMult;
        }

        namespace SpellScoreHook {
            using Score_t = float (*)(RE::CombatInventoryItem*, RE::CombatController*);
            Score_t s_original = nullptr;

            float Thunk(RE::CombatInventoryItem* item, RE::CombatController* controller) {
                return AddSpellHold(item, controller, s_original(item, controller));
            }

            void Patch(REL::VariantID vtableID) {
                constexpr std::size_t kCalculateScoreIndex = 0x0C;
                REL::Relocation<std::uintptr_t> vtable{ vtableID };
                auto old = reinterpret_cast<Score_t>(vtable.write_vfunc(kCalculateScoreIndex, Thunk));
                if (!s_original) s_original = old;
                else if (old != s_original) logger::warn("[CombatHold] a magic vtable holds a different CalculateScore");
            }

            void Install() {
                using namespace RE;
                const REL::VariantID vtables[] = {
                    VTABLE_CombatInventoryItemMagic[0],
                    VTABLE_CombatInventoryItemStaff[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterArmor_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterBoundItem_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterCloak_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterDisarm_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterInvisibility_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterLight_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterOffensive_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterParalyze_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterReanimate_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterRestore_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterScript_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterStagger_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterSummon_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterTargetEffect_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterWard_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterArmor_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterBoundItem_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterCloak_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterDisarm_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterInvisibility_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterLight_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterOffensive_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterParalyze_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterReanimate_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterRestore_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterScript_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterStagger_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterSummon_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterTargetEffect_[0],
                    VTABLE_CombatInventoryItemMagicT_CombatInventoryItemStaff_CombatMagicCasterWard_[0],
                };
                for (const auto& id : vtables) Patch(id);
                logger::info("[CombatHold] spell hold on {} magic vtables", std::size(vtables));
            }
        }

        void RestoreHeldGear(RE::ActorHandle handle, RE::FormID actorID, int triesLeft);

        void RestoreLater(RE::ActorHandle handle, RE::FormID actorID, int triesLeft) {
            std::thread([handle, actorID, triesLeft]() {
                std::this_thread::sleep_for(kRestoreDelay);
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([handle, actorID, triesLeft]() { RestoreHeldGear(handle, actorID, triesLeft); });
                }
            }).detach();
        }

        void ClearRestorePending(RE::FormID actorID) {
            std::lock_guard lock(s_holdLock);
            if (auto it = s_holds.find(actorID); it != s_holds.end()) it->second.restorePending = false;
        }

        void DropHeldItem(RE::FormID actorID, RE::FormID baseID) {
            std::lock_guard lock(s_holdLock);
            auto it = s_holds.find(actorID);
            if (it == s_holds.end()) return;
            HeldGear& held = it->second;
            if (held.right.baseID == baseID) held.right = {};
            if (held.left.baseID == baseID) held.left = {};
            std::erase_if(held.armor, [&](const GearCatalog::InstanceKey& k) { return k.baseID == baseID; });
        }

        bool EquipHeldItem(RE::Actor* actor, const GearCatalog::InstanceKey& key, bool leftHand,
                           RE::ActorEquipManager* em) {
            if (key.baseID == 0) return false;
            auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(key.baseID);
            if (!obj) return false;

            auto inv = actor->GetInventory([&](RE::TESBoundObject& o) { return &o == obj; });
            auto it = inv.find(obj);
            if (it == inv.end() || it->second.first <= 0) {
                logger::info("[CombatHold] held {:08X} left the inventory of {:08X}, hold dropped",
                    key.baseID, actor->GetFormID());
                DropHeldItem(actor->GetFormID(), key.baseID);
                return false;
            }
            if (it->second.second && it->second.second->IsWorn()) return false;

            bool found = false;
            RE::ExtraDataList* xl = GearCatalog::ResolveExtraList(actor, key, found);
            if (!found) {
                DropHeldItem(actor->GetFormID(), key.baseID);
                return false;
            }

            const RE::BGSEquipSlot* slot = obj->IsWeapon() ? (leftHand ? s_leftSlot : s_rightSlot) : nullptr;
            constexpr bool kQueue = false, kForce = false, kSounds = false, kApplyNow = true;
            em->EquipObject(actor, obj, xl, 1, slot, kQueue, kForce, kSounds, kApplyNow);
            logger::info("[CombatHold] put {:08X} back on {:08X}", key.baseID, actor->GetFormID());
            return true;
        }

        void RestoreHeldGear(RE::ActorHandle handle, RE::FormID actorID, int triesLeft) {
            auto  actorPtr = handle.get();
            auto* actor = actorPtr.get();
            auto* em = RE::ActorEquipManager::GetSingleton();
            if (!actor || !em || actor->IsDead() || !actor->IsPlayerTeammate()) {
                ClearRestorePending(actorID);
                return;
            }

            if (!actor->Is3DLoaded() || actor->IsInCombat()) {
                if (triesLeft > 0) RestoreLater(handle, actorID, triesLeft - 1);
                else ClearRestorePending(actorID);
                return;
            }

            HeldGear copy;
            {
                std::lock_guard lock(s_holdLock);
                auto it = s_holds.find(actorID);
                if (it == s_holds.end()) return;
                HeldGear& held = it->second;
                held.restorePending = false;

                const auto now = std::chrono::steady_clock::now();
                if (now - held.windowStart > kRestoreWindow) {
                    held.windowStart = now;
                    held.restoresInWindow = 0;
                }
                if (held.restoresInWindow >= kMaxRestoresPerWindow) {
                    logger::info("[CombatHold] {:08X} keeps losing held gear, restore paused", actorID);
                    return;
                }
                copy = held;
            }

            bool putBack = false;
            for (const auto& key : copy.armor) putBack |= EquipHeldItem(actor, key, false, em);
            putBack |= EquipHeldItem(actor, copy.left, true, em);
            putBack |= EquipHeldItem(actor, copy.right, false, em);
            if (!putBack) return;

            std::lock_guard lock(s_holdLock);
            if (auto it = s_holds.find(actorID); it != s_holds.end()) ++it->second.restoresInWindow;
        }
    }

    void RememberGear(RE::Actor* actor, RE::TESBoundObject* obj, const RE::BGSEquipSlot* slot,
                      const GearCatalog::InstanceKey& key) {
        if (!s_holdInstalled || actor->IsPlayerRef()) return;

        auto* weap = obj->As<RE::TESObjectWEAP>();
        auto* armor = obj->As<RE::TESObjectARMO>();
        if (!weap && !armor) return;

        const bool isShield = armor && armor->IsShield();
        const bool oneHanded = weap && (weap->IsOneHandedSword() || weap->IsOneHandedDagger() ||
                                        weap->IsOneHandedAxe() || weap->IsOneHandedMace());

        std::lock_guard lock(s_holdLock);
        HeldGear& held = s_holds[actor->GetFormID()];
        held.logged = false;
        held.restoresInWindow = 0;
        if (isShield || (oneHanded && slot == s_leftSlot)) {
            held.left = key;
            std::erase_if(held.spells, [](const HeldSpell& h) { return h.leftHand; });
        } else if (oneHanded) {
            held.right = key;
            std::erase_if(held.spells, [](const HeldSpell& h) { return !h.leftHand; });
        } else if (weap) {
            held.right = key;
            held.left = {};
            held.spells.clear();
        } else {
            std::erase_if(held.armor, [&](const GearCatalog::InstanceKey& k) { return SlotsOverlap(k.baseID, armor); });
            held.armor.push_back(key);
        }
        s_anyHold = true;
    }

    void RememberSpell(RE::Actor* actor, RE::SpellItem* spell, bool leftHand) {
        if (!s_holdInstalled || actor->IsPlayerRef()) return;

        std::lock_guard lock(s_holdLock);
        HeldGear& held = s_holds[actor->GetFormID()];
        const RE::FormID id = spell->GetFormID();
        std::erase_if(held.spells, [id](const HeldSpell& h) { return h.formID == id; });
        held.spells.push_back({ id, leftHand });
        if (held.spells.size() > kMaxHeldSpells) held.spells.erase(held.spells.begin());
        if (leftHand) {
            held.left = {};
            auto* rightWeap = RE::TESForm::LookupByID<RE::TESObjectWEAP>(held.right.baseID);
            if (rightWeap && !(rightWeap->IsOneHandedSword() || rightWeap->IsOneHandedDagger() ||
                               rightWeap->IsOneHandedAxe() || rightWeap->IsOneHandedMace()))
                held.right = {};
        } else {
            held.right = {};
        }
        s_anyHold = true;
    }

    void Forget(RE::FormID actorID, const GearCatalog::InstanceKey& key) {
        std::lock_guard lock(s_holdLock);
        auto it = s_holds.find(actorID);
        if (it == s_holds.end()) return;
        HeldGear& held = it->second;
        if (held.right == key) held.right = {};
        if (held.left == key) held.left = {};
        std::erase_if(held.armor, [&](const GearCatalog::InstanceKey& k) { return k == key; });
    }

    void Install() {
        if (s_holdInstalled) return;

        if (GetModuleHandleW(L"FollowerEquipControl.dll")) {
            logger::info("[CombatHold] FollowerEquipControl found, combat hold off");
            return;
        }

        s_rightSlot = HandSlot(false);
        s_leftSlot = HandSlot(true);

        ScoreHook<0>::Install(RE::VTABLE_CombatInventoryItemMelee[0]);
        ScoreHook<1>::Install(RE::VTABLE_CombatInventoryItemRanged[0]);
        ScoreHook<2>::Install(RE::VTABLE_CombatInventoryItemShield[0]);
        SpellScoreHook::Install();
        s_holdInstalled = true;
        logger::info("[CombatHold] combat hold installed");
    }

    void OnEquipChanged(RE::TESObjectREFR* ref) {
        if (!s_anyHold || !ref || ref->IsPlayerRef()) return;
        auto* actor = ref->As<RE::Actor>();
        if (!actor) return;

        const RE::FormID actorID = actor->GetFormID();
        {
            std::lock_guard lock(s_holdLock);
            auto it = s_holds.find(actorID);
            if (it == s_holds.end() || it->second.restorePending) return;
            it->second.restorePending = true;
        }
        RestoreLater(actor->GetHandle(), actorID, kRestoreTries);
    }

    void Clear() {
        std::lock_guard lock(s_holdLock);
        s_holds.clear();
        s_anyHold = false;
    }

    namespace {
        constexpr std::uint32_t kMaxSavedActors = 256;
        constexpr std::uint32_t kMaxSavedArmor = 64;
        constexpr std::uint32_t kMaxSavedNameBytes = 512;

        class RecordReader {
        public:
            RecordReader(SKSE::SerializationInterface* a_serde, std::uint32_t a_length) :
                serde(a_serde), remaining(a_length) {}

            template <class T>
            bool Read(T& a_value) {
                return ReadBytes(std::addressof(a_value), sizeof(T));
            }

            bool ReadBytes(void* a_dst, std::uint32_t a_size) {
                if (!serde || a_size > remaining) return false;
                const std::uint32_t read = serde->ReadRecordData(a_dst, a_size);
                if (read > remaining) return false;
                remaining -= read;
                return read == a_size;
            }

            bool ReadString(std::string& a_out) {
                std::uint32_t size = 0;
                if (!Read(size) || size > kMaxSavedNameBytes || size > remaining) return false;
                a_out.resize(size);
                return size == 0 || ReadBytes(a_out.data(), size);
            }

        private:
            SKSE::SerializationInterface* serde;
            std::uint32_t                 remaining;
        };

        template <class T>
        bool WriteChecked(SKSE::SerializationInterface* a_serde, const T& a_value) {
            return a_serde && a_serde->WriteRecordData(std::addressof(a_value), sizeof(T));
        }

        bool WriteStringChecked(SKSE::SerializationInterface* a_serde, const std::string& a_str) {
            const std::uint32_t size = static_cast<std::uint32_t>(a_str.length());
            if (!WriteChecked(a_serde, size)) return false;
            return size == 0 || a_serde->WriteRecordData(a_str.data(), size);
        }

        bool ResolveNonzero(SKSE::SerializationInterface* a_serde, RE::FormID a_saved, RE::FormID& a_out) {
            a_out = 0;
            if (a_saved == 0) return false;
            RE::FormID resolved = 0;
            if (!a_serde->ResolveFormID(a_saved, resolved) || resolved == 0) return false;
            a_out = resolved;
            return true;
        }

        bool WriteKey(SKSE::SerializationInterface* a_serde, const GearCatalog::InstanceKey& k) {
            const std::uint8_t hasExtra = k.hasExtra ? 1 : 0;
            return WriteChecked(a_serde, k.baseID) && WriteChecked(a_serde, k.health) &&
                   WriteChecked(a_serde, k.enchantID) && WriteChecked(a_serde, k.poisonID) &&
                   WriteStringChecked(a_serde, k.customName) && WriteChecked(a_serde, hasExtra);
        }

        bool ReadKey(SKSE::SerializationInterface* a_serde, RecordReader& reader, GearCatalog::InstanceKey& k) {
            GearCatalog::InstanceKey saved;
            std::uint8_t             hasExtra = 0;
            if (!reader.Read(saved.baseID) || !reader.Read(saved.health) || !reader.Read(saved.enchantID) ||
                !reader.Read(saved.poisonID) || !reader.ReadString(saved.customName) || !reader.Read(hasExtra))
                return false;
            if (hasExtra > 1 || !std::isfinite(saved.health)) return false;

            k = {};
            if (saved.baseID == 0) return true;
            GearCatalog::InstanceKey resolved = saved;
            resolved.hasExtra = hasExtra == 1;
            if (!ResolveNonzero(a_serde, saved.baseID, resolved.baseID)) return true;
            if (saved.enchantID && !ResolveNonzero(a_serde, saved.enchantID, resolved.enchantID)) return true;
            if (saved.poisonID && !ResolveNonzero(a_serde, saved.poisonID, resolved.poisonID)) return true;
            k = std::move(resolved);
            return true;
        }

        bool ReadHoldRecord(SKSE::SerializationInterface* a_serde, std::uint32_t a_length,
                            std::unordered_map<RE::FormID, HeldGear>& a_out) {
            RecordReader  reader(a_serde, a_length);
            std::uint32_t actorCount = 0;
            if (!reader.Read(actorCount) || actorCount > kMaxSavedActors) return false;

            for (std::uint32_t i = 0; i < actorCount; ++i) {
                RE::FormID savedActor = 0;
                HeldGear   held;
                if (!reader.Read(savedActor)) return false;
                if (!ReadKey(a_serde, reader, held.right) || !ReadKey(a_serde, reader, held.left)) return false;

                std::uint32_t armorCount = 0;
                if (!reader.Read(armorCount) || armorCount > kMaxSavedArmor) return false;
                for (std::uint32_t a = 0; a < armorCount; ++a) {
                    GearCatalog::InstanceKey key;
                    if (!ReadKey(a_serde, reader, key)) return false;
                    if (key.baseID) held.armor.push_back(std::move(key));
                }

                std::uint32_t spellCount = 0;
                if (!reader.Read(spellCount) || spellCount > kMaxHeldSpells) return false;
                for (std::uint32_t sp = 0; sp < spellCount; ++sp) {
                    RE::FormID   savedSpell = 0;
                    std::uint8_t leftHand = 0;
                    std::int8_t  role = -1;
                    if (!reader.Read(savedSpell) || !reader.Read(leftHand) || !reader.Read(role)) return false;
                    if (leftHand > 1) return false;
                    HeldSpell spell{ 0, leftHand == 1, role };
                    if (ResolveNonzero(a_serde, savedSpell, spell.formID)) held.spells.push_back(spell);
                }

                RE::FormID actorID = 0;
                if (!ResolveNonzero(a_serde, savedActor, actorID)) continue;
                if (held.right.baseID || held.left.baseID || !held.armor.empty() || !held.spells.empty())
                    a_out[actorID] = std::move(held);
            }
            return true;
        }
    }

    void OnGameSave(SKSE::SerializationInterface* serde) {
        if (!serde || !s_holdInstalled) return;

        std::lock_guard lock(s_holdLock);
        if (s_holds.empty()) return;

        if (s_holds.size() > kMaxSavedActors) {
            logger::error("[CombatHold] save skipped, {} actors exceeds cap {}", s_holds.size(), kMaxSavedActors);
            return;
        }
        const auto nameTooLong = [](const GearCatalog::InstanceKey& k) { return k.customName.length() > kMaxSavedNameBytes; };
        for (const auto& [actorID, held] : s_holds) {
            if (actorID == 0 || held.armor.size() > kMaxSavedArmor || held.spells.size() > kMaxHeldSpells ||
                nameTooLong(held.right) || nameTooLong(held.left) ||
                std::any_of(held.armor.begin(), held.armor.end(), nameTooLong)) {
                logger::error("[CombatHold] save skipped, hold of {:08X} is out of range", actorID);
                return;
            }
        }

        if (!serde->OpenRecord(kRecordType, kRecordVersion)) {
            logger::error("[CombatHold] failed to open the cosave record");
            return;
        }

        const std::uint32_t actorCount = static_cast<std::uint32_t>(s_holds.size());
        if (!WriteChecked(serde, actorCount)) {
            logger::error("[CombatHold] save failed, actor count");
            return;
        }
        for (const auto& [actorID, held] : s_holds) {
            bool ok = WriteChecked(serde, actorID) && WriteKey(serde, held.right) && WriteKey(serde, held.left);

            const std::uint32_t armorCount = static_cast<std::uint32_t>(held.armor.size());
            ok = ok && WriteChecked(serde, armorCount);
            for (const auto& key : held.armor) ok = ok && WriteKey(serde, key);

            const std::uint32_t spellCount = static_cast<std::uint32_t>(held.spells.size());
            ok = ok && WriteChecked(serde, spellCount);
            for (const auto& spell : held.spells) {
                const std::uint8_t leftHand = spell.leftHand ? 1 : 0;
                const std::int8_t  role = static_cast<std::int8_t>(spell.role);
                ok = ok && WriteChecked(serde, spell.formID) && WriteChecked(serde, leftHand) && WriteChecked(serde, role);
            }
            if (!ok) {
                logger::error("[CombatHold] save failed, hold of {:08X}", actorID);
                return;
            }
        }
        logger::info("[CombatHold] saved holds for {} followers, v{}", actorCount, kRecordVersion);
    }

    void LoadRecord(SKSE::SerializationInterface* serde, std::uint32_t version, std::uint32_t length) {
        if (version < 1 || version > kRecordVersion) {
            logger::warn("[CombatHold] unsupported cosave version {}", version);
            return;
        }

        std::unordered_map<RE::FormID, HeldGear> loaded;
        if (!ReadHoldRecord(serde, length, loaded)) {
            logger::error("[CombatHold] rejected a malformed cosave record, v{} {} bytes", version, length);
            return;
        }
        if (!s_holdInstalled) return;

        std::lock_guard lock(s_holdLock);
        s_holds = std::move(loaded);
        s_anyHold = !s_holds.empty();
        logger::info("[CombatHold] loaded holds for {} followers", s_holds.size());
    }
}
