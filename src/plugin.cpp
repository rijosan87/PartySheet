
#include <ShlObj.h>
#include <Windows.h>
#include <chrono>
#include <thread>
#include <functional>
#include <spdlog/sinks/basic_file_sink.h>

#include "../includes/LocalizationManager.h"
#include "../includes/PCH.h"
#include "../includes/Settings.h"
#include "../includes/MenuUtils.h"
#include "../includes/EffectBlacklist.h"
#include "../includes/TrueDirectionalMovementAPI.h"
#include "../includes/ImGui/renderer.h"
#include "../includes/ImGui/PartySheet.h"
#include "../includes/ImGui/WidgetStableMount.h"
#include "../includes/ImGui/WidgetHorsePicker.h"
#include "../includes/ImGui/WidgetCollectibles.h"
#include "../includes/ImGui/WidgetPinnedItems.h"
#include "../includes/QuickHeal.h"
#include "../includes/InputHooks.h"
#include "../includes/InputHandler.h"
#include "../includes/ImGui/CharacterSheet.h"
#include "../includes/ImGui/PartySheetV2.h"
#include "../includes/ImGui/PartySheetV2Modal.h"
#include "../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../includes/ImGui/InspectCard.h"
#include "../includes/ImGui/PlayerHUD.h"
#include "../includes/ImGui/PlayerHUDEquipment.h"
#include "../includes/AllyEventHandler.h"
#include "../includes/InventoryEventHandler.h"
#include "../includes/PlayerStatus.h"
#include "../includes/PNGIIBridge.h"
#include "../includes/HUDVisibility.h"
#include "../includes/PartySheetStateAPIInternal.h"

static TDM_API::IVTDM1* g_tdm = nullptr;

static void PlayUISound(const char* editorID) {
    auto settings = Settings::GetSingleton();
    if (settings->enableSoundEffects == 0) {
        logger::debug("[PotionSfx] '{}' suppressed: EnableSoundEffects=0", editorID);
        return;
    }

    auto audioManager = RE::BSAudioManager::GetSingleton();
    if (!audioManager) {
        logger::warn("BSAudioManager not available");
        return;
    }

    RE::BSSoundHandle soundHandle;
    audioManager->GetSoundHandleByName(soundHandle, editorID, 0x1A);

    if (soundHandle.IsValid()) {
        soundHandle.Play();
        logger::debug("[PotionSfx] played '{}'", editorID);
    } else {
        logger::warn("Failed to play sound: {}", editorID);
    }
}

static void InitTDMAPI() {
    void* api = TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V1);
    if (api) {
        g_tdm = reinterpret_cast<TDM_API::IVTDM1*>(api);
        logger::info("TDM API acquired");
    } else {
        logger::warn("TDM API not available");
    }
}

static RE::Actor* GetCurrentTDMTarget() {
    if (!g_tdm) {
        return nullptr;
    }

    auto handle = g_tdm->GetCurrentTarget();
    if (!handle) {
        return nullptr;
    }

    auto actor = handle.get().get();
    return actor ? actor->As<RE::Actor>() : nullptr;
}

static RE::Actor* GetCrosshairTarget() {
    auto cross = RE::CrosshairPickData::GetSingleton();
    if (!cross) {
        return nullptr;
    }

    auto ref = cross->target.get();
    if (!ref) {
        return nullptr;
    }

    return ref->As<RE::Actor>();
}

static RE::Actor* GetLockedTargetFromTrueHUD() {
    if (auto tdmTarget = GetCurrentTDMTarget()) {
        return tdmTarget;
    }

    if (auto cs = GetCrosshairTarget()) {
        return cs;
    }

    return nullptr;
}

std::function<void(uint32_t)> g_hkCaptureCb = nullptr;
std::chrono::steady_clock::time_point g_hkCaptureArmedAt{};

static bool ModifierMatches(std::int32_t modifierType) {
    bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
    bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
    bool lalt  = (GetAsyncKeyState(VK_LMENU)    & 0x8000) != 0;
    bool ralt  = (GetAsyncKeyState(VK_RMENU)    & 0x8000) != 0;
    bool lctrl = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
    bool rctrl = (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
    bool lshft = (GetAsyncKeyState(VK_LSHIFT)   & 0x8000) != 0;
    bool rshft = (GetAsyncKeyState(VK_RSHIFT)   & 0x8000) != 0;
    switch (modifierType) {
        case 1:  return alt   && !ctrl && !shift;
        case 2:  return ctrl  && !alt  && !shift;
        case 3:  return shift && !alt  && !ctrl;
        case 4:  return  lalt && !ralt && !ctrl && !shift;
        case 5:  return  ralt && !lalt && !ctrl && !shift;
        case 6:  return  lctrl && !rctrl && !alt && !shift;
        case 7:  return  rctrl && !lctrl && !alt && !shift;
        case 8:  return  lshft && !rshft && !alt && !ctrl;
        case 9:  return  rshft && !lshft && !alt && !ctrl;
        default: return !alt && !ctrl && !shift;
    }
}

static uint32_t KeycodeFromScancode(uint32_t scancode, RE::INPUT_DEVICE device) {
    switch (device) {
        case RE::INPUT_DEVICE::kKeyboard:
            return scancode;
        case RE::INPUT_DEVICE::kMouse:
            return scancode + 256;
        case RE::INPUT_DEVICE::kGamepad:
            // NOT scancode + 266. A gamepad ButtonEvent's IDCode is the XInput BITMASK
            return SKSE::InputMap::GamepadMaskToKeycode(scancode);
        default:
            return scancode;
    }
}

// Devices a hotkey may be bound to. Gamepad is INI-only: press-to-capture in
static bool IsHotkeyDevice(RE::INPUT_DEVICE device) {
    return device == RE::INPUT_DEVICE::kKeyboard ||
           device == RE::INPUT_DEVICE::kGamepad;
}

static void PlayUISoundDelayed(const char* editorID, int delayMs = 100) {
    std::string soundID = editorID;
    
    auto taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        logger::warn("SKSE TaskInterface not available");
        return;
    }
    
    taskInterface->AddTask([soundID, delayMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        PlayUISound(soundID.c_str());
    });
}

class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
private:
    bool isStatsVisible = false;
    RE::ActorHandle lastTarget;
    
    std::chrono::steady_clock::time_point lastUpdateTime;
    static constexpr std::chrono::milliseconds UPDATE_COOLDOWN{50};
    
    std::chrono::steady_clock::time_point lastFrameTime;
    RE::TESImageSpaceModifier* activeISM = nullptr;
    
    void StartImmersiveEffect() {
        activeISM = RE::TESForm::LookupByEditorID<RE::TESImageSpaceModifier>("DetectLife01Imod");
        if (activeISM) {
            activeISM->TriggerIfNotActive(0.5f, nullptr);
            logger::info("ISM effect started: DetectLife01Imod");
        } else {
            logger::warn("ISM effect failed: DetectLife01Imod not found");
        }
        lastFrameTime = std::chrono::steady_clock::now();
    }
    
    void StopImmersiveEffect() {
        if (activeISM) {
            RE::ImageSpaceModifierInstanceForm::Stop(activeISM);
            activeISM = nullptr;
        }
    }

public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* ev, RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!ev || !*ev) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (g_hkCaptureCb) {
            if (!ImGuiRenderer::IsVisible()) {
                logger::debug("[HotkeyDbg] Settings closed mid-bind -> nulling callback");
                g_hkCaptureCb = nullptr;
            }
            else if (MenuUtils::IsBlockingMenuOpen()) {
                g_hkCaptureCb = nullptr;
                logger::debug("[HotkeyDbg] rebind capture cancelled: another menu opened mid-bind");
            }
            else if (std::chrono::steady_clock::now() - g_hkCaptureArmedAt > std::chrono::seconds(5)) {
                g_hkCaptureCb = nullptr;
                logger::debug("[HotkeyDbg] rebind capture timed out after 5s");
            }
        }

        auto settings = Settings::GetSingleton();
        
        if (settings->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible()) {
            auto ui = RE::UI::GetSingleton();
            bool inventoryOpen = ui && ui->IsMenuOpen("InventoryMenu");
            bool magicOpen = ui && ui->IsMenuOpen("MagicMenu");
            bool tweenOpen = ui && ui->IsMenuOpen("TweenMenu");

            if (MenuUtils::IsBlockingMenuOpen() && !inventoryOpen && !magicOpen && !tweenOpen) {
                CharacterSheet::GetSingleton()->Toggle();
                logger::debug("CharacterSheet: Auto-closed due to menu conflict");
            }
        }
        
        if (isStatsVisible || InspectCard::IsVisible()) {
            if (MenuUtils::IsBlockingMenuOpen()) {
                InspectCard::Hide();
                isStatsVisible = false;
                lastTarget = RE::ActorHandle();
                StopImmersiveEffect();
                return RE::BSEventNotifyControl::kContinue;
            }

            if (settings->requireAlteration == 1) {
                auto player = RE::PlayerCharacter::GetSingleton();
                if (player) {
                    float currentMagicka = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
                    if (currentMagicka <= 0.0f) {
                        InspectCard::Hide();
                        isStatsVisible = false;
                        lastTarget = RE::ActorHandle();
                        StopImmersiveEffect();
                        RE::SendHUDMessage::ShowHUDMessage(LocalizationManager::Get("InspectCard.ConcentrationFades").c_str());
                        return RE::BSEventNotifyControl::kContinue;
                    }
                }
            }

            bool isOpeningSettings = false;
            if (auto e = *ev) {
                if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                    if (auto* btn = e->AsButtonEvent()) {
                        if (btn->IsDown() && IsHotkeyDevice(btn->GetDevice())) {
                            uint32_t keyCode = KeycodeFromScancode(btn->GetIDCode(), btn->GetDevice());
                            if (keyCode == static_cast<uint32_t>(settings->menuSettingsKey)) {
                                isOpeningSettings = true;
                            }
                        }
                    }
                }
            }

            RE::Actor* actor = GetLockedTargetFromTrueHUD();

            if (!actor) {
                if (!ImGuiRenderer::IsVisible() && !isOpeningSettings) {
                    InspectCard::Hide();
                    isStatsVisible = false;
                    lastTarget = RE::ActorHandle();
                    StopImmersiveEffect();
                }
            } else {
                auto currentHandle = actor->GetHandle();
                if (currentHandle != lastTarget) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - lastUpdateTime < UPDATE_COOLDOWN) {
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    lastUpdateTime = now;
                    
                    InspectCard::SetTargetActor(actor);
                    lastTarget = currentHandle;
                    PlayUISound("UIMenuOK");
                }
            }
        }

        auto e = *ev;

        if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* btn = e->AsButtonEvent();
        if (!btn) {
            return RE::BSEventNotifyControl::kContinue;
        }
        
        if (btn->GetDevice() == RE::INPUT_DEVICE::kMouse &&
            (ImGuiRenderer::IsVisible() || PartySheetV2::IsEffectsModalOpen() || PartySheetV2::IsInventoryModalOpen())) {
            uint32_t idCode = btn->GetIDCode();
            if (idCode == 8 || idCode == 9) {
                if (btn->IsDown() || btn->IsPressed()) {
                    float scrollDelta = (idCode == 8) ? 1.0f : -1.0f;
                    ImGuiRenderer::AddScrollDelta(scrollDelta);
                    return RE::BSEventNotifyControl::kStop;
                }
            }
        }
        
        if (!btn->IsDown()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (auto* ue = RE::UserEvents::GetSingleton(); ue && btn->QUserEvent() == ue->shout)
            PlayerHUDEquipment::NotifyShoutPressed();

        uint32_t keyCode = KeycodeFromScancode(btn->GetIDCode(), btn->GetDevice());

        constexpr uint32_t SCANCODE_ESCAPE = 1;
        if (keyCode == SCANCODE_ESCAPE) {
            if (settings->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible()) {
                CharacterSheet::GetSingleton()->Toggle();
                return RE::BSEventNotifyControl::kStop;
            }
            if (PartySheet::IsVisible()) {
                PartySheet::Toggle();
                return RE::BSEventNotifyControl::kStop;
            }
            if (PartySheetV2::IsVisible()) {
                PartySheetV2::Toggle();
                return RE::BSEventNotifyControl::kStop;
            }
            if (ImGuiRenderer::IsVisible()) {
                ImGuiRenderer::Toggle();
                return RE::BSEventNotifyControl::kStop;
            }
        }
        
        constexpr uint32_t SCANCODE_TAB = 15;
        if (keyCode == SCANCODE_TAB) {
            if (settings->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible()) {
                CharacterSheet::GetSingleton()->Toggle();
                return RE::BSEventNotifyControl::kStop;
            }
        }

        if (auto* ui = RE::UI::GetSingleton()) {
            const char* menuName = nullptr;
            if (ui->IsMenuOpen("InventoryMenu")) menuName = "InventoryMenu";
            else if (ui->IsMenuOpen("MagicMenu")) menuName = "MagicMenu";
            if (menuName) {
                if (auto menu = ui->GetMenu(menuName); menu && menu->uiMovie) {
                    RE::GFxValue type;
                    if (menu->uiMovie->GetVariable(
                            &type, "_root.Menu_mc.inventoryLists.searchWidget.textField.type") &&
                        type.IsString() && std::string_view(type.GetString()) == "input") {
                        return RE::BSEventNotifyControl::kContinue;
                    }
                }
            }
        }

        // HOTKEY: Toggle ImGui Settings Menu (configurable via INI)
        if (IsHotkeyDevice(btn->GetDevice()) &&
            keyCode == static_cast<uint32_t>(settings->menuSettingsKey) &&
            ModifierMatches(settings->menuSettingsModifier)) {
            ImGuiRenderer::Toggle();
            return RE::BSEventNotifyControl::kContinue;
        }

        // HOTKEY: Toggle all HUD widgets
        if (IsHotkeyDevice(btn->GetDevice()) &&
            settings->hudToggleKey >= 1 &&
            keyCode == static_cast<uint32_t>(settings->hudToggleKey) &&
            ModifierMatches(settings->hudToggleModifier)) {
            if (MenuUtils::IsBlockingMenuOpen()) {
                return RE::BSEventNotifyControl::kContinue;
            }
            settings->hudOverrideHidden = !settings->hudOverrideHidden;
            PartySheetAPI::SetPendingReason(PartySheetAPI::Reason::UserToggle);
            PartySheetAPI::PublishIfChanged();
            logger::debug("HUD override toggled: {}", settings->hudOverrideHidden ? "hidden" : "visible");
            return RE::BSEventNotifyControl::kContinue;
        }

        // HOTKEY: Toggle Party Sheet (F6 default)
        if (IsHotkeyDevice(btn->GetDevice()) &&
            settings->partySheetHotkey >= 1 &&
            keyCode == static_cast<uint32_t>(settings->partySheetHotkey) &&
            ModifierMatches(settings->partySheetModifier)) {
            if (MenuUtils::ShouldBlockCharacterSheet()) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (settings->psUseFullscreenMode) {
                PlayUISound(PartySheetV2::IsVisible() ? "UIMenuCancel" : "UIMenuOK");
                PartySheetV2::Toggle();
            } else {
                PlayUISound(PartySheet::IsVisible() ? "UIMenuCancel" : "UIMenuOK");
                PartySheet::Toggle();
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        if (settings->qhEnabled &&
            IsHotkeyDevice(btn->GetDevice()) &&
            settings->qhHotkey >= 1 &&
            keyCode == static_cast<uint32_t>(settings->qhHotkey) &&
            ModifierMatches(settings->qhModifier))
        {
            if (MenuUtils::IsBlockingMenuOpen()) {
                return RE::BSEventNotifyControl::kContinue;
            }
            QuickHeal::TryCast();
            return RE::BSEventNotifyControl::kContinue;
        }

        // HOTKEY: quick-consume pinned potions (Health/Magicka/Stamina).
        if (IsHotkeyDevice(btn->GetDevice())) {
            const char* entryId = nullptr;
            if (settings->potionHealthKey >= 0 &&
                keyCode == static_cast<uint32_t>(settings->potionHealthKey) &&
                ModifierMatches(settings->potionHealthModifier)) {
                entryId = "health_potion";
            } else if (settings->potionMagickaKey >= 0 &&
                       keyCode == static_cast<uint32_t>(settings->potionMagickaKey) &&
                       ModifierMatches(settings->potionMagickaModifier)) {
                entryId = "magicka_potion";
            } else if (settings->potionStaminaKey >= 0 &&
                       keyCode == static_cast<uint32_t>(settings->potionStaminaKey) &&
                       ModifierMatches(settings->potionStaminaModifier)) {
                entryId = "stamina_potion";
            }
            if (entryId) {
                HUDVisibility::Trigger(HUDVisibility::WidgetID::PinnedItems);

                if (MenuUtils::IsBlockingMenuOpen()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                using CR = WidgetPinnedItems::ConsumeResult;
                const CR result = WidgetPinnedItems::TryConsume(entryId);

                const char* resultName = "?";
                switch (result) {
                case CR::kNone:          resultName = "None (no entry / none owned)"; break;
                case CR::kDrankVanilla:  resultName = "DrankVanilla"; break;
                case CR::kDrankAnimated: resultName = "DrankAnimated (UPNG owns sound)"; break;
                case CR::kDeclined:      resultName = "Declined (at full resource)"; break;
                case CR::kBlocked:       resultName = "Blocked (drink still in progress)"; break;
                }
                logger::debug("[PotionSfx] press entry='{}' -> {}", entryId, resultName);

                if (result == CR::kDrankVanilla) {
                    PlayUISound("ITMPotionUse");
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        if (settings->smEnableHorsePicker &&
            IsHotkeyDevice(btn->GetDevice()) &&
            settings->smSummonKey >= 0 &&
            keyCode == static_cast<uint32_t>(settings->smSummonKey) &&
            ModifierMatches(settings->smSummonModifier))
        {
            if (MenuUtils::IsBlockingMenuOpen()) {
                return RE::BSEventNotifyControl::kContinue;
            }
            WidgetHorsePicker::Toggle();
            return RE::BSEventNotifyControl::kContinue;
        }

        // HOTKEY: Toggle InspectCard UI (with modifier key support)
        if (IsHotkeyDevice(btn->GetDevice()) &&
            settings->inspectCardHotkey >= 1 &&
            keyCode == static_cast<uint32_t>(settings->inspectCardHotkey) &&
            ModifierMatches(settings->inspectCardModifier)) {
            if (MenuUtils::IsBlockingMenuOpen()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (InspectCard::IsVisible()) {
                PlayUISound("UIMenuCancel");
                InspectCard::Hide();
                isStatsVisible = false;
                lastTarget = RE::ActorHandle();
                StopImmersiveEffect();
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::Actor* target = GetLockedTargetFromTrueHUD();

            if (!target) {
                isStatsVisible = false;
                lastTarget = RE::ActorHandle();
                return RE::BSEventNotifyControl::kContinue;
            }

            if (settings->requireAlteration == 1) {
                auto player = RE::PlayerCharacter::GetSingleton();
                if (player) {
                    float alterationLevel = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAlteration);
                    if (alterationLevel < static_cast<float>(settings->requiredAlterationLevel)) {
                        // Requirement not met - show notification and abort
                        RE::SendHUDMessage::ShowHUDMessage(LocalizationManager::Get("InspectCard.LowAlterationEssence").c_str());
                        return RE::BSEventNotifyControl::kContinue;
                    }
                }
            }

            if (PlayerStatus::GetTransformState() != PlayerStatus::TransformState::None)
                return RE::BSEventNotifyControl::kContinue;

            InspectCard::Show(target);
            isStatsVisible = true;
            lastTarget = target->GetHandle();
            
            if (settings->requireAlteration == 1) {
                StartImmersiveEffect();
            }

            PlayUISound("UIMenuOK");

            return RE::BSEventNotifyControl::kContinue;
        }

        // HOTKEY: Toggle Character Sheet (configurable, default F7)
        if (IsHotkeyDevice(btn->GetDevice()) &&
            settings->enableCharacterSheet &&
            settings->characterSheetHotkey >= 1 &&
            keyCode == static_cast<uint32_t>(settings->characterSheetHotkey) &&
            ModifierMatches(settings->characterSheetModifier)) {
            if (settings->debugEnableCSLogging)
                logger::info("[DEBUG-CS] CS hotkey + modifier matched, calling Toggle()");
            bool wasVisible = CharacterSheet::GetSingleton()->IsVisible();
            CharacterSheet::GetSingleton()->Toggle();
            bool isNowVisible = CharacterSheet::GetSingleton()->IsVisible();
            if (settings->debugEnableCSLogging)
                logger::info("[DEBUG-CS] Toggle() done: wasVisible={} isNowVisible={}", wasVisible, isNowVisible);

            if (wasVisible != isNowVisible) {
                PlayUISound(isNowVisible ? "UIMenuOK" : "UIMenuCancel");
            }
            return RE::BSEventNotifyControl::kContinue;
        }
        if (IsHotkeyDevice(btn->GetDevice()) && settings->characterSheetHotkey >= 1 &&
            keyCode == static_cast<uint32_t>(settings->characterSheetHotkey)) {
            if (settings->debugEnableCSLogging)
                logger::info("[DEBUG-CS] Hotkey matched but blocked: enableCharacterSheet={}", settings->enableCharacterSheet);
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

static InputHandler g_inputHandler;

class TweenMenuListener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        const SKSE::ModCallbackEvent* a_event,
        RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }
        
        
        std::string eventName = a_event->eventName.c_str();
        
        if (eventName == "PartySheet_Open" ||
            eventName == "OpenTween_PartySheet" ||
            eventName == "PartySheet") {
            auto* settings = Settings::GetSingleton();
            if (settings && settings->psUseFullscreenMode) {
                if (!PartySheetV2::IsVisible()) {
                    PlayUISound("UIMenuOK");
                    PartySheetV2::Toggle();
                }
            } else {
                if (!PartySheet::IsVisible()) {
                    PlayUISound("UIMenuOK");
                    PartySheet::Show();
                }
            }
        }
        
        if (eventName == "CharacterStats_Open" || 
            eventName == "OpenTween_CharacterStats" ||
            eventName == "CharacterStats") {
            auto settings = Settings::GetSingleton();
            if (settings->enableCharacterSheet) {
                
                if (!CharacterSheet::GetSingleton()->IsVisible()) {
                    CharacterSheet::GetSingleton()->Open();
                    PlayUISoundDelayed("UIMenuOK", 150);
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    static TweenMenuListener* GetSingleton() {
        static TweenMenuListener singleton;
        return &singleton;
    }
};

extern "C" DLLEXPORT bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    // NG 7.x initializes the shared trampoline once. Reserve both 14-byte call stubs
    SKSE::Init(skse, {
        .log = false,
        .trampoline = true,
        .trampolineSize = 14 * 2
    });

    wchar_t* documentsPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPath);
    auto path = std::filesystem::path(documentsPath);
    CoTaskMemFree(documentsPath);

    path /= "My Games/Skyrim Special Edition/SKSE/PartySheet.log";

    std::filesystem::create_directories(path.parent_path());

    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
    auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));

    logger::info("Party Sheet Plugin Loading…");

    Settings::GetSingleton()->Load();

    if (Settings::GetSingleton()->debugEnableVerboseLogging) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        logger::info("Verbose logging enabled ([Debug] EnableVerboseLogging=1)");
    }

    auto loc = LocalizationManager::GetSingleton();
    loc->LoadLanguage(Settings::GetSingleton()->language);

    WidgetStableMount::Init();
    WidgetCollectibles::Init();
    WidgetPinnedItems::Init();
    QuickHeal::Init();

    if (auto messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
            switch (msg->type) {
                case SKSE::MessagingInterface::kPostLoad:
                    InitTDMAPI();
                    ImGuiRenderer::Install();
                    InputHooks::InstallInputHooks();
                    PNGIIBridge::Init();            // Optional PNG Icon Injector glyph API.
                    break;

                case SKSE::MessagingInterface::kPostPostLoad:
                    PartySheetAPI::DispatchInterface();
                    break;

                case SKSE::MessagingInterface::kInputLoaded:
                    if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
                        input->AddEventSink(&g_inputHandler);
                        logger::info("InputHandler registered");
                    }
                    break;

                case SKSE::MessagingInterface::kPreLoadGame:
                    PNGIIBridge::Clear();
                    break;

                case SKSE::MessagingInterface::kPostLoadGame:
                    if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
                        input->AddEventSink(&g_inputHandler);
                        logger::info("InputHandler re-registered [kPostLoadGame]");
                    }
                    WidgetStableMount::Init();
                    WidgetCollectibles::Init();
                    WidgetPinnedItems::Init();
                    QuickHeal::ReloadFromJson();
                    PlayerHUD::Init();
                    PlayerStatus::InvalidateCache();
                    PNGIIBridge::Clear();           // PNGII rebuilds its rules cache at load.
                    PlayerHUDEquipment::MarkSlotsDirty();
                    logger::info("PlayerStatus cache invalidated [kPostLoadGame]");
                    break;

                case SKSE::MessagingInterface::kNewGame:
                    WidgetStableMount::Init();
                    WidgetCollectibles::Init();
                    WidgetPinnedItems::Init();
                    QuickHeal::ReloadFromJson();
                    PlayerHUD::Init();
                    PlayerStatus::InvalidateCache();
                    PNGIIBridge::Clear();
                    PlayerHUDEquipment::MarkSlotsDirty();
                    logger::info("Widgets reset [kNewGame]");
                    break;

                case SKSE::MessagingInterface::kDataLoaded:
                    AllyEvents::RegisterEventSinks();

                    InventoryEvents::RegisterEventSinks();
                    
                    PlayerHUD::Init();

                    EffectBlacklist::Load();

                    if (auto modCallbackSource = SKSE::GetModCallbackEventSource()) {
                        modCallbackSource->AddEventSink(TweenMenuListener::GetSingleton());
                        logger::info("TweenMenu integration registered");
                    }

                    PartySheetAPI::PublishIfChanged();

                    logger::info("Data loaded");
                    break;

                default:
                    break;
            }
        });
    }

    logger::info("Party Sheet Plugin Loaded");
    return true;
}
