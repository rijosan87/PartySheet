#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/UIAnimation.h"

#include <RE/Skyrim.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr const char* kSurvivalESL  = "ccqdrsse001-survivalmode.esl";
static constexpr const char* kStarfrostESP = "Starfrost.esp";
static constexpr const char* kSunHelmESP   = "SunHelmSurvival.esp";
static constexpr const char* kFrostfallESP = "Frostfall.esp";
static constexpr const char* kLastSeedESP  = "LastSeed.esp";

enum class Backend  { None, VanillaCC, Starfrost, SunHelm, Frostfall, LastSeed };
enum class Axis     { Cold, Hunger, Fatigue, Thirst, Exposure, Wetness, Vitality, Alcohol, Skooma };
enum class ReadMode { SpellArray, MGEFArray, GlobalVar };

enum class AvailRule { ModeGlobal, AnyNeedSpell, BackendGlobal };

struct AxisDef {
    Axis        axis;
    const char* key;
    ReadMode    mode;
    int         maxStage;
    int         startStage;

    bool        hideWhenNoSpell;

    RE::FormID  ids[6];
    RE::FormID  idsAlt[6];
    const char* plugin;
    const char* pluginAlt;
};

struct ResolvedAxis {
    const AxisDef*     def      = nullptr;
    RE::SpellItem*     spells[6]   = {};
    RE::EffectSetting* mgefs[6]    = {};
    RE::EffectSetting* mgefsAlt[6] = {};
    RE::TESGlobal*     global      = nullptr;
};

static const AxisDef kVanillaAxes[] = {
    { Axis::Cold,    "cold",    ReadMode::SpellArray, 5, 0, false,
      { 0x890, 0x86E, 0x891, 0x86D, 0x870, 0x871 }, {}, kSurvivalESL, nullptr },
    { Axis::Hunger,  "hunger",  ReadMode::SpellArray, 5, 0, false,
      { 0x876, 0x87E, 0x880, 0x881, 0x886, 0x885 }, {}, kSurvivalESL, nullptr },
    { Axis::Fatigue, "fatigue", ReadMode::SpellArray, 5, 0, false,
      { 0,     0x878, 0x879, 0x87A, 0x87B, 0x87D }, {}, kSurvivalESL, nullptr },
};

static const AxisDef kStarfrostAxes[] = {
    { Axis::Cold,    "cold",    ReadMode::MGEFArray,  5, 0, false,
      { 0xA03, 0x834, 0x956, 0x955, 0x954, 0x95F }, {}, kSurvivalESL, nullptr },
    { Axis::Hunger,  "hunger",  ReadMode::SpellArray, 5, 0, false,
      { 0x876, 0x87E, 0x880, 0x881, 0x886, 0x885 }, {}, kSurvivalESL, nullptr },
    { Axis::Fatigue, "fatigue", ReadMode::SpellArray, 5, 0, false,
      { 0,     0x878, 0x879, 0x87A, 0x87B, 0x87D }, {}, kSurvivalESL, nullptr },
};

static const AxisDef kSunHelmAxes[] = {
    { Axis::Cold,    "cold",    ReadMode::SpellArray, 5, 0, true,
      { 0x6E81DA, 0x6E81DB, 0x6E81DE, 0x6E81E0, 0x6E81E2, 0x6E81E4 }, {}, kSunHelmESP, nullptr },
    { Axis::Hunger,  "hunger",  ReadMode::SpellArray, 5, 0, true,
      { 0x001824, 0x001825, 0x001827, 0x001829, 0x00182B, 0x00182D }, {}, kSunHelmESP, nullptr },
    { Axis::Fatigue, "fatigue", ReadMode::SpellArray, 5, 0, true,
      { 0x01E846, 0x01E845, 0x01E848, 0x01E84A, 0x01E84C, 0x01E84E }, {}, kSunHelmESP, nullptr },
    { Axis::Thirst,  "thirst",  ReadMode::SpellArray, 5, 0, true,
      { 0x01E850, 0x05C47E, 0x05C47F, 0x05C481, 0x05C483, 0x05C485 }, {}, kSunHelmESP, nullptr },
};

static const AxisDef kFrostfallAxes[] = {
    { Axis::Exposure, "exposure", ReadMode::GlobalVar, 6, 0, false,
      { 0x068119 }, {}, kFrostfallESP, nullptr },
    { Axis::Wetness,  "wetness",  ReadMode::GlobalVar, 3, 0, false,
      { 0x06458D }, {}, kFrostfallESP, nullptr },
};

static const AxisDef kLastSeedAxes[] = {
    { Axis::Hunger,  "hunger",  ReadMode::GlobalVar, 5, 0, false,
      { 0x00F7D6 }, {}, kLastSeedESP, nullptr },
    { Axis::Thirst,  "thirst",  ReadMode::GlobalVar, 5, 0, false,
      { 0x00F7D7 }, {}, kLastSeedESP, nullptr },
    { Axis::Fatigue, "fatigue", ReadMode::GlobalVar, 5, 0, false,
      { 0x00F7D8 }, {}, kLastSeedESP, nullptr },
};

// Selected backend state. Set once at the end of Init(), read everywhere else.
static Backend   g_backend   = Backend::None;
static AvailRule g_availRule = AvailRule::ModeGlobal;
static std::vector<ResolvedAxis> g_axes;

static RE::TESGlobal* g_modeEnabled = nullptr;

static bool           g_frostfallActive  = false;
static RE::TESGlobal* g_frostfallRunning = nullptr;

static bool FrostfallLive()
{
    return g_frostfallActive && g_frostfallRunning && g_frostfallRunning->value == 2.0f;
}

static bool           g_lastSeedActive  = false;
static RE::TESGlobal* g_lastSeedRunning = nullptr;

static bool LastSeedLive()
{
    return g_lastSeedActive && g_lastSeedRunning && g_lastSeedRunning->value == 2.0f;
}

static constexpr RE::FormID kVitalitySpells[8] = {
    0x00F258,
    0x00F259,
    0x00F25C,
    0x00F7D3,
    0x00F260,
    0x00F262,
    0x00F264,
    0x00F266,
};

static RE::SpellItem* g_vitalitySpells[8] = {};

static bool  g_initialized = false;
static bool  g_ForcePos    = false;

static ID3D11ShaderResourceView* g_iconCold     = nullptr;
static ID3D11ShaderResourceView* g_iconHunger   = nullptr;
static ID3D11ShaderResourceView* g_iconFatigue  = nullptr;
static ID3D11ShaderResourceView* g_iconThirst   = nullptr;
static ID3D11ShaderResourceView* g_iconExposure = nullptr;
static ID3D11ShaderResourceView* g_iconWetness  = nullptr;
static ID3D11ShaderResourceView* g_iconVitality = nullptr;

static RE::SpellItem* LookupSpell(RE::TESDataHandler* dh, RE::FormID id, const char* plugin)
{
    if (!id) return nullptr;
    auto form = dh->LookupForm(id, plugin);
    return form ? form->As<RE::SpellItem>() : nullptr;
}

static RE::EffectSetting* LookupMGEF(RE::TESDataHandler* dh, RE::FormID id, const char* plugin)
{
    if (!id) return nullptr;
    auto form = dh->LookupForm(id, plugin);
    return form ? form->As<RE::EffectSetting>() : nullptr;
}

static bool HasAnySpell(RE::Actor* p, RE::SpellItem* const spells[6])
{
    for (int i = 0; i < 6; ++i)
        if (spells[i] && p->HasSpell(spells[i])) return true;
    return false;
}

static const char* BackendName()
{
    switch (g_backend) {
        case Backend::SunHelm:   return "SunHelm";
        case Backend::Starfrost: return "Starfrost";
        case Backend::VanillaCC: return "VanillaCC";
        case Backend::Frostfall: return "Frostfall";
        case Backend::LastSeed:  return "LastSeed";
        default:                 return "None";
    }
}

static bool BuildAxes(RE::TESDataHandler* dh, const AxisDef* defs, int count,
                      std::vector<ResolvedAxis>& out)
{
    out.clear();
    out.reserve(count);
    for (int d = 0; d < count; ++d) {
        const AxisDef& def = defs[d];
        ResolvedAxis   ra;
        ra.def = &def;

        const char* altPlugin = def.pluginAlt ? def.pluginAlt : def.plugin;
        for (int i = 0; i < 6; ++i) {
            switch (def.mode) {
                case ReadMode::SpellArray:
                    ra.spells[i] = LookupSpell(dh, def.ids[i], def.plugin);
                    break;
                case ReadMode::MGEFArray:
                    ra.mgefs[i]    = LookupMGEF(dh, def.ids[i],    def.plugin);
                    ra.mgefsAlt[i] = LookupMGEF(dh, def.idsAlt[i], altPlugin);
                    break;
                case ReadMode::GlobalVar:
                    if (i == 0 && def.ids[0]) {
                        auto form = dh->LookupForm(def.ids[0], def.plugin);
                        ra.global = form ? form->As<RE::TESGlobal>() : nullptr;
                    }
                    break;
            }
        }
        out.push_back(ra);
    }

    for (const auto& a : out) {
        if (a.def->mode == ReadMode::SpellArray && a.spells[1]) return true;
        if (a.def->mode == ReadMode::MGEFArray  && a.mgefs[0])  return true;
        if (a.def->mode == ReadMode::GlobalVar  && a.global)    return true;
    }
    return false;
}

namespace PlayerHUDSurvival {

void Init()
{
    auto* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) return;

    if (dh->LookupModByName(kSurvivalESL)) {
        auto modeForm = dh->LookupForm(RE::FormID(0x826), kSurvivalESL);
        g_modeEnabled = modeForm ? modeForm->As<RE::TESGlobal>() : nullptr;
    }

    std::vector<ResolvedAxis> axes;

    if (dh->LookupModByName(kLastSeedESP) &&
        BuildAxes(dh, kLastSeedAxes, static_cast<int>(std::size(kLastSeedAxes)), axes)) {
        g_lastSeedActive = true;
        g_backend        = Backend::LastSeed;
        g_availRule      = AvailRule::BackendGlobal;
        auto lsRun = dh->LookupForm(RE::FormID(0x00B162), kLastSeedESP);
        g_lastSeedRunning = lsRun ? lsRun->As<RE::TESGlobal>() : nullptr;
        logger::debug("PlayerHUDSurvival: LastSeed.esp detected - needs backend (running={}).",
                      g_lastSeedRunning ? static_cast<int>(g_lastSeedRunning->value) : -1);
    } else if (dh->LookupModByName(kSunHelmESP) &&
        BuildAxes(dh, kSunHelmAxes, static_cast<int>(std::size(kSunHelmAxes)), axes)) {
        g_backend   = Backend::SunHelm;
        g_availRule = AvailRule::AnyNeedSpell;
    } else if (dh->LookupModByName(kStarfrostESP) &&
               BuildAxes(dh, kStarfrostAxes, static_cast<int>(std::size(kStarfrostAxes)), axes)) {
        g_backend   = Backend::Starfrost;
        g_availRule = AvailRule::ModeGlobal;
    } else if (dh->LookupModByName(kSurvivalESL) &&
               BuildAxes(dh, kVanillaAxes, static_cast<int>(std::size(kVanillaAxes)), axes)) {
        g_backend   = Backend::VanillaCC;
        g_availRule = AvailRule::ModeGlobal;
    }

    if (dh->LookupModByName(kLastSeedESP)) {
        int resolved = 0;
        for (int i = 0; i < 8; ++i) {
            auto* form = dh->LookupForm(kVitalitySpells[i], kLastSeedESP);
            g_vitalitySpells[i] = form ? form->As<RE::SpellItem>() : nullptr;
            if (g_vitalitySpells[i]) ++resolved;
        }
        logger::debug("PlayerHUDSurvival: Vitality tiers resolved {}/8.", resolved);
    }

    std::vector<ResolvedAxis> ffAxes;
    if (dh->LookupModByName(kFrostfallESP) &&
        BuildAxes(dh, kFrostfallAxes, static_cast<int>(std::size(kFrostfallAxes)), ffAxes)) {
        g_frostfallActive = true;
        axes.insert(axes.end(), ffAxes.begin(), ffAxes.end());
        if (g_backend == Backend::None) {
            g_backend   = Backend::Frostfall;
            g_availRule = AvailRule::BackendGlobal;
        }
        auto ffRun = dh->LookupForm(RE::FormID(0x06DCFB), kFrostfallESP);
        g_frostfallRunning = ffRun ? ffRun->As<RE::TESGlobal>() : nullptr;
        logger::debug("PlayerHUDSurvival: Frostfall.esp detected - cold axes appended (running={}).",
                      g_frostfallRunning ? static_cast<int>(g_frostfallRunning->value) : -1);
    }

    if (axes.empty()) {
        g_backend = Backend::None;
        logger::debug("PlayerHUDSurvival: no survival backend found - survival cluster disabled.");
        return;
    }

    g_axes = std::move(axes);

    logger::debug("PlayerHUDSurvival: backend={} ({} axes, modeGlobal={}).",
                  BackendName(), g_axes.size(), g_modeEnabled ? "ok" : "null");
    for (const auto& a : g_axes) {
        logger::debug("PlayerHUDSurvival:   axis {} mode={} maxStage={} s1={} m0={} glob={}",
                      a.def->key,
                      a.def->mode == ReadMode::SpellArray ? "spell"
                        : a.def->mode == ReadMode::MGEFArray ? "mgef" : "global",
                      a.def->maxStage,
                      a.spells[1] ? "ok" : "null",
                      a.mgefs[0]  ? "ok" : "null",
                      a.global    ? "ok" : "null");
    }

    g_iconCold    = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_cold.png");
    g_iconHunger  = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_hunger.png");
    g_iconFatigue = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_fatigue.png");
    g_iconThirst  = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_thirst.png");
    g_iconExposure = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_exposure.png");
    g_iconWetness  = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_wetness.png");
    if (!g_iconExposure) g_iconExposure = g_iconCold;

    g_iconVitality = TextureManager::GetTexture("Data\\Interface\\PartySheet\\Widgets\\survival_vitality.png");

    g_initialized = true;
    logger::debug("PlayerHUDSurvival: Initialized successfully.");
}

bool IsAvailable()
{
    if (FrostfallLive()) return true;

    if (g_backend == Backend::None) return false;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    switch (g_availRule) {
    case AvailRule::ModeGlobal:
        if (!g_initialized || !g_modeEnabled) return false;
        return g_modeEnabled->value == 1.0f;

    case AvailRule::AnyNeedSpell:
        for (const auto& a : g_axes) {
            if (a.def->axis == Axis::Cold) continue;
            if (a.def->mode == ReadMode::SpellArray && HasAnySpell(player, a.spells))
                return true;
        }
        return false;

    case AvailRule::BackendGlobal:
        return g_backend == Backend::LastSeed ? LastSeedLive() : FrostfallLive();
    }
    return false;
}

static int ReadAxisStage(RE::Actor* player, const ResolvedAxis& a)
{
    switch (a.def->mode) {
    case ReadMode::SpellArray: {
        if (a.def->hideWhenNoSpell && !HasAnySpell(player, a.spells)) return -1;
        for (int i = 5; i >= 0; --i)
            if (a.spells[i] && player->HasSpell(a.spells[i])) return i;
        return 0;
    }
    case ReadMode::MGEFArray: {
        auto* mt = player->AsMagicTarget();
        if (!mt) return 0;
        const int count = std::min(a.def->maxStage - a.def->startStage + 1, 6);
        for (int i = count - 1; i >= 0; --i) {
            if (a.mgefs[i]    && mt->HasMagicEffect(a.mgefs[i]))    return i + a.def->startStage;
            if (a.mgefsAlt[i] && mt->HasMagicEffect(a.mgefsAlt[i])) return i + a.def->startStage;
        }
        return 0;
    }
    case ReadMode::GlobalVar: {
        if (!a.global) return -1;

        if (a.def->axis == Axis::Exposure || a.def->axis == Axis::Wetness) {
            if (!FrostfallLive()) return -1;
        }

        if (a.def->plugin == kLastSeedESP && !LastSeedLive()) return -1;

        int v = static_cast<int>(a.global->value);

        if (v < 0) v = 0;
        return v > a.def->maxStage ? a.def->maxStage : v;
    }
    }
    return -1;
}

static int GetStage(Axis axis)
{
    if (!IsAvailable()) return -1;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return -1;
    for (const auto& a : g_axes)
        if (a.def->axis == axis) return ReadAxisStage(player, a);
    return -1;
}

int GetColdStage()    { return GetStage(Axis::Cold); }
int GetHungerStage()  { return GetStage(Axis::Hunger); }
int GetFatigueStage() { return GetStage(Axis::Fatigue); }
int GetThirstStage()  { return GetStage(Axis::Thirst); }

static int GetExposureStage() { return GetStage(Axis::Exposure); }
static int GetWetnessStage()  { return GetStage(Axis::Wetness); }

bool IsNeedSpell(RE::SpellItem* spell)
{
    if (!spell) return false;
    for (const auto& a : g_axes)
        for (int i = 0; i < 6; ++i)
            if (a.spells[i] == spell) return true;
    return false;
}

static std::string ResolveNeedStageName(const char* key, int stage)
{
    int s = stage < 0 ? 0 : (stage > 5 ? 5 : stage);
    auto L = [](const char* k, const char* fallback) {
        std::string v = LocalizationManager::Get(k);
        return (v.empty() || v == k) ? std::string(fallback) : v;
    };
    char buf[24];

    if (!strcmp(key, "exposure")) {
        static const char* ffExpFall[7] = { "Warm", "Comfortable", "Cold", "Very Cold",
                                            "Freezing", "Freezing to Death", "Maximum Exposure" };
        int es = stage < 0 ? 0 : (stage > 6 ? 6 : stage);
        snprintf(buf, sizeof(buf), "ff_exposure_%d", es);
        return L(buf, ffExpFall[es]);
    }
    if (!strcmp(key, "wetness")) {
        static const char* ffWetFall[4] = { "Not Wet", "Damp", "Wet", "Drenched" };
        int ws = stage < 0 ? 0 : (stage > 3 ? 3 : stage);
        snprintf(buf, sizeof(buf), "ff_wetness_%d", ws);
        return L(buf, ffWetFall[ws]);
    }

    if (g_backend == Backend::SunHelm) {
        static const char* shFall[4][6] = {
            { "Warm", "Comfortable", "Chilly", "Cold", "Freezing", "Frigid" },
            { "Well Fed", "Satisfied", "Peckish", "Hungry", "Ravenous", "Starving" },
            { "Well Rested", "Rested", "Slightly Tired", "Tired", "Weary", "Exhausted" },
            { "Quenched", "Sated", "Thirsty", "Parched", "Dehydrated", "Severely Dehydrated" },
        };
        int row = !strcmp(key, "cold") ? 0 : !strcmp(key, "hunger") ? 1 : !strcmp(key, "fatigue") ? 2 : 3;
        snprintf(buf, sizeof(buf), "sh_%s_%d", key, s);
        return L(buf, shFall[row][s]);
    }
    if (g_backend == Backend::LastSeed) {
        static const char* lsFall[3][6] = {
            { "Well Fed", "Satisfied", "Hungry", "Very Hungry", "Ravenous", "Starving" },
            { "Quenched", "Refreshed", "Thirsty", "Very Thirsty", "Parched", "Dehydrated" },
            { "Rested",   "Sharp",     "Tired",   "Very Tired",   "Haggard", "Exhausted" },
        };
        int row = !strcmp(key, "hunger") ? 0 : !strcmp(key, "thirst") ? 1 : 2;
        snprintf(buf, sizeof(buf), "ls_%s_%d", key, s);
        return L(buf, lsFall[row][s]);
    }
    if (g_backend == Backend::Starfrost) {
        static const char* sfColdFall[6]    = { "Warm", "Comfortable", "Chilly", "Cold", "Very Cold", "Freezing" };
        static const char* sfHungerFall[6]  = { "Not Hungry", "Neutral", "Neutral", "Hungry", "Very Hungry", "Starving" };
        static const char* sfFatigueFall[6] = { "Rested", "Refreshed", "Slightly Tired", "Tired", "Very Tired", "Exhausted" };
        if (!strcmp(key, "hunger")) {
            snprintf(buf, sizeof(buf), "sf_hunger_%d", s);
            return L(buf, sfHungerFall[s]);
        }
        snprintf(buf, sizeof(buf), "sf_%s_%d", key, s);
        return L(buf, !strcmp(key, "cold") ? sfColdFall[s] : sfFatigueFall[s]);
    }
    snprintf(buf, sizeof(buf), "%s_%d", key, s);
    return LocalizationManager::Get(buf);
}

static void LogNeedsBaseline(const std::vector<NeedStatus>& needs,
                             int coldStage, int hungerStage,
                             int fatigueStage, int thirstStage)
{
    auto* settings = Settings::GetSingleton();
    if (!settings || !settings->debugEnableVerboseLogging) return;

    std::string line = fmt::format(
        "[SurvivalBaseline] backend={} ff={} ls={} avail={} raw(cold={},hunger={},fatigue={},thirst={},exposure={},wetness={}) cards={}",
        BackendName(),
        g_frostfallActive ? (FrostfallLive() ? "live" : "off") : "no",
        g_lastSeedActive  ? (LastSeedLive()  ? "live" : "off") : "no",
        IsAvailable() ? 1 : 0,
        coldStage, hungerStage, fatigueStage, thirstStage,
        GetExposureStage(), GetWetnessStage(),
        needs.size());

    for (const auto& n : needs) {
        line += fmt::format(" | {} stage={}/{} name=\"{}\" stageName=\"{}\" icon={}",
                            n.key, n.stage, n.maxStage, n.needName, n.stageName,
                            n.icon ? 1 : 0);
    }

    static std::string                            s_last;
    static std::chrono::steady_clock::time_point  s_lastLog;
    const auto now = std::chrono::steady_clock::now();
    if (line == s_last &&
        std::chrono::duration<float>(now - s_lastLog).count() < 1.0f)
        return;

    s_last    = line;
    s_lastLog = now;
    logger::info("{}", line);
}

std::vector<NeedStatus> GetActiveNeeds()
{
    std::vector<NeedStatus> out;
    if (!IsAvailable()) return out;

    const int coldStage    = GetColdStage();
    const int hungerStage   = GetHungerStage();
    const int fatigueStage = GetFatigueStage();
    const int thirstStage   = GetThirstStage();
    const int exposureStage = GetExposureStage();
    const int wetnessStage  = GetWetnessStage();

    auto add = [&](const char* key, const char* needKey, int stage, int maxStage,
                   ID3D11ShaderResourceView* icon) {
        if (stage < 0) return;
        NeedStatus ns;
        ns.needName  = LocalizationManager::Get(needKey);
        if (ns.needName.empty() || ns.needName == needKey) {
            if      (!strcmp(key, "exposure")) ns.needName = "Exposure";
            else if (!strcmp(key, "wetness"))  ns.needName = "Wetness";
        }
        ns.stageName = ResolveNeedStageName(key, stage);
        ns.stage     = stage;
        ns.maxStage  = maxStage;
        ns.icon      = icon;
        ns.key       = key;
        out.push_back(std::move(ns));
    };

    auto maxStageFor = [](Axis axis) {
        for (const auto& a : g_axes)
            if (a.def->axis == axis) return a.def->maxStage;
        return 5;
    };

    if (g_backend == Backend::SunHelm || g_backend == Backend::LastSeed) {
        add("hunger",  "need_hunger",  hungerStage,  maxStageFor(Axis::Hunger),  g_iconHunger);
        add("thirst",  "need_thirst",  thirstStage,  maxStageFor(Axis::Thirst),  g_iconThirst);
        add("fatigue", "need_fatigue", fatigueStage, maxStageFor(Axis::Fatigue), g_iconFatigue);
        add("cold",    "need_cold",    coldStage,    maxStageFor(Axis::Cold),    g_iconCold);
    } else {
        add("cold",    "need_cold",    coldStage,    maxStageFor(Axis::Cold),    g_iconCold);
        add("hunger",  "need_hunger",  hungerStage,  maxStageFor(Axis::Hunger),  g_iconHunger);
        add("fatigue", "need_fatigue", fatigueStage, maxStageFor(Axis::Fatigue), g_iconFatigue);
    }

    add("exposure", "need_exposure", exposureStage, maxStageFor(Axis::Exposure), g_iconExposure);
    add("wetness",  "need_wetness",  wetnessStage,  maxStageFor(Axis::Wetness),  g_iconWetness);
    return out;
}

bool IsVitalitySpell(RE::SpellItem* spell)
{
    if (!spell) return false;
    for (auto* s : g_vitalitySpells)
        if (s && s == spell) return true;
    return false;
}

std::optional<VitalityStatus> GetVitality()
{
    if (!LastSeedLive()) return std::nullopt;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return std::nullopt;

    for (int tier = 7; tier >= 0; --tier) {
        auto* spell = g_vitalitySpells[tier];
        if (!spell || !player->HasSpell(spell)) continue;

        VitalityStatus vs;
        vs.name = LocalizationManager::Get("need_vitality");
        if (vs.name.empty() || vs.name == "need_vitality") vs.name = "Vitality";
        const char* nm = spell->GetName();
        vs.tierName = (nm && *nm) ? nm : "";
        vs.tier     = tier;
        vs.maxTier  = 7;
        vs.icon     = g_iconVitality;
        return vs;
    }

    return std::nullopt;
}

// ── Master scale helpers ────────────────────────────────────────────────
static float MasterScale()
{
    const auto* settings = Settings::GetSingleton();
    return settings
        ? std::clamp(static_cast<float>(settings->phSurvivalScale) / 100.0f, 0.5f, 2.0f)
        : 1.0f;
}

static float ScalePx(float value)
{
    return value * MasterScale();
}

static int ScalePxInt(int value)
{
    return (std::max)(1, static_cast<int>(std::lround(ScalePx(static_cast<float>(value)))));
}

static void RenderWindow(bool allowDrag)
{
    auto* settings = Settings::GetSingleton();
    if (!settings->enablePlayerHud || !settings->enablePlayerSurvivalWidget) return;
    if (!IsAvailable()) return;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    const int hungerStage   = GetHungerStage();
    const int coldStage     = GetColdStage();
    const int fatigueStage  = GetFatigueStage();
    const int thirstStage   = GetThirstStage();
    const int exposureStage = GetExposureStage();
    const int wetnessStage  = GetWetnessStage();

    LogNeedsBaseline(GetActiveNeeds(), coldStage, hungerStage, fatigueStage, thirstStage);

    const std::string coldLabelStr[6] = {
        LocalizationManager::Get("cold_0"), LocalizationManager::Get("cold_1"),
        LocalizationManager::Get("cold_2"), LocalizationManager::Get("cold_3"),
        LocalizationManager::Get("cold_4"), LocalizationManager::Get("cold_5") };
    const std::string hungerLabelStr[6] = {
        LocalizationManager::Get("hunger_0"), LocalizationManager::Get("hunger_1"),
        LocalizationManager::Get("hunger_2"), LocalizationManager::Get("hunger_3"),
        LocalizationManager::Get("hunger_4"), LocalizationManager::Get("hunger_5") };
    const std::string fatigueLabelStr[6] = {
        LocalizationManager::Get("fatigue_0"), LocalizationManager::Get("fatigue_1"),
        LocalizationManager::Get("fatigue_2"), LocalizationManager::Get("fatigue_3"),
        LocalizationManager::Get("fatigue_4"), LocalizationManager::Get("fatigue_5") };
    const char* kColdLabels[6]    = { coldLabelStr[0].c_str(),   coldLabelStr[1].c_str(),   coldLabelStr[2].c_str(),   coldLabelStr[3].c_str(),   coldLabelStr[4].c_str(),   coldLabelStr[5].c_str()   };
    const char* kHungerLabels[6]  = { hungerLabelStr[0].c_str(), hungerLabelStr[1].c_str(), hungerLabelStr[2].c_str(), hungerLabelStr[3].c_str(), hungerLabelStr[4].c_str(), hungerLabelStr[5].c_str() };
    const char* kFatigueLabels[6] = { fatigueLabelStr[0].c_str(),fatigueLabelStr[1].c_str(),fatigueLabelStr[2].c_str(),fatigueLabelStr[3].c_str(),fatigueLabelStr[4].c_str(),fatigueLabelStr[5].c_str()};

    auto SfLabel = [](const char* key, const char* fallback) {
        std::string v = LocalizationManager::Get(key);
        return (v.empty() || v == key) ? std::string(fallback) : v;
    };
    const std::string sfHungerLabelStr[6] = {
        SfLabel("sf_hunger_0", "Not Hungry"),
        SfLabel("sf_hunger_1", "Neutral"),
        SfLabel("sf_hunger_2", "Neutral"),
        SfLabel("sf_hunger_3", "Hungry"),
        SfLabel("sf_hunger_4", "Very Hungry"),
        SfLabel("sf_hunger_5", "Starving") };
    const char* kSfHungerLabels[6] = {
        sfHungerLabelStr[0].c_str(), sfHungerLabelStr[1].c_str(),
        sfHungerLabelStr[2].c_str(), sfHungerLabelStr[3].c_str(),
        sfHungerLabelStr[4].c_str(), sfHungerLabelStr[5].c_str() };

    const std::string sfColdLabelStr[6] = {
        SfLabel("sf_cold_0", "Warm"),        SfLabel("sf_cold_1", "Comfortable"),
        SfLabel("sf_cold_2", "Chilly"),      SfLabel("sf_cold_3", "Cold"),
        SfLabel("sf_cold_4", "Very Cold"),   SfLabel("sf_cold_5", "Freezing") };
    const char* kSfColdLabels[6] = { sfColdLabelStr[0].c_str(), sfColdLabelStr[1].c_str(),
                                     sfColdLabelStr[2].c_str(), sfColdLabelStr[3].c_str(),
                                     sfColdLabelStr[4].c_str(), sfColdLabelStr[5].c_str() };

    const std::string sfFatigueLabelStr[6] = {
        SfLabel("sf_fatigue_0", "Rested"),        SfLabel("sf_fatigue_1", "Refreshed"),
        SfLabel("sf_fatigue_2", "Slightly Tired"), SfLabel("sf_fatigue_3", "Tired"),
        SfLabel("sf_fatigue_4", "Very Tired"),     SfLabel("sf_fatigue_5", "Exhausted") };
    const char* kSfFatigueLabels[6] = { sfFatigueLabelStr[0].c_str(), sfFatigueLabelStr[1].c_str(),
                                        sfFatigueLabelStr[2].c_str(), sfFatigueLabelStr[3].c_str(),
                                        sfFatigueLabelStr[4].c_str(), sfFatigueLabelStr[5].c_str() };

    const std::string shHungerLabelStr[6] = {
        SfLabel("sh_hunger_0", "Well Fed"),  SfLabel("sh_hunger_1", "Satisfied"),
        SfLabel("sh_hunger_2", "Peckish"),   SfLabel("sh_hunger_3", "Hungry"),
        SfLabel("sh_hunger_4", "Ravenous"),  SfLabel("sh_hunger_5", "Starving") };
    const char* kShHungerLabels[6] = { shHungerLabelStr[0].c_str(), shHungerLabelStr[1].c_str(),
                                       shHungerLabelStr[2].c_str(), shHungerLabelStr[3].c_str(),
                                       shHungerLabelStr[4].c_str(), shHungerLabelStr[5].c_str() };

    const std::string shThirstLabelStr[6] = {
        SfLabel("sh_thirst_0", "Quenched"),  SfLabel("sh_thirst_1", "Sated"),
        SfLabel("sh_thirst_2", "Thirsty"),   SfLabel("sh_thirst_3", "Parched"),
        SfLabel("sh_thirst_4", "Dehydrated"),SfLabel("sh_thirst_5", "Severely Dehydrated") };
    const char* kShThirstLabels[6] = { shThirstLabelStr[0].c_str(), shThirstLabelStr[1].c_str(),
                                       shThirstLabelStr[2].c_str(), shThirstLabelStr[3].c_str(),
                                       shThirstLabelStr[4].c_str(), shThirstLabelStr[5].c_str() };

    const std::string shFatigueLabelStr[6] = {
        SfLabel("sh_fatigue_0", "Well Rested"),   SfLabel("sh_fatigue_1", "Rested"),
        SfLabel("sh_fatigue_2", "Slightly Tired"),SfLabel("sh_fatigue_3", "Tired"),
        SfLabel("sh_fatigue_4", "Weary"),         SfLabel("sh_fatigue_5", "Exhausted") };
    const char* kShFatigueLabels[6] = { shFatigueLabelStr[0].c_str(), shFatigueLabelStr[1].c_str(),
                                        shFatigueLabelStr[2].c_str(), shFatigueLabelStr[3].c_str(),
                                        shFatigueLabelStr[4].c_str(), shFatigueLabelStr[5].c_str() };

    const std::string shColdLabelStr[6] = {
        SfLabel("sh_cold_0", "Warm"),       SfLabel("sh_cold_1", "Comfortable"),
        SfLabel("sh_cold_2", "Chilly"),     SfLabel("sh_cold_3", "Cold"),
        SfLabel("sh_cold_4", "Freezing"),   SfLabel("sh_cold_5", "Frigid") };
    const char* kShColdLabels[6] = { shColdLabelStr[0].c_str(), shColdLabelStr[1].c_str(),
                                     shColdLabelStr[2].c_str(), shColdLabelStr[3].c_str(),
                                     shColdLabelStr[4].c_str(), shColdLabelStr[5].c_str() };

    const std::string lsHungerLabelStr[6] = {
        SfLabel("ls_hunger_0", "Well Fed"),    SfLabel("ls_hunger_1", "Satisfied"),
        SfLabel("ls_hunger_2", "Hungry"),      SfLabel("ls_hunger_3", "Very Hungry"),
        SfLabel("ls_hunger_4", "Ravenous"),    SfLabel("ls_hunger_5", "Starving") };
    const char* kLsHungerLabels[6] = { lsHungerLabelStr[0].c_str(), lsHungerLabelStr[1].c_str(),
                                       lsHungerLabelStr[2].c_str(), lsHungerLabelStr[3].c_str(),
                                       lsHungerLabelStr[4].c_str(), lsHungerLabelStr[5].c_str() };

    const std::string lsThirstLabelStr[6] = {
        SfLabel("ls_thirst_0", "Quenched"),    SfLabel("ls_thirst_1", "Refreshed"),
        SfLabel("ls_thirst_2", "Thirsty"),     SfLabel("ls_thirst_3", "Very Thirsty"),
        SfLabel("ls_thirst_4", "Parched"),     SfLabel("ls_thirst_5", "Dehydrated") };
    const char* kLsThirstLabels[6] = { lsThirstLabelStr[0].c_str(), lsThirstLabelStr[1].c_str(),
                                       lsThirstLabelStr[2].c_str(), lsThirstLabelStr[3].c_str(),
                                       lsThirstLabelStr[4].c_str(), lsThirstLabelStr[5].c_str() };

    const std::string lsFatigueLabelStr[6] = {
        SfLabel("ls_fatigue_0", "Rested"),     SfLabel("ls_fatigue_1", "Sharp"),
        SfLabel("ls_fatigue_2", "Tired"),      SfLabel("ls_fatigue_3", "Very Tired"),
        SfLabel("ls_fatigue_4", "Haggard"),    SfLabel("ls_fatigue_5", "Exhausted") };
    const char* kLsFatigueLabels[6] = { lsFatigueLabelStr[0].c_str(), lsFatigueLabelStr[1].c_str(),
                                        lsFatigueLabelStr[2].c_str(), lsFatigueLabelStr[3].c_str(),
                                        lsFatigueLabelStr[4].c_str(), lsFatigueLabelStr[5].c_str() };

    const std::string ffExposureLabelStr[7] = {
        SfLabel("ff_exposure_0", "Warm"),                SfLabel("ff_exposure_1", "Comfortable"),
        SfLabel("ff_exposure_2", "Cold"),                SfLabel("ff_exposure_3", "Very Cold"),
        SfLabel("ff_exposure_4", "Freezing"),            SfLabel("ff_exposure_5", "Freezing to Death"),
        SfLabel("ff_exposure_6", "Maximum Exposure") };
    const char* kFfExposureLabels[7] = { ffExposureLabelStr[0].c_str(), ffExposureLabelStr[1].c_str(),
                                         ffExposureLabelStr[2].c_str(), ffExposureLabelStr[3].c_str(),
                                         ffExposureLabelStr[4].c_str(), ffExposureLabelStr[5].c_str(),
                                         ffExposureLabelStr[6].c_str() };

    const std::string ffWetnessLabelStr[4] = {
        SfLabel("ff_wetness_0", "Not Wet"),  SfLabel("ff_wetness_1", "Damp"),
        SfLabel("ff_wetness_2", "Wet"),      SfLabel("ff_wetness_3", "Drenched") };
    const char* kFfWetnessLabels[4] = { ffWetnessLabelStr[0].c_str(), ffWetnessLabelStr[1].c_str(),
                                        ffWetnessLabelStr[2].c_str(), ffWetnessLabelStr[3].c_str() };

    const std::uint32_t alarmArgb = settings->phSurvivalAlarmColor;
    auto StageColor = [alarmArgb](std::uint32_t argb, int stage, int maxStage) -> ImU32 {
        if (maxStage > 0 && stage >= maxStage)
            return IM_COL32((alarmArgb >> 16) & 0xFF, (alarmArgb >> 8) & 0xFF, alarmArgb & 0xFF, (argb >> 24) & 0xFF);
        return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
    };

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));

    const ImVec2 targetPos(static_cast<float>(settings->phSurvivalPosX),
                           static_cast<float>(settings->phSurvivalPosY));
    if (!allowDrag || g_ForcePos) {
        ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
        g_ForcePos = false;
    } else {
        ImGui::SetNextWindowPos(targetPos, ImGuiCond_FirstUseEver);
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar      |
                             ImGuiWindowFlags_NoResize         |
                             ImGuiWindowFlags_NoScrollbar      |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoBackground;
    if (!allowDrag) flags |= ImGuiWindowFlags_NoMove;

    ImGui::Begin("##playerHUDsurvival", nullptr, flags);

    ImFont* font        = ImGui::GetFont();
    float   labelSize   = font->FontSize * settings->phSurvivalLabelScale;
    float   stageSize   = font->FontSize * settings->phSurvivalStageScale;
    float   iconSize    = ScalePx(static_cast<float>(settings->phSurvivalIconSize));
    const bool hasArc = settings->phSurvivalIndicatorStyle == 3;
    const float arcRadius = ScalePx(settings->phSurvivalArcRadius);
    const float arcThickness = ScalePx(settings->phSurvivalArcThickness);
    const float percentageSize = font->FontSize * settings->phSurvivalPercentageScale;
    const float percentageW = settings->phSurvivalShowPercentage
                            ? font->CalcTextSizeA(percentageSize, FLT_MAX, 0.0f, "100%").x : 0.0f;
    const float iconRegion = (std::max)(percentageW, hasArc ? (std::max)(iconSize, arcRadius * 2.0f + arcThickness) : iconSize);
    const float iconInset = (iconRegion - iconSize) * 0.5f;
    const float percentageH = settings->phSurvivalShowPercentage ? percentageSize + ScalePx(4.0f) : 0.0f;
    float   pipR        = ScalePx(4.0f);
    float   cardW       = ScalePx(static_cast<float>(settings->phSurvivalCardWidth));
    const bool overridePaddingX = settings->phSurvivalInnerPaddingX >= 0;
    const bool overridePaddingY = settings->phSurvivalInnerPaddingY >= 0;
    const float innerPaddingX = ScalePx(static_cast<float>(std::clamp(settings->phSurvivalInnerPaddingX, 0, 40)));
    const float innerPaddingY = ScalePx(static_cast<float>(std::clamp(settings->phSurvivalInnerPaddingY, 0, 40)));
    float   iconPad     = overridePaddingY ? innerPaddingY : ScalePx(6.0f);
    const float horizontalPadding = overridePaddingX ? innerPaddingX : 0.0f;
    const float standardW = (std::max)(cardW, iconRegion) + horizontalPadding * 2.0f;
    const ImVec2 iconOffset(ScalePx(static_cast<float>(std::clamp(settings->phSurvivalIconOffsetX, -300, 300))),
                            ScalePx(static_cast<float>(std::clamp(settings->phSurvivalIconOffsetY, -300, 300))));
    const ImVec2 labelOffset(ScalePx(static_cast<float>(std::clamp(settings->phSurvivalLabelOffsetX, -300, 300))),
                             ScalePx(static_cast<float>(std::clamp(settings->phSurvivalLabelOffsetY, -300, 300))));
    const ImVec2 stageOffset(ScalePx(static_cast<float>(std::clamp(settings->phSurvivalStageOffsetX, -300, 300))),
                             ScalePx(static_cast<float>(std::clamp(settings->phSurvivalStageOffsetY, -300, 300))));
    const ImVec2 indicatorOffset(ScalePx(static_cast<float>(std::clamp(settings->phSurvivalIndicatorOffsetX, -300, 300))),
                                 ScalePx(static_cast<float>(std::clamp(settings->phSurvivalIndicatorOffsetY, -300, 300))));
    const ImVec2 percentageOffset(ScalePx(static_cast<float>(settings->phSurvivalPercentageOffsetX)),
                                  ScalePx(static_cast<float>(settings->phSurvivalPercentageOffsetY)));
    float   textGap     = ScalePx(4.0f);
    const int indStyle  = settings->phSurvivalIndicatorStyle;
    const bool hasIndicatorRow = (indStyle == 0 || indStyle == 1);
    float   stageLineH  = 0.0f;
    float   cardH       = 0.0f;
    float   cardSpacing = ScalePx(static_cast<float>(settings->phSurvivalCardSpacing));
    float   bgRadius    = ScalePx(static_cast<float>(settings->phSurvivalBgCornerRadius));

    HUDDraw::IconFx iconFx;
    iconFx.duotone      = settings->phSurvivalIconDuotone;
    iconFx.shadow       = settings->phSurvivalIconShadow;
    iconFx.outline      = settings->phSurvivalIconOutline;
    iconFx.glow         = settings->phSurvivalIconGlow;
    iconFx.gloss        = settings->phSurvivalIconGloss;
    iconFx.outlineWidth = ScalePxInt(settings->phSurvivalIconOutlineWidth);
    iconFx.outlineColor = settings->phSurvivalIconOutlineColor;

    HUDDraw::TextFx textFx;
    textFx.shadow         = settings->phSurvivalTextShadow;
    textFx.outline        = settings->phSurvivalTextOutline;
    textFx.outlineOpacity = settings->phSurvivalTextOutlineOpacity;
    textFx.duotone        = settings->phSurvivalTextDuotone;

    struct NeedEntry {
        const char*               label;
        int                       stage;
        std::uint32_t             baseColor;
        const char* const*        stageLabels;
        int                       maxStage;
        ID3D11ShaderResourceView* icon;
        const char*               key;
    };

    const std::string needNameCold    = LocalizationManager::Get("need_cold");
    const std::string needNameHunger  = LocalizationManager::Get("need_hunger");
    const std::string needNameFatigue = LocalizationManager::Get("need_fatigue");
    const std::string needNameThirst  = LocalizationManager::Get("need_thirst");
    std::string needNameExposure = LocalizationManager::Get("need_exposure");
    if (needNameExposure.empty() || needNameExposure == "need_exposure") needNameExposure = "Exposure";
    std::string needNameWetness = LocalizationManager::Get("need_wetness");
    if (needNameWetness.empty() || needNameWetness == "need_wetness") needNameWetness = "Wetness";

    const bool sf = (g_backend == Backend::Starfrost);

    const bool ls = (g_backend == Backend::LastSeed);
    std::vector<NeedEntry> needs;
    if (g_backend == Backend::SunHelm || ls) {
        if (hungerStage  >= 0) needs.push_back({ needNameHunger.c_str(),  hungerStage,  settings->phSurvivalHungerColor,  ls ? kLsHungerLabels  : kShHungerLabels,  5, g_iconHunger,  "hunger"  });
        if (thirstStage  >= 0) needs.push_back({ needNameThirst.c_str(),  thirstStage,  settings->phSurvivalThirstColor,  ls ? kLsThirstLabels  : kShThirstLabels,  5, g_iconThirst,  "thirst"  });
        if (fatigueStage >= 0) needs.push_back({ needNameFatigue.c_str(), fatigueStage, settings->phSurvivalFatigueColor, ls ? kLsFatigueLabels : kShFatigueLabels, 5, g_iconFatigue, "fatigue" });
        if (coldStage    >= 0) needs.push_back({ needNameCold.c_str(),    coldStage,    settings->phSurvivalColdColor,    kShColdLabels,    5, g_iconCold,    "cold"    });
    } else {
        needs.push_back({ needNameCold.c_str(),    coldStage,    settings->phSurvivalColdColor,
                          sf ? kSfColdLabels   : kColdLabels,   5, g_iconCold,    "cold"    });
        needs.push_back({ needNameHunger.c_str(),  hungerStage,  settings->phSurvivalHungerColor,
                          sf ? kSfHungerLabels : kHungerLabels,
                          5,
                          g_iconHunger, "hunger" });
        needs.push_back({ needNameFatigue.c_str(), fatigueStage, settings->phSurvivalFatigueColor,
                          sf ? kSfFatigueLabels : kFatigueLabels, 5, g_iconFatigue, "fatigue" });
    }

    if (exposureStage >= 0)
        needs.push_back({ needNameExposure.c_str(), exposureStage, settings->phSurvivalColdColor,
                          kFfExposureLabels, 6, g_iconExposure, "exposure" });
    if (wetnessStage >= 0)
        needs.push_back({ needNameWetness.c_str(), wetnessStage, settings->phSurvivalColdColor,
                          kFfWetnessLabels, 3, g_iconWetness, "wetness" });

    const int needCount = static_cast<int>(needs.size());
    if (needCount == 0) { ImGui::End(); ImGui::PopStyleVar(2); ImGui::PopStyleColor(2); return; }

    auto CardHidden = [&](const NeedEntry& need) {
        return !allowDrag && settings->phSurvivalHideBelowStage > 0 &&
               need.stage < settings->phSurvivalHideBelowStage;
    };
    if (settings->phSurvivalShowStage) {
        float maxStageH = stageSize;
        for (int n = 0; n < needCount; ++n) {
            const auto& need = needs[n];
            const int   s    = std::clamp(need.stage, 0, need.maxStage);
            const char* sName = (need.stage >= 0) ? need.stageLabels[s] : "\xe2\x80\x94";
            ImVec2 sz = font->CalcTextSizeA(stageSize, FLT_MAX, cardW, sName);
            if (sz.y > maxStageH) maxStageH = sz.y;
        }
        stageLineH = maxStageH;
    }
    cardH = iconPad + iconRegion + percentageH + textGap
          + (settings->phSurvivalShowLabel ? labelSize + ScalePx(3.0f) : 0.0f)
          + stageLineH
          + (hasIndicatorRow ? pipR * 2.0f + ScalePx(8.0f) + iconPad : iconPad);

    {
        static std::unordered_map<std::string, int> s_lastStage;
        for (int n = 0; n < needCount; ++n) {
            const auto& need = needs[n];
            auto it = s_lastStage.find(need.key);
            if (it == s_lastStage.end()) {
                s_lastStage[need.key] = need.stage;
            } else if (need.stage != it->second) {
                const bool worsened = need.stage > it->second;
                it->second = need.stage;
                if (worsened)
                    UIAnimation::TriggerBounce(std::string("surv_bounce_") + need.key);
            }
        }
    }

    static std::unordered_map<std::string, float> s_cardAlpha;
    std::vector<float> cardAlpha(needCount, 1.0f);
    {
        static std::chrono::steady_clock::time_point s_lastFadeTick{};
        const auto nowTick = std::chrono::steady_clock::now();
        float dt = (s_lastFadeTick.time_since_epoch().count() != 0)
            ? std::chrono::duration<float>(nowTick - s_lastFadeTick).count()
            : 0.016f;
        s_lastFadeTick = nowTick;
        dt = std::clamp(dt, 0.0f, 0.1f);

        const float dur = (std::max)(0.1f, settings->hvFadeDuration);
        for (int n = 0; n < needCount; ++n) {
            const auto& need = needs[n];
            float& a = s_cardAlpha[need.key];
            if (CardHidden(need))
                a = (std::max)(a - dt / dur, 0.0f);
            else
                a = (std::min)(a + dt * 5.0f, 1.0f);
            cardAlpha[n] = a;
        }
    }
    {
        bool anyVisible = false;
        for (int n = 0; n < needCount; ++n) if (cardAlpha[n] > 0.004f) { anyVisible = true; break; }
        if (!anyVisible) { ImGui::End(); ImGui::PopStyleVar(2); ImGui::PopStyleColor(2); return; }
    }

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      wPos  = ImGui::GetCursorScreenPos();

    dl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);

    auto FadeCardRange = [&](int v0, int v1, float a) {
        if (a >= 0.999f) return;
        const float eased = a * a * (3.0f - 2.0f * a);
        for (int vi = v0; vi < v1 && vi < dl->VtxBuffer.Size; ++vi) {
            ImDrawVert& v = dl->VtxBuffer[vi];
            const unsigned int oldA = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
            const unsigned int newA = static_cast<unsigned int>(oldA * eased + 0.5f);
            v.col = (v.col & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
        }
    };

    const bool hasAlarmBorder = settings->phSurvivalShowBorder && (settings->phSurvivalBorderColor >> 24) != 0;
    const int alarmTarget = hasAlarmBorder ? 0 : indStyle != 2 ? 1 : 2;  // Border, Indicator, Icon
    static const auto alarmStart = std::chrono::steady_clock::now();
    const float alarmTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - alarmStart).count();
    const float alarmWave = std::sin(std::fmod(alarmTime, 3.0f) * (2.0f * 3.14159265358979323846f / 3.0f)) * 0.5f + 0.5f;
    auto PulseAlarmRange = [&](int v0, int v1, int stage, int maxStage, int target) {
        if (!settings->phSurvivalAlarmPulse || alarmTarget != target || maxStage <= 0 || stage < maxStage) return;
        // Keep opacity and critical hue steady; only a small brightness change marks urgency.
        const float brightness = 0.70f + 0.30f * alarmWave;
        for (int i = v0; i < v1; ++i) {
            ImU32& color = dl->VtxBuffer[i].col;
            const int r = static_cast<int>(((color >> IM_COL32_R_SHIFT) & 0xFF) * brightness);
            const int g = static_cast<int>(((color >> IM_COL32_G_SHIFT) & 0xFF) * brightness);
            const int b = static_cast<int>(((color >> IM_COL32_B_SHIFT) & 0xFF) * brightness);
            color = (color & IM_COL32_A_MASK) | IM_COL32(r, g, b, 0);
        }
    };

    const std::uint32_t alarmRing = settings->phSurvivalAlarmColor;
    auto DrawAlarmRing = [&](float x0, float y0, float x1, float y1, float rounding, int stage, int maxStage) {
        if (!settings->phSurvivalAlarmPulse || !hasAlarmBorder || maxStage <= 0 || stage < maxStage) return;
        const float pulse = 0.15f + 0.35f * alarmWave;
        const int   baseA = (alarmRing >> 24) & 0xFF;
        const int   r = (alarmRing >> 16) & 0xFF, g = (alarmRing >> 8) & 0xFF, b = alarmRing & 0xFF;
        ImU32 inner = IM_COL32(r, g, b, static_cast<int>(baseA * pulse));
        ImU32 bloom = IM_COL32(r, g, b, static_cast<int>(baseA * pulse * 0.4f));
        const float o1 = ScalePx(1.0f);
        const float o3 = ScalePx(3.0f);
        const float thick = ScalePx(2.0f);
        dl->AddRect(ImVec2(x0 - o1, y0 - o1), ImVec2(x1 + o1, y1 + o1), inner, rounding + o1, 0, thick);
        dl->AddRect(ImVec2(x0 - o3, y0 - o3), ImVec2(x1 + o3, y1 + o3), bloom, rounding + o3, 0, thick);
    };

    ImU32 mutedCol = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phSurvivalLabelColor));

    // Percentages describe stage severity, not a backend's continuous needs meter.
    auto DrawPercentage = [&](float centerX, float y, int stage, int maxStage, ImU32 color) {
        if (!settings->phSurvivalShowPercentage) return;
        char text[16];
        const int percent = maxStage > 0 ? static_cast<int>(std::lround(100.0f * stage / maxStage)) : 0;
        std::snprintf(text, sizeof(text), "%d%%", percent);
        const ImVec2 size = font->CalcTextSizeA(percentageSize, FLT_MAX, 0.0f, text);
        HUDDraw::TextFx fx = textFx;
        fx.font = font;
        fx.fontSize = percentageSize;
        HUDDraw::DrawTextStyled(dl, ImVec2(centerX - size.x * 0.5f + percentageOffset.x, y + percentageOffset.y), color, text, fx);
    };

    auto DrawCardBg = [&](float x0, float y0, float x1, float y1, float rounding) {
        if ((settings->phSurvivalBgColor >> 24) != 0) {
            HUDDraw::ChipStyle cs;
            cs.rounding    = rounding;
            cs.topColor    = settings->phSurvivalBgDuotone
                           ? HUDDraw::ChipTopFromArgb(settings->phSurvivalBgColor) : settings->phSurvivalBgColor;
            cs.bottomColor = settings->phSurvivalBgDuotone
                           ? HUDDraw::ChipBottomFromArgb(settings->phSurvivalBgColor) : settings->phSurvivalBgColor;
            if (!settings->phSurvivalBgTexture) cs.grainAlpha = 0;
            // Survival owns one editable stroke instead of the chip's fixed double border.
            cs.innerBorder = 0;
            cs.outerBorder = 0;
            HUDDraw::DrawChip(dl, ImVec2(x0, y0), ImVec2(x1, y1), cs);
        }
        if (settings->phSurvivalShowBorder && (settings->phSurvivalBorderColor >> 24) != 0) {
            const float width = ScalePx(std::clamp(settings->phSurvivalBorderWidth, 0.5f, 6.0f));
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1),
                        ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phSurvivalBorderColor)),
                        rounding, 0, width);
        }
    };

    auto DrawIndicator = [&](float x, float y, float w, float h,
                             int s, int maxStage, ImU32 tintedCol,
                             ID3D11ShaderResourceView* icon,
                             float iconX, float iconY, float iconSz)
    {
        // Icon Fill stays attached to iconOffset; only separate meters use this nudge.
        x += indicatorOffset.x;
        y += indicatorOffset.y;
        const int indicatorV0 = dl->VtxBuffer.Size;
        switch (indStyle) {
        case 0: {
            HUDDraw::PipStyle ps;
            ps.shape    = settings->phSurvivalPipShape;
            ps.radius   = pipR;
            ps.litColor = HUDDraw::PackedToArgb(tintedCol);
            ps.socket   = settings->phSurvivalIndicatorShadow;
            ps.contour  = settings->phSurvivalIndicatorShadow;
            ps.duotone  = settings->phSurvivalIndicatorDuotone;
            ps.glow     = settings->phSurvivalIndicatorGlow;
            ps.specular = settings->phSurvivalIndicatorHighlight;
            float pipTotalW = HUDDraw::MeasurePipRow(maxStage, ps);
            HUDDraw::DrawPipRow(dl, ImVec2(x + (w - pipTotalW) * 0.5f, y + h * 0.5f),
                                s, maxStage, ps);
            break;
        }
        case 1: {
            // Standard's full-card slot needs its own inset and thickness to read as a pill.
            const bool standardBar = settings->phSurvivalStyle != 1;
            const float inset = standardBar ? (std::min)(ScalePx(6.0f), w * 0.25f) : 0.0f;
            x += inset;
            w -= inset * 2.0f;
            float barH   = standardBar ? ScalePx(8.0f) : (std::max)(ScalePx(4.0f), h * 0.35f);
            float barY   = y + (h - barH) * 0.5f;
            float frac   = (maxStage > 0) ? static_cast<float>(s) / static_cast<float>(maxStage) : 0.0f;
            HUDDraw::BarStyle bar;
            bar.width = w;
            bar.height = barH;
            bar.rounding = barH * 0.5f;
            bar.fraction = frac;
            bar.fillColor = HUDDraw::PackedToArgb(tintedCol);
            bar.shadow = settings->phSurvivalIndicatorShadow;
            bar.trackDepth = settings->phSurvivalIndicatorShadow;
            bar.duotone = settings->phSurvivalIndicatorDuotone;
            bar.glow = settings->phSurvivalIndicatorGlow;
            bar.highlight = settings->phSurvivalIndicatorHighlight;
            bar.edgeLight = settings->phSurvivalIndicatorHighlight;
            // Highlight shades the fill edge; Glow alone owns the bloom.
            bar.tip = false;
            bar.trackTop = settings->phSurvivalIndicatorDuotone ? 0xC8141820 : 0xC8282C36;
            bar.trackBottom = settings->phSurvivalIndicatorDuotone ? 0xC8383E4A : bar.trackTop;
            HUDDraw::DrawBarStyled(dl, ImVec2(x, barY), bar);
            break;
        }
        case 3: {
            HUDDraw::TimerArcStyle arc;
            arc.radius = arcRadius;
            arc.thickness = arcThickness;
            arc.fraction = maxStage > 0 ? static_cast<float>(s) / maxStage : 0.0f;
            arc.fillColor = HUDDraw::PackedToArgb(tintedCol);
            arc.startAngle = settings->phSurvivalArcRotation - settings->phSurvivalArcSweep * 0.5f;
            arc.sweepAngle = static_cast<float>(settings->phSurvivalArcSweep);
            arc.centeredFill = true;
            arc.roundCaps = true;
            arc.shadow = settings->phSurvivalIndicatorShadow;
            arc.trackDepth = settings->phSurvivalIndicatorShadow;
            arc.duotone = settings->phSurvivalIndicatorDuotone;
            arc.glow = settings->phSurvivalIndicatorGlow;
            arc.edgeLight = settings->phSurvivalIndicatorHighlight;
            arc.tip = false;
            const ImVec2 center(iconX + iconSz * 0.5f + indicatorOffset.x,
                                iconY + iconSz * 0.5f + indicatorOffset.y);
            const ImU32 track = (tintedCol & 0x00FFFFFF) | (static_cast<ImU32>((tintedCol >> 24) * 0.25f) << 24);
            arc.trackColor = HUDDraw::PackedToArgb(track);
            HUDDraw::DrawTimerArc(dl, center, arc);
            break;
        }
        default: break;
        }
        PulseAlarmRange(indicatorV0, dl->VtxBuffer.Size, s, maxStage, 1);
        const int iconFillV0 = dl->VtxBuffer.Size;
        if (settings->phSurvivalIconFill) {
            if (icon) {
                ImU32 ghostCol = (tintedCol & 0x00FFFFFF) | 0x26000000;
                dl->AddImage(reinterpret_cast<ImTextureID>(icon),
                    ImVec2(iconX, iconY), ImVec2(iconX + iconSz, iconY + iconSz),
                    ImVec2(0, 0), ImVec2(1, 1), ghostCol);

                float frac     = (maxStage > 0) ? static_cast<float>(s) / static_cast<float>(maxStage) : 0.0f;
                float clipTop  = iconY + iconSz * (1.0f - frac);
                if (frac > 0.0f) {
                    dl->PushClipRect(ImVec2(iconX, clipTop), ImVec2(iconX + iconSz, iconY + iconSz), true);
                    HUDDraw::DrawIconStyled(dl, ImVec2(iconX, iconY), iconSz,
                        reinterpret_cast<ImTextureID>(icon),
                        HUDDraw::PackedToArgb(tintedCol), iconFx);
                    dl->PopClipRect();
                }
            }
        }
        PulseAlarmRange(iconFillV0, dl->VtxBuffer.Size, s, maxStage, 2);
    };

    const bool isCompact   = (settings->phSurvivalStyle     == 1);
    const bool isVertical  = (settings->phSurvivalDirection == 1);

    if (isCompact) {
        // ── Compact style: [icon] [label/stage left-aligned] [indicator] ──────
        float rowPadX  = overridePaddingX ? innerPaddingX : ScalePx(8.0f);
        float rowPadY  = overridePaddingY ? innerPaddingY : ScalePx(5.0f);
        float textGapX = ScalePx(6.0f);
        float indGapX  = ScalePx(8.0f);
        float rowH     = iconRegion + percentageH + rowPadY * 2.0f;
        const bool iconAttachedIndicator = !hasIndicatorRow;
        float indW     = (indStyle == 0)
                          ? (static_cast<float>(needs[0].maxStage) * (pipR * 2.0f + ScalePx(2.0f))) - ScalePx(2.0f)
                          : iconAttachedIndicator ? 0.0f
                          : ScalePx(60.0f);
        float rowW     = rowPadX + iconRegion + textGapX + cardW
                       + (iconAttachedIndicator ? 0.0f : indGapX + indW)
                       + rowPadX;

        for (int n = 0; n < needCount; ++n) {
            float rowX = isVertical ? wPos.x                          : wPos.x + n * (rowW + cardSpacing);
            float rowY = isVertical ? wPos.y + n * (rowH + cardSpacing) : wPos.y;

            const auto& need = needs[n];
            if (cardAlpha[n] <= 0.004f) continue;
            const int   s    = std::clamp(need.stage, 0, need.maxStage);
            ImU32 tintedCol  = StageColor(need.baseColor, s, need.maxStage);

            const int cardV0 = dl->VtxBuffer.Size;

            const float rowRounding = std::min(bgRadius, rowH * 0.5f);
            DrawCardBg(rowX, rowY, rowX + rowW, rowY + rowH, rowRounding);
            DrawAlarmRing(rowX, rowY, rowX + rowW, rowY + rowH, rowRounding, need.stage, need.maxStage);

            float cx = rowX + rowPadX + iconInset;
            float cy = rowY + rowPadY + iconInset;

            // Icon Fill needs a faint base so the clipped fill remains visible.
            bool  iconIsGhost = settings->phSurvivalIconFill;
            ImU32 iconDrawCol = iconIsGhost ? ((tintedCol & 0x00FFFFFF) | 0x26000000) : tintedCol;
            float bounceY = UIAnimation::GetBounceOffsetY(std::string("surv_bounce_") + need.key) * MasterScale();
            const int iconV0 = dl->VtxBuffer.Size;
            if (need.icon) {
                if (iconIsGhost)
                    dl->AddImage(reinterpret_cast<ImTextureID>(need.icon), ImVec2(cx + iconOffset.x, cy + iconOffset.y + bounceY), ImVec2(cx + iconOffset.x + iconSize, cy + iconOffset.y + bounceY + iconSize),
                                 ImVec2(0, 0), ImVec2(1, 1), iconDrawCol);
                else
                    HUDDraw::DrawIconStyled(dl, ImVec2(cx + iconOffset.x, cy + iconOffset.y + bounceY), iconSize,
                                            (ImTextureID)need.icon, HUDDraw::PackedToArgb(iconDrawCol), iconFx);
            }
            PulseAlarmRange(iconV0, dl->VtxBuffer.Size, s, need.maxStage, 2);
            DrawPercentage(rowX + rowPadX + iconRegion * 0.5f,
                           rowY + rowPadY + iconRegion + ScalePx(4.0f), s, need.maxStage, tintedCol);
            cx = rowX + rowPadX + iconRegion + textGapX;

            float textMidY = cy + (iconSize - labelSize - stageSize - ScalePx(2.0f)) * 0.5f;
            if (settings->phSurvivalShowLabel) {
                HUDDraw::TextFx lblFx = textFx;
                lblFx.font = font; lblFx.fontSize = labelSize;
                HUDDraw::DrawTextStyled(dl, ImVec2(cx + labelOffset.x, textMidY + labelOffset.y), mutedCol, need.label, lblFx);
            }
            if (settings->phSurvivalShowStage) {
                const char* stageName = (need.stage >= 0) ? need.stageLabels[s] : "\xe2\x80\x94";
                HUDDraw::TextFx stgFx = textFx;
                stgFx.font = font; stgFx.fontSize = stageSize;
                HUDDraw::DrawTextStyledWrapped(dl, ImVec2(cx + stageOffset.x, textMidY + labelSize + ScalePx(2.0f) + stageOffset.y), cardW,
                                               tintedCol, stageName, stgFx);
            }

            cx += cardW + (iconAttachedIndicator ? 0.0f : indGapX);
            DrawIndicator(cx, rowY, indW, rowH, s, need.maxStage, tintedCol,
                          need.icon, rowX + rowPadX + iconInset + iconOffset.x, cy + iconOffset.y, iconSize);

            FadeCardRange(cardV0, dl->VtxBuffer.Size, cardAlpha[n]);
        }

        if (isVertical) {
            float totalH = needCount * rowH + (needCount - 1) * cardSpacing;
            ImGui::Dummy(ImVec2(rowW, totalH));
        } else {
            float totalW = needCount * rowW + (needCount - 1) * cardSpacing;
            ImGui::Dummy(ImVec2(totalW, rowH));
        }

    } else {
        for (int n = 0; n < needCount; ++n) {
            // Horizontal: cards side-by-side. Vertical: cards stacked.
            float cardX = isVertical ? wPos.x                            : wPos.x + n * (standardW + cardSpacing);
            float cardY = isVertical ? wPos.y + n * (cardH + cardSpacing) : wPos.y;

            const auto& need = needs[n];
            if (cardAlpha[n] <= 0.004f) continue;
            const int   s    = std::clamp(need.stage, 0, need.maxStage);
            ImU32 tintedCol  = StageColor(need.baseColor, s, need.maxStage);

            const int cardV0 = dl->VtxBuffer.Size;

            const float cardRounding = std::min(bgRadius, cardH * 0.5f);
            DrawCardBg(cardX, cardY, cardX + standardW, cardY + cardH, cardRounding);
            DrawAlarmRing(cardX, cardY, cardX + standardW, cardY + cardH, cardRounding, need.stage, need.maxStage);

            float drawY = cardY + iconPad + iconInset;
            float iconX = cardX + (standardW - iconSize) * 0.5f + iconOffset.x;

            const int iconV0 = dl->VtxBuffer.Size;
            if (need.icon) {
                // Icon Fill uses a faint base; the clipped pass below adds the lit portion.
                bool  iconIsGhost = settings->phSurvivalIconFill;
                ImU32 iconDrawCol = iconIsGhost ? ((tintedCol & 0x00FFFFFF) | 0x26000000) : tintedCol;
                // Bounce shifts only the icon draw, not drawY (text stays put).
                float bounceY = UIAnimation::GetBounceOffsetY(std::string("surv_bounce_") + need.key) * MasterScale();
                if (iconIsGhost)
                    dl->AddImage(reinterpret_cast<ImTextureID>(need.icon),
                        ImVec2(iconX, drawY + iconOffset.y + bounceY), ImVec2(iconX + iconSize, drawY + iconOffset.y + bounceY + iconSize),
                        ImVec2(0, 0), ImVec2(1, 1), iconDrawCol);
                else
                    HUDDraw::DrawIconStyled(dl, ImVec2(iconX, drawY + iconOffset.y + bounceY), iconSize,
                                            (ImTextureID)need.icon, HUDDraw::PackedToArgb(iconDrawCol), iconFx);
            }
            PulseAlarmRange(iconV0, dl->VtxBuffer.Size, s, need.maxStage, 2);
            DrawPercentage(cardX + standardW * 0.5f, cardY + iconPad + iconRegion + ScalePx(4.0f), s, need.maxStage, tintedCol);
            drawY = cardY + iconPad + iconRegion + percentageH + textGap;

            if (settings->phSurvivalShowLabel) {
                ImVec2 nameSz = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, need.label);
                float labelX = cardX + (standardW - nameSz.x) * 0.5f + labelOffset.x;
                HUDDraw::TextFx lblFx = textFx;
                lblFx.font = font; lblFx.fontSize = labelSize;
                HUDDraw::DrawTextStyled(dl, ImVec2(labelX, drawY + labelOffset.y), mutedCol, need.label, lblFx);
                drawY += labelSize + ScalePx(3.0f);
            }

            if (settings->phSurvivalShowStage) {
                const char* stageName = (need.stage >= 0) ? need.stageLabels[s] : "\xe2\x80\x94";
                HUDDraw::TextFx stgFx = textFx;
                stgFx.font = font; stgFx.fontSize = stageSize;
                HUDDraw::DrawTextStyledWrapped(dl, ImVec2(cardX + (standardW - cardW) * 0.5f + stageOffset.x, drawY + stageOffset.y), cardW,
                                               tintedCol, stageName, stgFx, true);
            }

            float indY      = cardY + cardH - pipR * 2.0f - iconPad - ScalePx(3.0f);
            float indH      = pipR * 2.0f + ScalePx(4.0f);
            float iconDrawY = cardY + iconPad + iconInset + iconOffset.y;
            DrawIndicator(cardX + (standardW - cardW) * 0.5f, indY, cardW, indH, s, need.maxStage, tintedCol,
                          need.icon, iconX, iconDrawY, iconSize);

            FadeCardRange(cardV0, dl->VtxBuffer.Size, cardAlpha[n]);
        }

        if (isVertical) {
            float totalH = needCount * cardH + (needCount - 1) * cardSpacing;
            ImGui::Dummy(ImVec2(standardW, totalH));
        } else {
            float totalW = needCount * standardW + (needCount - 1) * cardSpacing;
            ImGui::Dummy(ImVec2(totalW, cardH));
        }
    }

    dl->PopClipRect();

    if (allowDrag) {
        ImVec2 winPos = ImGui::GetWindowPos();
        settings->phSurvivalPosX = static_cast<int>(winPos.x);
        settings->phSurvivalPosY = static_cast<int>(winPos.y);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void ForceReposition() { g_ForcePos = true; }

void RenderHUD()
{
    auto* settings = Settings::GetSingleton();
    if (!settings->enablePlayerHud || !settings->enablePlayerSurvivalWidget) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderWindow(false);
    ImGui::EndFrame();
    ImGui::Render();
    ImGuiRenderer::RenderToBoundTarget(ImGui::GetDrawData());
}

void RenderPreview() { RenderWindow(true); }

}
