#include "../../includes/Utils/GearSwitcher.h"
#include "../../includes/LocalizationManager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#undef GetObject

namespace GearSwitcher {

    namespace {
        std::atomic<bool> s_busy{ false };

        std::mutex                 s_messageLock;
        std::vector<ActionMessage> s_messages;
        constexpr auto             kMessageLife = std::chrono::seconds(4);
        constexpr std::size_t      kMaxMessages = 3;

        std::string FillNames(std::string text, const char* first, const char* second) {
            auto fill = [&text](const char* tag, const char* value) {
                if (auto at = text.find(tag); at != std::string::npos) text.replace(at, 2, value ? value : "");
            };
            fill("%1", first);
            fill("%2", second);
            return text;
        }

        void AddMessage(const char* key, const char* first, const char* second = nullptr) {
            std::string text = FillNames(LocalizationManager::Get(key), first, second);
            std::lock_guard lock(s_messageLock);
            s_messages.push_back({ std::move(text), std::chrono::steady_clock::now() });
            if (s_messages.size() > kMaxMessages) s_messages.erase(s_messages.begin());
        }

        const char* EquipMessageKey(RE::TESBoundObject* obj) {
            if (auto* alch = obj->As<RE::AlchemyItem>()) return alch->IsFood() ? "PartySheetV2.MsgAte" : "PartySheetV2.MsgDrank";
            return "PartySheetV2.MsgEquipped";
        }

        const RE::BGSEquipSlot* HandSlot(bool leftHand) {
            auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
            if (!dom) return nullptr;
            return dom->GetObject<RE::BGSEquipSlot>(leftHand ? RE::DEFAULT_OBJECT::kLeftHandEquip
                                                             : RE::DEFAULT_OBJECT::kRightHandEquip);
        }

        const RE::BGSEquipSlot* SlotFor(RE::TESBoundObject* obj, GearCatalog::Card card) {
            const bool leftCard = card == GearCatalog::Card::LeftHand;
            if (auto* weap = obj->As<RE::TESObjectWEAP>()) {
                const bool oneHanded = weap->IsOneHandedSword() || weap->IsOneHandedDagger() ||
                                       weap->IsOneHandedAxe() || weap->IsOneHandedMace();
                return HandSlot(leftCard && oneHanded);
            }
            if (obj->Is(RE::FormType::Light)) return HandSlot(leftCard);
            return nullptr;
        }

        struct HeldGear {
            GearCatalog::InstanceKey              right;
            GearCatalog::InstanceKey              left;
            std::vector<GearCatalog::InstanceKey> armor;
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

        void RememberHold(RE::Actor* actor, RE::TESBoundObject* obj, const RE::BGSEquipSlot* slot,
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
            } else if (oneHanded) {
                held.right = key;
            } else if (weap) {
                held.right = key;
                held.left = {};
            } else {
                std::erase_if(held.armor, [&](const GearCatalog::InstanceKey& k) { return SlotsOverlap(k.baseID, armor); });
                held.armor.push_back(key);
            }
            s_anyHold = true;
        }

        void ForgetHold(RE::FormID actorID, const GearCatalog::InstanceKey& key) {
            std::lock_guard lock(s_holdLock);
            auto it = s_holds.find(actorID);
            if (it == s_holds.end()) return;
            HeldGear& held = it->second;
            if (held.right == key) held.right = {};
            if (held.left == key) held.left = {};
            std::erase_if(held.armor, [&](const GearCatalog::InstanceKey& k) { return k == key; });
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
                logger::info("[GearSwitcher] hold bonus {:08X} on {:08X}, score {:.1f} to {:.1f}",
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

        int CountInInventory(RE::Actor* actor, RE::FormID baseID) {
            auto inv = actor->GetInventory([&](RE::TESBoundObject& o) { return o.GetFormID() == baseID; });
            int total = 0;
            for (auto& [obj, data] : inv) total += data.first;
            return total;
        }

        void FreeOtherHand(RE::Actor* actor, RE::TESBoundObject* obj, const RE::BGSEquipSlot* slot,
                           RE::ActorEquipManager* em) {
            if (!slot || !obj->IsWeapon()) return;
            const bool toLeft = slot == HandSlot(true);
            auto* otherHandForm = actor->GetEquippedObject(!toLeft);
            if (otherHandForm != obj || CountInInventory(actor, obj->GetFormID()) > 1) return;

            constexpr bool kQueue = false, kForce = false, kSounds = false, kApplyNow = true;
            em->UnequipObject(actor, obj, nullptr, 1, HandSlot(!toLeft), kQueue, kForce, kSounds, kApplyNow);
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
                logger::info("[GearSwitcher] held {:08X} left the inventory of {:08X}, hold dropped",
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

            const RE::BGSEquipSlot* slot = obj->IsWeapon() ? HandSlot(leftHand) : nullptr;
            constexpr bool kQueue = false, kForce = false, kSounds = false, kApplyNow = true;
            em->EquipObject(actor, obj, xl, 1, slot, kQueue, kForce, kSounds, kApplyNow);
            logger::info("[GearSwitcher] put {:08X} back on {:08X}", key.baseID, actor->GetFormID());
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
                    logger::info("[GearSwitcher] {:08X} keeps losing held gear, restore paused", actorID);
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

        void DoEquip(RE::ActorHandle handle, GearCatalog::InstanceKey key, GearCatalog::Card card, int count) {
            auto  actorPtr = handle.get();
            auto* actor = actorPtr.get();
            auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(key.baseID);
            auto* em = RE::ActorEquipManager::GetSingleton();
            if (!actor || !obj || !em || !actor->Is3DLoaded()) {
                s_busy = false;
                return;
            }
            if (!actor->IsPlayerRef() && actor->IsInCombat()) {
                logger::info("[GearSwitcher] {:08X} in combat, equip skipped", actor->GetFormID());
                AddMessage("PartySheetV2.MsgInCombat", actor->GetDisplayFullName());
                s_busy = false;
                return;
            }

            bool found = false;
            RE::ExtraDataList* xl = GearCatalog::ResolveExtraList(actor, key, found);
            if (!found) {
                logger::info("[GearSwitcher] instance gone, equip skipped {:08X}", key.baseID);
                s_busy = false;
                return;
            }

            const RE::BGSEquipSlot* slot = SlotFor(obj, card);
            FreeOtherHand(actor, obj, slot, em);

            constexpr bool kQueue = false, kForce = false, kSounds = true, kApplyNow = true;
            em->EquipObject(actor, obj, xl, static_cast<std::uint32_t>(count), slot, kQueue, kForce, kSounds, kApplyNow);
            logger::info("[GearSwitcher] equipped {:08X} on {:08X}, card {}, extra {}",
                key.baseID, actor->GetFormID(), static_cast<int>(card), xl ? "yes" : "no");

            AddMessage(EquipMessageKey(obj), actor->GetDisplayFullName(), obj->GetName());

            RememberHold(actor, obj, slot, key);

            GearCatalog::Invalidate(actor->GetFormID());

            s_busy = false;
        }
    }

    static void DoUnequip(RE::ActorHandle handle, GearCatalog::InstanceKey key) {
        auto  actorPtr = handle.get();
        auto* actor = actorPtr.get();
        auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(key.baseID);
        auto* em = RE::ActorEquipManager::GetSingleton();
        if (!actor || !obj || !em) {
            s_busy = false;
            return;
        }

        bool found = false;
        RE::ExtraDataList* xl = GearCatalog::ResolveExtraList(actor, key, found);
        if (!found) {
            logger::info("[GearSwitcher] instance gone, unequip skipped {:08X}", key.baseID);
            s_busy = false;
            return;
        }

        constexpr bool kLeftHand = true;
        const RE::BGSEquipSlot* slot = nullptr;
        if (obj->Is(RE::FormType::Weapon) || obj->Is(RE::FormType::Light))
            slot = HandSlot(actor->GetEquippedObject(kLeftHand) == obj && actor->GetEquippedObject(!kLeftHand) != obj);

        ForgetHold(actor->GetFormID(), key);

        const auto count = obj->Is(RE::FormType::Ammo) ? static_cast<std::uint32_t>(actor->GetInventoryCounts()[obj]) : 1u;
        constexpr bool kQueue = false, kForce = false, kSounds = true, kApplyNow = true;
        em->UnequipObject(actor, obj, xl, count, slot, kQueue, kForce, kSounds, kApplyNow);
        logger::info("[GearSwitcher] unequipped {:08X} on {:08X}", key.baseID, actor->GetFormID());
        AddMessage("PartySheetV2.MsgUnequipped", actor->GetDisplayFullName(), obj->GetName());

        GearCatalog::Invalidate(actor->GetFormID());
        s_busy = false;
    }

    bool Unequip(RE::Actor* actor, const GearCatalog::InstanceKey& key) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!actor || !tasks || key.baseID == 0) return false;
        if (s_busy.exchange(true)) return false;

        tasks->AddTask([handle = actor->GetHandle(), key]() { DoUnequip(handle, key); });
        return true;
    }

    bool Equip(RE::Actor* actor, const GearCatalog::InstanceKey& key, GearCatalog::Card card, int count) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!actor || !tasks || key.baseID == 0) return false;
        if (s_busy.exchange(true)) return false;

        tasks->AddTask([handle = actor->GetHandle(), key, card, count]() { DoEquip(handle, key, card, count); });
        return true;
    }

    static void DoSend(RE::ActorHandle fromHandle, RE::ActorHandle toHandle, GearCatalog::InstanceKey key) {
        auto src = fromHandle.get();
        auto dst = toHandle.get();
        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(key.baseID);
        if (!src || !dst || !item) return;

        bool found = false;
        RE::ExtraDataList* list = GearCatalog::ResolveExtraList(src.get(), key, found);
        if (!found) {
            logger::info("[GearSwitcher] send {:08X} skipped, copy is gone", key.baseID);
            return;
        }

        RE::InventoryEntryData copy(item, 1);
        if (list) copy.AddExtraList(list);
        if (copy.IsQuestObject()) {
            logger::info("[GearSwitcher] send {:08X} refused, quest item", key.baseID);
            return;
        }

        constexpr int kOne = 1;
        src->RemoveItem(item, kOne, RE::ITEM_REMOVE_REASON::kStoreInContainer, list, dst.get());
        GearCatalog::Invalidate(src->GetFormID());
        GearCatalog::Invalidate(dst->GetFormID());
        AddMessage("PartySheetV2.MsgSent", item->GetName(), dst->GetDisplayFullName());
        logger::info("[GearSwitcher] sent {:08X} from {:08X} to {:08X}", key.baseID, src->GetFormID(), dst->GetFormID());
    }

    bool Send(RE::Actor* from, RE::Actor* to, const GearCatalog::InstanceKey& key) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!from || !to || !tasks || key.baseID == 0) return false;
        tasks->AddTask([fromHandle = from->GetHandle(), toHandle = to->GetHandle(), key]() { DoSend(fromHandle, toHandle, key); });
        return true;
    }

    std::vector<ActionMessage> RecentMessages() {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(s_messageLock);
        std::erase_if(s_messages, [now](const ActionMessage& m) { return now - m.at > kMessageLife; });
        return s_messages;
    }

    void InstallCombatHold() {
        if (s_holdInstalled) return;

        if (GetModuleHandleW(L"FollowerEquipControl.dll")) {
            logger::info("[GearSwitcher] FollowerEquipControl found, combat hold off");
            return;
        }

        s_rightSlot = HandSlot(false);
        s_leftSlot = HandSlot(true);

        ScoreHook<0>::Install(RE::VTABLE_CombatInventoryItemMelee[0]);
        ScoreHook<1>::Install(RE::VTABLE_CombatInventoryItemRanged[0]);
        ScoreHook<2>::Install(RE::VTABLE_CombatInventoryItemShield[0]);
        s_holdInstalled = true;
        logger::info("[GearSwitcher] combat hold installed");
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

    void ClearCombatHolds() {
        std::lock_guard lock(s_holdLock);
        s_holds.clear();
        s_anyHold = false;
    }

    bool IsBusy() {
        return s_busy.load();
    }
}
