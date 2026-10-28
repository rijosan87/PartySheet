#pragma once

// STATUS: Working
// Translates string keys using localization files.


#include <string>
#include <unordered_map>

class LocalizationManager {
public:
    static LocalizationManager* GetSingleton() {
        static LocalizationManager singleton;
        return &singleton;
    }
    void LoadLanguage(const std::string& languageCode);
    std::string GetLabel(const std::string& key) const;
    static std::string Get(const std::string& key) {
        return GetSingleton()->GetLabel(key);
    }
private:
    LocalizationManager() = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;
    bool LoadLanguageFile(const std::wstring& filename, std::unordered_map<std::string, std::string>& target);
    std::unordered_map<std::string, std::string> labels;
    std::unordered_map<std::string, std::string> englishFallback;
};
