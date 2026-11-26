#include "../../includes/Utils/SurvivalState.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <unordered_map>

namespace SurvivalState {

    namespace {

        constexpr float kSampleEveryHours = 0.25f;
        constexpr float kJumpHours        = 2.0f;
        constexpr float kMinSpanHours     = 0.5f;
        constexpr std::size_t kMaxSamples = 8;
        constexpr float kRestedHours      = 8.0f;
        constexpr std::uint32_t kRecordVersion = 1;

        struct Sample {
            float hours = 0.0f;
            std::unordered_map<std::string, float> fill;
        };

        std::deque<Sample> g_samples;
        std::atomic<float> g_lastSleepHours{ -1.0f };

        Snapshot g_snap;
        int      g_snapFrame = -1;

        constexpr RE::FormID kRestedSpells[] = { 0x0FB981, 0x0FB984, 0x0CDA1D };

        constexpr RE::FormID kRegionEffects[] = { 0x83C, 0x83D, 0x83E, 0x86A, 0x840 };
        std::vector<RE::EffectSetting*> g_regionEffects;
        std::vector<RE::SpellItem*>     g_restedSpells;

        float HoursNow()
        {
            auto* cal = RE::Calendar::GetSingleton();
            return cal ? cal->GetHoursPassed() : -1.0f;
        }

        void TakeSample(float now)
        {
            Sample s;
            s.hours = now;
            for (const auto& n : PlayerHUDSurvival::GetActiveNeeds())
                if (n.fill >= 0.0f) s.fill[n.key] = n.fill;
            g_samples.push_back(std::move(s));
            while (g_samples.size() > kMaxSamples) g_samples.pop_front();
        }

        std::optional<float> Slope(const char* key)
        {
            const Sample* first = nullptr;
            const Sample* last  = nullptr;
            for (const auto& s : g_samples) {
                if (!s.fill.contains(key)) continue;
                if (!first) first = &s;
                last = &s;
            }
            if (!first || last->hours - first->hours < kMinSpanHours) return std::nullopt;
            return (last->fill.at(key) - first->fill.at(key)) / (last->hours - first->hours);
        }

        class SleepSink : public RE::BSTEventSink<RE::TESSleepStopEvent> {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent*,
                                                  RE::BSTEventSource<RE::TESSleepStopEvent>*) override
            {
                g_lastSleepHours = HoursNow();
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        void ReadRegionAndRest(RE::PlayerCharacter* player, Snapshot& out)
        {
            auto* mt = player->AsMagicTarget();
            auto* list = mt ? mt->GetActiveEffectList() : nullptr;
            if (!list) return;
            for (auto* eff : *list) {
                if (!eff) continue;
                if (eff->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                    eff->flags.any(RE::ActiveEffect::Flag::kInactive))
                    continue;
                auto* base = eff->GetBaseObject();
                if (base && out.regionWord.empty() &&
                    std::find(g_regionEffects.begin(), g_regionEffects.end(), base) != g_regionEffects.end())
                    out.regionWord = base->GetFullName();
                if (eff->spell &&
                    std::find(g_restedSpells.begin(), g_restedSpells.end(), eff->spell) != g_restedSpells.end())
                    out.wellRested = true;
            }
        }
    }

    void Install()
    {
        static SleepSink sink;
        if (auto* src = RE::ScriptEventSourceHolder::GetSingleton()) {
            src->AddEventSink<RE::TESSleepStopEvent>(&sink);
            logger::info("SurvivalState: sleep sink registered");
        }

        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) return;
        g_restedSpells.clear();
        for (auto id : kRestedSpells)
            if (auto* f = dh->LookupForm(id, "Skyrim.esm"))
                if (auto* sp = f->As<RE::SpellItem>()) g_restedSpells.push_back(sp);
        g_regionEffects.clear();
        if (dh->LookupModByName("ccqdrsse001-survivalmode.esl"))
            for (auto id : kRegionEffects)
                if (auto* f = dh->LookupForm(id, "ccqdrsse001-survivalmode.esl"))
                    if (auto* mg = f->As<RE::EffectSetting>()) g_regionEffects.push_back(mg);
    }

    void Tick()
    {
        if (!PlayerHUDSurvival::IsAvailable()) { g_samples.clear(); return; }
        const float now = HoursNow();
        if (now < 0.0f) return;
        if (!g_samples.empty()) {
            const float gap = now - g_samples.back().hours;
            if (gap < 0.0f || gap > kJumpHours) g_samples.clear();
            else if (gap < kSampleEveryHours) return;
        }
        TakeSample(now);
    }

    const Snapshot& Get()
    {
        const int frame = ImGui::GetFrameCount();
        if (frame == g_snapFrame) return g_snap;
        g_snapFrame = frame;

        Snapshot s;
        auto* player = RE::PlayerCharacter::GetSingleton();
        s.available = player && PlayerHUDSurvival::IsAvailable();
        if (s.available) {
            for (auto& st : PlayerHUDSurvival::GetActiveNeeds()) {
                Need n;
                n.status = std::move(st);
                if (auto slope = Slope(n.status.key)) {
                    n.rising = *slope > 0.0001f;
                    if (n.rising && n.status.nextFill > n.status.fill)
                        n.hoursToNextStage = (n.status.nextFill - n.status.fill) / *slope;
                }
                s.needs.push_back(std::move(n));
            }
            s.vitality     = PlayerHUDSurvival::GetVitality();
            s.stackWarning = PlayerHUDSurvival::GetStackWarning();
            s.ambient      = PlayerHUDSurvival::GetAmbientCold();
        }

        if (player) {
            ReadRegionAndRest(player, s);
            if (auto* cell = player->GetParentCell()) s.indoors = cell->IsInteriorCell();
            if (auto* cal = RE::Calendar::GetSingleton()) {
                const float hour = cal->GetHour();
                s.night = hour < 6.0f || hour >= 20.0f;
            }
            if (auto* sky = RE::Sky::GetSingleton(); sky && sky->currentWeather) {
                const auto flags = sky->currentWeather->data.flags;
                s.raining = flags.any(RE::TESWeather::WeatherDataFlag::kRainy);
                s.snowing = flags.any(RE::TESWeather::WeatherDataFlag::kSnow);
            }
        }

        const float slept = g_lastSleepHours.load();
        const float now = HoursNow();
        if (slept >= 0.0f && now >= slept) {
            s.hoursSinceSleep = now - slept;
            if (s.wellRested) s.restedHoursLeft = (std::max)(0.0f, kRestedHours - s.hoursSinceSleep);
        }

        g_snap = std::move(s);
        return g_snap;
    }

    void Clear()
    {
        g_samples.clear();
        g_lastSleepHours = -1.0f;
    }

    void OnGameSave(SKSE::SerializationInterface* serde)
    {
        const float slept = g_lastSleepHours.load();
        if (slept < 0.0f) return;
        if (!serde->OpenRecord(kRecordType, kRecordVersion)) {
            logger::warn("SurvivalState: could not open cosave record");
            return;
        }
        serde->WriteRecordData(&slept, sizeof(slept));
    }

    void LoadRecord(SKSE::SerializationInterface* serde, std::uint32_t version, std::uint32_t length)
    {
        float slept = -1.0f;
        if (version != kRecordVersion || length != sizeof(slept) ||
            serde->ReadRecordData(&slept, sizeof(slept)) != sizeof(slept)) {
            logger::warn("SurvivalState: sleep record skipped, version {} length {}", version, length);
            return;
        }
        g_lastSleepHours = slept;
        g_samples.clear();
    }
}
