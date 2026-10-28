#pragma once

// STATUS: Working
// Tracks owned horses and their stats.


#include <string>
#include <vector>
#include <mutex>
#include <RE/Skyrim.h>

namespace PartyHorses {
    struct HorseEntry {
        std::string pluginName;
        RE::FormID localFormID = 0;
        bool isLight = false;
        std::string lastKnownName;
    };

    struct HorseRecord {
        HorseEntry entry;
        RE::FormID runtimeFormID = 0;
        RE::Actor* actorPtr = nullptr;
        bool is3DLoaded = false;
    };

    class HorseRegistry {
    public:
        static HorseRegistry* GetSingleton() {
            static HorseRegistry singleton;
            return &singleton;
        }
        void Load();
        void Save();
        void Register(RE::Actor* actor);
        void Purge();
        std::vector<HorseRecord> GetAll();
        void Summon(RE::FormID runtimeFormID);
        void SummonAll(const std::vector<HorseEntry>& entries);
        void SummonAllFromRegistry();
        bool HasAny() const;
    private:
        HorseRegistry() = default;
        ~HorseRegistry() = default;
        HorseRegistry(const HorseRegistry&) = delete;
        HorseRegistry& operator=(const HorseRegistry&) = delete;
        std::vector<HorseEntry> m_entries;
        mutable std::mutex m_mutex;
    };
}
