#pragma once

// STATUS: Working
// Filters out internal or invisible spells.


#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Utils/PathUtils.h"

namespace EffectBlacklist {
    inline std::vector<std::string>& Tokens() {
        static std::vector<std::string> tokens;
        return tokens;
    }

    inline bool& Loaded() {
        static bool loaded = false;
        return loaded;
    }

    inline std::string ToLower(std::string s) {
        for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    inline std::string Trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    inline void ParseFile(const std::filesystem::path& path) {
        std::ifstream f(path);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            for (char delim : {'#', ';'}) {
                auto pos = line.find(delim);
                if (pos != std::string::npos) line.erase(pos);
            }
            std::string tok = Trim(line);
            if (tok.empty()) continue;
            if (tok.front() == '[') continue;
            Tokens().push_back(ToLower(tok));
        }
        logger::info("[EffectBlacklist] Loaded {} ({} total tokens)", PathUtils::ToUtf8(path.filename()), Tokens().size());
    }

    inline void Load() {
        if (Loaded()) return;
        Loaded() = true;
        Tokens().clear();
        namespace fs = std::filesystem;
        const fs::path pluginsDir = "Data/SKSE/Plugins";
        const char* primary = "activeeffects_blacklist.ini";
        const char* suffix  = "activeeffects_blacklist.ini";
        ParseFile(pluginsDir / primary);
        std::error_code ec;
        if (!fs::exists(pluginsDir, ec)) return;
        for (auto& entry : fs::directory_iterator(pluginsDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            std::string name = ToLower(PathUtils::ToUtf8(entry.path().filename()));
            if (name == primary) continue;
            if (name.size() > std::strlen(suffix) &&
                name.compare(name.size() - std::strlen(suffix), std::strlen(suffix), suffix) == 0) {
                ParseFile(entry.path());
            }
        }
    }

    inline void Reload() {
        Loaded() = false;
        Load();
    }

    inline bool IsBlacklisted(const char* effectName) {
        if (!effectName || !effectName[0]) return false;
        const auto& toks = Tokens();
        if (toks.empty()) return false;
        std::string name = ToLower(effectName);
        for (const auto& t : toks) {
            if (name.find(t) != std::string::npos) return true;
        }
        return false;
    }
}
