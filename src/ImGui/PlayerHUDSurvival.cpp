#include "../../includes/ImGui/PlayerHUDSurvival.h"
#include "../../includes/ImGui/PlayerHUDEnvironment.h"
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
#include <atomic>
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
static constexpr const char* kRndESP       = "RealisticNeedsandDiseases.esp";

enum class Backend  { None, VanillaCC, Starfrost, SunHelm, Frostfall, LastSeed, RND };
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
      { 0x068119 },
      { 0x012C12, 0x004E56, 0x004E57, 0x004E5A, 0x004E5C, 0x004E5E }, kFrostfallESP, nullptr },
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

static const AxisDef kRndAxes[] = {
    { Axis::Hunger,  "hunger",  ReadMode::SpellArray, 5, 0, true,
      { 0x001D97, 0x001D99, 0x001D9B, 0x001D9D, 0x00231B, 0x00231C }, {}, kRndESP, nullptr },
    { Axis::Thirst,  "thirst",  ReadMode::SpellArray, 4, 0, true,
      { 0x001DA1, 0x001DA3, 0x001DA5, 0x002DFA, 0x002DFC, 0 }, {}, kRndESP, nullptr },
    { Axis::Fatigue, "fatigue", ReadMode::SpellArray, 4, 0, false,
      { 0x00BACD, 0x00231E, 0x002320, 0x001DB3, 0x001DB5, 0 }, {}, kRndESP, nullptr },
};

static constexpr const char* kSmiESP = "SurvivalModeImproved.esp";

struct RawDef {
    const char* key;
    RE::FormID  value;
    RE::FormID  max;
    const char* plugin;
    RE::FormID  stageIds[5];
    const char* stagePlugin;
    float       steps[5];
    float       fixedMax;
};

static const RawDef kCcRaw[] = {
    { "hunger",  0x81A, 0x80C, kSurvivalESL, { 0x806, 0x802, 0x803, 0x804, 0x805 }, kSurvivalESL, {}, 0 },
    { "fatigue", 0x816, 0x84A, kSurvivalESL, { 0xA17, 0xA18, 0xA19, 0xA1A, 0xA1B }, kSmiESP, {}, 0 },
    { "cold",    0x81B, 0x84B, kSurvivalESL, { 0xD20, 0xD21, 0xD22, 0xD23, 0xD24 }, kSmiESP, {}, 0 },
};

static const RawDef kSunHelmRaw[] = {
    { "hunger",  0x00EAAE, 0, kSunHelmESP, {}, nullptr, { 40, 80, 140, 240, 360 }, 360 },
    { "thirst",  0x05C472, 0, kSunHelmESP, {}, nullptr, { 40, 60, 120, 180, 300 }, 300 },
    { "fatigue", 0x021E3F, 0, kSunHelmESP, {}, nullptr, { 80, 160, 240, 360, 480 }, 480 },
    { "cold",    0x6A13C5, 0, kSunHelmESP, {}, nullptr, { 25, 150, 250, 450, 700 }, 700 },
};

static const RawDef kLastSeedRaw[] = {
    { "hunger",  0x0029CD, 0, kLastSeedESP, {}, nullptr, { 20, 40, 60, 80, 100 }, 120 },
    { "thirst",  0x0029CE, 0, kLastSeedESP, {}, nullptr, { 20, 40, 60, 80, 100 }, 120 },
    { "fatigue", 0x0029CF, 0, kLastSeedESP, {}, nullptr, { 20, 40, 60, 80, 100 }, 120 },
};

static const RawDef kRndRaw[] = {
    { "hunger",  0x002884, 0x002DEE, kRndESP, { 0x002DEA, 0x002DEB, 0x002DEC, 0x002DED, 0x002DEE }, kRndESP, {}, 0 },
    { "thirst",  0x002E07, 0x002E0E, kRndESP, { 0x002E0B, 0x002E0C, 0x002E0D, 0x002E0E, 0 }, kRndESP, {}, 0 },
    { "fatigue", 0x00BAC2, 0x00BAC8, kRndESP, { 0x00BAC5, 0x00BAC6, 0x00BAC7, 0x00BAC8, 0 }, kRndESP, {}, 0 },
};

struct ResolvedRaw {
    const RawDef*  def      = nullptr;
    RE::TESGlobal* value    = nullptr;
    RE::TESGlobal* max      = nullptr;
    RE::TESGlobal* stage[5] = {};
};
static std::vector<ResolvedRaw> g_raw;
static RE::TESGlobal* g_smiAmbientTemp = nullptr;
static RE::TESGlobal* g_compassTempLevel = nullptr;
static RE::BGSListForm* g_smiHeatObjects = nullptr;
static std::atomic<bool> g_smiNearHeat{ false };

struct SunHelmAmbient {
    RE::TESGlobal* temperature  = nullptr;
    RE::TESGlobal* nearHeat     = nullptr;
    RE::TESGlobal* interiorType = nullptr;
    RE::TESGlobal* coldActive   = nullptr;
    RE::TESGlobal* inWater      = nullptr;
};
static SunHelmAmbient g_shAmbient;

struct FrostfallAmbient {
    RE::TESGlobal* nearFire    = nullptr;
    RE::TESGlobal* shelter     = nullptr;
    RE::TESGlobal* temperature = nullptr;
    RE::TESGlobal* exposure    = nullptr;
    RE::TESGlobal* target      = nullptr;
    RE::TESGlobal* heatSize    = nullptr;
    RE::TESGlobal* wetness     = nullptr;
};
static FrostfallAmbient g_ffAmbient;
static RE::TESGlobal* g_vitalityValue  = nullptr;
static RE::TESGlobal* g_vitalityTarget = nullptr;
static constexpr std::uint32_t kVitalityColor = 0xFFD24B46;
static constexpr ImU32 kVitalityIconColor = IM_COL32(100, 200, 120, 255);

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

static bool           g_rndActive  = false;
static RE::TESGlobal* g_rndRunning = nullptr;

static bool RndLive()
{
    return g_rndActive && g_rndRunning && g_rndRunning->value == 1.0f;
}

static RE::TESGlobal* g_rndState  = nullptr;
static bool           g_rndSeenOn = false;

static bool RndStateOn() { return g_rndState && g_rndState->value == 1.0f; }

enum class Stack { RND, LastSeed, Frostfall, SunHelmCold, RndNotStarted, Count };
static bool g_stackFired[static_cast<int>(Stack::Count)] = {};
static std::chrono::steady_clock::time_point g_stackNextCheck{};

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
        case Backend::RND:       return "RND";
        default:                return "None";
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
                    ra.spells[i] = LookupSpell(dh, def.idsAlt[i], def.plugin);
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

static RE::TESGlobal* LookupGlobal(RE::TESDataHandler* dh, RE::FormID id, const char* plugin)
{
    if (!id || !plugin || !dh->LookupModByName(plugin)) return nullptr;
    auto form = dh->LookupForm(id, plugin);
    return form ? form->As<RE::TESGlobal>() : nullptr;
}

static void BuildRaw(RE::TESDataHandler* dh)
{
    g_raw.clear();
    const RawDef* defs = nullptr;
    int count = 0;
    if (g_backend == Backend::VanillaCC || g_backend == Backend::Starfrost) {
        defs = kCcRaw; count = static_cast<int>(std::size(kCcRaw));
    } else if (g_backend == Backend::SunHelm) {
        defs = kSunHelmRaw; count = static_cast<int>(std::size(kSunHelmRaw));
    } else if (g_backend == Backend::LastSeed) {
        defs = kLastSeedRaw; count = static_cast<int>(std::size(kLastSeedRaw));
    } else if (g_backend == Backend::RND) {
        defs = kRndRaw; count = static_cast<int>(std::size(kRndRaw));
    }
    for (int i = 0; i < count; ++i) {
        ResolvedRaw r;
        r.def   = &defs[i];
        r.value = LookupGlobal(dh, defs[i].value, defs[i].plugin);
        r.max   = LookupGlobal(dh, defs[i].max, defs[i].plugin);
        for (int s = 0; s < 5; ++s)
            r.stage[s] = LookupGlobal(dh, defs[i].stageIds[s], defs[i].stagePlugin);
        if (r.value && (r.max || defs[i].fixedMax > 0.0f)) g_raw.push_back(r);
    }
    g_smiAmbientTemp = (g_backend == Backend::VanillaCC || g_backend == Backend::Starfrost)
        ? LookupGlobal(dh, 0xD1D, kSmiESP) : nullptr;
    auto* heatObjects = g_smiAmbientTemp ? dh->LookupForm(0x8AA, kSurvivalESL) : nullptr;
    g_smiHeatObjects = heatObjects ? heatObjects->As<RE::BGSListForm>() : nullptr;
    g_smiNearHeat = false;
    g_compassTempLevel = (g_backend == Backend::VanillaCC || g_backend == Backend::Starfrost) ? LookupGlobal(dh, 0x2EDD, "Update.esm")
                       : g_backend == Backend::SunHelm ? LookupGlobal(dh, 0x6B57CE, "SunHelmSurvival.esp") : nullptr;
    g_shAmbient = {};
    if (g_backend == Backend::SunHelm) {
        g_shAmbient.temperature  = LookupGlobal(dh, 0x989757, kSunHelmESP);
        g_shAmbient.nearHeat     = LookupGlobal(dh, 0x88743B, kSunHelmESP);
        g_shAmbient.interiorType = LookupGlobal(dh, 0x98975B, kSunHelmESP);
        g_shAmbient.coldActive   = LookupGlobal(dh, 0x79441D, kSunHelmESP);
        g_shAmbient.inWater      = LookupGlobal(dh, 0x88743C, kSunHelmESP);
    }
    g_ffAmbient = {};
    if (g_frostfallActive) {
        g_ffAmbient.nearFire    = LookupGlobal(dh, 0x064AFD, kFrostfallESP);
        g_ffAmbient.shelter     = LookupGlobal(dh, 0x068118, kFrostfallESP);
        g_ffAmbient.temperature = LookupGlobal(dh, 0x0665F9, kFrostfallESP);
        g_ffAmbient.exposure    = LookupGlobal(dh, 0x00183D, kFrostfallESP);
        g_ffAmbient.target      = LookupGlobal(dh, 0x0805E4, kFrostfallESP);
        g_ffAmbient.heatSize    = LookupGlobal(dh, 0x064AFC, kFrostfallESP);
        g_ffAmbient.wetness     = LookupGlobal(dh, 0xCC0E04, "Update.esm");
    }
    logger::debug("PlayerHUDSurvival: {} raw need values resolved.", g_raw.size());
}

static float RawNeedFill(const char* key)
{
    if (!strcmp(key, "exposure")) return g_ffAmbient.exposure ? std::clamp(g_ffAmbient.exposure->value / 120.0f, 0.0f, 1.0f) : -1.0f;
    if (!strcmp(key, "wetness"))  return g_ffAmbient.wetness  ? std::clamp(g_ffAmbient.wetness->value / 750.0f, 0.0f, 1.0f) : -1.0f;
    for (const auto& r : g_raw) {
        if (strcmp(r.def->key, key) != 0) continue;
        const float maxV = r.max ? r.max->value : r.def->fixedMax;
        return maxV > 0.0f ? std::clamp(r.value->value / maxV, 0.0f, 1.0f) : -1.0f;
    }
    return -1.0f;
}

static void FillFromRaw(PlayerHUDSurvival::NeedStatus& ns)
{
    ns.fill = RawNeedFill(ns.key);
    for (const auto& r : g_raw) {
        if (strcmp(r.def->key, ns.key) != 0) continue;
        const float maxV = r.max ? r.max->value : r.def->fixedMax;
        if (maxV <= 0.0f) return;
        if (ns.stage < 0 || ns.stage >= ns.maxStage || ns.stage >= 5) return;
        float next = r.def->steps[ns.stage];
        if (r.stage[ns.stage] && r.stage[ns.stage]->value > 0.0f) next = r.stage[ns.stage]->value;
        if (next > 0.0f) ns.nextFill = std::clamp(next / maxV, 0.0f, 1.0f);
        return;
    }
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

    g_backend    = Backend::None;
    g_rndActive  = false;
    g_rndRunning = nullptr;
    g_rndState   = nullptr;
    if (dh->LookupModByName(kRndESP)) {
        auto st = dh->LookupForm(RE::FormID(0x012C4C), kRndESP);
        g_rndState = st ? st->As<RE::TESGlobal>() : nullptr;
    }
    g_rndSeenOn = RndStateOn();

    if (dh->LookupModByName(kLastSeedESP) &&
        BuildAxes(dh, kLastSeedAxes, static_cast<int>(std::size(kLastSeedAxes)), axes)) {
        g_lastSeedActive = true;
        g_backend        = Backend::LastSeed;
        g_availRule      = AvailRule::BackendGlobal;
        auto lsRun = dh->LookupForm(RE::FormID(0x00B162), kLastSeedESP);
        g_lastSeedRunning = lsRun ? lsRun->As<RE::TESGlobal>() : nullptr;
        logger::debug("PlayerHUDSurvival: LastSeed.esp detected - needs backend (running={}).",
                      g_lastSeedRunning ? static_cast<int>(g_lastSeedRunning->value) : -1);
    } else if (g_rndSeenOn &&
               BuildAxes(dh, kRndAxes, static_cast<int>(std::size(kRndAxes)), axes)) {
        g_rndActive  = true;
        g_rndRunning = g_rndState;
        g_backend   = Backend::RND;
        g_availRule = AvailRule::BackendGlobal;
        logger::debug("PlayerHUDSurvival: {} running - needs backend.", kRndESP);
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
        g_vitalityValue  = LookupGlobal(dh, 0x004FC4, kLastSeedESP);
        g_vitalityTarget = LookupGlobal(dh, 0x44B08A, kLastSeedESP);
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

    for (auto& f : g_stackFired) f = false;
    g_stackNextCheck = std::chrono::steady_clock::now() + std::chrono::seconds(10);

    if (axes.empty()) {
        g_backend = Backend::None;
        g_raw.clear();
        logger::debug("PlayerHUDSurvival: no survival backend found - survival cluster disabled.");
        return;
    }

    g_axes = std::move(axes);
    BuildRaw(dh);

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
        if (g_backend == Backend::LastSeed) return LastSeedLive();
        if (g_backend == Backend::RND)      return RndLive();
        return FrostfallLive();
    }
    return false;
}

static int ReadAxisStage(RE::Actor* player, const ResolvedAxis& a)
{
    switch (a.def->mode) {
    case ReadMode::SpellArray: {
        if (a.def->plugin == kRndESP && !RndLive()) return -1;
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

static int MaxStageFor(Axis axis)
{
    for (const auto& a : g_axes)
        if (a.def->axis == axis) return a.def->maxStage;
    return 5;
}

static bool CcOn() { return g_modeEnabled && g_modeEnabled->value == 1.0f; }

static bool ConflictHolds(Stack s)
{
    switch (s) {
        case Stack::RND:       return CcOn() && RndLive();
        case Stack::LastSeed:  return CcOn() && LastSeedLive();
        case Stack::Frostfall: return CcOn() && FrostfallLive();
        case Stack::SunHelmCold:
            return g_backend == Backend::SunHelm && FrostfallLive() && GetStage(Axis::Cold) >= 0;
        case Stack::RndNotStarted:
            return g_rndState && !RndStateOn() && g_backend != Backend::LastSeed;
        default: return false;
    }
}

static const char* StackKey(Stack s)
{
    switch (s) {
        case Stack::RND:         return "Survival.StackWarnRND";
        case Stack::LastSeed:    return "Survival.StackWarnLastSeed";
        case Stack::Frostfall:   return "Survival.StackWarnFrostfall";
        case Stack::SunHelmCold: return "Survival.StackWarnSunHelmCold";
        case Stack::RndNotStarted: return "Survival.RndNotStarted";
        default:                 return "";
    }
}

static void TickStackWarning()
{
    const auto now = std::chrono::steady_clock::now();
    if (now < g_stackNextCheck) return;
    g_stackNextCheck = now + std::chrono::seconds(5);

    if (g_rndState && g_backend != Backend::LastSeed && RndStateOn() != g_rndSeenOn) {
        logger::info("PlayerHUDSurvival: RND_State changed to {}, reselecting backend.",
                     static_cast<int>(g_rndState->value));
        Init();
        return;
    }

    auto* settings = Settings::GetSingleton();
    if (!settings || !settings->phSurvivalStackWarning) return;
    for (int i = 0; i < static_cast<int>(Stack::Count); ++i) {
        const auto s = static_cast<Stack>(i);
        if (g_stackFired[i] || !ConflictHolds(s)) continue;
        g_stackFired[i] = true;
        std::string text = LocalizationManager::Get(StackKey(s));
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([text = std::move(text)]() {
                RE::SendHUDMessage::ShowHUDMessage(text.c_str());
            });
        }
        break;
    }
}

std::string GetStackWarning()
{
    for (int i = 0; i < static_cast<int>(Stack::Count); ++i)
        if (ConflictHolds(static_cast<Stack>(i)))
            return LocalizationManager::Get(StackKey(static_cast<Stack>(i)));
    return {};
}

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
    if (g_backend == Backend::RND) {
        static const char* rndFall[3][6] = {
            { "Gluttony", "Satiated", "Peckish", "Hungry", "Very Hungry", "Starving" },
            { "Quenched", "Slightly Thirsty", "Thirsty", "Very Thirsty", "Dehydrated", "Dehydrated" },
            { "Not Tired", "Slightly Tired", "Tired", "Very Tired", "Exhausted", "Exhausted" },
        };
        int row = !strcmp(key, "hunger") ? 0 : !strcmp(key, "thirst") ? 1 : 2;
        snprintf(buf, sizeof(buf), "rnd_%s_%d", key, s);
        return L(buf, rndFall[row][s]);
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

static void FillColdScale(AmbientCold& out, const ResolvedRaw& cold, float ambient)
{
    const float max = cold.max ? cold.max->value : cold.def->fixedMax;
    if (max <= 0.0f) return;
    out.selfFill = std::clamp(cold.value->value / max, 0.0f, 1.0f);
    out.areaFill = std::clamp(ambient / max, 0.0f, 1.0f);
}

static void FillStageRange(AmbientCold& out, const ResolvedRaw& cold)
{
    const float max = cold.max ? cold.max->value : cold.def->fixedMax;
    if (max <= 0.0f || out.areaFill < 0.0f) return;
    out.stageStart = 0.0f;
    out.stageEnd = 1.0f;
    for (int i = 0; i < 5; ++i) {
        const float step = (cold.stage[i] ? cold.stage[i]->value : cold.def->steps[i]) / max;
        if (step <= 0.0f) continue;
        if (out.areaFill >= step) out.stageStart = (std::min)(step, 1.0f);
        else { out.stageEnd = step; break; }
    }
}

static std::optional<AmbientCold> GetSunHelmAmbient(int compassLevel)
{
    if (g_shAmbient.coldActive && g_shAmbient.coldActive->value == 0.0f) return std::nullopt;

    AmbientCold out;
    out.nearFire = g_shAmbient.nearHeat && g_shAmbient.nearHeat->value > 0.0f;

    const bool warmInterior = g_shAmbient.interiorType && g_shAmbient.interiorType->value == 0.0f;
    const float ambient = warmInterior ? 0.0f : g_shAmbient.temperature->value;
    out.selfStage = GetColdStage();
    out.stage = 0;
    for (const auto& r : g_raw) {
        if (strcmp(r.def->key, "cold") != 0) continue;
        for (float step : r.def->steps)
            if (ambient >= step) ++out.stage;
        FillColdScale(out, r, ambient);
    }

    constexpr float kFrostbiteAmbient = 600.0f;
    constexpr int kFrigid = 5;
    if (ambient >= kFrostbiteAmbient) {
        out.stage = kFrigid;
        if (out.areaFill >= 0.0f) out.areaFill = 1.0f;
    }

    constexpr int kFreezing = 4;
    if (compassLevel == kFreezing) out.stage = (std::max)(out.stage, kFreezing);

    out.inFreezingWater = g_shAmbient.inWater && g_shAmbient.inWater->value > 0.0f;
    if (out.inFreezingWater && out.areaFill >= 0.0f) out.areaFill = 1.0f;

    for (const auto& r : g_raw)
        if (strcmp(r.def->key, "cold") == 0) FillStageRange(out, r);

    out.coldHeld = warmInterior && !out.nearFire && !out.inFreezingWater;

    if (out.selfFill >= 0.0f) {
        constexpr float kLevel = 0.0015f;
        const bool byFire = out.nearFire && !out.inFreezingWater;
        const float diff = (byFire ? 0.0f : out.areaFill) - out.selfFill;
        if (!out.coldHeld) out.trend = diff > kLevel ? 1 : (diff < -kLevel ? -1 : 0);
    } else if (compassLevel == 1 || compassLevel == 2) {
        out.trend = -1;
    } else if (compassLevel >= 3 && !out.nearFire) {
        out.trend = 1;
    }
    if (out.trend > 0 && out.stage == 0 && out.selfStage <= 0 && !out.inFreezingWater) out.trend = 0;
    return out;
}

static bool ScanForSmiHeat()
{
    constexpr float kSmiHeatRange = 580.0f;
    auto* tes = RE::TES::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!tes || !player || !player->GetParentCell() || !g_smiHeatObjects) return false;
    auto* state = player->AsActorState();
    if (player->IsRunning() || state->IsSprinting() || state->IsSwimming()) return false;

    bool found = false;
    auto checkRef = [&](RE::TESObjectREFR* ref) {
        auto* base = ref && !ref->IsDisabled() && ref->Is3DLoaded() ? ref->GetBaseObject() : nullptr;
        if (!base || !g_smiHeatObjects->HasForm(base)) return RE::BSContainer::ForEachResult::kContinue;
        found = true;
        return RE::BSContainer::ForEachResult::kStop;
    };

    const auto origin = player->GetPosition();
    auto* playerCell = player->GetParentCell();
    if (playerCell->IsInteriorCell()) {
        playerCell->ForEachReferenceInRange(origin, kSmiHeatRange, checkRef);
        return found;
    }

    auto* grid = tes->gridCells;
    if (!grid) return false;
    constexpr float kCellSize = 4096.0f;
    for (std::uint32_t x = 0; x < grid->length && !found; ++x) {
        for (std::uint32_t y = 0; y < grid->length && !found; ++y) {
            auto* cell = grid->GetCell(x, y);
            const auto* coords = cell && cell->IsAttached() ? cell->GetCoordinates() : nullptr;
            if (!coords) continue;
            const bool inRange = coords->worldX < origin.x + kSmiHeatRange && coords->worldX + kCellSize > origin.x - kSmiHeatRange &&
                                 coords->worldY < origin.y + kSmiHeatRange && coords->worldY + kCellSize > origin.y - kSmiHeatRange;
            if (inRange) cell->ForEachReferenceInRange(origin, kSmiHeatRange, checkRef);
        }
    }
    return found;
}

static void UpdateSmiNearHeat()
{
    constexpr int kHeldMisses = 3;
    static int misses = 0;
    if (ScanForSmiHeat()) {
        misses = 0;
        g_smiNearHeat = true;
    } else if (++misses > kHeldMisses) {
        g_smiNearHeat = false;
    }
}

static bool IsNearSmiHeat()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point lastScan{};
    const auto now = Clock::now();
    if (now - lastScan >= std::chrono::seconds(1)) {
        lastScan = now;
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask(UpdateSmiNearHeat);
    }
    return g_smiNearHeat;
}

static float SmiHeatFloor(const ResolvedRaw& cold, float ambient)
{
    constexpr float kSmiFreezingArea = 250.0f;
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* cell = player ? player->GetParentCell() : nullptr;
    const bool freezingOutdoors = ambient >= kSmiFreezingArea && cell && !cell->IsInteriorCell();
    return freezingOutdoors && cold.stage[0] ? cold.stage[0]->value : 0.0f;
}

std::string GetColdStageName(int stage) { return ResolveNeedStageName(FrostfallLive() ? "exposure" : "cold", stage); }

std::optional<AmbientCold> GetAmbientCold()
{
    if (FrostfallLive() && g_ffAmbient.temperature) {
        AmbientCold out;
        out.hasTemperature = true;
        out.temperature = g_ffAmbient.temperature->value;
        out.heatSize = g_ffAmbient.heatSize ? static_cast<int>(g_ffAmbient.heatSize->value) : 0;
        out.nearFire = out.heatSize > 0;
        constexpr float kFrostfallYes = 2.0f;
        out.sheltered = g_ffAmbient.shelter && g_ffAmbient.shelter->value == kFrostfallYes;
        out.shelterLevel = out.sheltered ? 2 : 0;
        out.selfStage = GetExposureStage();
        if (g_ffAmbient.exposure && g_ffAmbient.target) {
            const float diff = g_ffAmbient.target->value - g_ffAmbient.exposure->value;
            out.trend = diff > 0.25f ? 1 : (diff < -0.25f ? -1 : 0);
            constexpr float kMaxExposure = 120.0f;
            out.selfFill = std::clamp(g_ffAmbient.exposure->value / kMaxExposure, 0.0f, 1.0f);
            out.areaFill = std::clamp(g_ffAmbient.target->value / kMaxExposure, 0.0f, 1.0f);
            out.fireFill = out.areaFill;
            out.stage = 0;
            out.stageStart = 0.0f;
            out.stageEnd = 1.0f;
            for (float step : { 20.0f, 40.0f, 60.0f, 80.0f, 100.0f, kMaxExposure }) {
                if (g_ffAmbient.target->value < step) {
                    out.stageEnd = step / kMaxExposure;
                    break;
                }
                ++out.stage;
                out.stageStart = step / kMaxExposure;
            }
            if (out.trend > 0 && out.stage == 0 && out.selfStage <= 0) out.trend = 0;
        }
        return out;
    }
    if (!IsAvailable()) return std::nullopt;
    constexpr int kCompassNearHeat = 1;
    const int compassLevel = g_compassTempLevel ? static_cast<int>(g_compassTempLevel->value) : -1;
    if (g_shAmbient.temperature) return GetSunHelmAmbient(compassLevel);
    if (!g_smiAmbientTemp) {
        if (compassLevel < 0) return std::nullopt;
        AmbientCold out;
        out.nearFire = compassLevel == kCompassNearHeat;
        out.stage = compassLevel == 2 ? 0 : compassLevel == 3 ? 2 : compassLevel == 4 ? 4 : -1;
        return out;
    }
    for (const auto& r : g_raw) {
        if (strcmp(r.def->key, "cold") != 0) continue;
        constexpr float kSmiCapAmbient = 600.0f;
        float ambient = g_smiAmbientTemp->value;
        if (ambient >= kSmiCapAmbient && r.max) ambient = (std::max)(ambient, r.max->value);
        AmbientCold out;
        out.nearFire = g_smiHeatObjects ? IsNearSmiHeat() : compassLevel == kCompassNearHeat;
        out.stage = 0;
        for (int i = 0; i < 5; ++i)
            if (r.stage[i] && ambient >= r.stage[i]->value) out.stage = i + 1;
        const float target = out.nearFire ? SmiHeatFloor(r, ambient) : ambient;
        const float diff = target - r.value->value;
        out.trend = diff > 1.0f ? 1 : (diff < -1.0f ? -1 : 0);
        FillColdScale(out, r, ambient);
        FillStageRange(out, r);
        out.selfStage = GetColdStage();
        const bool staysWarm = out.stage == 0 && out.selfStage == 0;
        if (out.trend > 0 && (staysWarm || out.nearFire)) out.trend = 0;
        return out;
    }
    return std::nullopt;
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
        FillFromRaw(ns);
        out.push_back(std::move(ns));
    };

    if (g_backend == Backend::SunHelm || g_backend == Backend::LastSeed || g_backend == Backend::RND) {
        add("hunger",  "need_hunger",  hungerStage,  MaxStageFor(Axis::Hunger),  g_iconHunger);
        add("thirst",  "need_thirst",  thirstStage,  MaxStageFor(Axis::Thirst),  g_iconThirst);
        add("fatigue", "need_fatigue", fatigueStage, MaxStageFor(Axis::Fatigue), g_iconFatigue);
        add("cold",    "need_cold",    coldStage,    MaxStageFor(Axis::Cold),    g_iconCold);
    } else {
        add("cold",    "need_cold",    coldStage,    MaxStageFor(Axis::Cold),    g_iconCold);
        add("hunger",  "need_hunger",  hungerStage,  MaxStageFor(Axis::Hunger),  g_iconHunger);
        add("fatigue", "need_fatigue", fatigueStage, MaxStageFor(Axis::Fatigue), g_iconFatigue);
    }

    add("exposure", "need_exposure", exposureStage, MaxStageFor(Axis::Exposure), g_iconExposure);
    add("wetness",  "need_wetness",  wetnessStage,  MaxStageFor(Axis::Wetness),  g_iconWetness);
    return out;
}

bool IsVitalitySpell(RE::SpellItem* spell)
{
    if (!spell) return false;
    for (auto* s : g_vitalitySpells)
        if (s && s == spell) return true;
    return false;
}

ID3D11ShaderResourceView* GetNeedSpellIcon(RE::SpellItem* spell)
{
    if (!spell) return nullptr;
    if (IsVitalitySpell(spell)) return g_iconVitality;
    for (const auto& a : g_axes) {
        if (std::find(std::begin(a.spells), std::end(a.spells), spell) == std::end(a.spells)) continue;
        switch (a.def->axis) {
            case Axis::Cold:     return g_iconCold;
            case Axis::Hunger:   return g_iconHunger;
            case Axis::Fatigue:  return g_iconFatigue;
            case Axis::Thirst:   return g_iconThirst;
            case Axis::Exposure: return g_iconExposure;
            case Axis::Wetness:  return g_iconWetness;
            default:             return nullptr;
        }
    }
    return nullptr;
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
        if (g_vitalityValue) vs.fill = std::clamp(g_vitalityValue->value / 140.0f, 0.0f, 1.0f);
        if (g_vitalityValue && g_vitalityTarget) {
            const float diff = g_vitalityTarget->value - g_vitalityValue->value;
            vs.trend = diff > 2.0f ? 1 : (diff < -2.0f ? -1 : 0);
        }
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

    const std::string rndHungerLabelStr[6] = {
        SfLabel("rnd_hunger_0", "Gluttony"),   SfLabel("rnd_hunger_1", "Satiated"),
        SfLabel("rnd_hunger_2", "Peckish"),    SfLabel("rnd_hunger_3", "Hungry"),
        SfLabel("rnd_hunger_4", "Very Hungry"),SfLabel("rnd_hunger_5", "Starving") };
    const char* kRndHungerLabels[6] = { rndHungerLabelStr[0].c_str(), rndHungerLabelStr[1].c_str(),
                                        rndHungerLabelStr[2].c_str(), rndHungerLabelStr[3].c_str(),
                                        rndHungerLabelStr[4].c_str(), rndHungerLabelStr[5].c_str() };

    const std::string rndThirstLabelStr[5] = {
        SfLabel("rnd_thirst_0", "Quenched"),   SfLabel("rnd_thirst_1", "Slightly Thirsty"),
        SfLabel("rnd_thirst_2", "Thirsty"),    SfLabel("rnd_thirst_3", "Very Thirsty"),
        SfLabel("rnd_thirst_4", "Dehydrated") };
    const char* kRndThirstLabels[6] = { rndThirstLabelStr[0].c_str(), rndThirstLabelStr[1].c_str(),
                                        rndThirstLabelStr[2].c_str(), rndThirstLabelStr[3].c_str(),
                                        rndThirstLabelStr[4].c_str(), rndThirstLabelStr[4].c_str() };

    const std::string rndFatigueLabelStr[5] = {
        SfLabel("rnd_fatigue_0", "Not Tired"), SfLabel("rnd_fatigue_1", "Slightly Tired"),
        SfLabel("rnd_fatigue_2", "Tired"),     SfLabel("rnd_fatigue_3", "Very Tired"),
        SfLabel("rnd_fatigue_4", "Exhausted") };
    const char* kRndFatigueLabels[6] = { rndFatigueLabelStr[0].c_str(), rndFatigueLabelStr[1].c_str(),
                                         rndFatigueLabelStr[2].c_str(), rndFatigueLabelStr[3].c_str(),
                                         rndFatigueLabelStr[4].c_str(), rndFatigueLabelStr[4].c_str() };

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
    float   labelSize   = font->LegacySize * settings->phSurvivalLabelScale;
    float   stageSize   = font->LegacySize * settings->phSurvivalStageScale;
    float   iconSize    = ScalePx(static_cast<float>(settings->phSurvivalIconSize));
    const bool hasArc = settings->phSurvivalIndicatorStyle == 3;
    const float arcRadius = ScalePx(settings->phSurvivalArcRadius);
    const float arcThickness = ScalePx(settings->phSurvivalArcThickness);
    const float percentageSize = font->LegacySize * settings->phSurvivalPercentageScale;
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
        float                     meter  = -1.0f;
        ImU32                     iconColor = 0;
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

    const bool ls  = (g_backend == Backend::LastSeed);
    const bool rnd = (g_backend == Backend::RND);
    std::vector<NeedEntry> needs;
    if (g_backend == Backend::SunHelm || ls || rnd) {
        if (hungerStage  >= 0) needs.push_back({ needNameHunger.c_str(),  hungerStage,  settings->phSurvivalHungerColor,  rnd ? kRndHungerLabels  : ls ? kLsHungerLabels  : kShHungerLabels,  MaxStageFor(Axis::Hunger),  g_iconHunger,  "hunger"  });
        if (thirstStage  >= 0) needs.push_back({ needNameThirst.c_str(),  thirstStage,  settings->phSurvivalThirstColor,  rnd ? kRndThirstLabels  : ls ? kLsThirstLabels  : kShThirstLabels,  MaxStageFor(Axis::Thirst),  g_iconThirst,  "thirst"  });
        if (fatigueStage >= 0) needs.push_back({ needNameFatigue.c_str(), fatigueStage, settings->phSurvivalFatigueColor, rnd ? kRndFatigueLabels : ls ? kLsFatigueLabels : kShFatigueLabels, MaxStageFor(Axis::Fatigue), g_iconFatigue, "fatigue" });
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

    const auto vitality = PlayerHUDSurvival::GetVitality();
    const char* vitalityLabels[8];
    if (vitality) {
        for (auto& l : vitalityLabels) l = vitality->tierName.c_str();
        const int maxTier = (std::max)(1, (std::min)(vitality->maxTier, 7));
        const float decline = vitality->fill >= 0.0f ? 1.0f - vitality->fill
                            : 1.0f - static_cast<float>(vitality->tier) / maxTier;
        needs.push_back({ vitality->name.c_str(), std::clamp(maxTier - vitality->tier, 0, maxTier), kVitalityColor,
                          vitalityLabels, maxTier, g_iconVitality, "vitality", decline, kVitalityIconColor });
    }

    for (auto& need : needs)
        if (need.meter < 0.0f && need.stage >= 0) need.meter = RawNeedFill(need.key);

    if (PlayerHUDEnvironment::HidesColdNeed())
        std::erase_if(needs, [](const NeedEntry& n) { return strcmp(n.key, "cold") == 0 || strcmp(n.key, "exposure") == 0; });

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
    std::vector<float> cardAlpha(needCount, 1.0f);
    {
        constexpr float kCardFadeIn = 0.2f;
        const float fadeOut = (std::max)(0.1f, settings->hvFadeDuration);
        for (int n = 0; n < needCount; ++n) {
            const auto& need = needs[n];
            cardAlpha[n] = UIAnimation::Step(std::string("surv_card_") + need.key, !CardHidden(need), kCardFadeIn, fadeOut);
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
    const float alarmWave = UIAnimation::Pulse(2.0f * 3.14159265f / 3.0f, 0.0f, 1.0f);
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

    float cardMeter = -1.0f;
    bool cardMeterPips = false;
    auto LitFraction = [&](int stage, int maxStage) {
        if (cardMeter >= 0.0f) return cardMeter;
        return maxStage > 0 ? static_cast<float>(stage) / static_cast<float>(maxStage) : 0.0f;
    };

    // Percentages describe stage severity, not a backend's continuous needs meter.
    auto DrawPercentage = [&](float centerX, float y, int stage, int maxStage, ImU32 color) {
        if (!settings->phSurvivalShowPercentage) return;
        char text[16];
        const int percent = static_cast<int>(std::lround(100.0f * LitFraction(stage, maxStage)));
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
            const int lit = cardMeterPips && cardMeter >= 0.0f ? static_cast<int>(std::lround(cardMeter * maxStage)) : s;
            HUDDraw::DrawPipRow(dl, ImVec2(x + (w - pipTotalW) * 0.5f, y + h * 0.5f),
                                lit, maxStage, ps);
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
            float frac   = LitFraction(s, maxStage);
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
            arc.fraction = LitFraction(s, maxStage);
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

                float frac     = LitFraction(s, maxStage);
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
            cardMeter       = need.meter;
            cardMeterPips   = strcmp(need.key, "vitality") == 0;
            ImU32 tintedCol  = StageColor(need.baseColor, s, need.maxStage);

            const int cardV0 = dl->VtxBuffer.Size;

            const float rowRounding = std::min(bgRadius, rowH * 0.5f);
            DrawCardBg(rowX, rowY, rowX + rowW, rowY + rowH, rowRounding);
            DrawAlarmRing(rowX, rowY, rowX + rowW, rowY + rowH, rowRounding, need.stage, need.maxStage);

            float cx = rowX + rowPadX + iconInset;
            float cy = rowY + rowPadY + iconInset;

            // Icon Fill needs a faint base so the clipped fill remains visible.
            bool  iconIsGhost = settings->phSurvivalIconFill;
            const ImU32 iconCol = (need.iconColor && s < need.maxStage) ? need.iconColor : tintedCol;
            ImU32 iconDrawCol = iconIsGhost ? ((iconCol & 0x00FFFFFF) | 0x26000000) : iconCol;
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
            cardMeter       = need.meter;
            cardMeterPips   = strcmp(need.key, "vitality") == 0;
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
                const ImU32 iconCol = (need.iconColor && s < need.maxStage) ? need.iconColor : tintedCol;
            ImU32 iconDrawCol = iconIsGhost ? ((iconCol & 0x00FFFFFF) | 0x26000000) : iconCol;
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
    TickStackWarning();

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
