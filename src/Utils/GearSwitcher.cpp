#include "../../includes/Utils/GearSwitcher.h"
#include "../../includes/Utils/SpellCatalog.h"
#include "../../includes/Utils/FollowerCombatHold.h"
#include "../../includes/LocalizationManager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <mutex>
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

        void AddMessage(const char* key, const char* first, const char* second = nullptr, RE::FormID receiver = 0) {
            std::string text = FillNames(LocalizationManager::Get(key), first, second);
            std::lock_guard lock(s_messageLock);
            s_messages.push_back({ std::move(text), std::chrono::steady_clock::now(), receiver });
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
            if (obj->Is(RE::FormType::Light)) return HandSlot(true);
            return nullptr;
        }

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

            FollowerCombatHold::RememberGear(actor, obj, slot, key);

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

        FollowerCombatHold::Forget(actor->GetFormID(), key);

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

    static void DoEquipSpell(RE::ActorHandle handle, RE::FormID formID, GearCatalog::Card card) {
        auto  actorPtr = handle.get();
        auto* actor = actorPtr.get();
        auto* form = RE::TESForm::LookupByID(formID);
        auto* em = RE::ActorEquipManager::GetSingleton();
        if (!actor || !form || !em || !actor->Is3DLoaded()) {
            s_busy = false;
            return;
        }
        if (!actor->IsPlayerRef() && actor->IsInCombat()) {
            AddMessage("PartySheetV2.MsgInCombat", actor->GetDisplayFullName());
            s_busy = false;
            return;
        }

        if (auto* shout = form->As<RE::TESShout>()) {
            em->EquipShout(actor, shout);
        } else if (auto* spell = form->As<RE::SpellItem>()) {
            using ST = RE::MagicSystem::SpellType;
            const bool isPower = spell->GetSpellType() == ST::kPower || spell->GetSpellType() == ST::kLesserPower;
            bool leftHand = card == GearCatalog::Card::LeftHand;
            if (const auto lock = SpellCatalog::HandLockOf(spell); lock != SpellCatalog::HandLock::None)
                leftHand = lock == SpellCatalog::HandLock::Left;
            const RE::BGSEquipSlot* slot = HandSlot(leftHand);
            if (isPower)
                if (auto* dom = RE::BGSDefaultObjectManager::GetSingleton())
                    slot = dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kVoiceEquip);
            em->EquipSpell(actor, spell, slot);
            if (!isPower) FollowerCombatHold::RememberSpell(actor, spell, leftHand);
        } else {
            s_busy = false;
            return;
        }
        logger::info("[GearSwitcher] equipped spell {:08X} on {:08X}, card {}", formID, actor->GetFormID(), static_cast<int>(card));
        AddMessage("PartySheetV2.MsgEquipped", actor->GetDisplayFullName(), form->GetName());
        GearCatalog::Invalidate(actor->GetFormID());
        s_busy = false;
    }

    bool EquipSpell(RE::Actor* actor, RE::FormID formID, GearCatalog::Card card) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!actor || !tasks || formID == 0) return false;
        if (s_busy.exchange(true)) return false;

        tasks->AddTask([handle = actor->GetHandle(), formID, card]() { DoEquipSpell(handle, formID, card); });
        return true;
    }

    bool Equip(RE::Actor* actor, const GearCatalog::InstanceKey& key, GearCatalog::Card card, int count) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!actor || !tasks || key.baseID == 0) return false;
        if (s_busy.exchange(true)) return false;

        tasks->AddTask([handle = actor->GetHandle(), key, card, count]() { DoEquip(handle, key, card, count); });
        return true;
    }

    static constexpr auto kSendQuiet = std::chrono::milliseconds(1000);
    static std::mutex s_sendLock;
    static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> s_lastSend;

    static void MarkSent(RE::FormID actorID) {
        std::lock_guard lock(s_sendLock);
        s_lastSend[actorID] = std::chrono::steady_clock::now();
    }

    bool SentRecently(RE::FormID actorID) {
        std::lock_guard lock(s_sendLock);
        const auto it = s_lastSend.find(actorID);
        return it != s_lastSend.end() && std::chrono::steady_clock::now() - it->second < kSendQuiet;
    }

    static void DoSend(RE::ActorHandle fromHandle, RE::ActorHandle toHandle, GearCatalog::InstanceKey key, int count) {
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

        count = (std::max)(count, 1);
        MarkSent(src->GetFormID());
        MarkSent(dst->GetFormID());
        src->RemoveItem(item, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, list, dst.get());
        GearCatalog::Invalidate(src->GetFormID());
        GearCatalog::Invalidate(dst->GetFormID());
        const std::string name = count > 1 ? std::format("{} ({})", item->GetName(), count) : std::string(item->GetName());
        AddMessage("PartySheetV2.MsgSent", name.c_str(), dst->GetDisplayFullName(), dst->GetFormID());
        logger::info("[GearSwitcher] sent {}x {:08X} from {:08X} to {:08X}", count, key.baseID, src->GetFormID(), dst->GetFormID());
    }

    bool Send(RE::Actor* from, RE::Actor* to, const GearCatalog::InstanceKey& key, int count) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!from || !to || !tasks || key.baseID == 0) return false;
        tasks->AddTask([fromHandle = from->GetHandle(), toHandle = to->GetHandle(), key, count]() { DoSend(fromHandle, toHandle, key, count); });
        return true;
    }

    std::vector<ActionMessage> RecentMessages() {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(s_messageLock);
        std::erase_if(s_messages, [now](const ActionMessage& m) { return now - m.at > kMessageLife; });
        return s_messages;
    }

    bool IsBusy() {
        return s_busy.load();
    }
}
