// STATUS: Working
// Handles custom font loading.

#include "..//includes/FontManager.h"
#include "..//includes/Settings.h"
#include <imgui_freetype.h>
#include <Windows.h>

void FontManager::LoadFonts(ImGuiIO& io) {
    auto settings = Settings::GetSingleton();
    std::string fontPath = GetFontPath(settings->language);
    ImFontConfig fontConfig;
    fontConfig.PixelSnapH = true;
    bool isCJK = settings->language.find("_zh") != std::string::npos ||
                    settings->language.find("_ja") != std::string::npos ||
                    settings->language.find("_ko") != std::string::npos;
    fontConfig.OversampleH = isCJK ? 2 : 3;
    fontConfig.OversampleV = 2;

    fontConfig.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_NoHinting;
    float baseFontSize = static_cast<float>(settings->fontSize);
    float headingSize = static_cast<float>(settings->headingFontSize);
    baseFont = nullptr;
    headingFont = nullptr;
    io.Fonts->Clear();

    io.Fonts->TexMinWidth = 2048;
    io.Fonts->TexMinHeight = 2048;
    if (!fontPath.empty()) {
        baseFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize, &fontConfig);
        if (!isCJK && headingSize > baseFontSize) {
            headingFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), headingSize, &fontConfig);
        }
    }
    if (baseFont) {
    } else {
        io.Fonts->AddFontDefault();
        baseFont = io.Fonts->Fonts[0];
    }
}

std::string FontManager::GetFontPath(const std::string& language) {
    std::string customPath = "Data/SKSE/Plugins/fonts/" + language + ".ttf";
    if (std::filesystem::exists(customPath)) {
        return customPath;
    }
    std::string segoeUIPath = "C:\\Windows\\Fonts\\seguisb.ttf";
    if (std::filesystem::exists(segoeUIPath)) {
        return segoeUIPath;
    }
    return "";
}
