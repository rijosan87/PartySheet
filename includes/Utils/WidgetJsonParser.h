#pragma once

// STATUS: Working
// Loads and parses widget layout configurations.

#include <string>
#include <vector>
#include <RE/Skyrim.h>

namespace WidgetJsonParser {
    inline std::string ExtractValue(const std::string& json, const std::string& key)
    {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return "";
        size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return "";
        size_t start = json.find_first_not_of(" \t\n\r", colonPos + 1);
        if (start == std::string::npos) return "";
        if (json[start] == '"') {
            size_t end = json.find('"', start + 1);
            if (end == std::string::npos) return "";
            return json.substr(start + 1, end - start - 1);
        } else if (json[start] == '[') {
            size_t end = json.find(']', start);
            if (end == std::string::npos) return "";
            return json.substr(start, end - start + 1);
        } else {
            size_t end = json.find_first_of(",}\n\r", start);
            if (end == std::string::npos) return "";
            std::string val = json.substr(start, end - start);
            size_t first = val.find_first_not_of(" \t");
            size_t last  = val.find_last_not_of(" \t");
            return (first == std::string::npos) ? "" : val.substr(first, last - first + 1);
        }
    }

    inline std::string ExtractArray(const std::string& json, const std::string& key)
    {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return "";

        size_t bracketStart = json.find('[', keyPos);
        if (bracketStart == std::string::npos) return "";

        int depth = 1;
        size_t i = bracketStart + 1;
        while (i < json.size() && depth > 0) {
            if      (json[i] == '[') ++depth;
            else if (json[i] == ']') --depth;
            ++i;
        }
        return json.substr(bracketStart, i - bracketStart);
    }

    inline std::vector<std::string> SplitObjects(const std::string& json)
    {
        std::vector<std::string> objects;
        int depth = 0;
        size_t start = 0;
        for (size_t i = 0; i < json.size(); ++i) {
            if (json[i] == '{') {
                if (depth == 0) start = i;
                ++depth;
            } else if (json[i] == '}') {
                --depth;
                if (depth == 0)
                    objects.push_back(json.substr(start, i - start + 1));
            }
        }
        return objects;
    }

    inline RE::FormID ParseHexFormID(const std::string& s)
    {
        if (s.empty()) return 0;
        std::string clean = s;
        if (clean.size() > 2 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X'))
            clean = clean.substr(2);
        try {
            return static_cast<RE::FormID>(std::stoul(clean, nullptr, 16));
        } catch (...) {
            return 0;
        }
    }

    inline bool ParseBool(const std::string& s, bool def = true)
    {
        if (s == "true"  || s == "1") return true;
        if (s == "false" || s == "0") return false;
        return def;
    }
}
