#pragma once

// STATUS: Working
// Categorizes magic effects and provides icons.


#include <RE/Skyrim.h>
// Effect display category
namespace EffectCategories {
    enum class EffectType {
        Standalone,
        Modifier,
        Unknown
    };
    // used for source badge/tint
    enum class EffectSource {
        Unknown,
        Spell,
        Power,
        Shout,
        Potion,
        Food,
        Ingredient,
        Scroll,
        Blessing,
        Enchantment,
        Ability,
        Staff,
    };

    EffectSource GetEffectSource(RE::ActiveEffect* activeEffect);
    // Get icon ID for source badge
    inline const char* GetSourceIconId(EffectSource source) {
        switch (source) {
            case EffectSource::Spell:       return "source_spell";
            case EffectSource::Power:       return "source_power";
            case EffectSource::Shout:       return "source_shout";
            case EffectSource::Potion:      return "source_potion";
            case EffectSource::Food:        return "source_food";
            case EffectSource::Ingredient:  return "source_ingredient";
            case EffectSource::Scroll:      return "source_scroll";
            case EffectSource::Blessing:    return "source_blessing";
            default:                        return "";
        }
    }

    inline const char* GetSpellSchoolIconId(RE::MagicItem* spell) {
        if (!spell) return "";
        auto* spellItem = spell->As<RE::SpellItem>();
        if (!spellItem) return "";
        switch (spellItem->GetAssociatedSkill()) {
            case RE::ActorValue::kAlteration:  return "source_alteration";
            case RE::ActorValue::kConjuration: return "source_conjuration";
            case RE::ActorValue::kDestruction: return "source_destruction";
            case RE::ActorValue::kIllusion:    return "source_illusion";
            case RE::ActorValue::kRestoration: return "source_restoration";
            default:                           return "";
        }
    }

    EffectType GetEffectType(RE::EffectArchetypes::ArchetypeID archetype);
    const char* GetArchetypeName(RE::EffectArchetypes::ArchetypeID archetype);
    const char* GetActorValueName(RE::ActorValue av);
    bool IsNegativeArchetype(RE::EffectArchetypes::ArchetypeID archetype);
    const char* GetStandaloneIconId(RE::EffectArchetypes::ArchetypeID archetype);
    const char* GetActorValueIconId(RE::ActorValue av);
    bool IsPercentageMagnitude(RE::ActorValue av);
    const char* GetScriptEffectIconId(const std::string& name, RE::ActorValue school = RE::ActorValue::kNone);
    const char* GetFrostbiteIconId(const std::string& name);
}
