// STATUS: Working
// Coordinates potion consumption animations.

#include "../includes/PotionAnimationBridge.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace PotionAnimationBridge {
    namespace {
        constexpr RE::FormID kDrinkPotionAnimObjectFormID = 0x0D36C8;
        constexpr auto       kFallbackTimeout = std::chrono::milliseconds(1800);
        struct PendingDrink {
            RE::FormID potionFormID = 0;
            std::string entryId;
            std::uint64_t generation = 0;
            bool consumed = false;
            bool waitingForHeal = false;
        };
        std::mutex s_mutex;
        std::unique_ptr<PendingDrink> s_pending;
        std::atomic<std::uint64_t> s_generation{0};
        bool ConsumePending(std::string_view reason);
        void FinishPending(bool consumeIfNeeded);
        void QueueFinishPending(bool consumeIfNeeded)
        {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([consumeIfNeeded]() {
                    FinishPending(consumeIfNeeded);
                });
            }
        }
        class DrinkEventSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::BSAnimationGraphEvent* event,
                RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
            {
                if (!event) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const std::string_view tag = event->tag.c_str();
                const std::string_view payload = event->payload.c_str();
                if (tag == "AnimObjLoad" && payload == "AnimObjectDrinkPotion") {
                    std::scoped_lock lock(s_mutex);
                    if (s_pending) {
                        s_pending->waitingForHeal = true;
                    }
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (tag == "IUHeal") {
                    bool shouldConsume = false;
                    {
                        std::scoped_lock lock(s_mutex);
                        shouldConsume = s_pending && s_pending->waitingForHeal && !s_pending->consumed;
                    }
                    if (shouldConsume) {
                        ConsumePending("IUHeal");
                    }
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (tag == "DrinkPotionStop" || tag == "EnableBumper" || tag == "MTState") {
                    QueueFinishPending(false);
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
        std::unique_ptr<DrinkEventSink> s_sink;
        HMODULE GetUapngModule()
        {
            if (HMODULE module = GetModuleHandleA("UAPNG.dll")) {
                return module;
            }
            if (HMODULE module = GetModuleHandleA("SKSE_UAPNG.dll")) {
                return module;
            }
            return GetModuleHandleA("UltimatePotionsNG.dll");
        }
        std::string GetUapngIniPath(HMODULE module)
        {
            char dllPath[MAX_PATH]{};
            if (!module || GetModuleFileNameA(module, dllPath, sizeof(dllPath)) == 0) {
                return {};
            }
            std::filesystem::path path(dllPath);
            path.replace_filename("UAPNG.ini");
            return path.string();
        }
        int GetUapngSetting(const char* name, int defaultValue)
        {
            const std::string iniPath = GetUapngIniPath(GetUapngModule());
            if (iniPath.empty()) {
                return defaultValue;
            }
            return GetPrivateProfileIntA("Settings", name, defaultValue, iniPath.c_str());
        }
        bool NotifyGraph(RE::Actor* actor, const char* eventName)
        {
            if (!actor || !eventName) {
                return false;
            }
            auto* graphHolder = static_cast<RE::IAnimationGraphManagerHolder*>(actor);
            return graphHolder && graphHolder->NotifyAnimationGraph(RE::BSFixedString(eventName));
        }
        void SetGraphVariableBool(RE::Actor* actor, const char* variableName, bool value)
        {
            if (!actor || !variableName) {
                return;
            }
            auto* graphHolder = static_cast<RE::IAnimationGraphManagerHolder*>(actor);
            if (graphHolder) {
                graphHolder->SetGraphVariableBool(RE::BSFixedString(variableName), value);
            }
        }
        int GetGraphVariableInt(RE::Actor* actor, const char* variableName)
        {
            if (!actor || !variableName) {
                return 0;
            }
            std::int32_t value = 0;
            auto* graphHolder = static_cast<RE::IAnimationGraphManagerHolder*>(actor);
            if (graphHolder) {
                graphHolder->GetGraphVariableInt(RE::BSFixedString(variableName), value);
            }
            return value;
        }
        bool GetGraphVariableBool(RE::Actor* actor, const char* variableName)
        {
            if (!actor || !variableName) {
                return false;
            }
            bool value = false;
            auto* graphHolder = static_cast<RE::IAnimationGraphManagerHolder*>(actor);
            if (graphHolder) {
                graphHolder->GetGraphVariableBool(RE::BSFixedString(variableName), value);
            }
            return value;
        }
        bool IsActorReadyForAnimatedDrink(RE::Actor* actor)
        {
            if (!actor) {
                return false;
            }
            const int currentDefaultState = GetGraphVariableInt(actor, "currentDefaultState");
            const bool isPaired = GetGraphVariableBool(actor, "bIsSynced");
            const auto* actorState = actor->AsActorState();
            const bool swimming = actorState && actorState->IsSwimming();
            return (currentDefaultState < 10 || currentDefaultState > 20) &&
                !isPaired &&
                !actor->IsOnMount() &&
                !actor->IsInMidair() &&
                !swimming;
        }
        void HideWeapon(RE::Actor* actor, bool cull)
        {
            if (!actor) {
                return;
            }
            const RE::BSFixedString nodeName("Weapon");
            if (auto* node = actor->GetNodeByName(nodeName)) {
                node->CullNode(cull);
            }
        }
        void PlayPotionSound()
        {
            if (auto* audio = RE::BSAudioManager::GetSingleton()) {
                audio->Play(static_cast<RE::FormID>(0x03C7BA));
            }
        }
        std::string GetAnimObjectModelPath(RE::AlchemyItem* potion)
        {
            if (!potion) {
                return "Animobjects\\AnimObjectDrinkPotion.nif";
            }
            std::string modelPath = potion->GetModel();
            if (const std::size_t slash = modelPath.find_last_of("/\\"); slash != std::string::npos) {
                modelPath = modelPath.substr(slash + 1);
            }
            if (modelPath.empty()) {
                return "Animobjects\\AnimObjectDrinkPotion.nif";
            }
            return "AnimObjects\\" + modelPath;
        }
        bool MeshExists(std::string_view modelPath)
        {
            std::filesystem::path path("Data\\meshes");
            path /= std::filesystem::path(modelPath);
            return std::filesystem::exists(path);
        }
        void SetDrinkAnimObjectModel(RE::AlchemyItem* potion)
        {
            auto* form = RE::TESObjectANIO::LookupByID(kDrinkPotionAnimObjectFormID);
            if (!form) {
                return;
            }
            auto* anio = static_cast<RE::TESObjectANIO*>(form);
            auto* model = static_cast<RE::TESModel*>(anio);
            if (!model) {
                return;
            }
            std::string modelPath = GetAnimObjectModelPath(potion);
            if (!MeshExists(modelPath)) {
                modelPath = "Animobjects\\AnimObjectDrinkPotion.nif";
            }
            model->SetModel(modelPath.c_str());
        }
        const char* GetStartEvent()
        {
            switch (GetUapngSetting("iMovementType", 0)) {
                case 1:
                    return "DrinkPotion_Slow";
                case 2:
                    return "DrinkPotion_NoMove";
                default:
                    return "DrinkPotionStart";
            }
        }
        void CleanupAnimationState(RE::Actor* actor)
        {
            if (!actor) {
                return;
            }
            HideWeapon(actor, false);
            NotifyGraph(actor, "HeadtrackingOn");
            SetGraphVariableBool(actor, "tdmHeadtrackingBehavior", true);
            SetGraphVariableBool(actor, "bPlayEquipSound", false);
        }
        bool ConsumePending(std::string_view reason)
        {
            RE::FormID potionFormID = 0;
            std::string entryId;
            {
                std::scoped_lock lock(s_mutex);
                if (!s_pending || s_pending->consumed) {
                    return false;
                }
                s_pending->consumed = true;
                potionFormID = s_pending->potionFormID;
                entryId = s_pending->entryId;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* potion = RE::TESForm::LookupByID<RE::AlchemyItem>(potionFormID);
            if (!player || !potion || player->GetItemCount(potion) <= 0) {
                logger::debug("[PinnedConsumeAnim] pending consume skipped reason='{}' entry='{}' form={:08X}",
                    reason, entryId, potionFormID);
                FinishPending(false);
                return false;
            }
            player->DrinkPotion(potion, nullptr);
            logger::debug("[PinnedConsumeAnim] consumed reason='{}' entry='{}' form={:08X}",
                reason, entryId, potionFormID);
            return true;
        }
        void FinishPending(bool consumeIfNeeded)
        {
            if (consumeIfNeeded) {
                ConsumePending("timeout");
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && s_sink) {
                player->RemoveAnimationGraphEventSink(s_sink.get());
            }
            CleanupAnimationState(player);
            std::scoped_lock lock(s_mutex);
            s_pending.reset();
            s_sink.reset();
        }
        void StartFallbackTimer(std::uint64_t generation)
        {
            std::thread([generation]() {
                std::this_thread::sleep_for(kFallbackTimeout);
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([generation]() {
                        bool stillPending = false;
                        {
                            std::scoped_lock lock(s_mutex);
                            stillPending = s_pending &&
                                s_pending->generation == generation &&
                                !s_pending->consumed;
                        }
                        if (stillPending) {
                            logger::debug("[PinnedConsumeAnim] IUHeal timeout; falling back to DrinkPotion");
                            FinishPending(true);
                        }
                    });
                }
            }).detach();
        }
    }

    bool IsBusy()
    {
        std::scoped_lock lock(s_mutex);
        return static_cast<bool>(s_pending);
    }

    std::string ActiveEntryId()
    {
        std::scoped_lock lock(s_mutex);
        return s_pending ? s_pending->entryId : std::string();
    }

    bool TryDrinkWithAnimation(RE::AlchemyItem* potion, std::string_view entryId)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !potion || player->GetItemCount(potion) <= 0) {
            return false;
        }
        if (!GetUapngModule()) {
            return false;
        }
        if (GetUapngSetting("bExcludePlayer", 0) != 0) {
            return false;
        }
        if (!IsActorReadyForAnimatedDrink(player)) {
            return false;
        }
        {
            std::scoped_lock lock(s_mutex);
            if (s_pending) {
                return false;
            }
            s_sink = std::make_unique<DrinkEventSink>();
            auto pending = std::make_unique<PendingDrink>();
            pending->potionFormID = potion->GetFormID();
            pending->entryId = std::string(entryId);
            pending->generation = ++s_generation;
            s_pending = std::move(pending);
        }
        player->AddAnimationGraphEventSink(s_sink.get());
        SetDrinkAnimObjectModel(potion);
        PlayPotionSound();
        NotifyGraph(player, "HeadtrackingOff");
        SetGraphVariableBool(player, "tdmHeadtrackingBehavior", false);
        SetGraphVariableBool(player, "bPlayEquipSound", true);
        if (!NotifyGraph(player, GetStartEvent())) {
            FinishPending(false);
            return false;
        }
        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(s_mutex);
            generation = s_pending ? s_pending->generation : 0;
        }
        if (generation != 0) {
            StartFallbackTimer(generation);
        }
        logger::debug("[PinnedConsumeAnim] started entry='{}' form={:08X}", entryId, potion->GetFormID());
        return true;
    }
}
