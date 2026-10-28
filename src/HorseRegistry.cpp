#include "../includes/HorseRegistry.h"

#include <SimpleIni.h>
#include <REL/Relocation.h>

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <system_error>

namespace PartyHorses {

    namespace {
        constexpr auto INI_SECTION = "Horses";

        std::filesystem::path GetIniFilePath() {
            const std::filesystem::path runtimePath{ REL::Module::get().filePath().data() };
            return (runtimePath.parent_path() / L"Data" / L"SKSE" / L"Plugins" /
                    L"PartyHorses.ini").lexically_normal();
        }
    }

    void HorseRegistry::Load() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();

        const auto filePath = GetIniFilePath();
        std::error_code ec;
        const bool fileExists = std::filesystem::exists(filePath, ec);
        if (ec) {
            logger::error("HorseRegistry: Failed to inspect '{}': {}", filePath.generic_string(), ec.message());
            return;
        }
        if (!fileExists) {
            logger::info("HorseRegistry: Loaded 0 horses from INI.");
            return;
        }

        CSimpleIniA ini;
        ini.SetUnicode();
        if (const SI_Error rc = ini.LoadFile(filePath.c_str()); rc < SI_OK) {
            logger::error("HorseRegistry: Failed to load '{}': SimpleIni error {}",
                filePath.generic_string(), static_cast<int>(rc));
            return;
        }

        int index = 0;
        while (true) {
            const std::string key = "Horse" + std::to_string(index);
            const char* value = ini.GetValue(INI_SECTION, key.c_str(), nullptr);
            if (!value || !*value) {
                break;
            }

            // Parse format: PluginName|LocalFormID(hex)|IsLight|LastKnownName
            std::stringstream stream(value);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(stream, token, '|')) {
                tokens.push_back(token);
            }

            if (tokens.size() >= 2) {
                HorseEntry entry;
                entry.pluginName = tokens[0];

                try {
                    entry.localFormID = std::stoul(tokens[1], nullptr, 16);
                } catch (...) {
                    index++;
                    continue;
                }

                if (tokens.size() >= 3) {
                    entry.isLight = (tokens[2] == "1" || tokens[2] == "true");
                }

                if (tokens.size() >= 4) {
                    entry.lastKnownName = tokens[3];
                }

                m_entries.push_back(entry);
            }

            index++;
        }

        logger::info("HorseRegistry: Loaded {} horses from INI.", m_entries.size());
    }

    void HorseRegistry::Save() {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto filePath = GetIniFilePath();

        CSimpleIniA ini;
        ini.SetUnicode();

        std::error_code ec;
        const bool fileExists = std::filesystem::exists(filePath, ec);
        if (ec) {
            logger::error("HorseRegistry: Failed to inspect '{}': {}", filePath.generic_string(), ec.message());
            return;
        }
        if (fileExists) {
            if (const SI_Error rc = ini.LoadFile(filePath.c_str()); rc < SI_OK) {
                logger::error("HorseRegistry: Refusing to overwrite unreadable '{}': SimpleIni error {}",
                    filePath.generic_string(), static_cast<int>(rc));
                return;
            }
        }

        // Replace only the owned section so unrelated content survives.
        ini.Delete(INI_SECTION, nullptr);

        for (size_t i = 0; i < m_entries.size(); ++i) {
            const auto& entry = m_entries[i];
            const std::string key = "Horse" + std::to_string(i);

            // Format FormID as hex
            char hexBuffer[32];
            std::snprintf(hexBuffer, sizeof(hexBuffer), "0x%06X", entry.localFormID);

            // Construct line: PluginName|LocalFormID|IsLight|LastKnownName
            const std::string line = entry.pluginName + "|" + hexBuffer + "|" +
                (entry.isLight ? "1" : "0") + "|" + entry.lastKnownName;

            if (const SI_Error rc = ini.SetValue(INI_SECTION, key.c_str(), line.c_str()); rc < SI_OK) {
                logger::error("HorseRegistry: Failed to stage [{}] {}: SimpleIni error {}",
                    INI_SECTION, key, static_cast<int>(rc));
                return;
            }
        }

        std::filesystem::create_directories(filePath.parent_path(), ec);
        if (ec) {
            logger::error("HorseRegistry: Failed to create directory '{}': {}",
                filePath.parent_path().generic_string(), ec.message());
            return;
        }

        if (const SI_Error rc = ini.SaveFile(filePath.c_str()); rc < SI_OK) {
            logger::error("HorseRegistry: Failed to save '{}': SimpleIni error {}",
                filePath.generic_string(), static_cast<int>(rc));
            return;
        }

        logger::info("HorseRegistry: Saved {} horses to INI.", m_entries.size());
    }

    void HorseRegistry::Register(RE::Actor* actor) {
        if (!actor) return;

        RE::FormID formID = actor->GetFormID();
        if (formID == 0) return;

        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;

        auto file = actor->GetFile(0);
        if (!file) return;

        std::string pluginName = file->GetFilename().data();
        RE::FormID localID = formID & 0x00FFFFFF;

        bool isLight = false;
        if ((formID & 0xFF000000) == 0xFE000000) {
            isLight = true;
            localID = formID & 0x00000FFF;
        }

        const char* nameStrRaw = actor->GetDisplayFullName();
        if (!nameStrRaw || nameStrRaw[0] == '\0') {
            nameStrRaw = actor->GetName();
        }
        std::string nameStr = nameStrRaw ? nameStrRaw : "Horse";

        bool shouldSave = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            for (auto& entry : m_entries) {
                if (entry.pluginName == pluginName && entry.localFormID == localID && entry.isLight == isLight) {
                    if (entry.lastKnownName != nameStr) {
                        entry.lastKnownName = nameStr;
                        shouldSave = true;
                    }
                    goto done;
                }
            }

            HorseEntry newEntry;
            newEntry.pluginName = pluginName;
            newEntry.localFormID = localID;
            newEntry.isLight = isLight;
            newEntry.lastKnownName = nameStr;
            m_entries.push_back(std::move(newEntry));
            shouldSave = true;
            logger::info("HorseRegistry: Registered new horse {} ({}:0x{:X})", nameStr, pluginName, localID);
        }
        done:
        if (shouldSave) {
            Save();
        }
    }

    void HorseRegistry::Purge() {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) return;

            auto it = m_entries.begin();
            while (it != m_entries.end()) {
                RE::FormID runtimeID = dataHandler->LookupFormID(it->localFormID, it->pluginName);

                bool remove = false;
                if (runtimeID == 0) {
                    remove = true;
                } else {
                    auto* actor = RE::TESForm::LookupByID<RE::Actor>(runtimeID);
                    if (!actor || actor->IsDead(true) || actor->IsDeleted()) {
                        remove = true;
                    }
                }

                if (remove) {
                    logger::info("HorseRegistry: Purging horse from {}:0x{:X}", it->pluginName, it->localFormID);
                    it = m_entries.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
        }

        if (changed) {
            Save();
        }
    }

    std::vector<HorseRecord> HorseRegistry::GetAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<HorseRecord> records;
        records.reserve(m_entries.size());

        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return records;

        for (const auto& entry : m_entries) {
            HorseRecord record;
            record.entry = entry;
            record.runtimeFormID = dataHandler->LookupFormID(entry.localFormID, entry.pluginName);

            if (record.runtimeFormID != 0) {
                record.actorPtr = RE::TESForm::LookupByID<RE::Actor>(record.runtimeFormID);
                if (record.actorPtr) {
                    record.is3DLoaded = record.actorPtr->Is3DLoaded();
                }
            }

            records.push_back(record);
        }

        return records;
    }

    void HorseRegistry::Summon(RE::FormID runtimeFormID) {
        if (runtimeFormID == 0) return;

        auto taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) return;

        taskInterface->AddTask([runtimeFormID]() {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) return;

            auto* horse = RE::TESForm::LookupByID<RE::Actor>(runtimeFormID);
            if (horse && !horse->IsDead(true) && !horse->IsDeleted()) {
                auto* owner = horse->extraList.GetOwner();
                bool owned = false;
                if (owner) {
                    if (owner == player->GetActorBase()) {
                        owned = true;
                    } else if (auto* fac = owner->As<RE::TESFaction>()) {
                        owned = player->IsInFaction(fac);
                    }
                }
                if (!owned) {
                    logger::info("HorseRegistry: Skipping summon of horse (0x{:X}) — player no longer owns it.", runtimeFormID);
                    return;
                }
                if (horse->GetParentCell() == player->GetParentCell()) {
                    logger::info("HorseRegistry: Horse (0x{:X}) already in player's cell, skipping summon.", runtimeFormID);
                    return;
                }
                horse->MoveTo(player);
                logger::info("HorseRegistry: Summoned horse (FormID: 0x{:X}) to player.", runtimeFormID);
            }
        });
    }

    void HorseRegistry::SummonAll(const std::vector<HorseEntry>& entries) {
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;

        for (const auto& entry : entries) {
            RE::FormID runtimeID = dataHandler->LookupFormID(entry.localFormID, entry.pluginName);
            if (runtimeID != 0) {
                Summon(runtimeID);
            }
        }
    }

    bool HorseRegistry::HasAny() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_entries.empty();
    }

    void HorseRegistry::SummonAllFromRegistry() {
        std::vector<HorseEntry> entriesCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            entriesCopy = m_entries;
        }
        SummonAll(entriesCopy);
        logger::info("HorseRegistry: SummonAllFromRegistry called for {} horses.", entriesCopy.size());
    }

}
