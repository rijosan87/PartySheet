#include "../../includes/ImGui/WidgetHorsePicker.h"
#include "../../includes/ImGui/WidgetStableMount.h"
#include "../../includes/HorseRegistry.h"
#include "../../includes/Settings.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/TextureManager.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/LocalizationManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <RE/Skyrim.h>
#include <algorithm>

namespace WidgetHorsePicker {

    static bool g_Visible = false;

    static void DisablePlayerInput() {
        if (auto* pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler)        pc->lookHandler->SetInputEventHandlingEnabled(false);
            if (pc->movementHandler)    pc->movementHandler->SetInputEventHandlingEnabled(false);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(false);
        }
    }
    static void EnablePlayerInput() {
        if (auto* pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler)        pc->lookHandler->SetInputEventHandlingEnabled(true);
            if (pc->movementHandler)    pc->movementHandler->SetInputEventHandlingEnabled(true);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(true);
        }
    }

    void Show() {
        if (g_Visible) return;
        g_Visible = true;
        DisablePlayerInput();
    }
    void Hide() {
        if (!g_Visible) return;
        g_Visible = false;
        EnablePlayerInput();
    }
    bool IsVisible() { return g_Visible; }

    void Toggle() {
        auto* settings = Settings::GetSingleton();
        if (!settings->smEnableHorsePicker) return;
        if (g_Visible) Hide();
        else           Show();
    }

    static bool IsSummonBlocked(bool& outIsInterior) {
        outIsInterior = false;
        auto* settings = Settings::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return true;
        auto* cell = player->GetParentCell();
        if (cell && cell->IsInteriorCell()) {
            outIsInterior = true;
            return true;
        }
        if (!settings->smSummonAllowCities) {
            auto* worldspace = player->GetWorldspace();
            constexpr RE::FormID kTamriel = 0x0000003C;
            if (worldspace && worldspace->GetFormID() != kTamriel) return true;
        }
        return false;
    }

    enum class Unavailable { None, Near, Dead, NotOwned };

    static Unavailable GetUnavailableReason(const PartyHorses::HorseRecord& rec) {
        auto* horse  = rec.actorPtr;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!horse || !player) return Unavailable::NotOwned;

        if (horse->IsDead(true) || horse->IsDeleted()) return Unavailable::Dead;

        auto* owner = horse->extraList.GetOwner();
        bool owned = false;
        if (owner) {
            if (owner == player->GetActorBase()) owned = true;
            else if (auto* fac = owner->As<RE::TESFaction>()) owned = player->IsInFaction(fac);
        }
        if (!owned) return Unavailable::NotOwned;

        if (rec.is3DLoaded) return Unavailable::Near;

        return Unavailable::None;
    }

    static bool RenderContent() {
        bool requestClose = false;

        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        const float preferredW = (std::min)(360.0f, disp.x * 0.95f);
        ImGui::SetNextWindowSize(ImVec2(preferredW, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2((std::min)(280.0f, preferredW), 0.0f),
            ImVec2(disp.x * 0.95f, disp.y * 0.90f));

        const std::string title = LocalizationManager::Get("HorsePicker.Title") + "##horsePicker";
        bool keepOpen = true;
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (!ImGui::Begin(title.c_str(), &keepOpen, flags)) {
            ImGui::End();
            return !keepOpen;
        }

        if (WidgetStableMount::IsImmersiveHorsesActive()) {
            ImGui::TextWrapped("%s", LocalizationManager::Get("HorsePicker.IHManaged").c_str());
            ImGui::End();
            return !keepOpen;
        }

        bool isInterior = false;
        const bool blocked = IsSummonBlocked(isInterior);
        if (blocked) {
            ImGui::TextDisabled("%s", LocalizationManager::Get(
                isInterior ? "HorsePicker.BlockedInterior" : "HorsePicker.BlockedCity").c_str());
            ImGui::Separator();
        }

        auto horses = PartyHorses::HorseRegistry::GetSingleton()->GetAll();
        if (horses.empty()) {
            ImGui::TextDisabled("%s", LocalizationManager::Get("HorsePicker.NoHorses").c_str());
            ImGui::TextDisabled("%s", LocalizationManager::Get("HorsePicker.NoHorsesHint").c_str());
            ImGui::End();
            return !keepOpen;
        }

        ImGui::TextDisabled("%s", LocalizationManager::Get("HorsePicker.Prompt").c_str());
        ImGui::Spacing();

        const std::string unknownName = LocalizationManager::Get("HorsePicker.UnknownHorse");

        const float pad      = 8.0f;
        const float portSz   = 44.0f;
        const float rowH     = portSz + pad * 2.0f;
        ImDrawList* dl       = ImGui::GetWindowDrawList();

        for (const auto& rec : horses) {
            const char* name = rec.entry.lastKnownName.empty()
                ? unknownName.c_str() : rec.entry.lastKnownName.c_str();

            const Unavailable reason = blocked ? Unavailable::None : GetUnavailableReason(rec);
            const bool rowDisabled = blocked || reason != Unavailable::None;

            ImGui::PushID(static_cast<int>(rec.runtimeFormID));
            if (rowDisabled) ImGui::BeginDisabled();

            const ImVec2 rowMin = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(0.0f, rowH))) {
                PartyHorses::HorseRegistry::GetSingleton()->Summon(rec.runtimeFormID);
                requestClose = true;
            }

            const ImVec2 pMin(rowMin.x + pad, rowMin.y + pad);
            const ImVec2 pMax(pMin.x + portSz, pMin.y + portSz);
            const ImU32 imgTint = rowDisabled ? IM_COL32(255, 255, 255, 110) : IM_COL32_WHITE;
            if (auto* portrait = TextureManager::GetPortrait(rec.actorPtr)) {
                dl->AddImageRounded(reinterpret_cast<ImTextureID>(portrait), pMin, pMax,
                                    ImVec2(0, 0), ImVec2(1, 1), imgTint, portSz * 0.5f);
            } else {
                dl->AddCircleFilled(ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f),
                                    portSz * 0.5f, IM_COL32(60, 60, 60, 255));
            }

            const ImVec2 ts = ImGui::CalcTextSize(name);
            ImU32 textCol = ImGui::GetColorU32(rowDisabled ? ImGuiCol_TextDisabled : ImGuiCol_Text);
            dl->AddText(ImVec2(pMax.x + pad, rowMin.y + (rowH - ts.y) * 0.5f), textCol, name);

            if (rowDisabled) ImGui::EndDisabled();

            if (!blocked && (reason == Unavailable::Near || reason == Unavailable::NotOwned) &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", LocalizationManager::Get(
                    reason == Unavailable::Near ? "HorsePicker.NearbyTooltip"
                                                : "HorsePicker.NotOwnedTooltip").c_str());
            }
            ImGui::PopID();
        }

        ImGui::End();
        return requestClose || !keepOpen;
    }

    void Render() {
        if (!g_Visible) return;

        auto* settings = Settings::GetSingleton();
        if (!settings->smEnableHorsePicker) { Hide(); return; }
        if (MenuUtils::ShouldBlockOverlay()) { Hide(); return; }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        auto& io = ImGui::GetIO();

        POINT mousePos;
        if (GetCursorPos(&mousePos)) {
            if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
                HWND hwnd = (HWND)renderer->GetRuntimeData().renderWindows[0].hWnd;
                if (ScreenToClient(hwnd, &mousePos)) {
                    float mx = (std::max)(0.0f, (std::min)(static_cast<float>(mousePos.x), io.DisplaySize.x - 1.0f));
                    float my = (std::max)(0.0f, (std::min)(static_cast<float>(mousePos.y), io.DisplaySize.y - 1.0f));
                    io.AddMousePosEvent(mx, my);
                }
            }
        }
        io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);

        ImGui::NewFrame();
        const bool shouldClose = RenderContent();
        ImGui::EndFrame();
        ImGuiRenderer::DrawSkyrimCursor();
        ImGui::Render();
        ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());

        if (shouldClose) Hide();
    }

}
