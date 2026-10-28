// STATUS: Working

#include "../includes/ImGui/InspectCardKYE.h"

#include <RE/Skyrim.h>

// ============================================================
// Inspect Card - Know Your Enemy 2 (KYE2) integration.
// ============================================================

namespace InspectCardKYE {
    static constexpr const char* kPluginBase  = "know_your_enemy2.esp";
    static constexpr const char* kPluginArmor = "know_your_enemy_2_armors.esp";
    static constexpr const char* kPluginPatch = "know_your_enemy_2_integration_patch.esp";

    static bool s_resolved   = false;
    static bool s_kye2Loaded = false;
    static bool s_kye2aLoaded = false;

    static RE::BGSPerk* p_animal = nullptr;
    static RE::BGSPerk* p_beast = nullptr;
    static RE::BGSPerk* p_dead = nullptr;
    static RE::BGSPerk* p_insect = nullptr;
    static RE::BGSPerk* p_machine = nullptr;
    static RE::BGSPerk* p_skeleton = nullptr;
    static RE::BGSPerk* p_ashguardian = nullptr;
    static RE::BGSPerk* p_ashspawn = nullptr;
    static RE::BGSPerk* p_spriggan = nullptr;
    static RE::BGSPerk* p_flameatronach = nullptr;
    static RE::BGSPerk* p_frostatronach = nullptr;
    static RE::BGSPerk* p_stormatronach = nullptr;
    static RE::BGSPerk* p_hagraven = nullptr;
    static RE::BGSPerk* p_icewraith = nullptr;
    static RE::BGSPerk* p_magicanomaly = nullptr;
    static RE::BGSPerk* p_netch = nullptr;
    static RE::BGSPerk* p_wisp = nullptr;
    static RE::BGSPerk* p_wispmother = nullptr;
    static RE::BGSPerk* p_ghost = nullptr;
    static RE::BGSPerk* p_gargoyle = nullptr;
    static RE::BGSPerk* p_lurker = nullptr;
    static RE::BGSPerk* p_seeker = nullptr;
    static RE::BGSPerk* p_karstaag = nullptr;
    static RE::BGSPerk* p_lowint = nullptr;
    static RE::BGSPerk* p_modint = nullptr;
    static RE::BGSPerk* p_highint = nullptr;

    static RE::SpellItem* s_troll = nullptr;
    static RE::SpellItem* s_daedra = nullptr;
    static RE::SpellItem* s_lycan = nullptr;
    static RE::SpellItem* s_frozen = nullptr;

    static RE::BGSKeyword* k_animalShaggy = nullptr;
    static RE::BGSKeyword* k_beastShaggy = nullptr;
    static RE::BGSKeyword* k_armorCuirass = nullptr;
    static RE::BGSKeyword* k_armorBoots = nullptr;
    static RE::BGSKeyword* k_armorGauntlets = nullptr;
    static RE::BGSKeyword* k_armorHelmet = nullptr;
    static RE::BGSKeyword* k_aHeavyCuirass = nullptr;
    static RE::BGSKeyword* k_aLightCuirass = nullptr;
    static RE::BGSKeyword* k_aLeather = nullptr;
    static RE::BGSKeyword* k_aMetal = nullptr;
    static RE::BGSKeyword* k_aRobes = nullptr;

    static RE::TESGlobal* g_physIntensity = nullptr;
    static RE::TESGlobal* g_magIntensity = nullptr;
    static RE::TESGlobal* g_npcIntensity = nullptr;

    template <class T>
    static T* LookupKye(const char* editorID) {
        return RE::TESForm::LookupByEditorID<T>(editorID);
    }

    static void Resolve() {
        if (s_resolved) return;
        s_resolved = true;
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) { s_resolved = false; return; }
        bool base  = dh->LookupModByName(kPluginBase) != nullptr;
        bool patch = dh->LookupModByName(kPluginPatch) != nullptr;
        s_kye2aLoaded = dh->LookupModByName(kPluginArmor) != nullptr;
        s_kye2Loaded  = base || patch;
        if (!s_kye2Loaded) return;
        p_animal       = LookupKye<RE::BGSPerk>("kye2_animal_perk");
        p_beast        = LookupKye<RE::BGSPerk>("kye2_beast_perk");
        p_dead         = LookupKye<RE::BGSPerk>("kye2_dead_perk");
        p_insect       = LookupKye<RE::BGSPerk>("kye2_insect_perk");
        p_machine      = LookupKye<RE::BGSPerk>("kye2_machine_perk");
        p_skeleton     = LookupKye<RE::BGSPerk>("kye2_skeleton_perk");
        p_ashguardian  = LookupKye<RE::BGSPerk>("kye2_ashguardian_perk");
        p_ashspawn     = LookupKye<RE::BGSPerk>("kye2_ashspawn_perk");
        p_spriggan     = LookupKye<RE::BGSPerk>("kye2_spriggan_perk");
        p_flameatronach = LookupKye<RE::BGSPerk>("kye2_flameatronach_perk");
        p_frostatronach = LookupKye<RE::BGSPerk>("kye2_frostatronach_perk");
        p_stormatronach = LookupKye<RE::BGSPerk>("kye2_stormatronach_perk");
        p_hagraven     = LookupKye<RE::BGSPerk>("kye2_hagraven_perk");
        p_icewraith    = LookupKye<RE::BGSPerk>("kye2_icewraith_perk");
        p_magicanomaly = LookupKye<RE::BGSPerk>("kye2_magicanomaly_perk");
        p_netch        = LookupKye<RE::BGSPerk>("kye2_netch_perk");
        p_wisp         = LookupKye<RE::BGSPerk>("kye2_wisp_perk");
        p_wispmother   = LookupKye<RE::BGSPerk>("kye2_wispmother_perk");
        p_ghost        = LookupKye<RE::BGSPerk>("kye2_ghost_perk");
        p_gargoyle     = LookupKye<RE::BGSPerk>("kye2_gargoyle_perk");
        p_lurker       = LookupKye<RE::BGSPerk>("kye2_lurker_perk");
        p_seeker       = LookupKye<RE::BGSPerk>("kye2_seeker_perk");
        p_karstaag     = LookupKye<RE::BGSPerk>("kye2_karstaag_perk");
        p_lowint       = LookupKye<RE::BGSPerk>("kye2_lowintelligence_perk");
        p_modint       = LookupKye<RE::BGSPerk>("kye2_moderateintelligence_perk");
        p_highint      = LookupKye<RE::BGSPerk>("kye2_highintelligence_perk");
        s_troll   = LookupKye<RE::SpellItem>("kye2_troll_ab");
        s_daedra  = LookupKye<RE::SpellItem>("kye2_daedra_ab");
        s_lycan   = LookupKye<RE::SpellItem>("kye2_lycanthrope_ab");
        s_frozen  = LookupKye<RE::SpellItem>("DLC1AbFrozenCreatures");
        k_animalShaggy   = LookupKye<RE::BGSKeyword>("kye2_animal_shaggy");
        k_beastShaggy    = LookupKye<RE::BGSKeyword>("kye2_beast_shaggy");
        k_armorCuirass   = LookupKye<RE::BGSKeyword>("ArmorCuirass");
        k_armorBoots     = LookupKye<RE::BGSKeyword>("ArmorBoots");
        k_armorGauntlets = LookupKye<RE::BGSKeyword>("ArmorGauntlets");
        k_armorHelmet    = LookupKye<RE::BGSKeyword>("ArmorHelmet");
        k_aHeavyCuirass  = LookupKye<RE::BGSKeyword>("kye2a_heavy_cuirass");
        k_aLightCuirass  = LookupKye<RE::BGSKeyword>("kye2a_light_cuirass");
        k_aLeather       = LookupKye<RE::BGSKeyword>("kye2a_leather");
        k_aMetal         = LookupKye<RE::BGSKeyword>("kye2a_metal");
        k_aRobes         = LookupKye<RE::BGSKeyword>("kye2a_robes");
        g_physIntensity = LookupKye<RE::TESGlobal>("kye2_physical_intensity");
        g_magIntensity  = LookupKye<RE::TESGlobal>("kye2_magical_intensity");
        if (s_kye2aLoaded) {
            if (auto* f = dh->LookupForm(0x801, kPluginArmor))
                g_npcIntensity = f->As<RE::TESGlobal>();
        }
    }

    bool IsKYE2Loaded() {
        Resolve();
        return s_kye2Loaded;
    }

    static bool HasPerk(RE::Actor* a, RE::BGSPerk* p)   { return p && a->HasPerk(p); }
    static bool HasSpell(RE::Actor* a, RE::SpellItem* s) { return s && a->HasSpell(s); }
    static bool HasKwd(RE::Actor* a, RE::BGSKeyword* k)  { return k && a->HasKeyword(k); }

    static bool WornKwd(RE::Actor* a, RE::BGSKeyword* k) {
        if (!k) return false;
        using Slot = RE::BIPED_MODEL::BipedObjectSlot;
        static const Slot slots[] = {
            Slot::kHead, Slot::kBody, Slot::kFeet, Slot::kHands,
            Slot::kAmulet, Slot::kRing, Slot::kShield, Slot::kForearms,
            Slot::kCirclet, Slot::kHair
        };
        for (Slot s : slots) {
            if (auto* armor = a->GetWornArmor(s)) {
                if (armor->HasKeyword(k)) return true;
            }
        }
        return false;
    }

    // ============================================================
    // Badges
    // ============================================================
    std::vector<Badge> GetBadges(RE::Actor* actor) {
        std::vector<Badge> out;
        if (!actor) return out;
        Resolve();
        auto add = [&](const char* text, BadgeCategory cat) {
            out.push_back({text, cat});
        };
        if (!s_kye2Loaded) {
            if (actor->HasKeywordString("ActorTypeDaedra")) add("Daedra", BadgeCategory::Trait);
            return out;
        }
        if (HasPerk(actor, p_animal) && HasKwd(actor, k_animalShaggy)) add("Cold Adapted", BadgeCategory::Trait);
        if (HasPerk(actor, p_beast)  && HasKwd(actor, k_beastShaggy))  add("Cold Adapted", BadgeCategory::Trait);
        if (HasSpell(actor, s_troll)) add("Troll Blood", BadgeCategory::Trait);
        if (HasSpell(actor, s_frozen)) add("Frozen",     BadgeCategory::Trait);
        if (HasSpell(actor, s_daedra)) add("Daedra",     BadgeCategory::Trait);
        if (HasSpell(actor, s_lycan))  add("Lycanthrope", BadgeCategory::Trait);
        if (HasPerk(actor, p_lowint))  add("Unintelligent", BadgeCategory::Intelligence);
        if (HasPerk(actor, p_modint))  add("Aware",         BadgeCategory::Intelligence);
        if (HasPerk(actor, p_highint)) add("Intelligent",   BadgeCategory::Intelligence);
        if (s_kye2aLoaded) {
            if (WornKwd(actor, k_armorCuirass)) {
                if (WornKwd(actor, k_aHeavyCuirass))      add("Heavy Armor", BadgeCategory::Armor);
                else if (WornKwd(actor, k_aLightCuirass)) add("Light Armor", BadgeCategory::Armor);
                else                                      add("Cuirass",     BadgeCategory::Armor);
            }
            if (WornKwd(actor, k_armorCuirass) && WornKwd(actor, k_armorBoots) &&
                WornKwd(actor, k_armorGauntlets) && WornKwd(actor, k_armorHelmet))
                add("Full Set", BadgeCategory::Armor);
            if (WornKwd(actor, k_aLeather))       add("Leather",       BadgeCategory::Armor);
            if (WornKwd(actor, k_aMetal))         add("Metal",         BadgeCategory::Armor);
            if (WornKwd(actor, k_aRobes))         add("Robes",         BadgeCategory::Armor);
        }
        return out;
    }

    // ============================================================
    // Resistance math (port of the Papyrus multiplier pipeline)
    // ============================================================
    std::vector<ResistEntry> GetResists(RE::Actor* actor) {
        std::vector<ResistEntry> out;
        if (!actor) return out;
        Resolve();
        if (!s_kye2Loaded) return out;
        auto gi = [](RE::TESGlobal* g) -> int {
            return g ? static_cast<int>(g->value) : 0;
        };
        int physical_intensity = gi(g_physIntensity);
        int magical_intensity  = gi(g_magIntensity);
        int intensity          = gi(g_npcIntensity);
        if (intensity < 1 || intensity > 4) intensity = magical_intensity;
        const float physical_weakness = 1.25f;
        float physical_resistance_low = 1.0f, physical_resistance_high = 1.0f;
        switch (physical_intensity) {
            case 1: physical_resistance_low = 0.9f;  physical_resistance_high = 0.8f;  break;
            case 2: physical_resistance_low = 0.8f;  physical_resistance_high = 0.67f; break;
            case 3: physical_resistance_low = 0.67f; physical_resistance_high = 0.5f;  break;
            case 4: physical_resistance_low = 0.5f;  physical_resistance_high = 0.25f; break;
            default: break;
        }
        float magical_resistance_low = 1.0f, magical_resistance_high = 1.0f;
        float magical_percent_low = 0.0f, magical_percent_high = 0.0f;
        switch (magical_intensity) {
            case 1: magical_resistance_low = 0.9f;  magical_resistance_high = 0.75f; magical_percent_low = 10; magical_percent_high = 25;  break;
            case 2: magical_resistance_low = 0.75f; magical_resistance_high = 0.5f;  magical_percent_low = 25; magical_percent_high = 50;  break;
            case 3: magical_resistance_low = 0.67f; magical_resistance_high = 0.25f; magical_percent_low = 33; magical_percent_high = 75;  break;
            case 4: magical_resistance_low = 0.5f;  magical_resistance_high = 0.0f;  magical_percent_low = 50; magical_percent_high = 100; break;
            default: break;
        }
        float a_physical_resistance_low = 1.0f, a_physical_resistance_high = 1.0f;
        float a_magical_resistance_low = 1.0f, a_magical_resistance_high = 1.0f;
        float a_magical_percent_low = 0.0f, a_magical_percent_high = 0.0f;
        switch (intensity) {
            case 1: a_physical_resistance_low = 0.9f;  a_physical_resistance_high = 0.8f;
                    a_magical_resistance_low = 0.9f;  a_magical_resistance_high = 0.75f; a_magical_percent_low = 10; a_magical_percent_high = 25;  break;
            case 2: a_physical_resistance_low = 0.8f;  a_physical_resistance_high = 0.67f;
                    a_magical_resistance_low = 0.75f; a_magical_resistance_high = 0.5f;  a_magical_percent_low = 25; a_magical_percent_high = 50;  break;
            case 3: a_physical_resistance_low = 0.67f; a_physical_resistance_high = 0.5f;
                    a_magical_resistance_low = 0.67f; a_magical_resistance_high = 0.25f; a_magical_percent_low = 33; a_magical_percent_high = 75;  break;
            case 4: a_physical_resistance_low = 0.5f;  a_physical_resistance_high = 0.25f;
                    a_magical_resistance_low = 0.5f;  a_magical_resistance_high = 0.0f;  a_magical_percent_low = 50; a_magical_percent_high = 100; break;
            default: break;
        }
        float blade = 1.0f, blunt = 1.0f, axe = 1.0f, bow = 1.0f, crossbow = 1.0f;
        float sneak = 1.0f, fist = 1.0f, bash = 1.0f, drain = 1.0f, illusion = 1.0f;
        auto* avo = actor->AsActorValueOwner();
        float fire    = avo ? avo->GetActorValue(RE::ActorValue::kResistFire)    : 0.0f;
        float frost   = avo ? avo->GetActorValue(RE::ActorValue::kResistFrost)   : 0.0f;
        float shock   = avo ? avo->GetActorValue(RE::ActorValue::kResistShock)   : 0.0f;
        float poison  = avo ? avo->GetActorValue(RE::ActorValue::kPoisonResist)  : 0.0f;
        float disease = avo ? avo->GetActorValue(RE::ActorValue::kResistDisease) : 0.0f;
        const float pw = physical_weakness;
        const float pl = physical_resistance_low;
        const float ph = physical_resistance_high;
        if (HasPerk(actor, p_animal)) { bow *= pw; }
        if (HasPerk(actor, p_beast))  { blade *= pw; axe *= pl; blunt *= ph; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_dead))   { axe *= pw; blunt *= pl; bow *= ph; crossbow *= pw; fist *= pl; bash *= pl; }
        if (HasPerk(actor, p_insect)) { blade *= pl; axe *= pl; bow *= pl; }
        if (HasPerk(actor, p_machine)){ blade *= ph; axe *= pl; bow *= ph; crossbow *= ph; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_skeleton)){ blade *= ph; axe *= pl; blunt *= pw; bow *= ph; crossbow *= ph; bash *= pw; }
        if (HasPerk(actor, p_ashguardian)){ blade *= ph; axe *= pl; bow *= ph; crossbow *= pl; fist *= pl; bash *= pl; }
        if (HasPerk(actor, p_ashspawn)){ blade *= ph; axe *= pl; bow *= ph; crossbow *= pl; fist *= pl; bash *= pl; }
        if (HasPerk(actor, p_spriggan)){ blade *= pl; blunt *= pl; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_flameatronach)){ fist *= pl; bash *= pl; }
        if (HasPerk(actor, p_frostatronach)){ blade *= ph; axe *= ph; bow *= ph; crossbow *= pl; fist *= ph; bash *= ph; blunt *= pw; }
        if (HasPerk(actor, p_hagraven)){ blade *= pw; }
        if (HasPerk(actor, p_icewraith)){ blade *= pl; axe *= pl; bow *= pl; }
        if (HasPerk(actor, p_karstaag)){ axe *= pl; blunt *= ph; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_magicanomaly)){ blade *= pl; axe *= pl; }
        if (HasPerk(actor, p_netch)){ axe *= pl; blunt *= ph; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_stormatronach)){ blade *= ph; axe *= ph; blunt *= pl; bow *= ph; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_wisp)){ blade *= pl; }
        if (HasPerk(actor, p_wispmother)){ blunt *= pl; bow *= pl; fist *= pl; bash *= pl; }
        if (HasPerk(actor, p_ghost)){ blade *= ph; axe *= ph; blunt *= ph; bow *= ph; crossbow *= ph; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_gargoyle)){ blade *= ph; blunt *= pw; bow *= ph; crossbow *= pl; }
        if (HasPerk(actor, p_seeker)){ blade *= pw; axe *= pl; blunt *= ph; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_lurker)){ axe *= pl; blunt *= ph; crossbow *= pl; fist *= ph; bash *= ph; }
        if (HasPerk(actor, p_lowint)) { sneak *= ph; }
        if (HasPerk(actor, p_modint)) { sneak *= pl; drain *= magical_resistance_low;  illusion *= magical_resistance_low; }
        if (HasPerk(actor, p_highint)){ drain *= magical_resistance_high; illusion *= magical_resistance_high; }
        bool wearsCuirass = s_kye2aLoaded && WornKwd(actor, k_armorCuirass);
        if (wearsCuirass) {
            if (WornKwd(actor, k_aHeavyCuirass)) {
                blade *= a_physical_resistance_high; axe *= a_physical_resistance_low;
                bow *= a_physical_resistance_high; crossbow *= a_physical_resistance_low; fist *= a_physical_resistance_low;
            } else if (WornKwd(actor, k_aLightCuirass)) {
                blunt *= a_physical_resistance_low; crossbow *= a_physical_resistance_low; fist *= a_physical_resistance_low;
            }
            sneak *= a_physical_resistance_low;
        }
        if (wearsCuirass && WornKwd(actor, k_armorBoots) &&
            WornKwd(actor, k_armorGauntlets) && WornKwd(actor, k_armorHelmet)) {
            illusion *= 1.33f; drain *= 1.33f;
        }
        if (s_kye2aLoaded && WornKwd(actor, k_aRobes)) {
            illusion *= magical_resistance_high; drain *= magical_resistance_high;
        }
        auto classifyPhys = [&](float v, const char* label, float low, float high) {
            if (v <= high)        out.push_back({label, ResistTier::HighlyResistant});
            else if (v <= low)    out.push_back({label, ResistTier::Resistant});
            else if (v > 1.05f)   out.push_back({label, ResistTier::Weak});
        };
        auto classifyPct = [&](float v, const char* label, float low, float high) {
            if (high > 0.0f && v >= high && v > 5.0f)
                out.push_back({label, ResistTier::HighlyResistant});
            else if (low > 0.0f && v >= low && v > 5.0f)
                out.push_back({label, ResistTier::Resistant});
            else if (v < -5.0f)
                out.push_back({label, ResistTier::Weak});
        };
        if (wearsCuirass) {
            const float aL = a_physical_resistance_low, aH = a_physical_resistance_high;
            classifyPhys(blade,    "Blade",    aL, aH);
            classifyPhys(blunt,    "Blunt",    aL, aH);
            classifyPhys(axe,      "Axe",      aL, aH);
            classifyPhys(bow,      "Bow",      aL, aH);
            classifyPhys(crossbow, "Crossbow", aL, aH);
            classifyPhys(fist,     "Fist",     aL, aH);
            classifyPhys(bash,     "Bash",     aL, aH);
            classifyPct(fire,      "Fire",      a_magical_percent_low, a_magical_percent_high);
            classifyPct(frost,     "Frost",     a_magical_percent_low, a_magical_percent_high);
            classifyPct(shock,     "Shock",     a_magical_percent_low, a_magical_percent_high);
            classifyPct(poison,    "Poison",    a_magical_percent_low, a_magical_percent_high);
            classifyPct(disease,   "Disease",   a_magical_percent_low, a_magical_percent_high);
            classifyPhys(sneak,    "Sneak",    aL, aH);
            if (drain <= a_magical_resistance_high)     out.push_back({"Drain", ResistTier::HighlyResistant});
            else if (drain <= a_magical_resistance_low) out.push_back({"Drain", ResistTier::Resistant});
            else if (drain > 1.05f)                     out.push_back({"Drain", ResistTier::Weak});
            if (illusion <= a_magical_resistance_high)     out.push_back({"Illusion", ResistTier::HighlyResistant});
            else if (illusion <= a_magical_resistance_low) out.push_back({"Illusion", ResistTier::Resistant});
            else if (illusion > 1.05f)                     out.push_back({"Illusion", ResistTier::Weak});
        } else {
            classifyPhys(blade,    "Blade",    pl, ph);
            classifyPhys(blunt,    "Blunt",    pl, ph);
            classifyPhys(axe,      "Axe",      pl, ph);
            classifyPhys(bow,      "Bow",      pl, ph);
            classifyPhys(crossbow, "Crossbow", pl, ph);
            classifyPhys(fist,     "Fist",     pl, ph);
            classifyPhys(bash,     "Bash",     pl, ph);
            classifyPct(fire,      "Fire",      magical_percent_low, magical_percent_high);
            classifyPct(frost,     "Frost",     magical_percent_low, magical_percent_high);
            classifyPct(shock,     "Shock",     magical_percent_low, magical_percent_high);
            classifyPct(poison,    "Poison",    magical_percent_low, magical_percent_high);
            classifyPct(disease,   "Disease",   magical_percent_low, magical_percent_high);
            classifyPhys(sneak,    "Sneak",    pl, ph);
            if (drain <= magical_resistance_high)     out.push_back({"Drain", ResistTier::HighlyResistant});
            else if (drain <= magical_resistance_low) out.push_back({"Drain", ResistTier::Resistant});
            else if (drain > 1.05f)                   out.push_back({"Drain", ResistTier::Weak});
            if (illusion <= magical_resistance_high)     out.push_back({"Illusion", ResistTier::HighlyResistant});
            else if (illusion <= magical_resistance_low) out.push_back({"Illusion", ResistTier::Resistant});
            else if (illusion > 1.05f)                   out.push_back({"Illusion", ResistTier::Weak});
        }
        if (actor->HasKeywordString("ActorTypeUndead"))
            out.push_back({"Sun", ResistTier::Weak});
        return out;
    }
}
