// STATUS: Working
// MCM menu for hotkey configuration.

#include "../../includes/ImGui/RendererSections.h"

#include "../../includes/InputHandler.h"
#include "../../includes/Settings.h"
#include <imgui.h>
#include <chrono>
#include <functional>
#include <string>

extern std::function<void(uint32_t)> g_hkCaptureCb;
extern std::chrono::steady_clock::time_point g_hkCaptureArmedAt;

namespace ImGuiRenderer::SettingsUI {
    static std::string s_bindingId;

    static void RenderBindRow(Settings* settings, const char* label, const char* id,
                              std::int32_t* keyField, std::int32_t* modField,
                              bool allowUnbound = false, const char* tooltip = nullptr)
    {
        if (!s_bindingId.empty() && !g_hkCaptureCb) {
            logger::debug("[HotkeyDbg] UI saw callback already null -> clearing stale prompt for '{}'", s_bindingId);
            s_bindingId.clear();
        }
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s:", label);
        ImGui::SameLine();
        const bool binding = (s_bindingId == id);
        const std::string btnId = std::string("##bind_") + id;
        if (binding) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Press any key...");
            ImGui::SameLine();
            if (ImGui::Button((std::string("Cancel") + btnId).c_str())) {
                logger::debug("[HotkeyDbg] Cancel button pressed for '{}' -> nulling callback", s_bindingId);
                s_bindingId.clear();
                g_hkCaptureCb = nullptr;
            }
            return;
        }
        std::string lbl;
        if (allowUnbound && *keyField < 1)
            lbl = "(unbound)";
        else
            lbl = FormatHotkeyWithModifier(*keyField, *modField);
        lbl += btnId;
        if (ImGui::Button(lbl.c_str())) {
            logger::debug("[HotkeyDbg] ARMED capture for '{}' (waiting for key)", id);
            s_bindingId = id;
            g_hkCaptureArmedAt = std::chrono::steady_clock::now();
            g_hkCaptureCb = [settings, keyField, modField, idCopy = std::string(id)](uint32_t scancode) {
                *keyField = static_cast<std::int32_t>(scancode);
                uint32_t mod = GetHeldModifierSided();
                if (mod == 0) mod = GetHeldModifier();
                *modField = static_cast<std::int32_t>(mod);
                settings->Save();
                if (s_bindingId == idCopy) s_bindingId.clear();
            };
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            *modField = 0;
            settings->Save();
        }
        if (ImGui::IsItemHovered()) {
            if (tooltip)
                ImGui::SetTooltip("%s", tooltip);
            else
                ImGui::SetTooltip("Click then press a key (hold Alt/Ctrl/Shift to bind it too).");
        }
        if (allowUnbound && *keyField >= 1) {
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("Unbind") + btnId).c_str())) {
                *keyField = -1;
                *modField = 0;
                settings->Save();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Turn this hotkey off so another mod can own the key.");
        }
    }

    void RenderHotkeysSection(Settings* settings)
    {
        if (!settings) {
            return;
        }
        ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(42, 26, 58, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(58, 36, 78, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(74, 46, 98, 255));
        bool hkOpen = ImGui::CollapsingHeader("Hotkeys");
        ImGui::PopStyleColor(3);
        if (hkOpen) {
            ImGui::Indent(10);
            const char* offTip = "Click then press a key (hold Alt/Ctrl/Shift to bind it too), or Unbind to turn it off.";
            RenderBindRow(settings, "Inspect Card", "inspect", &settings->inspectCardHotkey, &settings->inspectCardModifier, true, offTip);
            RenderBindRow(settings, "Settings Menu", "settings", &settings->menuSettingsKey, &settings->menuSettingsModifier);
            RenderBindRow(settings, "Party Sheet", "party", &settings->partySheetHotkey, &settings->partySheetModifier, true, offTip);
            RenderBindRow(settings, "Character Sheet", "charsheet", &settings->characterSheetHotkey, &settings->characterSheetModifier, true, offTip);
            RenderBindRow(settings, "HUD Toggle (All)", "hud", &settings->hudToggleKey, &settings->hudToggleModifier, true, offTip);
            RenderBindRow(settings, "Summon Horses", "summon", &settings->smSummonKey, &settings->smSummonModifier,
                          true, "Requires 'Enable Summon Picker Hotkey' under Widgets -> Stable Mount -> Summoning.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Quick Heal");
            RenderBindRow(settings, "Quick Heal", "quickheal", &settings->qhHotkey, &settings->qhModifier, true, offTip);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Quick-Consume Potions (Pinned Items)");
            const char* potTip = "Click to bind, or Unbind to turn it off and let another mod handle potion consume.";
            RenderBindRow(settings, "Health Potion", "pot_health", &settings->potionHealthKey, &settings->potionHealthModifier, true, potTip);
            RenderBindRow(settings, "Magicka Potion", "pot_magicka", &settings->potionMagickaKey, &settings->potionMagickaModifier, true, potTip);
            RenderBindRow(settings, "Stamina Potion", "pot_stamina", &settings->potionStaminaKey, &settings->potionStaminaModifier, true, potTip);
            ImGui::Unindent(10);
        }
    }
}
