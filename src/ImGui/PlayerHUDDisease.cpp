
#include "../../includes/ImGui/PlayerHUDDisease.h"
#include "../../includes/ImGui/Renderer.h"
#include "../../includes/Settings.h"
#include "../../includes/TextureManager.h"
#include "../../includes/MenuUtils.h"
#include "../../includes/Utils/ColorUtils.h"
#include "../../includes/Utils/HUDDraw.h"
#include "../../includes/LocalizationManager.h"
#include "../../includes/UIAnimation.h"
#include "../../includes/EffectCategories.h"

#include <RE/Skyrim.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

    constexpr const char* kSurvivalESL   = "ccqdrsse001-survivalmode.esl";
    constexpr const char* kSunHelmDisESP = "SunHelmDiseases.esp";

    struct DiseaseDef {
        const char*  key;
        const char*  iconId;
        RE::FormID   base;
        const char*  basePlugin;
        RE::FormID   stage2;
        RE::FormID   stage3;
        RE::FormID   trap;
                                  // dungeon disease traps (TrapLSpellDiseases LVSP).
        RE::FormID   stage4;
        RE::FormID   stage5;
        RE::FormID   stage1;
    };

    const DiseaseDef kDiseases[] = {
        { "rockjoint",  "disease_rockjoint",  0x000B8782, "Skyrim.esm",     0x98E, 0x98F, 0x0010A24F },
        { "ataxia",     "disease_ataxia",     0x000B877C, "Skyrim.esm",     0x992, 0x993, 0x0010A24A },
        { "bonebreak",  "disease_bonebreak",  0x000B877E, "Skyrim.esm",     0x984, 0x985, 0x0010A24C },
        { "brainrot",   "disease_brainrot",   0x000B877F, "Skyrim.esm",     0x990, 0x991, 0x0010A24D },
        { "rattles",    "disease_rattles",    0x000B8781, "Skyrim.esm",     0x98C, 0x98D, 0x0010A24E },
        { "witbane",    "disease_witbane",    0x000B8783, "Skyrim.esm",     0x994, 0x995, 0x0010A250 },
        { "droops",     "disease_droops",     0x040285C1, "Dragonborn.esm", 0x996, 0x997, 0 },
        { "brownrot",   "disease_brownrot",   0x914,      kSurvivalESL,     0x98B, 0x986, 0 },
        { "greenspore", "disease_greenspore", 0x912,      kSurvivalESL,     0x987, 0x988, 0 },
        { "gutworm",    "disease_gutworm",    0x915,      kSurvivalESL,     0x989, 0x98A, 0 },
        { "foodpoison", "disease_foodpoison", 0x918,      kSurvivalESL,     0,     0,     0 },
    };

    const DiseaseDef kSunHelmDiseases[] = {
        { "rockjoint",   "disease_rockjoint",    0x000B8782, "Skyrim.esm",     0x856, 0x857, 0x0010A24F },
        { "ataxia",      "disease_ataxia",       0x000B877C, "Skyrim.esm",     0x84C, 0x84D, 0x0010A24A },
        { "bonebreak",   "disease_bonebreak",    0x000B877E, "Skyrim.esm",     0x84E, 0x84F, 0x0010A24C },
        { "brainrot",    "disease_brainrot",     0x000B877F, "Skyrim.esm",     0x850, 0x851, 0x0010A24D },
        { "rattles",     "disease_rattles",      0x000B8781, "Skyrim.esm",     0x854, 0x855, 0x0010A24E },
        { "witbane",     "disease_witbane",      0x000B8783, "Skyrim.esm",     0x858, 0x859, 0x0010A250 },
        { "droops",      "disease_droops",       0x040285C1, "Dragonborn.esm", 0x852, 0x853, 0 },
        { "astralvapors","disease_astralvapors", 0x849,      kSunHelmDisESP,   0x84A, 0x84B, 0 },
        { "chills",      "disease_chills",       0x83D,      kSunHelmDisESP,   0x83C, 0x83B, 0 },
        { "dampworm",    "disease_dampworm",     0x83A,      kSunHelmDisESP,   0x845, 0x846, 0 },
        { "feeblelimb",  "disease_feeblelimb",   0x837,      kSunHelmDisESP,   0x847, 0x848, 0 },
        { "shakes",      "disease_shakes",       0x838,      kSunHelmDisESP,   0x83E, 0x83F, 0 },
        { "swampfever",  "disease_swampfever",   0x840,      kSunHelmDisESP,   0x841, 0x842, 0 },
        { "wither",      "disease_wither",       0x839,      kSunHelmDisESP,   0x843, 0x844, 0 },
    };

    constexpr const char* kLastSeedESP = "LastSeed.esp";

    const DiseaseDef kLastSeedDiseases[] = {
        { "rockjoint",  "disease_rockjoint",  0x000B8782, "Skyrim.esm",     0x02AE1E, 0x02AE1F, 0x0010A24F, 0x171D8A, 0x171D8B, 0x02AE1D },
        { "ataxia",     "disease_ataxia",     0x000B877C, "Skyrim.esm",     0x02AE12, 0x02AE13, 0x0010A24A, 0x171D82, 0x171D83, 0x02AE11 },
        { "bonebreak",  "disease_bonebreak",  0x000B877E, "Skyrim.esm",     0x02AE15, 0x02AE16, 0x0010A24C, 0x171D84, 0x171D85, 0x02AE14 },
        { "brainrot",   "disease_brainrot",   0x000B877F, "Skyrim.esm",     0x02AE18, 0x02AE19, 0x0010A24D, 0x171D86, 0x171D87, 0x02AE17 },
        { "rattles",    "disease_rattles",    0x000B8781, "Skyrim.esm",     0x02AE1B, 0x02AE1C, 0x0010A24E, 0x171D88, 0x171D89, 0x02AE1A },
        { "witbane",    "disease_witbane",    0x000B8783, "Skyrim.esm",     0x02AE21, 0x02AE22, 0x0010A250, 0x171D8C, 0x171D8D, 0x02AE20 },
        { "blackheartblight", "disease_blackheartblight", 0x0401FF2E, "Dragonborn.esm", 0x32A6E6, 0x32A6E7, 0, 0x32A6E8, 0x32A6E9, 0x32A6E5 },
        { "droops",     "disease_droops",     0x040285C1, "Dragonborn.esm", 0,        0,        0,          0,        0,        0        },

        { "ashchancre",          "disease_ashchancre",          0x5C1D57, kLastSeedESP,   0x5C1D52, 0x5C1D53, 0x5C1D56, 0x5C1D54, 0x5C1D55, 0x5C1D51 },
        { "ashwoeblight",        "disease_ashwoeblight",        0x5C1D44, kLastSeedESP,   0x5C1D3F, 0x5C1D40, 0x5C1D43, 0x5C1D41, 0x5C1D42, 0x5C1D3E },
        { "astralvapors",        "disease_astralvapors",        0x5C1CCC, kLastSeedESP,   0x5C1CC7, 0x5C1CC8, 0x5C1CCB, 0x5C1CC9, 0x5C1CCA, 0x5C1CC6 },
        { "bloodlung",           "disease_bloodlung",           0x5C1D27, kLastSeedESP,   0x5C1D22, 0x5C1D23, 0x5C1D26, 0x5C1D24, 0x5C1D25, 0x5C1D21 },
        { "brownrot",            "disease_brownrot",            0x59E54D, kLastSeedESP,   0x59E549, 0x59E54A, 0x59E54E, 0x59E54B, 0x59E54C, 0x59E548 },
        { "chanthraxblight",     "disease_chanthraxblight",     0x5C1D08, kLastSeedESP,   0x5C1D03, 0x5C1D04, 0x5C1D07, 0x5C1D05, 0x5C1D06, 0x5C1D02 },
        { "chills",              "disease_chills",              0x5C1D8C, kLastSeedESP,   0x5C1D87, 0x5C1D88, 0x5C1D8B, 0x5C1D89, 0x5C1D8A, 0x5C1D86 },
        { "collywobbles",        "disease_collywobbles",        0x5C1CAE, kLastSeedESP,   0x5C1CB0, 0x5C1CB1, 0x5C1CB4, 0x5C1CB2, 0x5C1CB3, 0x5C1CAF },
        { "dampworm",            "disease_dampworm",            0x5C1D74, kLastSeedESP,   0x5C1D6F, 0x5C1D70, 0x5C1D73, 0x5C1D71, 0x5C1D72, 0x5C1D6E },
        { "feeblelimb",          "disease_feeblelimb",          0x5C1CC0, kLastSeedESP,   0x5C1CBB, 0x5C1CBC, 0x5C1CBF, 0x5C1CBD, 0x5C1CBE, 0x5C1CBA },
        { "greenspore",          "disease_greenspore",          0x5C1C92, kLastSeedESP,   0x5C1C94, 0x5C1C95, 0x5C1C98, 0x5C1C96, 0x5C1C97, 0x5C1C93 },
        { "helljoint",           "disease_helljoint",           0x5C1D14, kLastSeedESP,   0x5C1D0F, 0x5C1D10, 0x5C1D13, 0x5C1D11, 0x5C1D12, 0x5C1D0E },
        { "redrage",             "disease_redrage",             0x5C1D68, kLastSeedESP,   0x5C1D63, 0x5C1D64, 0x5C1D67, 0x5C1D65, 0x5C1D66, 0x5C1D62 },
        { "rotbone",             "disease_rotbone",             0x5C1D38, kLastSeedESP,   0x5C1D33, 0x5C1D34, 0x5C1D37, 0x5C1D35, 0x5C1D36, 0x5C1D32 },
        { "rustchancre",         "disease_rustchancre",         0x5C6E94, kLastSeedESP,   0x5C6E8F, 0x5C6E90, 0x5C6E93, 0x5C6E91, 0x5C6E92, 0x5C6E8E },
        { "scalonsunburn",       "disease_scalonsunburn",       0x5C1D80, kLastSeedESP,   0x5C1D7B, 0x5C1D7C, 0x5C1D7F, 0x5C1D7D, 0x5C1D7E, 0x5C1D7A },
        { "serpiginousdementia", "disease_serpiginousdementia", 0x5C1D20, kLastSeedESP,   0x5C1D1B, 0x5C1D1C, 0x5C1D1F, 0x5C1D1D, 0x5C1D1E, 0x5C1D1A },
        { "shakes",              "disease_shakes",              0x5C1CA8, kLastSeedESP,   0x5C1CA3, 0x5C1CA4, 0x5C1CA7, 0x5C1CA5, 0x5C1CA6, 0x5C1CA2 },
        { "stomachrot",          "disease_stomachrot",          0x59E557, kLastSeedESP,   0x59E559, 0x59E55A, 0x59E55D, 0x59E55B, 0x59E55C, 0x59E558 },
        { "swampfever",          "disease_swampfever",          0x59E55E, kLastSeedESP,   0x59E560, 0x59E561, 0x59E564, 0x59E562, 0x59E563, 0x59E55F },
        { "ticklebritch",        "disease_ticklebritch",        0x5C1D50, kLastSeedESP,   0x5C1D4B, 0x5C1D4C, 0x5C1D4F, 0x5C1D4D, 0x5C1D4E, 0x5C1D4A },
        { "tunnelcough",         "disease_tunnelcough",         0x5C1CF0, kLastSeedESP,   0x5C1CEB, 0x5C1CEC, 0x5C1CEF, 0x5C1CED, 0x5C1CEE, 0x5C1CEA },
        { "wither",              "disease_wither",              0x5C1CFC, kLastSeedESP,   0x5C1CF7, 0x5C1CF8, 0x5C1CFB, 0x5C1CF9, 0x5C1CFA, 0x5C1CF6 },
        { "witlesspox",          "disease_witlesspox",          0x5C1CE4, kLastSeedESP,   0x5C1CDF, 0x5C1CE0, 0x5C1CE3, 0x5C1CE1, 0x5C1CE2, 0x5C1CDE },
        { "yellowtick",          "disease_yellowtick",          0x5C1CD8, kLastSeedESP,   0x5C1CD3, 0x5C1CD4, 0x5C1CD7, 0x5C1CD5, 0x5C1CD6, 0x5C1CD2 },
    };

    struct ResolvedDisease {
        const DiseaseDef*         def;
        RE::SpellItem*            base;
        RE::SpellItem*            stage2;
        RE::SpellItem*            stage3;
        RE::SpellItem*            stage4;
        RE::SpellItem*            stage5;
        RE::SpellItem*            trap;
        RE::SpellItem*            stage1;
        ID3D11ShaderResourceView* icon;
    };

    std::vector<ResolvedDisease> g_table;
    bool g_ccLoaded         = false;
    bool g_sunhelmDisActive = false;
    bool g_lastSeedActive   = false;
    bool g_initialized      = false;
    bool g_ForcePos         = false;

    int g_maxStage = 1;

    // ── Idle Collapse: event-driven flare/settle (mirrors FollowerHUD's Idle
    constexpr float kIdleHoldSeconds     = 5.0f;
    constexpr float kCollapseEaseSeconds = 0.35f;
    float s_idleT = 0.0f;
    std::chrono::steady_clock::time_point s_idleLastTick;
    bool  s_idleTickInit = false;
    std::chrono::steady_clock::time_point s_lastEventTime;
    bool  s_idleEverFlared = false;

    RE::TESGlobal* g_modeEnabled = nullptr;

    bool SurvivalActive() {
        return g_modeEnabled && g_modeEnabled->value == 1.0f;
    }

    RE::SpellItem* Look(RE::TESDataHandler* dh, RE::FormID id, const char* plugin) {
        if (!id) return nullptr;
        auto* f = dh->LookupForm(id, plugin);
        return f ? f->As<RE::SpellItem>() : nullptr;
    }

    std::unordered_set<RE::MagicItem*> CollectActiveSpells(RE::Actor* p) {
        std::unordered_set<RE::MagicItem*> out;
        auto* mt = p->AsMagicTarget();
        if (!mt) return out;
        auto* list = mt->GetActiveEffectList();
        if (!list) return out;
        for (auto* eff : *list) {
            if (!eff) continue;
            if (eff->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                eff->flags.any(RE::ActiveEffect::Flag::kInactive))
                continue;
            if (eff->spell) out.insert(eff->spell);
        }
        return out;
    }

    int ResolveStage(const std::unordered_set<RE::MagicItem*>& active,
                     const ResolvedDisease& r, RE::SpellItem** matched = nullptr) {
        auto hit = [&](RE::SpellItem* s) {
            return s && active.count(s);
        };
        auto take = [&](RE::SpellItem* s, int stage) {
            if (matched) *matched = s;
            return stage;
        };
        if (hit(r.stage5)) return take(r.stage5, 5);
        if (hit(r.stage4)) return take(r.stage4, 4);
        if (hit(r.stage3)) return take(r.stage3, 3);
        if (hit(r.stage2)) return take(r.stage2, 2);
        if (hit(r.stage1)) return take(r.stage1, 1);
        if (hit(r.base))   return take(r.base,   1);
        if (hit(r.trap))   return take(r.trap,   1);
        return 0;
    }

    std::string Loc(const char* key, const std::string& fallback) {
        std::string v = LocalizationManager::Get(key);
        return (v.empty() || v == key) ? fallback : v;
    }

    std::string LocalizedAVName(RE::ActorValue av) {
        using AV = RE::ActorValue;
        switch (av) {
            case AV::kHealth:  return Loc("Health",  "Health");
            case AV::kMagicka: return Loc("Magicka", "Magicka");
            case AV::kStamina: return Loc("Stamina", "Stamina");
            case AV::kHealRate:        return Loc("HealthRegen",     "Health Regen");
            case AV::kMagickaRate:     return Loc("MagickaRegen",    "Magicka Regen");
            case AV::kStaminaRate:     return Loc("StaminaRegen",    "Stamina Regen");
            case AV::kHealRateMult:    return Loc("HealthRegenMult", "Health Regen Mult");
            case AV::kMagickaRateMult: return Loc("MagickaRegenMult","Magicka Regen Mult");
            case AV::kStaminaRateMult: return Loc("StaminaRegenMult","Stamina Regen Mult");
            case AV::kOneHanded:
            case AV::kOneHandedModifier:   return Loc("OneHanded",   "One-Handed");
            case AV::kTwoHanded:
            case AV::kTwoHandedModifier:   return Loc("TwoHanded",   "Two-Handed");
            case AV::kArchery:
            case AV::kMarksmanModifier:    return Loc("Archery",     "Archery");
            case AV::kBlock:
            case AV::kBlockModifier:       return Loc("Block",       "Block");
            case AV::kSmithing:
            case AV::kSmithingModifier:    return Loc("Smithing",    "Smithing");
            case AV::kHeavyArmor:
            case AV::kHeavyArmorModifier:  return Loc("HeavyArmor",  "Heavy Armor");
            case AV::kLightArmor:
            case AV::kLightArmorModifier:  return Loc("LightArmor",  "Light Armor");
            case AV::kPickpocket:
            case AV::kPickpocketModifier:  return Loc("Pickpocket",  "Pickpocket");
            case AV::kLockpicking:
            case AV::kLockpickingModifier: return Loc("Lockpicking", "Lockpicking");
            case AV::kSneak:
            case AV::kSneakingModifier:    return Loc("Sneak",       "Sneak");
            case AV::kAlchemy:
            case AV::kAlchemyModifier:     return Loc("Alchemy",     "Alchemy");
            case AV::kSpeech:
            case AV::kSpeechcraftModifier: return Loc("Speech",      "Speech");
            case AV::kAlteration:
            case AV::kAlterationModifier:  return Loc("Alteration",  "Alteration");
            case AV::kConjuration:
            case AV::kConjurationModifier: return Loc("Conjuration", "Conjuration");
            case AV::kDestruction:
            case AV::kDestructionModifier: return Loc("Destruction", "Destruction");
            case AV::kIllusion:
            case AV::kIllusionModifier:    return Loc("Illusion",    "Illusion");
            case AV::kRestoration:
            case AV::kRestorationModifier: return Loc("Restoration", "Restoration");
            case AV::kEnchanting:
            case AV::kEnchantingModifier:  return Loc("Enchanting",  "Enchanting");
            default:
                if (const char* n = EffectCategories::GetActorValueName(av); n && n[0] && std::strcmp(n, "Unknown") != 0)
                    return n;
                return "";
        }
    }

    std::string FormatPenalty(RE::SpellItem* form) {
        if (!form) return "";
        RE::EffectSetting* base = form->GetAVEffect();
        if (!base) {
            if (form->effects.empty() || !form->effects[0]) return "";
            base = form->effects[0]->baseEffect;
        }
        if (!base) return "";

        RE::ActorValue av = base->data.primaryAV;
        if (av == RE::ActorValue::kNone) return "";

        float mag = 0.0f;
        RE::Effect* ce = form->GetCostliestEffectItem(RE::MagicSystem::Delivery::kTouch, false);
        if (!ce && !form->effects.empty()) ce = form->effects[0];
        if (ce) mag = ce->GetMagnitude();
        if (mag <= 0.0f) return "";

        std::string label = LocalizedAVName(av);
        if (label.empty()) return "";

        const bool isPct = EffectCategories::IsPercentageMagnitude(av);

        char buf[96];
        if (isPct)
            snprintf(buf, sizeof(buf), "%s -%.0f%%", label.c_str(), mag);
        else
            snprintf(buf, sizeof(buf), "%s -%.0f", label.c_str(), mag);
        return buf;
    }

    bool WrapNameTwoLines(ImFont* f, float size, float maxW, const std::string& s,
                          std::string& l1, std::string& l2) {
        if (!f || maxW <= 0.0f || s.empty()) return false;
        if (f->CalcTextSizeA(size, FLT_MAX, 0.0f, s.c_str()).x <= maxW) return false;

        size_t best = std::string::npos;
        for (size_t sp = s.find(' '); sp != std::string::npos; sp = s.find(' ', sp + 1)) {
            const std::string head = s.substr(0, sp);
            if (f->CalcTextSizeA(size, FLT_MAX, 0.0f, head.c_str()).x > maxW) break;
            best = sp;
        }
        if (best == std::string::npos) return false;

        l1 = s.substr(0, best);
        l2 = s.substr(best + 1);
        return !l2.empty();
    }

}

namespace PlayerHUDDisease {

void Init()
{
    auto* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) return;

    g_table.clear();

    g_ccLoaded = dh->LookupModByName(kSurvivalESL) != nullptr;

    auto* modeForm = dh->LookupForm(RE::FormID(0x826), kSurvivalESL);
    g_modeEnabled = modeForm ? modeForm->As<RE::TESGlobal>() : nullptr;

    g_sunhelmDisActive = dh->LookupModByName(kSunHelmDisESP) != nullptr;

    g_lastSeedActive = dh->LookupModByName(kLastSeedESP) != nullptr;

    const DiseaseDef* table       = g_lastSeedActive   ? kLastSeedDiseases
                                  : g_sunhelmDisActive ? kSunHelmDiseases
                                                       : kDiseases;
    const size_t      count       = g_lastSeedActive   ? std::size(kLastSeedDiseases)
                                  : g_sunhelmDisActive ? std::size(kSunHelmDiseases)
                                                       : std::size(kDiseases);
    const char*       stagePlugin = g_lastSeedActive   ? kLastSeedESP
                                  : g_sunhelmDisActive ? kSunHelmDisESP
                                                       : kSurvivalESL;

    g_maxStage = g_lastSeedActive ? 5
               : (g_sunhelmDisActive || g_ccLoaded) ? 3
                                                    : 1;

    for (size_t i = 0; i < count; ++i) {
        const auto& d = table[i];

        const bool ccOnly = (std::strcmp(d.basePlugin, kSurvivalESL) == 0);
        if (ccOnly && !g_ccLoaded) continue;

        ResolvedDisease r{};
        r.def    = &d;
        r.base   = Look(dh, d.base,   d.basePlugin);
        r.stage2 = Look(dh, d.stage2, stagePlugin);
        r.stage3 = Look(dh, d.stage3, stagePlugin);
        r.stage1 = Look(dh, d.stage1, stagePlugin);
        r.stage4 = Look(dh, d.stage4, stagePlugin);
        r.stage5 = Look(dh, d.stage5, stagePlugin);
        const char* trapPlugin = (std::strcmp(d.basePlugin, kLastSeedESP) == 0)
                               ? kLastSeedESP : "Skyrim.esm";
        r.trap   = Look(dh, d.trap, trapPlugin);

        std::string path = std::string("Data\\Interface\\PartySheet\\Widgets\\") + d.iconId + ".png";
        r.icon = TextureManager::GetTexture(path);

        g_table.push_back(r);
    }

    g_initialized = true;
    logger::info("PlayerHUDDisease: Initialized ({} diseases tracked, backend = {}, maxStage = {}).",
                 g_table.size(),
                 g_lastSeedActive   ? "LastSeed.esp"
                 : g_sunhelmDisActive ? "SunHelmDiseases.esp"
                                      : (g_ccLoaded ? "CC Survival" : "vanilla"),
                 g_maxStage);
}

std::vector<ActiveDisease> GetActiveDiseases()
{
    std::vector<ActiveDisease> out;
    if (!g_initialized) return out;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return out;

    const int maxStage = g_maxStage;

    auto active = CollectActiveSpells(player);

    for (const auto& r : g_table) {
        RE::SpellItem* matchedForm = nullptr;
        const int stage = ResolveStage(active, r, &matchedForm);
        if (stage == 0) continue;

        RE::SpellItem* nameForm = g_lastSeedActive
                                ? (matchedForm ? matchedForm : r.base)
                                : (r.base ? r.base
                                : r.trap ? r.trap
                                : r.stage3 ? r.stage3
                                : r.stage2 ? r.stage2 : r.stage1);
        std::string name = nameForm ? std::string(nameForm->GetName()) : std::string(r.def->key);

        ActiveDisease ad;
        ad.name     = std::move(name);
        ad.stage    = stage;
        ad.maxStage = maxStage;
        ad.icon     = r.icon;
        ad.key      = r.def->key;
        ad.penalty  = FormatPenalty(matchedForm ? matchedForm : r.base);
        out.push_back(std::move(ad));
    }

    return out;
}

void ForceReposition() { g_ForcePos = true; }

// ── Master scale helpers ────────────────────────────────────────────────
static float MasterScale()
{
    const auto* settings = Settings::GetSingleton();
    return settings
        ? std::clamp(static_cast<float>(settings->phDiseaseScale) / 100.0f, 0.5f, 2.0f)
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
    if (!settings->enablePlayerHud || !settings->enablePlayerDiseaseWidget) return;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    static std::unordered_map<std::string, int> s_lastStage;
    static bool s_seeded = false;
    static std::unordered_map<std::string, bool> s_pendingWorsenPop;
    struct GroupChange {
        std::string key;
        int stage;
        bool acquired;
    };
    static std::string s_groupPriorityKey;
    static std::vector<GroupChange> s_groupChanges;
    static float s_groupEventElapsed = 0.0f;
    static std::chrono::steady_clock::time_point s_groupLastTick;
    static bool s_groupTickInit = false;
    static bool s_groupRevealStarted = false;
    static bool s_previousGrouped = false;
    const bool grouped = settings->phDiseaseGrouped;
    if (grouped != s_previousGrouped) {
        s_previousGrouped = grouped;
        s_groupPriorityKey.clear();
        s_groupChanges.clear();
        s_groupEventElapsed = 0.0f;
        s_groupTickInit = false;
        s_groupRevealStarted = false;
        s_pendingWorsenPop.clear();
        s_idleT = 0.0f;
        s_idleTickInit = false;
        s_idleEverFlared = false;
    }

    auto diseases = GetActiveDiseases();
    auto QueueGroupChange = [&](const ActiveDisease& d, bool acquired) {
        auto pending = std::find_if(s_groupChanges.begin(), s_groupChanges.end(),
            [&](const GroupChange& change) { return change.key == d.key; });
        if (pending == s_groupChanges.end()) {
            s_groupChanges.push_back({d.key, d.stage, acquired});
        } else {
            *pending = {d.key, d.stage, acquired};
            if (pending == s_groupChanges.begin()) {
                s_groupEventElapsed = 0.0f;
                s_groupRevealStarted = false;
            }
        }
    };
    if (diseases.empty()) {
        if (grouped) {
            s_lastStage.clear();
            s_pendingWorsenPop.clear();
            s_groupPriorityKey.clear();
            s_groupChanges.clear();
            s_groupEventElapsed = 0.0f;
            s_groupTickInit = false;
            s_groupRevealStarted = false;
            s_idleT = 0.0f;
            s_idleTickInit = false;
            s_idleEverFlared = false;
        }
        return;
    }

    const bool staged = g_lastSeedActive || g_sunhelmDisActive || SurvivalActive();

    auto StageWord = [&](int stage) -> std::string {
        if (g_lastSeedActive) {
            (void)stage;
            return std::string();
        }
        if (g_sunhelmDisActive) {
            switch (stage) {
                case 2:  return Loc("disease_sh_stage_acute",  "Acute");
                case 3:  return Loc("disease_sh_stage_severe", "Severe");
                default: return Loc("disease_stage_infected",  "Infected");
            }
        }
        switch (stage) {
            case 2:  return Loc("disease_stage_severe",    "Severe");
            case 3:  return Loc("disease_stage_crippling", "Crippling");
            default: return Loc("disease_stage_infected",  "Infected");
        }
    };

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));

    const ImVec2 targetPos(static_cast<float>(settings->phDiseasePosX),
                           static_cast<float>(settings->phDiseasePosY));
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

    ImGui::Begin("##playerHUDdisease", nullptr, flags);

    ImFont* font      = ImGui::GetFont();
    float   labelSize = font->FontSize * settings->phDiseaseLabelScale;
    float   stageSize = font->FontSize * settings->phDiseaseStageScale;
    float   iconSize  = ScalePx(static_cast<float>(settings->phDiseaseIconSize));
    float   cardW     = ScalePx(static_cast<float>(settings->phDiseaseCardWidth));
    float   cardSpacing = ScalePx(static_cast<float>(settings->phDiseaseCardSpacing));
    float   bgRadius  = ScalePx(static_cast<float>(settings->phDiseaseBgCornerRadius));
    float   iconPad   = ScalePx(6.0f);
    float   textGap   = ScalePx(4.0f);

    const bool isCompact  = (settings->phDiseaseStyle     == 1);
    const bool isVertical = (settings->phDiseaseDirection == 1);
    const bool showLabel  = settings->phDiseaseShowLabel;
    const bool showStage  = settings->phDiseaseShowStage && staged;
    const bool showPips    = settings->phDiseaseShowPips  && staged;
    const bool showMagnitude = settings->phDiseaseShowMagnitude;
    bool hasMagnitude = false;
    if (showMagnitude) {
        for (const auto& d : diseases) {
            if (d.penalty.empty()) continue;
            hasMagnitude = true;
            if (!grouped)
                cardW = (std::max)(cardW, font->CalcTextSizeA(stageSize, FLT_MAX, 0.0f, d.penalty.c_str()).x);
        }
    }
    const float pipR       = ScalePx(4.0f);

    const std::uint32_t alarmArgb = settings->phDiseaseAlarmColor;
    auto StageColor = [&](int stage, int maxStage) -> ImU32 {
        std::uint32_t argb = settings->phDiseaseColor;
        if (maxStage > 0 && stage >= maxStage)
            argb = alarmArgb;
        return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
    };

    bool eventFired = false;
    {
        for (const auto& d : diseases) {
            auto it = s_lastStage.find(d.key);
            if (it == s_lastStage.end()) {
                s_lastStage[d.key] = d.stage;
                // New disease (key wasn't tracked before): an event, UNLESS this is
                if (s_seeded) {
                    eventFired = true;
                    if (grouped && !allowDrag) {
                        QueueGroupChange(d, true);
                    }
                }
            } else if (d.stage != it->second) {
                const bool worsened = d.stage > it->second;
                it->second = d.stage;
                if (worsened) {
                    if (grouped && !allowDrag) {
                        QueueGroupChange(d, false);
                    }
                    // Defer the pop until the expand finishes (drained below); still
                    if (!grouped) s_pendingWorsenPop[d.key] = true;
                    eventFired = true;
                }
            }
        }
        if (!diseases.empty() || s_seeded) {
            for (auto mIt = s_lastStage.begin(); mIt != s_lastStage.end(); ) {
                const bool stillActive = std::any_of(diseases.begin(), diseases.end(),
                    [&](const ActiveDisease& d) { return d.key == mIt->first; });
                if (stillActive) {
                    ++mIt;
                } else {
                    s_pendingWorsenPop.erase(mIt->first);
                    mIt = s_lastStage.erase(mIt);
                }
            }
        }
        s_seeded = true;
    }
    const ActiveDisease* groupPriority = nullptr;
    const ActiveDisease* groupEventDisease = nullptr;
    float groupEventAlpha = 0.0f;
    bool groupEventAcquired = false;
    bool groupEventStarted = false;
    if (grouped) {
        // The front belongs to the last revealed change and stays there until that
        auto current = std::find_if(diseases.begin(), diseases.end(),
            [&](const ActiveDisease& d) { return s_groupPriorityKey == d.key; });
        if (current == diseases.end()) {
            current = std::max_element(diseases.begin(), diseases.end(),
                [](const ActiveDisease& a, const ActiveDisease& b) { return a.stage < b.stage; });
            s_groupPriorityKey = current->key;
        }
        groupPriority = &*current;

        if (allowDrag) {
            // Preview is one composition, never a source of acquisition messages.
            s_groupChanges.clear();
            s_groupEventElapsed = 0.0f;
            s_groupTickInit = false;
            s_groupRevealStarted = false;
        } else {
            for (auto it = s_groupChanges.begin(); it != s_groupChanges.end(); ) {
                const auto active = std::find_if(diseases.begin(), diseases.end(),
                    [&](const ActiveDisease& d) { return it->key == d.key; });
                if (active == diseases.end() || active->stage < it->stage) {
                    if (it == s_groupChanges.begin()) {
                        s_groupEventElapsed = 0.0f;
                        s_groupRevealStarted = false;
                    }
                    it = s_groupChanges.erase(it);
                } else {
                    ++it;
                }
            }
            const auto now = std::chrono::steady_clock::now();
            float dt = s_groupTickInit ? std::chrono::duration<float>(now - s_groupLastTick).count() : 0.0f;
            s_groupLastTick = now;
            s_groupTickInit = true;
            // Hidden frames don't consume the visible hold; own clock avoids per-widget DeltaTime.
            dt = dt > 0.25f ? 0.0f : std::clamp(dt, 0.0f, 0.1f);
            constexpr float kGroupRevealSeconds = kIdleHoldSeconds + kCollapseEaseSeconds * 2.0f;
            if (!s_groupChanges.empty()) {
                if (!s_groupRevealStarted) {
                    s_groupEventElapsed = 0.0f;
                    s_groupRevealStarted = true;
                    groupEventStarted = true;
                    s_groupPriorityKey = s_groupChanges.front().key;
                } else {
                    s_groupEventElapsed += dt;
                }
                if (s_groupEventElapsed >= kGroupRevealSeconds) {
                    s_groupChanges.erase(s_groupChanges.begin());
                    s_groupEventElapsed = 0.0f;
                    s_groupRevealStarted = false;
                }
            }
            if (!s_groupChanges.empty()) {
                const auto& change = s_groupChanges.front();
                const auto active = std::find_if(diseases.begin(), diseases.end(),
                    [&](const ActiveDisease& d) { return change.key == d.key; });
                if (active != diseases.end()) {
                    groupEventDisease = &*active;
                    groupEventAcquired = change.acquired;
                    const float fadeIn = s_groupEventElapsed / kCollapseEaseSeconds;
                    // Idle Collapse off keeps the same disease's text at rest, so a
                    const float fadeOut = settings->phDiseaseIdleCollapse
                        ? (kGroupRevealSeconds - s_groupEventElapsed) / kCollapseEaseSeconds : 1.0f;
                    const float t = std::clamp((std::min)(fadeIn, fadeOut), 0.0f, 1.0f);
                    groupEventAlpha = t * t * (3.0f - 2.0f * t);
                }
            }
        }
        s_pendingWorsenPop.clear();
    }

    // Idle Collapse: event-driven flare/settle. Reuses eventFired (new disease OR
    if (!grouped) {
        const bool eligible = settings->phDiseaseIdleCollapse && !allowDrag;
        const auto now = std::chrono::steady_clock::now();

        if (eligible) {
            if (eventFired || (!s_idleEverFlared && !diseases.empty())) {
                s_lastEventTime  = now;
                s_idleEverFlared = true;
            }

            const bool wantCollapsed = s_idleEverFlared &&
                std::chrono::duration<float>(now - s_lastEventTime).count() >= kIdleHoldSeconds;
            if (!s_idleTickInit) {
                s_idleLastTick = now;
                s_idleTickInit = true;
            }
            float dt = std::chrono::duration<float>(now - s_idleLastTick).count();
            s_idleLastTick = now;
            dt = std::clamp(dt, 0.0f, 0.1f);

            const float step = dt / kCollapseEaseSeconds;
            s_idleT = wantCollapsed ? (std::min)(1.0f, s_idleT + step)
                                    : (std::max)(0.0f, s_idleT - step);
        } else {
            s_idleT          = 0.0f;
            s_idleTickInit   = false;
            s_idleEverFlared = false;
        }
    }
    const float idleT = s_idleT * s_idleT * (3.0f - 2.0f * s_idleT);

    // Drain deferred worsen-pops once the panel has expanded back to full (idleT ≈ 0)
    if (!grouped) {
        constexpr float kWorsenPopSettleDelay = 0.15f;
        static std::chrono::steady_clock::time_point s_fullSince;
        static bool s_fullTimerInit = false;
        const auto nowPop = std::chrono::steady_clock::now();
        if (idleT <= 0.02f) {
            if (!s_fullTimerInit) { s_fullSince = nowPop; s_fullTimerInit = true; }
            const float heldFull = std::chrono::duration<float>(nowPop - s_fullSince).count();
            if (heldFull >= kWorsenPopSettleDelay && !s_pendingWorsenPop.empty()) {
                for (auto& [key, pending] : s_pendingWorsenPop) {
                    if (pending) {
                        UIAnimation::TriggerBounce(std::string("dis_bounce_") + key);
                        pending = false;
                    }
                }
            }
        } else {
            s_fullTimerInit = false;
        }
    }
    const float fullCardAlpha = 1.0f - idleT;
    auto FadeCol = [](ImU32 col, float a) -> ImU32 {
        ImU32 alpha    = (col >> IM_COL32_A_SHIFT) & 0xFF;
        ImU32 newAlpha = static_cast<ImU32>(alpha * std::clamp(a, 0.0f, 1.0f));
        return (col & ~(0xFFu << IM_COL32_A_SHIFT)) | (newAlpha << IM_COL32_A_SHIFT);
    };

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      wPos = ImGui::GetCursorScreenPos();
    ImU32 mutedCol = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phDiseaseLabelColor));

    HUDDraw::IconFx iconFx;
    iconFx.duotone      = settings->phDiseaseIconDuotone;
    iconFx.shadow       = settings->phDiseaseIconShadow;
    iconFx.outline      = settings->phDiseaseIconOutline;
    iconFx.glow         = settings->phDiseaseIconGlow;
    iconFx.gloss        = settings->phDiseaseIconGloss;
    iconFx.outlineWidth = ScalePxInt(settings->phDiseaseIconOutlineWidth);
    iconFx.outlineColor = settings->phDiseaseIconOutlineColor;

    HUDDraw::TextFx textFx;
    textFx.shadow         = settings->phDiseaseTextShadow;
    textFx.outline        = settings->phDiseaseTextOutline;
    textFx.outlineOpacity = settings->phDiseaseTextOutlineOpacity;
    textFx.duotone        = settings->phDiseaseTextDuotone;

    auto DrawPips = [&](float x, float y, float w, float h, int stage, int maxStage, ImU32 tinted, float fadeAlpha = 1.0f) {
        if (maxStage <= 0) return;
        HUDDraw::PipStyle ps;
        ps.shape    = 1;
        ps.radius   = pipR;
        ps.litColor = HUDDraw::PackedToArgb(tinted);
        ps.alpha    = fadeAlpha;
        const float totalW = HUDDraw::MeasurePipRow(maxStage, ps);
        HUDDraw::DrawPipRow(dl, ImVec2(x + (w - totalW) * 0.5f, y + h * 0.5f),
                            stage, maxStage, ps);
    };

    // Keep the legacy ShowBorder field/INI key for preset compatibility.
    const bool alarmOnCard = ColorUtils::ArgbToVec4(settings->phDiseaseBgColor).w > 0.01f;
    static const auto alarmStart = std::chrono::steady_clock::now();
    const float alarmTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - alarmStart).count();
    const float alarmWave = std::sin(std::fmod(alarmTime, 3.0f) * (2.0f * 3.14159265358979323846f / 3.0f)) * 0.5f + 0.5f;
    auto IsAlarm = [&](const ActiveDisease& d) { return staged && d.maxStage > 1 && d.stage >= d.maxStage; };
    auto DrawAlarm = [&](ImVec2 a, ImVec2 b, float rounding, float fadeAlpha) {
        if (!settings->phDiseaseShowBorder) return;
        const float strength = (0.25f + 0.75f * alarmWave) * fadeAlpha;
        const ImU32 color = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(alarmArgb));
        dl->AddRect(a, b, FadeCol(color, strength * 0.45f), rounding, 0, ScalePx(4.0f));
        dl->AddRect(a, b, FadeCol(color, strength), rounding, 0, ScalePx(2.0f));
    };

    // Card fill has no built-in outlines. Alarm Pulse is drawn independently of fill opacity.
    auto DrawCardBg = [&](float x0, float y0, float x1, float y1, float rounding, float fadeAlpha = 1.0f, bool alarm = false) {
        ImVec4 base = ColorUtils::ArgbToVec4(settings->phDiseaseBgColor);
        if (base.w > 0.001f) {
            HUDDraw::ChipStyle cs;
            cs.rounding    = rounding;
            // Same BG Depth knobs as Survival's DrawCardBg.
            const std::uint32_t argb = settings->phDiseaseBgColor;
            cs.topColor    = FadeCol(settings->phDiseaseBgDuotone ? HUDDraw::ChipTopFromArgb(argb) : argb, fadeAlpha);
            cs.bottomColor = settings->phDiseaseBgDuotone ? HUDDraw::ChipBottomFromArgb(argb) : argb;
            cs.innerBorder = 0;
            cs.outerBorder = 0;
            cs.shadow      = false;
            if (!settings->phDiseaseBgTexture) cs.grainAlpha = 0;
            HUDDraw::DrawChip(dl, ImVec2(x0, y0), ImVec2(x1, y1), cs);
        }
        // Independent of fill opacity, like Survival's card stroke.
        if (settings->phDiseaseBgBorder && (settings->phDiseaseBgBorderColor >> 24) != 0) {
            const ImU32 border = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phDiseaseBgBorderColor));
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), FadeCol(border, fadeAlpha), rounding, 0, ScalePx(1.0f));
        }
        if (alarm && alarmOnCard)
            DrawAlarm(ImVec2(x0, y0), ImVec2(x1, y1), rounding, fadeAlpha);
    };

    // Icon backplate: a mini chip behind the icon so it sits in a slot. Same
    auto DrawIconPlate = [&](float ix, float iy, float size, ImU32 tinted, float fadeAlpha = 1.0f,
                             bool alarm = false, bool severityRing = true) {
        float pad   = ScalePx(3.0f);
        ImVec2 a(ix - pad, iy - pad), b(ix + size + pad, iy + size + pad);
        const bool border = settings->phDiseaseIconBorder;
        if ((settings->phDiseaseIconBgColor >> 24) != 0) {
            HUDDraw::ChipStyle cs;
            cs.rounding    = ScalePx(5.0f);
            cs.topColor    = FadeCol(HUDDraw::ChipTopFromArgb(settings->phDiseaseIconBgColor), fadeAlpha);
            cs.bottomColor = HUDDraw::ChipBottomFromArgb(settings->phDiseaseIconBgColor);
            cs.grainAlpha  = 0;
            if (!border) { cs.innerBorder = 0; cs.outerBorder = 0; }
            HUDDraw::DrawChip(dl, a, b, cs);
        }
        ImU32 ring = IM_COL32(((tinted >> IM_COL32_R_SHIFT) & 0xFF),
                              ((tinted >> IM_COL32_G_SHIFT) & 0xFF),
                              ((tinted >> IM_COL32_B_SHIFT) & 0xFF), static_cast<int>(160 * fadeAlpha));
        if (border && severityRing) dl->AddRect(a, b, ring, ScalePx(5.0f), 0, ScalePx(1.5f));
        if (alarm && !alarmOnCard) DrawAlarm(a, b, ScalePx(5.0f), fadeAlpha);
    };

    const int count = static_cast<int>(diseases.size());
    ImVec2 groupDummy(0.0f, 0.0f);
    if (grouped) {
        // Feedback offsets affect painting only, leaving the drag anchor stable.
        dl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);
        const bool hasEvent = groupEventDisease != nullptr;
        // Keep the event icon, pips and alarm in sync with its text; rest on the worst disease.
        const auto& priority = hasEvent ? *groupEventDisease : *groupPriority;
        constexpr const char* feedbackKey = "disease_group_feedback";
        if (groupEventStarted && hasEvent) {
            // Each queued disease owns its feedback when it reaches the front.
            UIAnimation::AnimationManager::Get().GetState(feedbackKey) = UIAnimation::AnimState{};
            // Card-level feedback only. The icon stays still: a swelling/bouncing icon
            if (!groupEventAcquired && IsAlarm(priority)) {
                UIAnimation::TriggerHitFeedback(feedbackKey);
            } else {
                UIAnimation::TriggerHealFeedback(feedbackKey);
            }
            logger::debug("[DiseaseGroupFeedback] key={} acquired={} stage={}/{} count={}",
                          priority.key, groupEventAcquired,
                          priority.stage, priority.maxStage, count);
        }
        const float flash = hasEvent ? (std::max)(UIAnimation::GetHitFlashIntensity(feedbackKey),
                                                 UIAnimation::GetHealFlashIntensity(feedbackKey)) : 0.0f;
        const ImU32 feedbackTint = groupEventAcquired ? IM_COL32(195, 168, 255, 255)
            : IsAlarm(priority) ? IM_COL32(255, 138, 128, 255) : IM_COL32(255, 179, 107, 255);
        if (hasEvent) wPos.x += ScalePx(UIAnimation::GetShakeOffsetX(feedbackKey));
        const ImU32 normalTint = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(settings->phDiseaseColor));
        const ImU32 tinted = staged ? StageColor(priority.stage, priority.maxStage) : normalTint;
        const float pad = ScalePx(4.0f);
        HUDDraw::PipStyle pipStyle;
        pipStyle.radius = pipR;
        const float pipsW = showPips ? HUDDraw::MeasurePipRow(priority.maxStage, pipStyle) : 0.0f;
        const float iconBandW = (std::max)(iconSize, pipsW) + pad * 2.0f;
        const float pipH = showPips ? pipR * 2.0f + ScalePx(8.0f) : 0.0f;
        ImFont* countFont = ImGuiRenderer::GetHeadingFont();
        if (!countFont) countFont = font;
        const float countFontSize = ScalePx(34.0f);
        // 14px floor: 0.65 of the severity size came out ~10px with the Grouped preset.
        const float countLabelSize = (std::max)(stageSize * 0.65f, ScalePx(14.0f));
        const std::string total = std::to_string(count);
        // Reuse the existing singular resistance label and plural V2 section label.
        const std::string countLabel = count == 1 ? Loc("Resistances.Disease", "Disease")
                                                 : Loc("PartySheetV2.SectionDiseases", "Diseases");
        const ImVec2 countSize = countFont->CalcTextSizeA(countFontSize, FLT_MAX, 0.0f, total.c_str());
        const ImVec2 countLabelSizePx = font->CalcTextSizeA(countLabelSize, FLT_MAX, 0.0f, countLabel.c_str());
        const float countW = (std::max)(ScalePx(52.0f), (std::max)(countSize.x, countLabelSizePx.x));
        const float countH = countSize.y + ScalePx(4.0f) + countLabelSizePx.y;
        const ImVec2 badgeSize(iconBandW, iconSize + pipH + pad * 2.0f);
        const ActiveDisease* detail = hasEvent ? groupEventDisease
            : (allowDrag || !settings->phDiseaseIdleCollapse ? groupPriority : nullptr);
        std::string name;
        std::string status;
        if (detail) {
            name = showLabel ? detail->name : std::string();
            status = showStage ? StageWord(detail->stage) : std::string();
            // Last Seed embeds severity in the live name. A hidden name needs a tier fallback.
            if (showStage && g_lastSeedActive && !showLabel)
                status = Loc("disease_stage_tier", "Stage") + " " + std::to_string(detail->stage)
                       + "/" + std::to_string(detail->maxStage);
            if (hasEvent && !groupEventAcquired) {
                if (!status.empty()) status += " | ";
                status += Loc("disease_event_worsened", "Worsened");
            }
        }
        const ImVec2 nameSize = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, name.c_str());
        const ImVec2 statusSize = font->CalcTextSizeA(stageSize, FLT_MAX, 0.0f, status.c_str());
        const std::string magnitude = detail && showMagnitude ? detail->penalty : std::string();
        const ImVec2 magnitudeSize = font->CalcTextSizeA(stageSize, FLT_MAX, 0.0f, magnitude.c_str());
        const float textW = (std::max)((std::max)(nameSize.x, statusSize.x), magnitudeSize.x);
        const float textH = (name.empty() ? 0.0f : labelSize)
                          + (name.empty() || status.empty() ? 0.0f : ScalePx(2.0f))
                          + (status.empty() ? 0.0f : stageSize)
                          + (magnitude.empty() ? 0.0f : stageSize + (name.empty() && status.empty() ? 0.0f : ScalePx(2.0f)));
        // Measure before painting so the shared material also covers the side text.
        const float sidePad = ScalePx(8.0f);
        const float columnGap = ScalePx(8.0f);
        const float contentW = sidePad * 2.0f + badgeSize.x
                             + (textW > 0.0f ? columnGap + textW : 0.0f)
                             + columnGap * 2.0f + countW;
        const ImVec2 cardSize(textW > 0.0f ? (std::max)(cardW, contentW) : contentW,
                             (std::max)((std::max)(badgeSize.y, textH + pad * 2.0f), countH + pad * 2.0f));
        const float rounding = std::min(bgRadius, cardSize.y * 0.5f);
        // Grouped routes Alarm Pulse to the pips (below), never the card or icon frame:
        DrawCardBg(wPos.x, wPos.y, wPos.x + cardSize.x, wPos.y + cardSize.y, rounding);
        if (flash > 0.0f) {
            dl->AddRectFilled(wPos, ImVec2(wPos.x + cardSize.x, wPos.y + cardSize.y),
                              FadeCol(feedbackTint, flash * 0.28f), rounding);
            dl->AddRect(ImVec2(wPos.x - ScalePx(2.0f), wPos.y - ScalePx(2.0f)),
                        ImVec2(wPos.x + cardSize.x + ScalePx(2.0f), wPos.y + cardSize.y + ScalePx(2.0f)),
                        FadeCol(feedbackTint, flash * 0.35f), rounding + ScalePx(2.0f), 0, ScalePx(3.0f));
        }
        const float ix = wPos.x + sidePad + (iconBandW - iconSize) * 0.5f;
        const float iy = wPos.y + (cardSize.y - iconSize - pipH) * 0.5f;
        DrawIconPlate(ix, iy, iconSize, tinted, 1.0f, false, false);
        if (priority.icon)
            HUDDraw::DrawIconStyled(dl, ImVec2(ix, iy), iconSize,
                                    (ImTextureID)priority.icon, HUDDraw::PackedToArgb(tinted), iconFx);
        if (showPips) {
            const float pipBandH = pipR * 2.0f + ScalePx(4.0f);
            const float pipY = iy + iconSize + ScalePx(4.0f);
            if (IsAlarm(priority) && settings->phDiseaseShowBorder) {
                // Breathing bloom behind each lit pip, same 3s wave as the card/icon pulse.
                const float strength = 0.25f + 0.75f * alarmWave;
                const ImU32 glow = ImGui::ColorConvertFloat4ToU32(ColorUtils::ArgbToVec4(alarmArgb));
                const float pitch = pipR * 2.0f + pipStyle.gap;
                const float left = wPos.x + sidePad + (iconBandW - pipsW) * 0.5f + pipR;
                const float cy = pipY + pipBandH * 0.5f;
                for (int n = 0; n < priority.stage; ++n) {
                    const ImVec2 c(left + n * pitch, cy);
                    dl->AddCircleFilled(c, pipR * 2.6f, FadeCol(glow, strength * 0.18f));
                    dl->AddCircleFilled(c, pipR * 1.7f, FadeCol(glow, strength * 0.35f));
                }
            }
            DrawPips(wPos.x + sidePad, pipY, iconBandW, pipBandH,
                     priority.stage, priority.maxStage, tinted);
        }
        const float countX = wPos.x + cardSize.x - sidePad - countW;
        const float countY = wPos.y + (cardSize.y - countH) * 0.5f;
        dl->AddLine(ImVec2(countX - columnGap, wPos.y + sidePad),
                    ImVec2(countX - columnGap, wPos.y + cardSize.y - sidePad),
                    FadeCol(mutedCol, 0.25f), ScalePx(1.0f));
        HUDDraw::TextFx countFx = textFx;
        countFx.font = countFont; countFx.fontSize = countFontSize;
        HUDDraw::DrawTextStyled(dl, ImVec2(countX + (countW - countSize.x) * 0.5f, countY),
                               tinted, total.c_str(), countFx);
        HUDDraw::TextFx countLabelFx = textFx;
        countLabelFx.font = font; countLabelFx.fontSize = countLabelSize;
        HUDDraw::DrawTextStyled(dl, ImVec2(countX + (countW - countLabelSizePx.x) * 0.5f,
                               countY + countSize.y + ScalePx(4.0f)),
                               mutedCol, countLabel.c_str(), countLabelFx);
        groupDummy = cardSize;
        if (detail) {
            if (textW > 0.0f) {
                const float alpha = hasEvent ? groupEventAlpha : 1.0f;
                const float textX = wPos.x + sidePad + badgeSize.x + columnGap;
                float textY = wPos.y + (cardSize.y - textH) * 0.5f;
                if (!name.empty()) {
                    HUDDraw::TextFx nameFx = textFx;
                    nameFx.font = font; nameFx.fontSize = labelSize;
                    HUDDraw::DrawTextStyled(dl, ImVec2(textX, textY), FadeCol(mutedCol, alpha), name.c_str(), nameFx);
                    textY += labelSize + ScalePx(2.0f);
                }
                if (!status.empty()) {
                    HUDDraw::TextFx statusFx = textFx;
                    statusFx.font = font; statusFx.fontSize = stageSize;
                    const ImU32 eventTint = hasEvent ? feedbackTint : tinted;
                    HUDDraw::DrawTextStyled(dl, ImVec2(textX, textY), FadeCol(eventTint, alpha), status.c_str(), statusFx);
                    textY += stageSize + ScalePx(2.0f);
                }
                if (!magnitude.empty()) {
                    HUDDraw::TextFx magnitudeFx = textFx;
                    magnitudeFx.font = font; magnitudeFx.fontSize = stageSize;
                    HUDDraw::DrawTextStyled(dl, ImVec2(textX, textY), FadeCol(mutedCol, alpha), magnitude.c_str(), magnitudeFx);
                }
            }
        }
        dl->PopClipRect();
    }

    bool anyStageWord = false;
    if (showStage) {
        for (int n = 0; n < count; ++n) {
            if (!StageWord(diseases[n].stage).empty()) { anyStageWord = true; break; }
        }
    }
    const bool pipsUnderName = showPips && !anyStageWord;

    bool anyNameWraps = false;
    if (!isCompact && showLabel) {
        std::string w1, w2;
        for (int n = 0; n < count; ++n) {
            if (WrapNameTwoLines(font, labelSize, cardW, diseases[n].name, w1, w2)) { anyNameWraps = true; break; }
        }
    }
    // Extra card height for the wrapped second line. Matches the name line's own
    const float nameWrapExtraH = anyNameWraps ? (labelSize + ScalePx(3.0f)) : 0.0f;

    // Full-card and chip-row passes each compute their own footprint below but
    ImVec2 fullDummy = groupDummy;

    // Match full cards and collapse cells so enabling effect text cannot shift the grid mid-fade.
    float compactContentH = 0.0f;
    if (hasMagnitude) {
        for (const auto& d : diseases) {
            float h = showLabel ? labelSize + ScalePx(2.0f) : 0.0f;
            if (showStage && !StageWord(d.stage).empty()) h += stageSize + ScalePx(2.0f);
            if (!d.penalty.empty()) h += stageSize + ScalePx(2.0f);
            if (pipsUnderName) h += pipR * 2.0f + ScalePx(4.0f);
            compactContentH = (std::max)(compactContentH, h);
        }
    }

    // idleT < 1: draw the full cards (faded per fullCardAlpha, computed above).
    if (!grouped && idleT < 1.0f) {
    if (isCompact) {
        // ── Compact, two arrangements ────────────────────────────────────────
        float rowPadX = ScalePx(8.0f), rowPadY = ScalePx(5.0f), textGapX = ScalePx(6.0f), pipGapX = ScalePx(8.0f);
        float pipRowH = pipR * 2.0f + ScalePx(4.0f);  // same pip band height the standard card uses
        // Stacked text column height when the pips ride under the name. The icon stays
        float stackH  = (showLabel ? labelSize + ScalePx(2.0f) : 0.0f) + pipRowH;
        float rowH    = (std::max)(pipsUnderName ? (std::max)(iconSize, stackH) : iconSize, compactContentH) + rowPadY * 2.0f;
        float pipsW   = showPips ? ((diseases[0].maxStage * (pipR * 2.0f + ScalePx(2.0f))) - ScalePx(2.0f)) : 0.0f;
        float rowW    = rowPadX + iconSize + textGapX + cardW
                      + ((showPips && !pipsUnderName) ? pipGapX + pipsW : 0.0f) + rowPadX;

        for (int n = 0; n < count; ++n) {
            const auto& d = diseases[n];
            float rowX = isVertical ? wPos.x : wPos.x + n * (rowW + cardSpacing);
            float rowY = isVertical ? wPos.y + n * (rowH + cardSpacing) : wPos.y;

            ImU32 tinted = StageColor(d.stage, d.maxStage);
            // Clamp to half the row height so a high radius caps at a clean pill, never overshoots.
            DrawCardBg(rowX, rowY, rowX + rowW, rowY + rowH, std::min(bgRadius, rowH * 0.5f), fullCardAlpha, IsAlarm(d));

            float cx = rowX + rowPadX;
            float cy = rowY + rowPadY;

            float bounceY = UIAnimation::GetBounceOffsetY(std::string("dis_bounce_") + d.key) * MasterScale();
            DrawIconPlate(cx, cy + bounceY, iconSize, tinted, fullCardAlpha, IsAlarm(d));
            if (d.icon)
                HUDDraw::DrawIconStyled(dl, ImVec2(cx, cy + bounceY), iconSize,
                                        (ImTextureID)d.icon, FadeCol(HUDDraw::PackedToArgb(tinted), fullCardAlpha), iconFx);
            cx += iconSize + textGapX;

            const std::string& name = d.name;
            float ty = cy;
            if (showLabel) {
                HUDDraw::TextFx lblFx = textFx;
                lblFx.font = font; lblFx.fontSize = labelSize;
                HUDDraw::DrawTextStyled(dl, ImVec2(cx, ty), FadeCol(mutedCol, fullCardAlpha), name.c_str(), lblFx);
                ty += labelSize + ScalePx(2.0f);
            }
            if (showStage) {
                std::string sw = StageWord(d.stage);
                if (!sw.empty()) {
                    HUDDraw::TextFx stgFx = textFx;
                    stgFx.font = font; stgFx.fontSize = stageSize;
                    HUDDraw::DrawTextStyled(dl, ImVec2(cx, ty), FadeCol(tinted, fullCardAlpha), sw.c_str(), stgFx);
                    ty += stageSize + ScalePx(2.0f);
                }
            }
            if (showMagnitude && !d.penalty.empty()) {
                HUDDraw::TextFx magnitudeFx = textFx;
                magnitudeFx.font = font; magnitudeFx.fontSize = stageSize;
                HUDDraw::DrawTextStyled(dl, ImVec2(cx, ty), FadeCol(mutedCol, fullCardAlpha), d.penalty.c_str(), magnitudeFx);
                ty += stageSize + ScalePx(2.0f);
            }
            if (showPips) {
                if (pipsUnderName) {
                    DrawPips(cx, ty, pipsW, pipRowH, d.stage, d.maxStage, tinted, fullCardAlpha);
                } else {
                    float pipX = cx + cardW + pipGapX;
                    DrawPips(pipX, rowY, pipsW, rowH, d.stage, d.maxStage, tinted, fullCardAlpha);
                }
            }
        }

        fullDummy = isVertical ? ImVec2(rowW, count * rowH + (count - 1) * cardSpacing)
                               : ImVec2(count * rowW + (count - 1) * cardSpacing, rowH);

    } else {
        float cardH = iconPad + iconSize + textGap
                    + (showLabel ? labelSize + ScalePx(3.0f) + nameWrapExtraH : 0.0f)
                    + (showStage ? stageSize + ScalePx(3.0f) : 0.0f)
                  + (hasMagnitude ? stageSize + ScalePx(3.0f) : 0.0f)
                    + (showPips  ? pipR * 2.0f + ScalePx(6.0f) : 0.0f)
                    + iconPad;

        for (int n = 0; n < count; ++n) {
            const auto& d = diseases[n];
            float cardX = isVertical ? wPos.x : wPos.x + n * (cardW + cardSpacing);
            float cardY = isVertical ? wPos.y + n * (cardH + cardSpacing) : wPos.y;

            ImU32 tinted = StageColor(d.stage, d.maxStage);
            DrawCardBg(cardX, cardY, cardX + cardW, cardY + cardH, std::min(bgRadius, cardH * 0.5f), fullCardAlpha, IsAlarm(d));

            float drawY = cardY + iconPad;
            float iconX = cardX + (cardW - iconSize) * 0.5f;
            float bounceY = UIAnimation::GetBounceOffsetY(std::string("dis_bounce_") + d.key) * MasterScale();
            DrawIconPlate(iconX, drawY + bounceY, iconSize, tinted, fullCardAlpha, IsAlarm(d));
            if (d.icon)
                HUDDraw::DrawIconStyled(dl, ImVec2(iconX, drawY + bounceY), iconSize,
                                        (ImTextureID)d.icon, FadeCol(HUDDraw::PackedToArgb(tinted), fullCardAlpha), iconFx);
            drawY += iconSize + textGap;

            auto Centered = [&](const char* text, float fsize, ImU32 col) {
                ImVec2 sz = font->CalcTextSizeA(fsize, FLT_MAX, 0.0f, text);
                float tx = cardX + (cardW - sz.x) * 0.5f;
                HUDDraw::TextFx fx = textFx;
                fx.font = font; fx.fontSize = fsize;
                HUDDraw::DrawTextStyled(dl, ImVec2(tx, drawY), FadeCol(col, fullCardAlpha), text, fx);
                drawY += fsize + ScalePx(3.0f);
            };

            if (showLabel) {
                std::string l1, l2;
                if (WrapNameTwoLines(font, labelSize, cardW, d.name, l1, l2)) {
                    Centered(l1.c_str(), labelSize, mutedCol);
                    Centered(l2.c_str(), labelSize, mutedCol);
                } else {
                    Centered(d.name.c_str(), labelSize, mutedCol);
                    drawY += nameWrapExtraH;
                }
            }
            if (showStage) {
                std::string sw = StageWord(d.stage);
                if (!sw.empty()) Centered(sw.c_str(), stageSize, tinted);
            }
            if (hasMagnitude) {
                if (!d.penalty.empty()) Centered(d.penalty.c_str(), stageSize, mutedCol);
                else drawY += stageSize + ScalePx(3.0f);
            }
            if (showPips) {
                DrawPips(cardX, drawY, cardW, pipR * 2.0f + ScalePx(4.0f), d.stage, d.maxStage, tinted, fullCardAlpha);
                drawY += pipR * 2.0f + ScalePx(6.0f);
            }
        }

        fullDummy = isVertical ? ImVec2(cardW, count * cardH + (count - 1) * cardSpacing)
                               : ImVec2(count * cardW + (count - 1) * cardSpacing, cardH);
    }
    }

    ImVec2 chipDummy(0.0f, 0.0f);
    if (!grouped && idleT > 0.0f) {
        float chipPad   = ScalePx(4.0f);
        float chipW     = iconSize + chipPad * 2.0f;
        float pipRowH   = showPips ? (pipR * 2.0f + ScalePx(4.0f)) : 0.0f;  // pip band under the icon
        float chipH     = chipPad + iconSize + (showPips ? ScalePx(4.0f) + pipRowH : 0.0f) + chipPad;

        // Per-style full-card icon origin (travel START), matching the full-card pass
        const float fullIconOffX = isCompact ? ScalePx(8.0f) /*rowPadX*/ : (cardW - iconSize) * 0.5f;
        const float fullIconOffY = isCompact ? ScalePx(5.0f) /*rowPadY*/ : ScalePx(6.0f) /*iconPad*/;

        float cellW, cellH;
        if (isCompact) {
            // MIRRORS the compact rowW/rowH above (incl. the pipsUnderName narrowing): 
            float pipsW  = showPips ? ((diseases[0].maxStage * (pipR * 2.0f + ScalePx(2.0f))) - ScalePx(2.0f)) : 0.0f;
            float rowPipRowH = pipR * 2.0f + ScalePx(4.0f);
            float stackH = (showLabel ? labelSize + ScalePx(2.0f) : 0.0f) + rowPipRowH;
            cellW = ScalePx(8.0f) + iconSize + ScalePx(6.0f) + cardW + ((showPips && !pipsUnderName) ? ScalePx(8.0f) + pipsW : 0.0f) + ScalePx(8.0f);
            cellH = (std::max)(pipsUnderName ? (std::max)(iconSize, stackH) : iconSize, compactContentH) + ScalePx(5.0f) * 2.0f;
        } else {
            cellW = cardW;
            cellH = iconPad + iconSize + textGap
                  + (showLabel ? labelSize + ScalePx(3.0f) + nameWrapExtraH : 0.0f)
                  + (showStage ? stageSize + ScalePx(3.0f) : 0.0f)
                  + (hasMagnitude ? stageSize + ScalePx(3.0f) : 0.0f)
                  + (showPips  ? pipR * 2.0f + ScalePx(6.0f) : 0.0f)
                  + iconPad;
        }

        for (int n = 0; n < count; ++n) {
            const auto& d = diseases[n];
            float cellX = isVertical ? wPos.x : wPos.x + n * (cellW + cardSpacing);
            float cellY = isVertical ? wPos.y + n * (cellH + cardSpacing) : wPos.y;
            float chipX = cellX + (cellW - chipW) * 0.5f;
            float chipY = cellY;

            ImU32 tinted = StageColor(d.stage, d.maxStage);
            DrawCardBg(chipX, chipY, chipX + chipW, chipY + chipH,
                       std::min(bgRadius, chipW * 0.5f), idleT, IsAlarm(d));

            float bounceY = UIAnimation::GetBounceOffsetY(std::string("dis_bounce_") + d.key) * MasterScale();
            float iconStartX = cellX + fullIconOffX, iconStartY = cellY + fullIconOffY;
            float iconEndX   = chipX + chipPad,      iconEndY   = chipY + chipPad;
            float ix = iconStartX + (iconEndX - iconStartX) * idleT;
            float iy = iconStartY + (iconEndY - iconStartY) * idleT;
            DrawIconPlate(ix, iy + bounceY, iconSize, tinted, idleT, IsAlarm(d));
            if (d.icon)
                HUDDraw::DrawIconStyled(dl, ImVec2(ix, iy + bounceY), iconSize,
                                        (ImTextureID)d.icon, FadeCol(HUDDraw::PackedToArgb(tinted), idleT), iconFx);
            if (showPips) {
                // Centered in the chip's full width, in the band directly below the icon.
                float pipBandY = chipY + chipPad + iconSize + ScalePx(4.0f);
                DrawPips(chipX, pipBandY, chipW, pipRowH, d.stage, d.maxStage, tinted, idleT);
            }
        }

        chipDummy = isVertical ? ImVec2(cellW, (count - 1) * (cellH + cardSpacing) + chipH)
                               : ImVec2(count * cellW + (count - 1) * cardSpacing, chipH);
    }

    ImVec2 dummySize((std::max)(fullDummy.x, chipDummy.x), (std::max)(fullDummy.y, chipDummy.y));
    dummySize.x = (std::max)(dummySize.x, 1.0f);
    dummySize.y = (std::max)(dummySize.y, 1.0f);
    if (idleT > 0.0f && idleT < 1.0f) {
        ImDrawList* clipDl = ImGui::GetWindowDrawList();
        clipDl->PushClipRect(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, false);
        ImGui::Dummy(dummySize);
        clipDl->PopClipRect();
    } else {
        ImGui::Dummy(dummySize);
    }

    if (allowDrag) {
        ImVec2 winPos = ImGui::GetWindowPos();
        settings->phDiseasePosX = static_cast<int>(winPos.x);
        settings->phDiseasePosY = static_cast<int>(winPos.y);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void RenderHUD()
{
    struct FadeFrame {
        std::unique_ptr<ImDrawList> drawList;
        std::vector<ImU32> colors;
    };
    static std::vector<FadeFrame> s_lastGroupFrame;
    static std::chrono::steady_clock::time_point s_lastGroupPaint;
    static float s_removalElapsed = -1.0f;
    auto ClearRemovalFrame = [&]() {
        s_lastGroupFrame.clear();
        s_removalElapsed = -1.0f;
    };
    auto* settings = Settings::GetSingleton();
    if (!settings->enablePlayerHud || !settings->enablePlayerDiseaseWidget) {
        ClearRemovalFrame();
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->Is3DLoaded() || MenuUtils::ShouldBlockOverlay()) {
        ClearRemovalFrame();
        return;
    }
    if (settings->phDiseaseHideInCombat) {
        static float s_lastCombatTime = -10.0f;
        const float now = static_cast<float>(ImGui::GetTime());
        if (player->IsInCombat()) s_lastCombatTime = now;
        if (now - s_lastCombatTime < 1.5f) {
            ClearRemovalFrame();
            return;
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderWindow(false);
    ImGui::EndFrame();
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    const auto now = std::chrono::steady_clock::now();
    if (!settings->phDiseaseGrouped) {
        ClearRemovalFrame();
    } else if (drawData->TotalVtxCount > 0) {
        // Cache pixels' geometry only. RenderWindow still removes diseases and
        s_lastGroupFrame.resize(drawData->CmdListsCount);
        for (int n = 0; n < drawData->CmdListsCount; ++n) {
            const auto* source = drawData->CmdLists[n];
            auto& frame = s_lastGroupFrame[n];
            if (!frame.drawList)
                frame.drawList = std::make_unique<ImDrawList>(ImGui::GetDrawListSharedData());
            auto& cached = *frame.drawList;
            cached.CmdBuffer = source->CmdBuffer;
            cached.IdxBuffer = source->IdxBuffer;
            cached.VtxBuffer = source->VtxBuffer;
            cached.Flags = source->Flags;
            // AddDrawList validates these writer fields even for finished geometry.
            cached._VtxCurrentIdx = source->_VtxCurrentIdx;
            cached._VtxWritePtr = cached.VtxBuffer.Data + cached.VtxBuffer.Size;
            cached._IdxWritePtr = cached.IdxBuffer.Data + cached.IdxBuffer.Size;
            frame.colors.resize(cached.VtxBuffer.Size);
            for (int v = 0; v < cached.VtxBuffer.Size; ++v)
                frame.colors[v] = cached.VtxBuffer[v].col;
        }
        s_removalElapsed = -1.0f;
        s_lastGroupPaint = now;
    } else if (!s_lastGroupFrame.empty()) {
        const float dt = std::chrono::duration<float>(now - s_lastGroupPaint).count();
        s_lastGroupPaint = now;
        // Don't replay an old card after an unobserved pause or settings preview.
        if (dt > 0.25f) {
            ClearRemovalFrame();
        } else {
            if (s_removalElapsed < 0.0f) {
                s_removalElapsed = 0.0f;
                logger::debug("[DiseaseGroupFade] Last card removed; fading rendered frame over {}s",
                              UIAnimation::Duration::FadeOut);
            }
            s_removalElapsed += (std::max)(0.0f, dt);
            const float alpha = 1.0f - UIAnimation::SmoothStep(s_removalElapsed / UIAnimation::Duration::FadeOut);
            if (alpha <= 0.0f) {
                ClearRemovalFrame();
            } else {
                for (auto& frame : s_lastGroupFrame) {
                    auto& cached = *frame.drawList;
                    // Explicit drawlist colors ignore ImGui style alpha. Always
                    for (int v = 0; v < cached.VtxBuffer.Size; ++v) {
                        const ImU32 original = frame.colors[v];
                        const ImU32 faded = static_cast<ImU32>(((original >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
                        cached.VtxBuffer[v].col = (original & ~(0xFFu << IM_COL32_A_SHIFT)) | (faded << IM_COL32_A_SHIFT);
                    }
                    drawData->AddDrawList(&cached);
                }
            }
        }
    }
    ImGuiRenderer::RenderToBoundTarget(drawData);
}

void RenderPreview() { RenderWindow(true); }

}
