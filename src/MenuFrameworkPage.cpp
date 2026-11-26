#include "../includes/MenuFrameworkPage.h"

#include "../includes/InputHandler.h"
#include "../includes/Settings.h"

#include <filesystem>

#ifndef NOMINMAX
#	define NOMINMAX
#endif
#pragma warning(push)
#pragma warning(disable: 4099 5054)
#include "../includes/SKSEMenuFramework.h"
#pragma warning(pop)

namespace ImGuiRenderer
{
	bool IsVisible();
	void Toggle();
}

namespace MenuFrameworkPage
{
	namespace
	{
		std::atomic<bool> g_openRequested{ false };

		constexpr auto kBanner = "Data/Interface/PartySheet/Backgrounds/MenuFrameworkBanner.png";
		constexpr float kBannerAspect = 5.0f;

		void DrawBanner()
		{
			static ImGuiMCP::ImTextureID texture = nullptr;
			static bool tried = false;
			if (!tried) {
				tried = true;
				std::error_code ec;
				if (std::filesystem::exists(kBanner, ec)) {
					texture = SKSEMenuFramework::LoadTexture(kBanner);
				}
				logger::info("SKSE Menu Framework banner {}", texture ? "loaded" : "not found");
			}
			if (texture) {
				const float width = ImGuiMCP::GetContentRegionAvail().x;
				ImGuiMCP::Image(texture, ImGuiMCP::ImVec2(width, width / kBannerAspect));
				ImGuiMCP::Spacing();
			}
		}

		void OpenWhenFrameworkClosed()
		{
			SKSE::GetTaskInterface()->AddTask([] {
				if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
					OpenWhenFrameworkClosed();
					return;
				}
				g_openRequested = false;
				if (!ImGuiRenderer::IsVisible()) {
					ImGuiRenderer::Toggle();
				}
			});
		}

		void __stdcall RenderPage()
		{
			DrawBanner();

			const auto* settings = Settings::GetSingleton();
			const std::string hotkey = ImGuiRenderer::FormatHotkeyWithModifier(settings->menuSettingsKey, settings->menuSettingsModifier);

			ImGuiMCP::TextWrapped("Party Sheet has its own Settings window for every HUD and widget.");
			ImGuiMCP::Text("Settings hotkey: %s", hotkey.c_str());
			ImGuiMCP::Spacing();

			if (ImGuiMCP::Button("Open HUD Settings") && !g_openRequested.exchange(true)) {
				if (auto* window = SKSEMenuFramework::GetMainWindow()) {
					window->IsOpen = false;
				}
				OpenWhenFrameworkClosed();
			}
		}
	}

	void Register()
	{
		auto* module = GetMenuFrameworkModule();
		if (!module || !GetProcAddress(module, "AddSectionItem")) {
			logger::info("SKSE Menu Framework not found, no page added");
			return;
		}
		SKSEMenuFramework::SetSection("Skyrim Party Sheet");
		SKSEMenuFramework::AddSectionItem("Settings", RenderPage);
		logger::info("SKSE Menu Framework page registered");
	}
}
