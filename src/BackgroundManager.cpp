// STATUS: Working
// Manages widget custom theme backgrounds.

#include "../includes/BackgroundManager.h"
#include "../includes/TextureManager.h"
#include "../includes/Utils/PathUtils.h"
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace logger = SKSE::log;
namespace fs = std::filesystem;

namespace BackgroundManager {
    static constexpr const char* BACKGROUNDS_PATH = "Data\\Interface\\PartySheet\\Backgrounds\\";
    static constexpr const char* BARS_PATH = "Data\\Interface\\PartySheet\\Bars\\";
    static constexpr const char* PORTRAITS_PATH = "Data\\Interface\\PartySheet\\Portraits\\";
    static constexpr const char* FRAMES_PATH = "Data\\Interface\\PartySheet\\Frames\\";

    static constexpr const char* PREFIX_PARTYSHEET = "PartySheet-";
    static constexpr const char* PREFIX_PARTYCARD = "PartyCard-";
    static constexpr const char* PREFIX_INSPECTCARD = "InspectCard-";
    static constexpr const char* PREFIX_CHARACTERSHEET = "CharacterSheet-";
    static constexpr const char* PREFIX_BARSTYLE = "BarStyle_";
    static constexpr const char* PREFIX_BARSTYLE_FILL = "BarStyleFill_";
    static constexpr const char* PREFIX_PORTRAITFRAME = "frame-";
    static constexpr const char* PREFIX_PLAYERPORTRAIT = "Portrait-";

    static std::vector<std::string> g_PartysheetBgs;
    static std::vector<std::string> g_PartycardBgs;
    static std::vector<std::string> g_InspectcardBgs;
    static std::vector<std::string> g_CharacterSheetBgs;
    static std::vector<std::string> g_BarStyleBgs;
    static std::vector<std::string> g_PortraitFrameBgs;
    static std::vector<std::string> g_PlayerPortraitBgs;

    static bool g_Initialized = false;
    static std::string g_EmptyString;

    static int ExtractIndex(const std::string& filename, const std::string& prefix) {
        try {
            std::string stem = PathUtils::ToUtf8(fs::path(filename).stem());
            if (stem.length() <= prefix.length()) return 0;
            for (size_t i = 0; i < prefix.length(); ++i) {
                if (std::tolower(static_cast<unsigned char>(stem[i])) !=
                    std::tolower(static_cast<unsigned char>(prefix[i]))) {
                    return 0;
                }
            }
            return std::stoi(stem.substr(prefix.length()));
        } catch (...) {
            return 0;
        }
    }

    static bool StartsWithCaseInsensitive(const std::string& str, const std::string& prefix) {
        if (str.length() < prefix.length()) return false;
        for (size_t i = 0; i < prefix.length(); ++i) {
            if (std::tolower(static_cast<unsigned char>(str[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }

    void Init() {
        if (g_Initialized) return;
        g_PartysheetBgs.clear();
        g_PartycardBgs.clear();
        g_InspectcardBgs.clear();
        g_CharacterSheetBgs.clear();
        g_BarStyleBgs.clear();
        g_PortraitFrameBgs.clear();
        g_PlayerPortraitBgs.clear();
        if (fs::exists(BACKGROUNDS_PATH)) {
            for (const auto& entry : fs::directory_iterator(BACKGROUNDS_PATH)) {
                if (!entry.is_regular_file()) continue;
                std::string filename = PathUtils::ToUtf8(entry.path().filename());
                auto ext = PathUtils::ToUtf8(entry.path().extension());
                for (auto& c : ext) c = static_cast<char>(std::tolower(c));
                if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".dds" && ext != ".bmp") continue;
                if (StartsWithCaseInsensitive(filename, PREFIX_PARTYSHEET)) {
                    g_PartysheetBgs.push_back(filename);
                } else if (StartsWithCaseInsensitive(filename, PREFIX_PARTYCARD)) {
                    g_PartycardBgs.push_back(filename);
                } else if (StartsWithCaseInsensitive(filename, PREFIX_INSPECTCARD)) {
                    g_InspectcardBgs.push_back(filename);
                } else if (StartsWithCaseInsensitive(filename, PREFIX_CHARACTERSHEET)) {
                    g_CharacterSheetBgs.push_back(filename);
                } else if (StartsWithCaseInsensitive(filename, PREFIX_PORTRAITFRAME)) {
                    g_PortraitFrameBgs.push_back(filename);
                }
            }
        } else {
            logger::info("BackgroundManager: Backgrounds folder not found at {}", BACKGROUNDS_PATH);
        }
        if (fs::exists(FRAMES_PATH)) {
            for (const auto& entry : fs::directory_iterator(FRAMES_PATH)) {
                if (!entry.is_regular_file()) continue;
                std::string filename = PathUtils::ToUtf8(entry.path().filename());
                auto ext = PathUtils::ToUtf8(entry.path().extension());
                for (auto& c : ext) c = static_cast<char>(std::tolower(c));
                if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".dds" && ext != ".bmp") continue;
                if (StartsWithCaseInsensitive(filename, PREFIX_PORTRAITFRAME)) {
                    if (std::find(g_PortraitFrameBgs.begin(), g_PortraitFrameBgs.end(), filename) == g_PortraitFrameBgs.end())
                        g_PortraitFrameBgs.push_back(filename);
                }
            }
        }
        if (fs::exists(PORTRAITS_PATH)) {
            for (const auto& entry : fs::directory_iterator(PORTRAITS_PATH)) {
                if (!entry.is_regular_file()) continue;
                std::string filename = PathUtils::ToUtf8(entry.path().filename());
                auto ext = PathUtils::ToUtf8(entry.path().extension());
                for (auto& c : ext) c = static_cast<char>(std::tolower(c));
                if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".dds" && ext != ".bmp") continue;
                if (StartsWithCaseInsensitive(filename, PREFIX_PLAYERPORTRAIT))
                    g_PlayerPortraitBgs.push_back(filename);
            }
        }
        if (fs::exists(BARS_PATH)) {
            for (const auto& entry : fs::directory_iterator(BARS_PATH)) {
                if (!entry.is_regular_file()) continue;
                std::string filename = PathUtils::ToUtf8(entry.path().filename());
                auto ext = PathUtils::ToUtf8(entry.path().extension());
                for (auto& c : ext) c = static_cast<char>(std::tolower(c));
                if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".dds" && ext != ".bmp") continue;
                if (StartsWithCaseInsensitive(filename, PREFIX_BARSTYLE)) {
                    if (!StartsWithCaseInsensitive(filename, PREFIX_BARSTYLE_FILL)) {
                        g_BarStyleBgs.push_back(filename);
                    }
                }
            }
        } else {
             logger::info("BackgroundManager: Bars folder not found at {}", BARS_PATH);
        }
        auto sortByIndex = [](std::vector<std::string>& list, const char* prefix) {
            std::sort(list.begin(), list.end(), [prefix](const std::string& a, const std::string& b) {
                return ExtractIndex(a, prefix) < ExtractIndex(b, prefix);
            });
        };
        sortByIndex(g_PartysheetBgs, PREFIX_PARTYSHEET);
        sortByIndex(g_PartycardBgs, PREFIX_PARTYCARD);
        sortByIndex(g_InspectcardBgs, PREFIX_INSPECTCARD);
        sortByIndex(g_CharacterSheetBgs, PREFIX_CHARACTERSHEET);
        sortByIndex(g_BarStyleBgs, PREFIX_BARSTYLE);
        sortByIndex(g_PortraitFrameBgs, PREFIX_PORTRAITFRAME);
        sortByIndex(g_PlayerPortraitBgs, PREFIX_PLAYERPORTRAIT);
        logger::info("BackgroundManager: Loaded {} Partysheet, {} Partycard, {} Inspectcard, {} CharacterSheet, {} BarStyle, {} PortraitFrame, {} PlayerPortrait backgrounds",
            g_PartysheetBgs.size(), g_PartycardBgs.size(), g_InspectcardBgs.size(), g_CharacterSheetBgs.size(), g_BarStyleBgs.size(), g_PortraitFrameBgs.size(), g_PlayerPortraitBgs.size());
        g_Initialized = true;
    }

    const std::vector<std::string>& GetBackgrounds(Type type) {
        switch (type) {
            case Type::Partysheet:     return g_PartysheetBgs;
            case Type::Partycard:      return g_PartycardBgs;
            case Type::Inspectcard:    return g_InspectcardBgs;
            case Type::CharacterSheet: return g_CharacterSheetBgs;
            case Type::BarStyle:       return g_BarStyleBgs;
            case Type::PortraitFrame:   return g_PortraitFrameBgs;
            case Type::PlayerPortrait:  return g_PlayerPortraitBgs;
            default:                    return g_PartysheetBgs;
        }
    }

    int GetBackgroundCount(Type type) {
        return static_cast<int>(GetBackgrounds(type).size());
    }

    const std::string& GetBackgroundName(Type type, int index) {
        const auto& list = GetBackgrounds(type);
        if (index >= 1 && index <= static_cast<int>(list.size())) {
            return list[index - 1];
        }
        return g_EmptyString;
    }

    ID3D11ShaderResourceView* GetBackground(Type type, int index) {
        const std::string& filename = GetBackgroundName(type, index);
        if (filename.empty()) return nullptr;
        if (type == Type::PortraitFrame) {
            if (auto* tex = TextureManager::GetTexture(std::string(FRAMES_PATH) + filename))
                return tex;
            return TextureManager::GetTexture(std::string(BACKGROUNDS_PATH) + filename);
        }
        std::string pathPrefix = (type == Type::BarStyle) ? BARS_PATH
                               : (type == Type::PlayerPortrait) ? PORTRAITS_PATH
                               : BACKGROUNDS_PATH;
        std::string fullPath = pathPrefix + filename;
        return TextureManager::GetTexture(fullPath);
    }

    ID3D11ShaderResourceView* GetBarFillTexture(int index) {
        const std::string& bgFilename = GetBackgroundName(Type::BarStyle, index);
        if (bgFilename.empty()) return nullptr;
        std::string fillFilename = bgFilename;
        fillFilename.replace(0, strlen(PREFIX_BARSTYLE), PREFIX_BARSTYLE_FILL);
        std::string fullPath = std::string(BARS_PATH) + fillFilename;
        return TextureManager::GetTexture(fullPath);
    }

    std::vector<BackgroundEntry> GetBackgroundEntries(Type type) {
        std::vector<BackgroundEntry> entries;
        const auto& filenames = GetBackgrounds(type);
        for (const auto& filename : filenames) {
            ID3D11ShaderResourceView* texture = nullptr;
            if (type == Type::BarStyle) {
                std::string fillFilename = filename;
                fillFilename.replace(0, strlen(PREFIX_BARSTYLE), PREFIX_BARSTYLE_FILL);
                texture = TextureManager::GetTexture(std::string(BARS_PATH) + fillFilename);
            } else if (type == Type::PortraitFrame) {
                texture = TextureManager::GetTexture(std::string(FRAMES_PATH) + filename);
                if (!texture) {
                    texture = TextureManager::GetTexture(std::string(BACKGROUNDS_PATH) + filename);
                }
            } else if (type == Type::PlayerPortrait) {
                texture = TextureManager::GetTexture(std::string(PORTRAITS_PATH) + filename);
            } else {
                texture = TextureManager::GetTexture(std::string(BACKGROUNDS_PATH) + filename);
            }
            if (texture) {
                entries.push_back({filename, texture});
            }
        }
        return entries;
    }
}
