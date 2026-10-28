
#include "../includes/InputHooks.h"
#include "../includes/ImGui/PartySheet.h"
#include "../includes/ImGui/PartySheetV2.h"
#include "../includes/ImGui/PartySheetV2Modal.h"
#include "../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../includes/ImGui/PartySheetV2StatusModal.h"
#include "../includes/ImGui/CharacterSheet.h"
#include "../includes/ImGui/Renderer.h"
#include "../includes/ImGui/InspectCard.h"
#include "../includes/ImGui/WidgetHorsePicker.h"
#include "../includes/InputHandler.h"
#include <imgui.h>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace logger = SKSE::log;

extern std::function<void(uint32_t)> g_hkCaptureCb;

namespace InputHooks {

    static constexpr uint32_t SCANCODE_ESC = 0x01;
    static constexpr uint32_t SCANCODE_TAB = 0x0F;

    using ProcessInputQueueFn = void (*)(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent**);
    static REL::Relocation<ProcessInputQueueFn> _ProcessInputQueue;

    static bool g_BlockInput = false;

    void SetBlockInput(bool a_block) {
        g_BlockInput = a_block;
    }

    bool IsTextInputActive() {
        if (!ImGui::GetCurrentContext()) return false;
        return ImGui::GetIO().WantTextInput;
    }

    static bool HasCharEventInQueue(RE::InputEvent** a_events) {
        if (!a_events || !*a_events) return false;
        RE::InputEvent* ev = *a_events;
        while (ev) {
            if (ev->eventType == RE::INPUT_EVENT_TYPE::kChar) return true;
            ev = ev->next;
        }
        return false;
    }

    static void Hook_ProcessInputQueue(RE::BSTEventSource<RE::InputEvent*>* a_source, RE::InputEvent** a_events) {
        const bool hasCharEventThisQueue = HasCharEventInQueue(a_events);

        
        bool partySheetVisible = PartySheet::IsVisible();
        bool partySheetV2Visible = PartySheetV2::IsVisible();
        bool characterSheetVisible = CharacterSheet::GetSingleton()->IsVisible();
        bool settingsVisible = ImGuiRenderer::IsVisible();
        bool inspectCardVisible = InspectCard::IsVisible();
        bool horsePickerVisible = WidgetHorsePicker::IsVisible();

        if ((partySheetVisible || partySheetV2Visible || characterSheetVisible || settingsVisible || inspectCardVisible || horsePickerVisible) && a_events && *a_events) {
            RE::InputEvent* prev = nullptr;
            RE::InputEvent* curr = *a_events;
            while (curr) {
                bool shouldConsume = false;

                if (curr->eventType == RE::INPUT_EVENT_TYPE::kChar) {
                    if (IsTextInputActive()) {
                        auto* charEv = static_cast<RE::CharEvent*>(curr);
                        unsigned int cp = static_cast<unsigned int>(charEv->keyCode);
                        if (cp >= 32) {
                            ImGui::GetIO().AddInputCharacter(cp);
                        }
                    }
                    shouldConsume = true;
                }
                else if (curr->eventType == RE::INPUT_EVENT_TYPE::kButton) {
                    auto* btn = curr->AsButtonEvent();
                    if (btn && (btn->userEvent == "Zoom In" || btn->userEvent == "Zoom Out" ||
                                btn->userEvent == "Toggle POV")) {
                        shouldConsume = true;
                        if (btn->GetDevice() == RE::INPUT_DEVICE::kMouse && (btn->IsDown() || btn->IsPressed())) {
                            uint32_t idCode = btn->GetIDCode();
                            if (idCode == 8 || idCode == 9)
                                ImGuiRenderer::AddScrollDelta(idCode == 8 ? 1.0f : -1.0f);
                        }
                    }
                    else if (btn && btn->GetDevice() == RE::INPUT_DEVICE::kKeyboard && btn->IsDown()) {
                        uint32_t scancode = btn->GetIDCode();

                        if (g_hkCaptureCb) {
                            shouldConsume = true;
                            if (scancode != SCANCODE_ESC && scancode != SCANCODE_TAB &&
                                !ImGuiRenderer::IsModifierScancode(scancode)) {
                                logger::debug("[HotkeyDbg] CAPTURED key 0x{:X} (in queue hook) -> firing callback", scancode);
                                auto cb = std::move(g_hkCaptureCb);
                                g_hkCaptureCb = nullptr;
                                cb(scancode);
                            }
                        }
                        // P = open Effects & Spells modal, I = open Inventory modal
                        else if (partySheetV2Visible && scancode == 0x19) {
                            if (!PartySheetV2::IsEffectsModalOpen() && !PartySheetV2::IsInventoryModalOpen())
                                PartySheetV2::OpenEffectsModal();
                            else
                                PartySheetV2::CloseEffectsModal();
                            shouldConsume = true;
                        }
                        else if (partySheetV2Visible && scancode == 0x17) {
                            if (!PartySheetV2::IsInventoryModalOpen() && !PartySheetV2::IsEffectsModalOpen())
                                PartySheetV2::OpenInventoryModal();
                            else
                                PartySheetV2::CloseInventoryModal();
                            shouldConsume = true;
                        }

                        else if (scancode == SCANCODE_ESC || scancode == SCANCODE_TAB) {

                            if (partySheetVisible) {
                                PartySheet::Toggle();
                                shouldConsume = true;
                                logger::trace("Consumed {} key - closed PartySheet",
                                    scancode == SCANCODE_ESC ? "ESC" : "TAB");
                                partySheetVisible = false;
                            }
                            else if (partySheetV2Visible) {
                                bool modalOpen = PartySheetV2::IsEffectsModalOpen() ||
                                                 PartySheetV2::IsInventoryModalOpen() ||
                                                 PartySheetV2::IsStatusModalOpen();
                                if (modalOpen) {
                                    PartySheetV2::CloseEffectsModal();
                                    PartySheetV2::CloseInventoryModal();
                                    PartySheetV2::CloseStatusModal();
                                    shouldConsume = true;
                                } else {
                                    PartySheetV2::Toggle();
                                    shouldConsume = true;
                                    partySheetV2Visible = false;
                                }
                            }
                            else if (characterSheetVisible) {
                                CharacterSheet::GetSingleton()->Toggle();
                                shouldConsume = true;
                                logger::trace("Consumed {} key - closed CharacterSheet", 
                                    scancode == SCANCODE_ESC ? "ESC" : "TAB");
                                characterSheetVisible = false;
                            }
                            else if (settingsVisible) {
                                ImGuiRenderer::Toggle();
                                shouldConsume = true;
                                logger::trace("Consumed {} key - closed HUD Settings", 
                                    scancode == SCANCODE_ESC ? "ESC" : "TAB");
                                settingsVisible = false;
                            }
                            else if (inspectCardVisible) {
                                InspectCard::Hide();
                                shouldConsume = true;
                                logger::trace("Consumed {} key - closed Inspect Card",
                                    scancode == SCANCODE_ESC ? "ESC" : "TAB");
                                inspectCardVisible = false;
                            }
                            else if (horsePickerVisible) {
                                WidgetHorsePicker::Hide();
                                shouldConsume = true;
                                logger::trace("Consumed {} key - closed Horse Picker",
                                    scancode == SCANCODE_ESC ? "ESC" : "TAB");
                                horsePickerVisible = false;
                            }
                        }
                    }
                    else if (btn && btn->GetDevice() == RE::INPUT_DEVICE::kGamepad && btn->IsDown()) {
                        uint32_t id = btn->GetIDCode();
                        shouldConsume = true; 
                        
                        
                        if (id == 0x0020) {
                             bool closedSomething = false;

                             if (partySheetVisible) {
                                PartySheet::Toggle();
                                closedSomething = true;
                                partySheetVisible = false;
                             }
                             else if (partySheetV2Visible) {
                                PartySheetV2::Toggle();
                                closedSomething = true;
                                partySheetV2Visible = false;
                             }
                             else if (characterSheetVisible) {
                                CharacterSheet::GetSingleton()->Toggle();
                                closedSomething = true;
                                characterSheetVisible = false;
                             }
                             else if (settingsVisible) {
                                ImGuiRenderer::Toggle();
                                closedSomething = true;
                                settingsVisible = false;
                             }
                             else if (inspectCardVisible) {
                                InspectCard::Hide();
                                closedSomething = true;
                                inspectCardVisible = false;
                             }
                             
                             if (closedSomething) {
                                 logger::trace("Consumed Gamepad Back (0x0020) - Force closed UI");
                             }
                        }
                        else if (id == 0x2000) {
                              
                              bool closedOverlay = false;
                              
                              if (partySheetVisible) {
                                 PartySheet::Toggle();
                                 closedOverlay = true;
                              }
                              else if (partySheetV2Visible) {
                                 PartySheetV2::Toggle();
                                 closedOverlay = true;
                              }
                              else if (characterSheetVisible) {
                                 CharacterSheet::GetSingleton()->Toggle();
                                 closedOverlay = true;
                              }
                              else if (inspectCardVisible) {
                                 InspectCard::Hide();
                                 closedOverlay = true;
                              }
                              
                        }
                        
                        if (partySheetVisible) {
                            if (id == 0x0100) {
                                PartySheet::PrevPage();
                                logger::trace("Gamepad LB - Party Sheet prev page");
                            }
                            else if (id == 0x0200) {
                                PartySheet::NextPage();
                                logger::trace("Gamepad RB - Party Sheet next page");
                            }
                        }
                        else if (partySheetV2Visible) {
                            if (id == 0x0100) {
                                PartySheetV2::PrevSubject();
                                logger::trace("Gamepad LB - PartySheetV2 prev subject");
                            }
                            else if (id == 0x0200) {
                                PartySheetV2::NextSubject();
                                logger::trace("Gamepad RB - PartySheetV2 next subject");
                            }
                        }
                    }
                }
                
                if (shouldConsume) {
                    if (prev) {
                        prev->next = curr->next;
                    } else {
                        *a_events = curr->next;
                    }
                    curr = curr->next;
                } else {
                    prev = curr;
                    curr = curr->next;
                }
            }
        }

        if (IsTextInputActive() && a_events && *a_events) {
            auto IsNumpadNavScancode = [](uint32_t sc) -> bool {
                switch (sc) {
                    case 0x47:
                    case 0x48:
                    case 0x49:
                    case 0x4B:
                    case 0x4D:
                    case 0x4F:
                    case 0x50:
                    case 0x51:
                    case 0x52:
                    case 0x53:
                        return true;
                    default:
                        return false;
                }
            };
            RE::InputEvent* prev = nullptr;
            RE::InputEvent* curr = *a_events;

            auto ToImGuiNavKey = [](uint32_t sc) -> ImGuiKey {
                switch (sc) {
                    case 0x0E: return ImGuiKey_Backspace;
                    case 0x53: return ImGuiKey_Delete;
                    case 0x4B: return ImGuiKey_LeftArrow;
                    case 0x4D: return ImGuiKey_RightArrow;
                    case 0x48: return ImGuiKey_UpArrow;
                    case 0x50: return ImGuiKey_DownArrow;
                    case 0x47: return ImGuiKey_Home;
                    case 0x4F: return ImGuiKey_End;
                    case 0x1C: return ImGuiKey_Enter;
                    default:   return ImGuiKey_None;
                }
            };

            while (curr) {
                bool shouldRemove = false;

                if (curr->eventType == RE::INPUT_EVENT_TYPE::kButton) {
                    auto* btn = curr->AsButtonEvent();
                    if (btn && btn->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                        uint32_t sc = btn->GetIDCode();
                        ImGuiKey navKey = ToImGuiNavKey(sc);

                        if (btn->IsDown()) {
                            bool msgHookActive = ImGuiRenderer::MsgHookFiredThisFrame();
                            bool isNumpadChar = hasCharEventThisQueue && IsNumpadNavScancode(sc);
                            if (navKey != ImGuiKey_None) {
                                if (!isNumpadChar && !msgHookActive) {
                                    ImGui::GetIO().AddKeyEvent(navKey, true);
                                }
                            }
                            shouldRemove = true;
                        } else {
                            bool msgHookActive = ImGuiRenderer::MsgHookFiredThisFrame();
                            bool isNumpadChar = hasCharEventThisQueue && IsNumpadNavScancode(sc);
                            if (navKey != ImGuiKey_None && !isNumpadChar && !msgHookActive) {
                                ImGui::GetIO().AddKeyEvent(navKey, false);
                            }
                            shouldRemove = (navKey != ImGuiKey_None);
                        }
                    }
                }

                if (shouldRemove) {
                    if (prev) {
                        prev->next = curr->next;
                    } else {
                        *a_events = curr->next;
                    }
                    curr = curr->next;
                } else {
                    prev = curr;
                    curr = curr->next;
                }
            }
        }

        _ProcessInputQueue(a_source, a_events);
    }

    void InstallInputHooks() {
        REL::RelocationID hookLocation(67315, 68617);
        uintptr_t hookAddress = hookLocation.address() + REL::Relocate(0x7B, 0x7B, 0x81);

        auto& trampoline = SKSE::GetTrampoline();

        _ProcessInputQueue = trampoline.write_call<5>(hookAddress, Hook_ProcessInputQueue);

        logger::info("InputHooks installed - keyboard/gamepad UI navigation, modal hotkeys (P/I), text input forwarding");
    }

}
