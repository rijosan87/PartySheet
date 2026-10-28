// STATUS: Working
// Loads and caches spell icons.

#include "../includes/SpellIconManager.h"
#include "../includes/TextureManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

namespace logger = SKSE::log;
namespace fs = std::filesystem;

namespace SpellIconManager {
    static constexpr const char* SPELLS_PATH = "Data\\Interface\\PartySheet\\Spells\\";
    static constexpr const char* CONFIG_FILE = "Data\\Interface\\PartySheet\\Spells\\SpellIcons.json";

    static ID3D11Device* g_Device = nullptr;

    static std::unordered_map<std::string, std::string> g_ModIconMap;
    static std::unordered_map<std::string, std::string> g_SpellIconMap;

    static std::string ToLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    static void ParseObjectSection(const std::string& jsonContent, const std::string& sectionName,
                                   std::unordered_map<std::string, std::string>& outMap) {
        std::string searchKey = "\"" + sectionName + "\"";
        size_t sectionPos = jsonContent.find(searchKey);
        if (sectionPos == std::string::npos) return;
        size_t braceStart = jsonContent.find('{', sectionPos);
        if (braceStart == std::string::npos) return;
        int depth = 1;
        size_t i = braceStart + 1;
        while (i < jsonContent.size() && depth > 0) {
            if (jsonContent[i] == '{') depth++;
            else if (jsonContent[i] == '}') depth--;
            i++;
        }
        std::string sectionContent = jsonContent.substr(braceStart + 1, i - braceStart - 2);
        size_t pos = 0;
        while (pos < sectionContent.size()) {
            size_t keyStart = sectionContent.find('"', pos);
            if (keyStart == std::string::npos) break;
            size_t keyEnd = sectionContent.find('"', keyStart + 1);
            if (keyEnd == std::string::npos) break;
            std::string key = sectionContent.substr(keyStart + 1, keyEnd - keyStart - 1);
            size_t colonPos = sectionContent.find(':', keyEnd);
            if (colonPos == std::string::npos) break;
            size_t valueStart = sectionContent.find('"', colonPos);
            if (valueStart == std::string::npos) break;
            size_t valueEnd = sectionContent.find('"', valueStart + 1);
            if (valueEnd == std::string::npos) break;
            std::string value = sectionContent.substr(valueStart + 1, valueEnd - valueStart - 1);
            outMap[ToLower(key)] = value;
            pos = valueEnd + 1;
        }
    }
    void Init(ID3D11Device* device) {
        g_Device = device;
        LoadConfig();
    }
    void LoadConfig() {
        g_ModIconMap.clear();
        g_SpellIconMap.clear();
        if (!fs::exists(CONFIG_FILE)) {
            logger::info("SpellIconManager: No SpellIcons.json found, using fallback icons only");
            return;
        }
        try {
            std::ifstream file(CONFIG_FILE);
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string jsonContent = buffer.str();
            ParseObjectSection(jsonContent, "mods", g_ModIconMap);
            logger::info("SpellIconManager: Loaded {} mod icon mappings", g_ModIconMap.size());
            ParseObjectSection(jsonContent, "spells", g_SpellIconMap);
            logger::info("SpellIconManager: Loaded {} spell icon mappings", g_SpellIconMap.size());
        } catch (const std::exception& e) {
            logger::error("SpellIconManager: Failed to parse SpellIcons.json: {}", e.what());
        }
    }
    std::string GetSpellSchool(RE::SpellItem* spell) {
        if (!spell) return "magic";
        auto effect = spell->GetAVEffect();
        if (!effect) return "magic";
        auto skill = effect->data.associatedSkill;
        switch (skill) {
            case RE::ActorValue::kDestruction: return "destruction";
            case RE::ActorValue::kAlteration: return "alteration";
            case RE::ActorValue::kConjuration: return "conjuration";
            case RE::ActorValue::kIllusion: return "illusion";
            case RE::ActorValue::kRestoration: return "restoration";
            default: return "magic";
        }
    }
    ID3D11ShaderResourceView* GetSpellIcon(RE::SpellItem* spell) {
        if (!spell) return nullptr;
        const char* spellName = spell->GetName();
        if (spellName && spellName[0] != '\0') {
            std::string lowerName = ToLower(spellName);
            auto it = g_SpellIconMap.find(lowerName);
            if (it != g_SpellIconMap.end()) {
                std::string iconPath = std::string(SPELLS_PATH) + it->second;
                if (auto srv = TextureManager::GetTexture(iconPath)) {
                    return srv;
                }
            }
        }
        if (auto* sourceFile = spell->GetFile(0)) {
            std::string pluginName = ToLower(std::string(sourceFile->GetFilename()));
            auto it = g_ModIconMap.find(pluginName);
            if (it != g_ModIconMap.end()) {
                std::string iconPath = std::string(SPELLS_PATH) + it->second;
                if (auto srv = TextureManager::GetTexture(iconPath)) {
                    return srv;
                }
            }
        }
        std::string school = GetSpellSchool(spell);
        std::string schoolPath = std::string(SPELLS_PATH) + school + ".png";
        if (auto srv = TextureManager::GetTexture(schoolPath)) {
            return srv;
        }
        std::string genericPath = std::string(SPELLS_PATH) + "magic.png";
        return TextureManager::GetTexture(genericPath);
    }
}
