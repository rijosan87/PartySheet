#include "../includes/SettingsIni.h"

#include <SimpleIni.h>
#include <REL/Relocation.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <cwchar>

namespace SettingsIni
{
    namespace
    {
        enum class State
        {
            Inactive,
            Loading,
            Saving
        };

        // Main-thread-only persistence state. Settings::Load, Settings::Save, and UI callbacks
        State s_state = State::Inactive;
        bool s_mutationFailed = false;

        std::unique_ptr<CSimpleIniA> s_userIni;
        std::unique_ptr<CSimpleIniA> s_presetIni;

        std::filesystem::path GetUserIniPath()
        {
            const std::filesystem::path runtimePath{ REL::Module::get().filePath().data() };
            return (runtimePath.parent_path() / L"Data" / L"SKSE" / L"Plugins" / L"PartyUserSettings.ini").lexically_normal();
        }

        std::filesystem::path GetPresetIniPath()
        {
            const std::filesystem::path runtimePath{ REL::Module::get().filePath().data() };
            return (runtimePath.parent_path() / L"Data" / L"SKSE" / L"Plugins" / L"PartySheet.ini").lexically_normal();
        }

        std::string WideToUtf8(const wchar_t* a_value)
        {
            if (!a_value || !*a_value) {
                return {};
            }

            const int required = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, a_value, -1, nullptr, 0, nullptr, nullptr);
            if (required <= 0) {
                logger::error("SettingsIni: WideCharToMultiByte conversion failed for wide string");
                return {};
            }

            std::string result(static_cast<std::size_t>(required), '\0');
            if (WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS, a_value, -1,
                    result.data(), required, nullptr, nullptr) != required) {
                logger::error("SettingsIni: WideCharToMultiByte conversion result mismatch");
                return {};
            }

            result.pop_back();
            return result;
        }

        std::wstring Utf8ToWide(const char* a_value)
        {
            if (!a_value || !*a_value) {
                return {};
            }

            const int required = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, a_value, -1, nullptr, 0);
            if (required <= 0) {
                logger::error("SettingsIni: MultiByteToWideChar conversion failed for UTF-8 string");
                return {};
            }

            std::wstring result(static_cast<std::size_t>(required), L'\0');
            if (MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, a_value, -1,
                    result.data(), required) != required) {
                logger::error("SettingsIni: MultiByteToWideChar conversion result mismatch");
                return {};
            }

            result.pop_back();
            return result;
        }

        CSimpleIniA* ResolveDoc(const wchar_t* a_path)
        {
            if (!a_path || !*a_path) {
                logger::error("SettingsIni: Null or empty path passed to compatibility adapter");
                return nullptr;
            }

            const std::filesystem::path p(a_path);
            const auto filename = p.filename().native();

            if (_wcsicmp(filename.c_str(), L"PartyUserSettings.ini") == 0) {
                return s_userIni ? s_userIni.get() : nullptr;
            }
            if (_wcsicmp(filename.c_str(), L"PartySheet.ini") == 0) {
                return s_presetIni ? s_presetIni.get() : nullptr;
            }

            logger::error("SettingsIni: Unknown INI path passed to adapter: '{}'", p.generic_string());
            return nullptr;
        }

        DWORD CopyToBuffer(const wchar_t* a_src, wchar_t* a_buffer, DWORD a_bufferSize)
        {
            if (!a_buffer || a_bufferSize == 0) {
                return 0;
            }
            if (!a_src) {
                a_buffer[0] = L'\0';
                return 0;
            }
            const size_t len = wcslen(a_src);
            const size_t toCopy = (std::min)(static_cast<size_t>(a_bufferSize - 1), len);
            wmemcpy(a_buffer, a_src, toCopy);
            a_buffer[toCopy] = L'\0';
            return static_cast<DWORD>(toCopy);
        }
    }

    bool BeginLoad()
    {
        const auto userPath = GetUserIniPath();
        const auto presetPath = GetPresetIniPath();

        auto tempUser = std::make_unique<CSimpleIniA>();
        tempUser->SetUnicode();
        if (std::filesystem::exists(userPath)) {
            const SI_Error rc = tempUser->LoadFile(userPath.c_str());
            if (rc < SI_OK) {
                logger::error("SettingsIni: Failed to load '{}': SimpleIni error {}", userPath.generic_string(), static_cast<int>(rc));
                s_state = State::Inactive;
                return false;
            }
        }

        auto tempPreset = std::make_unique<CSimpleIniA>();
        tempPreset->SetUnicode();
        if (std::filesystem::exists(presetPath)) {
            const SI_Error rc = tempPreset->LoadFile(presetPath.c_str());
            if (rc < SI_OK) {
                logger::error("SettingsIni: Failed to load '{}': SimpleIni error {}", presetPath.generic_string(), static_cast<int>(rc));
                s_state = State::Inactive;
                return false;
            }
        }

        s_userIni = std::move(tempUser);
        s_presetIni = std::move(tempPreset);
        s_state = State::Loading;
        return true;
    }

    void EndLoad()
    {
        s_state = State::Inactive;
        s_userIni.reset();
        s_presetIni.reset();
    }

    bool BeginSave()
    {
        const auto userPath = GetUserIniPath();
        const auto presetPath = GetPresetIniPath();

        auto tempUser = std::make_unique<CSimpleIniA>();
        tempUser->SetUnicode();
        if (std::filesystem::exists(userPath)) {
            const SI_Error rc = tempUser->LoadFile(userPath.c_str());
            if (rc < SI_OK) {
                logger::error("SettingsIni: Failed to load existing '{}' before saving: SimpleIni error {}", userPath.generic_string(), static_cast<int>(rc));
                s_state = State::Inactive;
                return false;
            }
        }

        auto tempPreset = std::make_unique<CSimpleIniA>();
        tempPreset->SetUnicode();
        if (std::filesystem::exists(presetPath)) {
            const SI_Error rc = tempPreset->LoadFile(presetPath.c_str());
            if (rc < SI_OK) {
                logger::error("SettingsIni: Failed to load existing '{}' before saving: SimpleIni error {}", presetPath.generic_string(), static_cast<int>(rc));
                s_state = State::Inactive;
                return false;
            }
        }

        s_userIni = std::move(tempUser);
        s_presetIni = std::move(tempPreset);
        s_mutationFailed = false;
        s_state = State::Saving;
        return true;
    }

    bool EndSave()
    {
        if (s_state != State::Saving) {
            logger::error("SettingsIni: EndSave called outside active save transaction");
            s_state = State::Inactive;
            return false;
        }

        if (s_mutationFailed) {
            logger::error("SettingsIni: Aborting save because one or more mutations failed");
            s_state = State::Inactive;
            s_userIni.reset();
            s_presetIni.reset();
            return false;
        }

        const auto userPath = GetUserIniPath();
        const auto presetPath = GetPresetIniPath();

        std::error_code ec;
        const auto parentDir = userPath.parent_path();
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            logger::error("SettingsIni: Failed to create directory '{}': {}", parentDir.generic_string(), ec.message());
            s_state = State::Inactive;
            s_userIni.reset();
            s_presetIni.reset();
            return false;
        }

        const SI_Error userRc = s_userIni ? s_userIni->SaveFile(userPath.c_str()) : SI_FAIL;
        const SI_Error presetRc = s_presetIni ? s_presetIni->SaveFile(presetPath.c_str()) : SI_FAIL;

        const bool userOk = (userRc >= SI_OK);
        const bool presetOk = (presetRc >= SI_OK);

        if (!userOk) {
            logger::error("SettingsIni: Failed to save '{}': SimpleIni error {}", userPath.generic_string(), static_cast<int>(userRc));
        }
        if (!presetOk) {
            logger::error("SettingsIni: Failed to save '{}': SimpleIni error {}", presetPath.generic_string(), static_cast<int>(presetRc));
        }

        if (userOk && !presetOk) {
            logger::error("SettingsIni: Partial save condition! '{}' saved successfully, but '{}' failed", userPath.generic_string(), presetPath.generic_string());
        } else if (!userOk && presetOk) {
            logger::error("SettingsIni: Partial save condition! '{}' saved successfully, but '{}' failed", presetPath.generic_string(), userPath.generic_string());
        } else if (userOk && presetOk) {
            logger::info("SettingsIni: Successfully saved PartyUserSettings.ini and PartySheet.ini");
        }

        s_state = State::Inactive;
        s_userIni.reset();
        s_presetIni.reset();
        return userOk && presetOk;
    }

    UINT GetIntW(
        const wchar_t* a_section,
        const wchar_t* a_key,
        INT a_default,
        const wchar_t* a_path)
    {
        if (s_state != State::Loading) {
            logger::error("SettingsIni: GetIntW called outside active load transaction");
            return static_cast<UINT>(a_default);
        }

        auto* doc = ResolveDoc(a_path);
        if (!doc) {
            return static_cast<UINT>(a_default);
        }

        if (!a_section || !a_key) {
            return static_cast<UINT>(a_default);
        }

        const std::string sec = WideToUtf8(a_section);
        const std::string key = WideToUtf8(a_key);
        if (sec.empty() && *a_section) {
            return static_cast<UINT>(a_default);
        }
        if (key.empty() && *a_key) {
            return static_cast<UINT>(a_default);
        }

        const char* val = doc->GetValue(sec.c_str(), key.c_str(), nullptr);
        if (!val) {
            return static_cast<UINT>(a_default);
        }

        return static_cast<UINT>(doc->GetLongValue(sec.c_str(), key.c_str(), static_cast<long>(a_default)));
    }

    DWORD GetStringW(
        const wchar_t* a_section,
        const wchar_t* a_key,
        const wchar_t* a_default,
        wchar_t* a_buffer,
        DWORD a_bufferSize,
        const wchar_t* a_path)
    {
        if (!a_buffer || a_bufferSize == 0) {
            return 0;
        }

        if (s_state != State::Loading) {
            logger::error("SettingsIni: GetStringW called outside active load transaction");
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }

        auto* doc = ResolveDoc(a_path);
        if (!doc) {
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }

        if (!a_section || !a_key) {
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }

        const std::string sec = WideToUtf8(a_section);
        const std::string key = WideToUtf8(a_key);
        if (sec.empty() && *a_section) {
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }
        if (key.empty() && *a_key) {
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }

        const char* val = doc->GetValue(sec.c_str(), key.c_str(), nullptr);
        if (!val) {
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }

        const std::wstring wval = Utf8ToWide(val);
        if (wval.empty() && *val) {
            return CopyToBuffer(a_default, a_buffer, a_bufferSize);
        }

        return CopyToBuffer(wval.c_str(), a_buffer, a_bufferSize);
    }

    BOOL WriteStringW(
        const wchar_t* a_section,
        const wchar_t* a_key,
        const wchar_t* a_value,
        const wchar_t* a_path)
    {
        if (s_state != State::Saving) {
            logger::error("SettingsIni: WriteStringW called outside active save transaction");
            return FALSE;
        }

        // A fully null Win32 flush request may return success because disk flushing is controlled by EndSave()
        if (!a_section && !a_key && !a_value) {
            return TRUE;
        }

        auto* doc = ResolveDoc(a_path);
        if (!doc) {
            s_mutationFailed = true;
            return FALSE;
        }

        if (!a_section) {
            logger::error("SettingsIni: Null section in WriteStringW");
            s_mutationFailed = true;
            return FALSE;
        }

        const std::string sec = WideToUtf8(a_section);
        if (sec.empty() && *a_section) {
            logger::error("SettingsIni: Failed to convert section name to UTF-8");
            s_mutationFailed = true;
            return FALSE;
        }

        // Section deletion: non-null section with null key and null value
        if (!a_key && !a_value) {
            doc->Delete(sec.c_str(), nullptr);
            return TRUE;
        }

        if (!a_key) {
            logger::error("SettingsIni: Null key with non-null value in WriteStringW");
            s_mutationFailed = true;
            return FALSE;
        }

        const std::string key = WideToUtf8(a_key);
        if (key.empty() && *a_key) {
            logger::error("SettingsIni: Failed to convert key name to UTF-8");
            s_mutationFailed = true;
            return FALSE;
        }

        // Key deletion: non-null section and key with null value
        if (!a_value) {
            doc->Delete(sec.c_str(), key.c_str());
            return TRUE;
        }

        const std::string val = WideToUtf8(a_value);
        if (val.empty() && *a_value) {
            logger::error("SettingsIni: Failed to convert value to UTF-8 for [{}] {}", sec, key);
            s_mutationFailed = true;
            return FALSE;
        }

        const SI_Error rc = doc->SetValue(sec.c_str(), key.c_str(), val.c_str());
        if (rc < SI_OK) {
            logger::error("SettingsIni: SetValue failed for [{}] {} (error {})", sec, key, static_cast<int>(rc));
            s_mutationFailed = true;
            return FALSE;
        }

        return TRUE;
    }

    int ReadPresetIntOnce(
        const wchar_t* a_section,
        const wchar_t* a_key,
        int a_default)
    {
        const auto presetPath = GetPresetIniPath();
        if (!std::filesystem::exists(presetPath)) {
            return a_default;
        }

        CSimpleIniA ini;
        ini.SetUnicode();
        const SI_Error rc = ini.LoadFile(presetPath.c_str());
        if (rc < SI_OK) {
            logger::error("SettingsIni: ReadPresetIntOnce failed to load '{}': SimpleIni error {}", presetPath.generic_string(), static_cast<int>(rc));
            return a_default;
        }

        if (!a_section || !a_key) {
            return a_default;
        }

        const std::string sec = WideToUtf8(a_section);
        const std::string key = WideToUtf8(a_key);
        if ((sec.empty() && *a_section) || (key.empty() && *a_key)) {
            return a_default;
        }

        const char* val = ini.GetValue(sec.c_str(), key.c_str(), nullptr);
        if (!val) {
            return a_default;
        }

        return static_cast<int>(ini.GetLongValue(sec.c_str(), key.c_str(), a_default));
    }

    bool WritePresetStringOnce(
        const wchar_t* a_section,
        const wchar_t* a_key,
        const wchar_t* a_value)
    {
        const auto presetPath = GetPresetIniPath();

        CSimpleIniA ini;
        ini.SetUnicode();
        if (std::filesystem::exists(presetPath)) {
            const SI_Error loadRc = ini.LoadFile(presetPath.c_str());
            if (loadRc < SI_OK) {
                logger::error("SettingsIni: WritePresetStringOnce refusing to overwrite unreadable '{}': SimpleIni error {}", presetPath.generic_string(), static_cast<int>(loadRc));
                return false;
            }
        }

        if (!a_section || !a_key || !a_value) {
            logger::error("SettingsIni: WritePresetStringOnce null argument provided");
            return false;
        }

        const std::string sec = WideToUtf8(a_section);
        const std::string key = WideToUtf8(a_key);
        const std::string val = WideToUtf8(a_value);
        if ((sec.empty() && *a_section) || (key.empty() && *a_key) || (val.empty() && *a_value)) {
            logger::error("SettingsIni: WritePresetStringOnce UTF-8 conversion failure");
            return false;
        }

        const SI_Error setRc = ini.SetValue(sec.c_str(), key.c_str(), val.c_str());
        if (setRc < SI_OK) {
            logger::error("SettingsIni: WritePresetStringOnce SetValue failed for [{}] {} (error {})", sec, key, static_cast<int>(setRc));
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(presetPath.parent_path(), ec);
        if (ec) {
            logger::error("SettingsIni: WritePresetStringOnce failed to create directory '{}': {}", presetPath.parent_path().generic_string(), ec.message());
            return false;
        }

        const SI_Error saveRc = ini.SaveFile(presetPath.c_str());
        if (saveRc < SI_OK) {
            logger::error("SettingsIni: WritePresetStringOnce failed to save '{}': SimpleIni error {}", presetPath.generic_string(), static_cast<int>(saveRc));
            return false;
        }

        return true;
    }
}
