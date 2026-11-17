#include "../../includes/Utils/SpellDamageResolver.h"
#include "../../includes/Utils/PerkModifiers.h"

namespace SpellDamageResolver {

    namespace {
        using Flag      = RE::EffectSetting::EffectSettingData::Flag;
        using Archetype = RE::EffectArchetypes::ArchetypeID;

        bool IsPool(RE::ActorValue av) {
            return av == RE::ActorValue::kHealth || av == RE::ActorValue::kStamina || av == RE::ActorValue::kMagicka;
        }

        RE::ActorValue PowerModifierFor(RE::ActorValue school) {
            switch (school) {
            case RE::ActorValue::kAlteration:  return RE::ActorValue::kAlterationPowerModifier;
            case RE::ActorValue::kConjuration: return RE::ActorValue::kConjurationPowerModifier;
            case RE::ActorValue::kDestruction: return RE::ActorValue::kDestructionPowerModifier;
            case RE::ActorValue::kIllusion:    return RE::ActorValue::kIllusionPowerModifier;
            case RE::ActorValue::kRestoration: return RE::ActorValue::kRestorationPowerModifier;
            default:                           return RE::ActorValue::kNone;
            }
        }
    }

    std::string ResistTypeName(RE::ActorValue resist) {
        switch (resist) {
        case RE::ActorValue::kResistFire:   return "fire_resist";
        case RE::ActorValue::kResistFrost:  return "frost_resist";
        case RE::ActorValue::kResistShock:  return "shock_resist";
        case RE::ActorValue::kPoisonResist: return "poison_resist";
        default:                            return "magic_resist";
        }
    }

    float MagicDamageEffect::Total(bool isConcentration) const {
        if (isConcentration || duration <= 1.0f) return magnitude;
        return magnitude * duration;
    }

    void CollectDamageEffects(const RE::MagicItem* item, std::vector<MagicDamageEffect>& out) {
        if (!item) return;
        std::uint32_t index = 0;
        for (auto* effect : item->effects) {
            const std::uint32_t effectIndex = index++;
            if (!effect || !effect->baseEffect) continue;
            auto* mgef = effect->baseEffect;
            const float mag = effect->effectItem.magnitude;
            if (mag <= 0.0f || !(mgef->IsHostile() || mgef->IsDetrimental())) continue;

            const auto arch = mgef->GetArchetype();
            if (arch != Archetype::kValueModifier && arch != Archetype::kDualValueModifier && arch != Archetype::kAbsorb) continue;
            if (mgef->data.flags.any(Flag::kRecover)) continue;
            if (!IsPool(mgef->data.primaryAV)) continue;

            MagicDamageEffect e;
            e.target        = mgef->data.primaryAV;
            e.resist        = mgef->data.resistVariable;
            e.baseMagnitude = mag;
            e.magnitude     = mag;
            e.name          = mgef->GetName() ? mgef->GetName() : "";
            e.effectIndex   = effectIndex;
            e.school        = mgef->data.associatedSkill;
            e.duration      = mgef->data.flags.any(Flag::kNoDuration) ? 0.0f : static_cast<float>(effect->effectItem.duration);
            e.conditional   = effect->conditions.head != nullptr || mgef->conditions.head != nullptr;
            e.powerAffectsMagnitude = mgef->data.flags.any(Flag::kPowerAffectsMagnitude);
            e.powerAffectsDuration  = mgef->data.flags.any(Flag::kPowerAffectsDuration);
            if (arch == Archetype::kDualValueModifier && IsPool(mgef->data.secondaryAV)) {
                e.secondaryTarget = mgef->data.secondaryAV;
                e.secondaryWeight = mgef->data.secondAVWeight;
            }
            if (e.target == RE::ActorValue::kHealth)       e.type = ResistTypeName(e.resist);
            else if (e.target == RE::ActorValue::kStamina) e.type = "stamina";
            else                                           e.type = "magicka";
            out.push_back(std::move(e));
        }
    }

    bool SpellDamage::HasDamage() const {
        return Primary() != nullptr;
    }

    const MagicDamageEffect* SpellDamage::Primary() const {
        const MagicDamageEffect* best = nullptr;
        for (const auto& e : effects) {
            if (e.target != RE::ActorValue::kHealth || e.conditional) continue;
            if (!best || e.Total(isConcentration) > best->Total(isConcentration)) best = &e;
        }
        return best;
    }

    const MagicDamageEffect* SpellDamage::Drain() const {
        const MagicDamageEffect* best = nullptr;
        for (const auto& e : effects) {
            if (e.target == RE::ActorValue::kHealth || e.conditional) continue;
            if (!best || e.Total(isConcentration) > best->Total(isConcentration)) best = &e;
        }
        return best;
    }

    float SpellDamage::Headline() const {
        float total = 0.0f;
        for (const auto& e : effects)
            if (e.target == RE::ActorValue::kHealth && !e.conditional) total += e.Total(isConcentration);
        return total;
    }

    SpellDamage ResolveSpellDamage(RE::Actor* actor, RE::TESForm* equippedForm) {
        SpellDamage sd;
        auto* spell = equippedForm ? equippedForm->As<RE::SpellItem>() : nullptr;
        auto* avo   = actor ? actor->AsActorValueOwner() : nullptr;
        if (!spell || !avo) return sd;

        sd.valid           = true;
        sd.isConcentration = spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
        sd.school          = spell->GetAssociatedSkill();
        CollectDamageEffects(spell, sd.effects);
        if (sd.effects.empty()) return sd;

        const auto est = PerkModifiers::GetSpellEstimate(actor, spell);
        for (auto& e : sd.effects) {
            if (est.known && e.effectIndex < est.magnitudes.size()) {
                e.perkMult = est.magnitudes[e.effectIndex] / e.baseMagnitude;
                if (e.duration > 0.0f) e.duration = est.durations[e.effectIndex];
            }
            const auto  powerAV = PowerModifierFor(e.school);
            const float power   = powerAV != RE::ActorValue::kNone ? 1.0f + avo->GetActorValue(powerAV) / 100.0f : 1.0f;
            if (e.powerAffectsMagnitude) e.fortifyMult = power;
            if (e.powerAffectsDuration)  e.duration *= power;
            e.magnitude = e.baseMagnitude * e.perkMult * e.fortifyMult;
        }
        return sd;
    }

}
