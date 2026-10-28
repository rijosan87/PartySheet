#include "../../includes/ImGui/Renderer.h"
#include "../../includes/InputHandler.h"
#include "../../includes/ImGui/FollowerHUD.h"
#include "../../includes/ImGui/PlayerHUD.h"
#include "../../includes/ImGui/PlayerHUDEquipment.h"
#include "../../includes/ImGui/PartySheet.h"
#include "../../includes/ImGui/PartySheetV2.h"
#include "../../includes/ImGui/PartySheetV2Modal.h"
#include "../../includes/ImGui/PartySheetV2InventoryModal.h"
#include "../../includes/ImGui/PartySheetV2StatusModal.h"
#include "../../includes/ImGui/InspectCard.h"
#include "../../includes/ImGui/CharacterSheet.h"
#include "../../includes/ImGui/WidgetStableMount.h"
#include "../../includes/ImGui/WidgetHorsePicker.h"
#include "../../includes/ImGui/WidgetCollectibles.h"
#include "../../includes/ImGui/WidgetPinnedItems.h"
#include "../../includes/ImGui/PlayerHUDResistances.h"
#include "../../includes/ImGui/PlayerHUDResources.h"
#include "../../includes/ImGui/PlayerHUDEffects.h"
#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/ImGui/RendererSections.h"
#include "../../includes/Settings.h"
#include "../../includes/PerfStats.h"
#include "../../includes/TextureManager.h"
#include "../../includes/SpellIconManager.h"
#include "../../includes/BackgroundManager.h"
#include "../../includes/FontManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/HUDVisibility.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/PartySheetStateAPIInternal.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <d3d11.h>
#include <dxgi.h>
#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <functional>

extern std::function<void(uint32_t)> g_hkCaptureCb;

namespace ImGuiRenderer {
    static bool g_Initialized = false;
    static bool g_MenuVisible = false;
    static bool g_ControlsDisabled = false;
    static bool g_GamePausedByUs = false;

    static void PatchImGuiSamplerOnce() {
        static bool s_Patched = false;
        if (s_Patched) return;

        struct ImGui_ImplDX11_Data_Mirror {
            ID3D11Device*             pd3dDevice;
            ID3D11DeviceContext*      pd3dDeviceContext;
            IDXGIFactory*             pFactory;
            ID3D11Buffer*             pVB;
            ID3D11Buffer*             pIB;
            ID3D11VertexShader*       pVertexShader;
            ID3D11InputLayout*        pInputLayout;
            ID3D11Buffer*             pVertexConstantBuffer;
            ID3D11PixelShader*        pPixelShader;
            ID3D11SamplerState*       pFontSampler;
        };

        auto& io = ImGui::GetIO();
        auto* bd = static_cast<ImGui_ImplDX11_Data_Mirror*>(io.BackendRendererUserData);
        if (!bd || !bd->pd3dDevice || !bd->pFontSampler) return;

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MipLODBias     = 0.0f;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MinLOD         = 0.0f;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        ID3D11SamplerState* trilinearSampler = nullptr;
        if (SUCCEEDED(bd->pd3dDevice->CreateSamplerState(&sd, &trilinearSampler))) {
            bd->pFontSampler->Release();
            bd->pFontSampler = trilinearSampler;
            logger::info("ImGui sampler patched: MaxLOD raised to FLT_MAX (trilinear mip sampling enabled)");
        } else {
            logger::warn("Sampler patch failed: CreateSamplerState returned an error");
        }
        s_Patched = true;
    }

    static void ApplyPauseState(bool wantPaused) {
        auto* main = RE::Main::GetSingleton();
        if (!main) return;
        if (wantPaused && !g_GamePausedByUs) {
            if (!main->freezeTime) {
                main->freezeTime = true;
                g_GamePausedByUs = true;
            }
        } else if (!wantPaused && g_GamePausedByUs) {
            main->freezeTime = false;
            g_GamePausedByUs = false;
        }
    }

    static HHOOK g_MessageHook = nullptr;
    static bool  g_CharHandledByMsgHook = false;
    static bool  g_MsgHookFiredThisFrame = false;
    static HWND  g_GameHwnd = nullptr;
    // Advanced Styling popup state is managed in RendererFollowerHUD.cpp
    static float g_SavedZoomOffset = 0.0f;
    static bool g_WasThirdPerson = true;
    static std::chrono::steady_clock::time_point g_LastCSVisibleTime;
    static std::chrono::steady_clock::time_point g_LastPSVisibleTime;

    
    static std::string g_ToastMessage = "";
    static float g_ToastTimer = 0.0f;
    static const float g_ToastDuration = 2.0f;

    static void ShowToast(const std::string& message) {
        g_ToastMessage = message;
        g_ToastTimer = g_ToastDuration;
    }
    
    static void DisablePlayerInput() {
        if (auto pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler) pc->lookHandler->SetInputEventHandlingEnabled(false);
            if (pc->movementHandler) pc->movementHandler->SetInputEventHandlingEnabled(false);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(false);
            logger::debug("Player input disabled (look, movement, attack handlers)");
        }
    }
    
    static void EnablePlayerInput() {
        if (auto pc = RE::PlayerControls::GetSingleton()) {
            if (pc->lookHandler) pc->lookHandler->SetInputEventHandlingEnabled(true);
            if (pc->movementHandler) pc->movementHandler->SetInputEventHandlingEnabled(true);
            if (pc->attackBlockHandler) pc->attackBlockHandler->SetInputEventHandlingEnabled(true);
            logger::debug("Player input enabled (look, movement, attack handlers)");
        }
    }
    
    static void SaveCameraZoom();
    static void RestoreCameraZoom();

    bool MsgHookFiredThisFrame() {
        return g_MsgHookFiredThisFrame;
    }

    static bool IsKeyboardMessage(UINT msg) {
        return msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_KEYUP ||
               msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP;
    }

    static LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && wParam == PM_REMOVE) {
            auto* pMsg = reinterpret_cast<MSG*>(lParam);
            if (pMsg && pMsg->hwnd == g_GameHwnd && g_Initialized) {
                const bool anyVisible = g_MenuVisible;
                const UINT msg = pMsg->message;

                if (anyVisible && IsKeyboardMessage(msg)) {
                    auto& io = ImGui::GetIO();

                    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
                        ImGui_ImplWin32_WndProcHandler(pMsg->hwnd, msg, pMsg->wParam, pMsg->lParam);
                        g_CharHandledByMsgHook = true;
                        g_MsgHookFiredThisFrame = true;
                    } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
                        ImGui_ImplWin32_WndProcHandler(pMsg->hwnd, msg, pMsg->wParam, pMsg->lParam);
                        g_CharHandledByMsgHook = true;
                    }
                }
            }
        }
        return CallNextHookEx(g_MessageHook, nCode, wParam, lParam);
    }
    
    void ImGuiRenderer::RenderBackgroundPicker(const char* popupId, int& currentIndex, BackgroundManager::Type type) {
        std::string bgLabel = "Image " + std::to_string(currentIndex);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        
        if (ImGui::Button((bgLabel + "##Btn" + popupId).c_str())) {
            ImGui::OpenPopup(popupId);
        }
        ImGui::PopStyleColor(2);
          
        if (ImGui::BeginPopup(popupId)) {
            ImGui::Text("Select Background");
            ImGui::Separator();
            
            if (ImGui::BeginChild("BGGrid", ImVec2(480, 350), false)) {
                auto entries = BackgroundManager::GetBackgroundEntries(type);
                if (entries.empty()) {
                    ImGui::TextDisabled("No images found.");
                } else {
                    int cols = 3;
                    float width = 120.0f;
                    float height = 67.5f;
                    
                    if (type == BackgroundManager::Type::Partycard || type == BackgroundManager::Type::Inspectcard || type == BackgroundManager::Type::CharacterSheet) {
                        cols = 4;
                        width = 90.0f;
                        height = 160.0f;
                    } else if (type == BackgroundManager::Type::BarStyle) {
                        cols = 2;
                        width = 200.0f;
                        height = 15.0f; 
                    }
                    
                    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
                        if (i % cols != 0) ImGui::SameLine();
                        
                        ImGui::PushID(i);
                        if (type == BackgroundManager::Type::BarStyle) {
                            if (ImGui::ImageButton("##bg_btn", (ImTextureID)entries[i].texture, ImVec2(width, height), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), ImVec4(1,1,1,1))) {
                                currentIndex = i + 1;
                                ImGui::CloseCurrentPopup();
                            }
                        } else {
                            if (ImGui::ImageButton("##bg_btn", (ImTextureID)entries[i].texture, ImVec2(width, height))) {
                                currentIndex = i + 1;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s", entries[i].filename.c_str());
                        }
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

    static float g_MouseWheelDelta = 0.0f;
    
    struct WndProc {
        static LRESULT thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
            
            
            if (uMsg == WM_INPUT && g_Initialized && (g_MenuVisible || PartySheetV2::IsEffectsModalOpen() || PartySheetV2::IsInventoryModalOpen() || PartySheetV2::IsStatusModalOpen())) {
                UINT dataSize = 0;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dataSize, sizeof(RAWINPUTHEADER));
                
                if (dataSize > 0 && dataSize <= 256) {
                    BYTE buffer[256];
                    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &dataSize, sizeof(RAWINPUTHEADER)) == dataSize) {
                        RAWINPUT* raw = (RAWINPUT*)buffer;
                        
                        if (raw->header.dwType == RIM_TYPEMOUSE) {
                            if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                                short wheelDelta = (short)raw->data.mouse.usButtonData;
                                g_MouseWheelDelta += static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
                                logger::debug("RawInput: Mouse wheel! delta={}, accumulated={}", wheelDelta, g_MouseWheelDelta);
                            }
                            if (raw->data.mouse.usButtonFlags & RI_MOUSE_HWHEEL) {
                                short wheelDelta = (short)raw->data.mouse.usButtonData;
                                logger::debug("RawInput: Horizontal wheel! delta={}", wheelDelta);
                            }
                        }
                    }
                }
            }
            
            
            if (g_Initialized && (g_MenuVisible || PartySheetV2::IsEffectsModalOpen() || PartySheetV2::IsInventoryModalOpen() || PartySheetV2::IsStatusModalOpen())) {
                if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL) {
                    short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                    g_MouseWheelDelta += static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
                    logger::info("WndProc: Scroll captured! delta={}, accumulated={}", wheelDelta, g_MouseWheelDelta);
                    return 1;
                }
                
                if (g_CharHandledByMsgHook && IsKeyboardMessage(uMsg)) {
                    g_CharHandledByMsgHook = false;
                    return 1;
                }
                if (uMsg == WM_CHAR) {
                    return 1;
                }

                if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
                    return 1;
                }
                
                auto& io = ImGui::GetIO();
                if (io.WantCaptureMouse) {
                    switch (uMsg) {
                        case WM_LBUTTONDOWN:
                        case WM_LBUTTONUP:
                        case WM_LBUTTONDBLCLK:
                        case WM_RBUTTONDOWN:
                        case WM_RBUTTONUP:
                        case WM_RBUTTONDBLCLK:
                        case WM_MBUTTONDOWN:
                        case WM_MBUTTONUP:
                        case WM_MBUTTONDBLCLK:
                            return 1;
                    }
                }
            }
            
            if (g_Initialized && uMsg == WM_KILLFOCUS) {
                ImGui::GetIO().ClearInputKeys();
            }
            

            return func(hWnd, uMsg, wParam, lParam);
        }
        static inline WNDPROC func;
    };
    
    static void RegisterRawInputMouse(HWND hwnd) {
        RAWINPUTDEVICE rid;
        rid.usUsagePage = 0x01;
        rid.usUsage = 0x02;
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;
        
        if (RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
            logger::info("Raw Input mouse registered successfully for scroll wheel capture");
        } else {
            logger::warn("Failed to register Raw Input mouse: {}", GetLastError());
        }
    }

    struct CreateD3DAndSwapChain {
        static void thunk() {
            func();

            if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
                const auto swapChain = (IDXGISwapChain*)renderer->GetRuntimeData().renderWindows[0].swapChain;
                if (!swapChain) {
                    logger::error("Couldn't find swapChain");
                    return;
                }

                DXGI_SWAP_CHAIN_DESC desc{};
                if (FAILED(swapChain->GetDesc(std::addressof(desc)))) {
                    logger::error("IDXGISwapChain::GetDesc failed");
                    return;
                }
                g_GameHwnd = desc.OutputWindow;

                const auto device = (ID3D11Device*)renderer->GetRuntimeData().forwarder;
                const auto context = (ID3D11DeviceContext*)renderer->GetRuntimeData().context;

                logger::info("Initializing ImGui...");

                ImGui::CreateContext();

                auto& io = ImGui::GetIO();
                io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
                io.IniFilename = nullptr;

                if (!ImGui_ImplWin32_Init(desc.OutputWindow)) {
                    logger::error("ImGui initialization failed (Win32)");
                    return;
                }
                if (!ImGui_ImplDX11_Init(device, context)) {
                    logger::error("ImGui initialization failed (DX11)");
                    return;
                }

                FontManager::GetSingleton()->LoadFonts(io);
                logger::info("Fonts loaded via FontManager");

                logger::info("Font atlas built");

                logger::info("ImGui initialized successfully");
                g_Initialized = true;
                
                TextureManager::Init(device);
                
                SpellIconManager::Init(device);
                
                BackgroundManager::Init();

                WndProc::func = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrA(desc.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc::thunk)));
                if (!WndProc::func) {
                    logger::error("SetWindowLongPtrA failed!");
                }
            }
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };

    // Capture and log render target / client / display metrics when debug verbose logging is enabled.
    static void LogRenderSpace(
        const char* result,
        const D3D11_TEXTURE2D_DESC* boundDesc,
        const ImVec2* computedScale,
        ID3D11DeviceContext* context,
        RE::BSGraphics::Renderer* renderer,
        ImDrawData* drawData)
    {
        auto* settings = Settings::GetSingleton();
        if (!settings || !settings->debugEnableVerboseLogging) return;

        static auto s_lastLogTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - s_lastLogTime).count() < 2.0f) {
            return;
        }
        s_lastLogTime = now;

        std::string boundStr = boundDesc ? std::format("{}x{}", boundDesc->Width, boundDesc->Height) : "unavailable";

        std::string backbufferStr = "unavailable";
        if (renderer) {
            auto* swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows[0].swapChain);
            if (swapChain) {
                ID3D11Texture2D* backTex = nullptr;
                if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backTex))) && backTex) {
                    D3D11_TEXTURE2D_DESC backDesc{};
                    backTex->GetDesc(&backDesc);
                    backTex->Release();
                    backbufferStr = std::format("{}x{}", backDesc.Width, backDesc.Height);
                }
            }
        }

        std::string clientStr = "unavailable";
        HWND hwnd = g_GameHwnd;
        if (!hwnd && renderer) {
            hwnd = reinterpret_cast<HWND>(renderer->GetRuntimeData().renderWindows[0].hWnd);
        }
        if (hwnd) {
            RECT clientRect{};
            if (GetClientRect(hwnd, &clientRect)) {
                clientStr = std::format("{}x{}", clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            }
        }

        std::string displayPosStr = "unavailable";
        std::string displaySizeStr = "unavailable";
        if (drawData) {
            displayPosStr = std::format("({}, {})", drawData->DisplayPos.x, drawData->DisplayPos.y);
            displaySizeStr = std::format("{}x{}", drawData->DisplaySize.x, drawData->DisplaySize.y);
        }
        const auto& io = ImGui::GetIO();
        std::string fbScaleStr = std::format("({}, {})", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

        std::string viewportsStr = "unavailable";
        if (context) {
            D3D11_VIEWPORT ports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
            UINT count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            context->RSGetViewports(&count, ports);
            viewportsStr = std::format("count={}", count);
            for (UINT i = 0; i < count; ++i) {
                viewportsStr += std::format(" [vp{} origin=({}, {}) size={}x{}]",
                    i, ports[i].TopLeftX, ports[i].TopLeftY, ports[i].Width, ports[i].Height);
            }
        }

        std::string scaleStr = computedScale ? std::format("({}, {})", computedScale->x, computedScale->y) : "unavailable";

        logger::info("[RenderSpace] result={} bound={} backbuffer={} client={} displayPos={} displaySize={} framebufferScale={} viewports={} scale={}",
            result, boundStr, backbufferStr, clientStr, displayPosStr, displaySizeStr, fbScaleStr, viewportsStr, scaleStr);
    }

    void RenderToBoundTarget(ImDrawData* drawData) {
        if (!drawData || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f) {
            return;
        }

        const auto renderDirect = [drawData]() {
            ImGui_ImplDX11_RenderDrawData(drawData);
        };

        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderer) {
            LogRenderSpace("fallback-query", nullptr, nullptr, nullptr, nullptr, drawData);
            renderDirect();
            return;
        }

        auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
        if (!context) {
            LogRenderSpace("fallback-query", nullptr, nullptr, nullptr, renderer, drawData);
            renderDirect();
            return;
        }

        ID3D11RenderTargetView* rtv = nullptr;
        context->OMGetRenderTargets(1, &rtv, nullptr);
        if (!rtv) {
            LogRenderSpace("fallback-query", nullptr, nullptr, context, renderer, drawData);
            renderDirect();
            return;
        }

        ID3D11Resource* resource = nullptr;
        rtv->GetResource(&resource);
        rtv->Release();
        if (!resource) {
            LogRenderSpace("fallback-query", nullptr, nullptr, context, renderer, drawData);
            renderDirect();
            return;
        }

        ID3D11Texture2D* texture = nullptr;
        const HRESULT hr = resource->QueryInterface(
            __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
        resource->Release();
        if (FAILED(hr) || !texture) {
            LogRenderSpace("fallback-query", nullptr, nullptr, context, renderer, drawData);
            renderDirect();
            return;
        }

        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        texture->Release();
        if (!desc.Width || !desc.Height) {
            LogRenderSpace("fallback-query", nullptr, nullptr, context, renderer, drawData);
            renderDirect();
            return;
        }

        const ImVec2 scale(
            static_cast<float>(desc.Width) / drawData->DisplaySize.x,
            static_cast<float>(desc.Height) / drawData->DisplaySize.y);

        if (!std::isfinite(scale.x) || !std::isfinite(scale.y) ||
            scale.x <= 0.0f || scale.y <= 0.0f) {
            LogRenderSpace("fallback-query", &desc, &scale, context, renderer, drawData);
            renderDirect();
            return;
        }

        if (scale.x == 1.0f && scale.y == 1.0f) {
            LogRenderSpace("direct-1to1", &desc, &scale, context, renderer, drawData);
            renderDirect();
            return;
        }

        const float largestScale = (std::max)(scale.x, scale.y);
        if (std::abs(scale.x - scale.y) > largestScale * 0.01f) {
            LogRenderSpace("fallback-nonproportional", &desc, &scale, context, renderer, drawData);
            renderDirect();
            return;
        }

        // Keep clients above the 1080p baseline on the pre-v3.4 direct path.
        if (drawData->DisplaySize.y > 1080.0f) {
            LogRenderSpace("direct-high-client", &desc, &scale, context, renderer, drawData);
            renderDirect();
            return;
        }

        LogRenderSpace("scaled", &desc, &scale, context, renderer, drawData);

        const ImVec2 originalDisplayPos = drawData->DisplayPos;
        const ImVec2 originalDisplaySize = drawData->DisplaySize;

        const auto scaleDrawData = [drawData](const ImVec2 factor) {
            for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
                for (auto& vertex : drawData->CmdLists[listIndex]->VtxBuffer) {
                    vertex.pos.x *= factor.x;
                    vertex.pos.y *= factor.y;
                }
            }
            drawData->ScaleClipRects(factor);
        };

        scaleDrawData(scale);
        drawData->DisplayPos = ImVec2(
            originalDisplayPos.x * scale.x,
            originalDisplayPos.y * scale.y);
        drawData->DisplaySize = ImVec2(
            static_cast<float>(desc.Width),
            static_cast<float>(desc.Height));

        ImGui_ImplDX11_RenderDrawData(drawData);

        scaleDrawData(ImVec2(1.0f / scale.x, 1.0f / scale.y));
        drawData->DisplayPos = originalDisplayPos;
        drawData->DisplaySize = originalDisplaySize;
    }

    struct PostDisplay {
        static void thunk(RE::IMenu* a_menu) {
            if (!a_menu) {
                logger::warn("PostDisplay: Invalid menu pointer - skipping frame");
                return;
            }

            PerfStats::BeginFrame();

            if (g_Initialized) PatchImGuiSamplerOnce();

            {
                static auto s_lastTime = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - s_lastTime).count();
                dt = std::clamp(dt, 0.001f, 0.1f);
                s_lastTime = now;
                HUDVisibility::Update(dt);
                UIAnimation::AnimationManager::Get().BeginFrame();
                UIAnimation::AnimationManager::Get().Update(dt);
            }

            PartySheetAPI::PublishIfChanged();

            if (!g_Initialized) {
                if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
                    const auto swapChain = (IDXGISwapChain*)renderer->GetRuntimeData().renderWindows[0].swapChain;
                    if (swapChain) {
                        DXGI_SWAP_CHAIN_DESC desc{};
                        if (SUCCEEDED(swapChain->GetDesc(std::addressof(desc)))) {
                            const auto device = (ID3D11Device*)renderer->GetRuntimeData().forwarder;
                            const auto context = (ID3D11DeviceContext*)renderer->GetRuntimeData().context;

                            ImGui::CreateContext();
                            auto& io = ImGui::GetIO();
                            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NavEnableKeyboard;
                            io.IniFilename = nullptr;

                            if (ImGui_ImplWin32_Init(desc.OutputWindow) && ImGui_ImplDX11_Init(device, context)) {
                                FontManager::GetSingleton()->LoadFonts(io);

                                logger::info("ImGui initialized successfully");
                                g_Initialized = true;

                                HUDVisibility::ApplyVanillaBarVisibility();

                                TextureManager::Init(device);
                                
                                SpellIconManager::Init(device);
                                
                                BackgroundManager::Init();
                                
                                ImGuiStyle& style = ImGui::GetStyle();
                                
                                style.WindowRounding = 0.0f;
                                style.FrameRounding = 4.0f;
                                style.GrabRounding = 12.0f;
                                style.PopupRounding = 4.0f;
                                style.ScrollbarRounding = 6.0f;
                                style.TabRounding = 4.0f;
                                style.WindowBorderSize = 0.0f;
                                
                                style.FramePadding = ImVec2(8, 4);
                                style.ItemSpacing = ImVec2(8, 6);
                                style.ScrollbarSize = 12.0f;
                                style.GrabMinSize = 15.0f;
                                
                                ImVec4* colors = style.Colors;
                                colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.13f, 0.96f);
                                colors[ImGuiCol_Border] = ImVec4(0.20f, 0.28f, 0.35f, 0.40f);
                                colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
                                colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.28f, 0.34f, 1.00f);
                                colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.36f, 0.44f, 1.00f);
                                colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.05f, 0.08f, 1.00f);
                                colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.08f, 0.12f, 1.00f);
                                colors[ImGuiCol_Header] = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
                                colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.34f, 0.42f, 1.00f);
                                colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.42f, 0.52f, 1.00f);
                                colors[ImGuiCol_Button] = ImVec4(0.24f, 0.30f, 0.38f, 1.00f);
                                colors[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.40f, 0.50f, 1.00f);
                                colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.50f, 0.62f, 1.00f);
                                colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.60f, 0.75f, 1.00f);
                                colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.70f, 0.85f, 1.00f);
                                colors[ImGuiCol_CheckMark] = ImVec4(0.55f, 0.70f, 0.90f, 1.00f);
                                colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.35f, 0.42f, 0.50f);
                                colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
                                colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);

                                WndProc::func = reinterpret_cast<WNDPROC>(
                                    SetWindowLongPtrA(desc.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc::thunk)));
                                if (!WndProc::func) {
                                    logger::error("SetWindowLongPtrA failed!");
                                } else {
                                    RegisterRawInputMouse(desc.OutputWindow);

                                    g_GameHwnd = desc.OutputWindow;
                                    DWORD threadId = GetWindowThreadProcessId(desc.OutputWindow, nullptr);
                                    g_MessageHook = SetWindowsHookExA(WH_GETMESSAGE, GetMsgProc, nullptr, threadId);
                                    if (g_MessageHook) {
                                        logger::info("WH_GETMESSAGE hook installed - primary keyboard input path active");
                                    } else {
                                        logger::warn("WH_GETMESSAGE hook failed (GetLastError={}) - falling back to kButton path", GetLastError());
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (g_Initialized && g_MenuVisible) {
                if (!ImGui::GetCurrentContext()) {
                    logger::warn("PostDisplay: ImGui context is null - skipping frame");
                    func(a_menu);
                    return;
                }
                
                if (MenuUtils::ShouldBlockOverlay() && !g_hkCaptureCb) {
                    g_MenuVisible = false;
                    g_ControlsDisabled = false;
                    EnablePlayerInput();
                    ApplyPauseState(false);
                    auto& ioCheck = ImGui::GetIO();
                    ioCheck.MouseDrawCursor = false;
                    logger::debug("ImGui: Menu auto-closed (blocking menu opened)");
                    func(a_menu);
                    return;
                }
                
                if (!g_ControlsDisabled) {
                    g_ControlsDisabled = true;
                    DisablePlayerInput();
                    logger::debug("ImGui: Menu opened - player input disabled");
                }

                ApplyPauseState(Settings::GetSingleton()->pauseGameInSettings);

                g_MsgHookFiredThisFrame = false;
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                
                auto& io = ImGui::GetIO();
                POINT mousePos;
                if (GetCursorPos(&mousePos)) {
                    if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
                        HWND hwnd = (HWND)renderer->GetRuntimeData().renderWindows[0].hWnd;
                        if (ScreenToClient(hwnd, &mousePos)) {
                            float mx = static_cast<float>(mousePos.x);
                            float my = static_cast<float>(mousePos.y);
                            mx = (std::max)(0.0f, (std::min)(mx, io.DisplaySize.x - 1.0f));
                            my = (std::max)(0.0f, (std::min)(my, io.DisplaySize.y - 1.0f));
                            io.AddMousePosEvent(mx, my);
                        }
                    }
                }
                
                io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
                io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
                io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
                
                if (g_MouseWheelDelta != 0.0f) {
                    io.AddMouseWheelEvent(0.0f, g_MouseWheelDelta);
                    g_MouseWheelDelta = 0.0f;
                }
                
                io.MouseDrawCursor = false;
                
                auto settings = Settings::GetSingleton();
                
                ImGui::NewFrame();
                ImGuiRenderer::g_csFocusRow = ImGuiRenderer::CSFocusRow::None;

                const float menuScale = std::clamp(static_cast<float>(settings->menuScale) / 100.0f, 0.5f, 2.0f);
                // Full geometry scaling makes the settings text feel oversized above 100%.
                const float menuFontScale = 1.0f + (menuScale - 1.0f) * 0.5f;

                // menuWidth/menuHeight stay in 100% logical pixels. Applying the scale only
                const float rawW = static_cast<float>(settings->menuWidth) * menuScale;
                const float rawH = static_cast<float>(settings->menuHeight) * menuScale;
                const float rawX = static_cast<float>(settings->menuPosX);
                const float rawY = static_cast<float>(settings->menuPosY);

                const float windowW = (std::min)(rawW, io.DisplaySize.x * 0.95f);
                const float windowH = (std::min)(rawH, io.DisplaySize.y * 0.95f);
                const float windowX = std::clamp(rawX, 0.0f, (std::max)(0.0f, io.DisplaySize.x - windowW));
                const float windowY = std::clamp(rawY, 0.0f, (std::max)(0.0f, io.DisplaySize.y - windowH));

                const bool isSettingsClamped = (windowW != rawW || windowH != rawH || windowX != rawX || windowY != rawY);
                static int lastSettingsClamp = -1;
                if (lastSettingsClamp != static_cast<int>(isSettingsClamped)) {
                    logger::debug("[SettingsWindow] clamp={} requested=({}, {}) {}x{} fitted=({}, {}) {}x{} display={}x{} scale={}",
                        isSettingsClamped, rawX, rawY, rawW, rawH, windowX, windowY, windowW, windowH,
                        io.DisplaySize.x, io.DisplaySize.y, settings->menuScale);
                    lastSettingsClamp = static_cast<int>(isSettingsClamped);
                }
                static int appliedMenuScale = -1;
                const bool menuScaleChanged = appliedMenuScale != settings->menuScale;
                static ImVec4 lastRequestedBounds(-1, -1, -1, -1);
                static ImVec2 lastSettingsDisplay(-1, -1);
                const bool boundsChanged = lastRequestedBounds.x != rawX || lastRequestedBounds.y != rawY
                    || lastRequestedBounds.z != rawW || lastRequestedBounds.w != rawH
                    || lastSettingsDisplay.x != io.DisplaySize.x || lastSettingsDisplay.y != io.DisplaySize.y;
                // Fit once when the inputs change. Reapplying every frame cancels dragging and resizing.
                const bool fitSettingsWindow = menuScaleChanged || (isSettingsClamped && boundsChanged);
                const ImGuiCond cond = fitSettingsWindow ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
                lastRequestedBounds = ImVec4(rawX, rawY, rawW, rawH);
                lastSettingsDisplay = io.DisplaySize;
                if (fitSettingsWindow)
                    logger::debug("[SettingsWindow] Applying fitted bounds once; clamp={}", isSettingsClamped);
                appliedMenuScale = settings->menuScale;

                // Scale ImGui's shared spacing only for this window, then restore it before
                const ImGuiStyle unscaledStyle = ImGui::GetStyle();
                ImGui::GetStyle().ScaleAllSizes(menuScale);

                ImGui::SetNextWindowPos(ImVec2(windowX, windowY), cond);
                ImGui::SetNextWindowSize(ImVec2(windowW, windowH), cond);
                ImGui::Begin("HUD Settings", &g_MenuVisible,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::SetWindowFontScale(menuFontScale);
                
                // Reserve space for fixed footer (Save button + hint text)
                float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2 + 8.0f * menuScale;
                
                // Scrollable content region (everything except footer)
                ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15.0f * menuScale);
                ImGui::BeginChild("SettingsContent", ImVec2(0, -footerHeight), false);
                
                SettingsUI::RenderHUDVisibilitySection(settings);

                // ========== SCALING ==========
                SettingsUI::RenderScalingSection(settings);

                SettingsUI::RenderInspectCardSection(settings, io.DisplaySize);
                
                SettingsUI::RenderFollowerHUDSection(settings, io.DisplaySize);

                SettingsUI::RenderPlayerHUDSection(settings, io.DisplaySize);
                
                SettingsUI::RenderPartySheetSection(settings);
                
                SettingsUI::RenderCharacterSheetSection(settings);
                
                SettingsUI::RenderHotkeysSection(settings);
                
                WidgetStableMount::RenderSettingsPanel(settings, io.DisplaySize);

                
                
                ImGui::EndChild();
                ImGui::PopStyleVar();
                
                ImGui::Separator();
                
                {
                    bool pauseFlag = settings->pauseGameInSettings;
                    if (ImGui::Checkbox("Pause Game While Settings Open", &pauseFlag)) {
                        settings->pauseGameInSettings = pauseFlag;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Freezes game time while this menu is open.\nApplies immediately. Click Save Settings to persist.");
                }

                if (ImGui::Button("Save Settings", ImVec2(-1, 0))) {
                    settings->Save();
                    ShowToast("Settings Saved");
                }
                ImGui::TextDisabled("Press %s to close | Changes require Save", FormatHotkeyWithModifier(settings->menuSettingsKey, settings->menuSettingsModifier).c_str());

                // Preserve automatic fitting, but allow deliberate movement/resizing to be saved.
                const ImVec2 currentPos = ImGui::GetWindowPos();
                const ImVec2 currentSize = ImGui::GetWindowSize();
                const bool settingsWindowAdjusted = std::fabs(currentPos.x - windowX) > 1.0f
                    || std::fabs(currentPos.y - windowY) > 1.0f
                    || std::fabs(currentSize.x - windowW) > 1.0f || std::fabs(currentSize.y - windowH) > 1.0f;
                if (!isSettingsClamped || (!fitSettingsWindow && settingsWindowAdjusted)) {
                    settings->menuPosX = static_cast<int>(currentPos.x);
                    settings->menuPosY = static_cast<int>(currentPos.y);
                    settings->menuWidth = static_cast<int>(currentSize.x / menuScale + 0.5f);
                    settings->menuHeight = static_cast<int>(currentSize.y / menuScale + 0.5f);
                }

                ImGui::End();
                ImGui::GetStyle() = unscaledStyle;
                
                if (settings->followerHudMode != 0) {
                    FollowerHUD::RenderContent(true);
                }
                if (settings->enablePlayerHud && settings->enablePlayerStatusWidget) {
                    PlayerHUD::RenderContent(true);
                }

                if (InspectCard::IsVisible()) {
                    InspectCard::RenderContent();
                }

                WidgetStableMount::RenderPreview();
                WidgetCollectibles::RenderPreview();
                WidgetPinnedItems::RenderPreview();
                if (settings->enablePlayerHud && settings->enablePlayerEquipWidget)         PlayerHUDEquipment::RenderContent(true);
                if (settings->enablePlayerHud && settings->enablePlayerResistWidget)        PlayerHUDResistances::RenderPreview();
                if (settings->enablePlayerHud && settings->enablePlayerResourcesWidget)     PlayerHUDResources::RenderPreview();
                if (settings->enablePlayerHud && settings->enablePlayerEffectWidget)        PlayerHUDEffects::RenderPreview();
                if (settings->enablePlayerHud && settings->enablePlayerSurvivalWidget)      PlayerHUDSurvival::RenderPreview();
                if (settings->enablePlayerHud && settings->enablePlayerDiseaseWidget)       PlayerHUDDisease::RenderPreview();

                if (Settings::GetSingleton()->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible()) {
                    CharacterSheet::GetSingleton()->DrawUI();
                }

                if (g_ToastTimer > 0.0f) {
                    float deltaTime = io.DeltaTime;
                    g_ToastTimer -= deltaTime;
                    
                    float alpha = 1.0f;
                    if (g_ToastTimer < 0.5f) {
                        alpha = g_ToastTimer / 0.5f;
                    }
                    
                    ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f), ImGuiCond_Always);
                    ImGui::SetNextWindowBgAlpha(0.8f * alpha);
                    
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.5f * alpha));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f * alpha));
                    
                    if (ImGui::Begin("##Toast", nullptr, 
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove)) 
                    {
                        ImGui::TextUnformatted(g_ToastMessage.c_str());
                    }
                    ImGui::End();
                    
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar(2);
                }

                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
                    g_MenuVisible = false;
                    g_ControlsDisabled = false;
                    EnablePlayerInput();
                    ApplyPauseState(false);
                    logger::debug("ImGui: Menu closed via Gamepad B");
                    func(a_menu);
                    return;
                }

                ImGui::EndFrame();
                DrawSkyrimCursor();
                ImGui::Render();
                RenderToBoundTarget(ImGui::GetDrawData());
                if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
            } else if (g_ControlsDisabled) {
                g_ControlsDisabled = false;
                if (!PartySheet::IsVisible() && !PartySheetV2::IsVisible()) {
                    EnablePlayerInput();
                    logger::debug("ImGui: Menu closed - player input enabled");
                } else {
                    logger::debug("ImGui: Menu closed but Party Sheet open - input stays disabled");
                }
            }
            
            {
                static bool s_PrevMenuVisible = false;
                if (s_PrevMenuVisible && !g_MenuVisible) {
                    ApplyPauseState(false);
                    if (g_ControlsDisabled && !PartySheet::IsVisible() && !PartySheetV2::IsVisible()) {
                        g_ControlsDisabled = false;
                        EnablePlayerInput();
                    }
                    auto& ioX = ImGui::GetIO();
                    ioX.MouseDrawCursor = false;
                    ioX.ClearInputKeys();
                    logger::debug("ImGui: Menu closed via [X] button - state synced");
                }
                s_PrevMenuVisible = g_MenuVisible;
            }

            if (!g_MenuVisible && g_Initialized) {
                
                
                if (PartySheet::IsVisible()) {
                    g_LastPSVisibleTime = std::chrono::steady_clock::now();
                    PartySheet::Render();
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                }
                else if (PartySheetV2::IsVisible()) {
                    PerfStats::BeginPSV2Frame();
                    PartySheetV2::Render();
                    PerfStats::EndPSV2Frame();
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                }
                else if (WidgetHorsePicker::IsVisible()) {
                    WidgetHorsePicker::Render();
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                }
                else if (Settings::GetSingleton()->enableCharacterSheet && CharacterSheet::GetSingleton()->IsVisible()) {
                    g_LastCSVisibleTime = std::chrono::steady_clock::now();
                    if (Settings::GetSingleton()->debugEnableCSLogging)
                        logger::info("[DEBUG-CS] Renderer: CS branch reached, calling Render()");
                    CharacterSheet::GetSingleton()->Render();
                    if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                }
                else if (!PartySheet::IsVisible() && !PartySheetV2::IsVisible()) {
                    static bool s_lastCSVisible = false;
                    bool csVisible = CharacterSheet::GetSingleton()->IsVisible();
                    if (csVisible != s_lastCSVisible) {
                        s_lastCSVisible = csVisible;
                        if (Settings::GetSingleton()->debugEnableCSLogging)
                            logger::info("[DEBUG-CS] Renderer: CS branch SKIPPED — enableCS={} csVisible={} psVisible={}",
                                Settings::GetSingleton()->enableCharacterSheet, csVisible, PartySheet::IsVisible());
                    }
                    auto& io = ImGui::GetIO();
                    io.MouseDrawCursor = false;

                    if (!Settings::GetSingleton()->hudOverrideHidden) {
                        PerfStats::BeginFHFrame();
                        FollowerHUD::Render();
                        PerfStats::EndFHFrame();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        PerfStats::BeginPHFrame();
                        PlayerHUD::Render();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        PerfStats::EndPHFrame();

                        PerfStats::BeginPHEquip();
                        PlayerHUDEquipment::Render();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        PerfStats::EndPHEquip();

                        if (InspectCard::IsVisible()) {
                            InspectCard::Render();
                            if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        }

                        PerfStats::BeginSMFrame();
                        WidgetStableMount::RenderHUD();
                        PerfStats::EndSMFrame();
                        PerfStats::BeginCCFrame();
                        WidgetCollectibles::RenderHUD();
                        PerfStats::EndCCFrame();
                        PerfStats::BeginPTFrame();
                        WidgetPinnedItems::RenderHUD();
                        PerfStats::EndPTFrame();

                        PerfStats::BeginPHResist();
                        PlayerHUDResistances::RenderHUD();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        PerfStats::EndPHResist();

                        PerfStats::BeginPHResource();
                        PlayerHUDResources::RenderHUD();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        PerfStats::EndPHResource();

                        PerfStats::BeginPHEffects();
                        PlayerHUDEffects::RenderHUD();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                        PerfStats::EndPHEffects();

                        PerfStats::BeginPHSurvival();
                        PlayerHUDSurvival::RenderHUD();
                        PerfStats::EndPHSurvival();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;

                        PlayerHUDDisease::RenderHUD();
                        if (PerfStats::g_Enabled) ++PerfStats::g_Current.imguiRenderPasses;
                    }
                }
            }

            PerfStats::EndFrame();

            if (g_Initialized && Settings::GetSingleton()->debugEnablePerformanceStats) {
                auto& s = PerfStats::g_Last;
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.75f);
                ImGuiWindowFlags overlayFlags =
                    ImGuiWindowFlags_NoTitleBar      |
                    ImGuiWindowFlags_NoResize        |
                    ImGuiWindowFlags_AlwaysAutoResize|
                    ImGuiWindowFlags_NoMove          |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoInputs        |
                    ImGuiWindowFlags_NoNav;
                ImGui::Begin("##perfstats", nullptr, overlayFlags);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "[PartySheet Perf]");
                ImGui::Separator();
                ImGui::Text("FPS:                %.1f  (%.2f ms)", s.fps, s.frameMs);
                ImGui::Text("ImGui passes/frame: %d", s.imguiRenderPasses);
                ImGui::Text("Summon refreshes:   %d", s.summonRefreshes);
                ImGui::Text("StableMount resolves:%d", s.stableMountResolves);
                ImGui::Text("Ally stats queries: %d", s.allyStatsQueries);
                ImGui::Text("Ally count:         %d", s.allyCount);
                if (s.psFrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[Party Sheet]");
                    ImGui::Text("PS frame time:      %.2f ms", s.psFrameMs);
                    ImGui::Text("PS actor queries:   %d", s.psActorStatQueries);
                }
                if (s.psV2FrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "[Party Sheet V2]");
                    ImGui::Text("Frame time:         %.2f ms", s.psV2FrameMs);
                }
                if (s.icFrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "[Inspect Card]");
                    ImGui::Text("IC frame time:      %.2f ms", s.icFrameMs);
                    ImGui::Text("IC stat queries:    %d", s.icStatQueries);
                }
                if (s.fhFrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "[Follower HUD]");
                    ImGui::Text("Frame time:         %.2f ms", s.fhFrameMs);
                }
                if (s.smFrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.4f, 1.0f), "[Stable Mount]");
                    ImGui::Text("Frame time:         %.2f ms", s.smFrameMs);
                }
                if (s.phFrameMs > 0.0f || s.phEquipMs > 0.0f || s.phResistMs > 0.0f || s.phResourceMs > 0.0f || s.phEffectsMs > 0.0f || s.phSurvivalMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.9f, 1.0f), "[Player HUD]");
                    if (s.phFrameMs   > 0.0f) ImGui::Text("  Bars/Portrait:    %.2f ms", s.phFrameMs);
                    if (s.phEquipMs   > 0.0f) ImGui::Text("  Equipment:        %.2f ms", s.phEquipMs);
                    if (s.phResistMs  > 0.0f) ImGui::Text("  Resistances:      %.2f ms", s.phResistMs);
                    if (s.phResourceMs> 0.0f) ImGui::Text("  Resources:        %.2f ms", s.phResourceMs);
                    if (s.phEffectsMs > 0.0f) ImGui::Text("  Effects:          %.2f ms", s.phEffectsMs);
                    if (s.phSurvivalMs> 0.0f) ImGui::Text("  Survival:         %.2f ms", s.phSurvivalMs);
                    float phTotal = s.phFrameMs + s.phEquipMs + s.phResistMs + s.phResourceMs + s.phEffectsMs + s.phSurvivalMs;
                    ImGui::Text("  Total:            %.2f ms", phTotal);
                }
                if (s.ccFrameMs > 0.0f || s.ptFrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.4f, 1.0f), "[Widgets]");
                    if (s.ccFrameMs > 0.0f) ImGui::Text("Collectibles:       %.2f ms", s.ccFrameMs);
                    if (s.ptFrameMs > 0.0f) ImGui::Text("Pinned Items:       %.2f ms", s.ptFrameMs);
                }
                if (s.csFrameMs > 0.0f) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[Character Sheet]");
                    ImGui::Text("CS frame time:      %.2f ms", s.csFrameMs);
                    ImGui::Text("CS draw cmds:       %d", s.csDrawCalls);
                    ImGui::Text("CS shadow passes:   %d", s.csShadowDrawCalls);
                    ImGui::Text("Equipment queries:  %d", s.csEquipmentQueries);
                    ImGui::Text("Stats queries:      %d", s.csStatsQueries);
                    ImGui::Text("Resist queries:     %d", s.csResistQueries);
                }
                if (!PerfStats::g_ZoneMicrosLast.empty()) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "[Zones]");
                    for (auto& [zname, us] : PerfStats::g_ZoneMicrosLast)
                        ImGui::Text("%-18s %.2f ms", zname, us / 1000.0);
                }
                {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "[Textures]");
                    auto tc = TextureManager::GetCacheStats();
                    ImGui::Text("Cached:             %d", tc.totalCached);
                    ImGui::Text("Loaded OK:          %d", tc.successfulLoads);
                    ImGui::Text("Failed:             %d", tc.failedLoads);
                    ImGui::Text("VRAM (textures):    %.1f MB", tc.totalVramBytes / (1024.0 * 1024.0));
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "[Backgrounds]");
                    ImGui::Text("PartySheet:         %d", BackgroundManager::GetBackgroundCount(BackgroundManager::Type::Partysheet));
                    ImGui::Text("PartyCard:          %d", BackgroundManager::GetBackgroundCount(BackgroundManager::Type::Partycard));
                    ImGui::Text("InspectCard:        %d", BackgroundManager::GetBackgroundCount(BackgroundManager::Type::Inspectcard));
                    ImGui::Text("CharacterSheet:     %d", BackgroundManager::GetBackgroundCount(BackgroundManager::Type::CharacterSheet));
                    ImGui::Text("BarStyle:           %d", BackgroundManager::GetBackgroundCount(BackgroundManager::Type::BarStyle));
                }
                ImGui::End();

                ImGui::EndFrame();
                ImGui::Render();
                RenderToBoundTarget(ImGui::GetDrawData());
            }

            func(a_menu);
        }
        static inline REL::Relocation<decltype(thunk)> func;
        static inline std::size_t idx{ 0x6 };
    };

    void DrawSkyrimCursor() {
        ImGuiMouseCursor cursorType = ImGui::GetMouseCursor();
        if (cursorType != ImGuiMouseCursor_Arrow) {
            ImGui::GetIO().MouseDrawCursor = true;
            return;
        }
        ImGui::GetIO().MouseDrawCursor = false;

        auto* tex = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Icons\\cursor.png");
        if (!tex) return;

        static float s_cursorW = 0.0f;
        static float s_cursorH = 0.0f;
        if (s_cursorW == 0.0f) {
            ID3D11Resource* res = nullptr;
            tex->GetResource(&res);
            if (res) {
                ID3D11Texture2D* tex2d = nullptr;
                if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex2d))) {
                    D3D11_TEXTURE2D_DESC desc{};
                    tex2d->GetDesc(&desc);
                    s_cursorW = static_cast<float>(desc.Width);
                    s_cursorH = static_cast<float>(desc.Height);
                    tex2d->Release();
                }
                res->Release();
            }
            if (s_cursorW == 0.0f) { s_cursorW = 24.0f; s_cursorH = 24.0f; }
        }

        ImVec2 mousePos = ImGui::GetMousePos();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawList->AddImage(
            (ImTextureID)tex,
            mousePos,
            ImVec2(mousePos.x + s_cursorW, mousePos.y + s_cursorH)
        );
    }

    void Install() {
        logger::info("Installing ImGui renderer with Present hook...");

        auto& trampoline = SKSE::GetTrampoline();

        REL::Relocation<std::uintptr_t> present_target{ RELOCATION_ID(75461, 77246) };
        PostDisplay::func = trampoline.write_call<5>(present_target.address() + 0x9, PostDisplay::thunk);

        logger::info("ImGui Present hook installed successfully");
    }

    void Shutdown() {
        ApplyPauseState(false);
        if (g_Initialized) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_Initialized = false;
        }
    }

    bool IsVisible() {
        return g_MenuVisible;
    }

    void Toggle() {
        auto ui = RE::UI::GetSingleton();
        if (ui && !g_MenuVisible) {
            if (PartySheet::IsVisible()) {
                PartySheet::Toggle();
                logger::debug("Settings: Auto-closed PartySheet to open Settings");
            }
            if (PartySheetV2::IsVisible()) {
                PartySheetV2::Toggle();
                logger::debug("Settings: Auto-closed PartySheetV2 to open Settings");
            }
            
            if (MenuUtils::ShouldBlockOverlay()) {
                return;
            }
        }
        
        bool wasVisible = g_MenuVisible;
        g_MenuVisible = !g_MenuVisible;
        
        if (g_MenuVisible && !wasVisible) {
            ImGui::GetIO().ClearInputKeys();
            SaveCameraZoom();
            if (Settings::GetSingleton()->pauseGameInSettings)
                ApplyPauseState(true);
            logger::debug("Settings: Opened (input disable handled in PostDisplay)");
        }
        else if (!g_MenuVisible && wasVisible) {
            ImGui::GetIO().ClearInputKeys();
            ImGui::GetIO().WantTextInput = false;

            ApplyPauseState(false);

            for (int wid = 0; wid < static_cast<int>(HUDVisibility::WidgetID::Count); ++wid)
                HUDVisibility::BeginFadeOut(static_cast<HUDVisibility::WidgetID>(wid));

            if (!PartySheet::IsVisible() && !PartySheetV2::IsVisible()) {
                g_ControlsDisabled = false;
                EnablePlayerInput();
                logger::debug("Settings: Closed - player input enabled");
            } else {
                logger::debug("Settings: Closed but PartySheet/V2 open - input stays disabled");
            }
        }
        
        if (g_Initialized) {
            ImGui::GetIO().MouseDrawCursor = false;
        }
        
        logger::debug("ImGui menu toggled: {}", g_MenuVisible ? "visible" : "hidden");
    }
    
    static void SaveCameraZoom() {
        auto camera = RE::PlayerCamera::GetSingleton();
        if (!camera) return;
        
        g_WasThirdPerson = camera->IsInThirdPerson();
        
        auto state = camera->currentState.get();
        if (!state) return;
        
        if (auto tps = skyrim_cast<RE::ThirdPersonState*>(state)) {
            g_SavedZoomOffset = tps->currentZoomOffset;
            logger::debug("Camera saved: zoom={}, thirdPerson={}", g_SavedZoomOffset, g_WasThirdPerson);
        }
    }
    
    static void RestoreCameraZoom() {
        auto camera = RE::PlayerCamera::GetSingleton();
        if (!camera) return;
        
        bool isNowThirdPerson = camera->IsInThirdPerson();
        if (isNowThirdPerson != g_WasThirdPerson) {
            if (g_WasThirdPerson) {
                camera->ForceThirdPerson();
            } else {
                camera->ForceFirstPerson();
            }
            logger::debug("Camera POV restored to: {}", g_WasThirdPerson ? "third person" : "first person");
        }
        
        auto state = camera->currentState.get();
        if (!state) return;
        
        if (auto tps = skyrim_cast<RE::ThirdPersonState*>(state)) {
            tps->targetZoomOffset = g_SavedZoomOffset;
            tps->currentZoomOffset = g_SavedZoomOffset;
        }
    }
    
    void AddScrollDelta(float delta) {
        bool modalOpen = PartySheetV2::IsEffectsModalOpen() || PartySheetV2::IsInventoryModalOpen() || PartySheetV2::IsStatusModalOpen();
        if (g_Initialized && (g_MenuVisible || modalOpen)) {
            g_MouseWheelDelta += delta;
            RestoreCameraZoom();
        }
    }

    void FlushScrollDelta() {
        if (g_MouseWheelDelta != 0.0f) {
            ImGui::GetIO().AddMouseWheelEvent(0.0f, g_MouseWheelDelta);
            g_MouseWheelDelta = 0.0f;
        }
    }

    float ConsumeScrollDelta() {
        float v = g_MouseWheelDelta;
        g_MouseWheelDelta = 0.0f;
        return v;
    }
    
    ImFont* GetHeadingFont() {
        return FontManager::GetSingleton()->GetHeadingFont();
    }

}
