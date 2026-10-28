#include "../includes/AllyEventHandler.h"
#include "../includes/ImGui/FollowerHUD.h"
#include "../includes/HUDVisibility.h"
#include "../includes/UIAnimation.h"
#include "../includes/Settings.h"

#include <chrono>
#include <unordered_map>

namespace AllyEvents {
    
    RE::BSEventNotifyControl CombatEventSink::ProcessEvent(
        const RE::TESCombatEvent* event,
        RE::BSTEventSource<RE::TESCombatEvent>*) {
        
        if (!event) return RE::BSEventNotifyControl::kContinue;
        
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;
        
        auto actor = event->actor.get();
        auto target = event->targetActor.get();
        
        bool playerInvolved = (actor == player) || (target == player);
        
        if (playerInvolved) {
            FollowerHUD::MarkQuestAlliesDirty();

            if (event->newState == RE::ACTOR_COMBAT_STATE::kCombat) {
                logger::debug("AllyEvents: Player entered combat - quest allies cache marked dirty");
                logger::debug("HUDVisibility [Resist+Equip]: combat start — triggering");
                HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerResist);
                HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEquip);
            } else if (event->newState == RE::ACTOR_COMBAT_STATE::kNone) {
                logger::debug("AllyEvents: Player exited combat - quest allies cache marked dirty");
            }
        }
        
        return RE::BSEventNotifyControl::kContinue;
    }
    
    RE::BSEventNotifyControl DeathEventSink::ProcessEvent(
        const RE::TESDeathEvent* event,
        RE::BSTEventSource<RE::TESDeathEvent>*) {
        
        if (!event) return RE::BSEventNotifyControl::kContinue;
        
        auto dyingActor = event->actorDying.get();
        if (!dyingActor) return RE::BSEventNotifyControl::kContinue;
        
        if (auto actor = dyingActor->As<RE::Actor>()) {
            const char* actorName = actor->GetName();
            std::string safeName = (actorName && strlen(actorName) > 0) ? actorName : "Unknown";
            
            FollowerHUD::RemoveFromCache(actor);
            logger::debug("AllyEvents: Actor '{}' died - removed from cache", safeName);
        }
        
        return RE::BSEventNotifyControl::kContinue;
    }
    
    RE::BSEventNotifyControl SpellCastEventSink::ProcessEvent(
        const RE::TESSpellCastEvent* event,
        RE::BSTEventSource<RE::TESSpellCastEvent>*) {
        
        if (!event) return RE::BSEventNotifyControl::kContinue;
        
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;
        
        auto caster = event->object.get();
        if (caster != player) return RE::BSEventNotifyControl::kContinue;
        
        auto spell = RE::TESForm::LookupByID<RE::SpellItem>(event->spell);
        if (!spell) return RE::BSEventNotifyControl::kContinue;
        
        bool isSummonSpell = false;
        for (auto* effect : spell->effects) {
            if (!effect || !effect->baseEffect) continue;
            
            auto archetype = effect->baseEffect->GetArchetype();
            if (archetype == RE::EffectArchetypes::ArchetypeID::kSummonCreature ||
                archetype == RE::EffectArchetypes::ArchetypeID::kReanimate ||
                archetype == RE::EffectArchetypes::ArchetypeID::kCommandSummoned) {
                isSummonSpell = true;
                break;
            }
        }
        
        if (isSummonSpell) {
            FollowerHUD::MarkSummonsDirty();
            logger::debug("AllyEvents: Summon spell cast - summons cache marked dirty");
        }
        
        return RE::BSEventNotifyControl::kContinue;
    }
    
    RE::BSEventNotifyControl MagicEffectApplySink::ProcessEvent(
        const RE::TESMagicEffectApplyEvent* event,
        RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*)
    {
        if (!event) return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;

        if (event->target.get() != player) return RE::BSEventNotifyControl::kContinue;

        HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEffects);

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EquipEventSink::ProcessEvent(
        const RE::TESEquipEvent* event,
        RE::BSTEventSource<RE::TESEquipEvent>*)
    {
        if (!event) return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;

        if (event->actor.get() != player) return RE::BSEventNotifyControl::kContinue;

        auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form) return RE::BSEventNotifyControl::kContinue;

        auto ft = form->GetFormType();
        if (ft == RE::FormType::Weapon || ft == RE::FormType::Spell ||
            ft == RE::FormType::Armor  || ft == RE::FormType::Shout) {
            logger::debug("[Equipment] TESEquipEvent ({}) — triggering visibility", form->GetName());
            HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerEquip);
            HUDVisibility::Trigger(HUDVisibility::WidgetID::PlayerResist);
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    static UIAnimation::AttackCategory ResolveSpellCategory(RE::SpellItem* spell) {
        if (!spell) return UIAnimation::AttackCategory::SpellMagic;
        for (auto* effect : spell->effects) {
            if (!effect || !effect->baseEffect) continue;
            auto resist = effect->baseEffect->data.resistVariable;
            if (resist == RE::ActorValue::kResistFire) return UIAnimation::AttackCategory::SpellFire;
            if (resist == RE::ActorValue::kResistFrost) return UIAnimation::AttackCategory::SpellFrost;
            if (resist == RE::ActorValue::kResistShock) return UIAnimation::AttackCategory::SpellShock;
        }
        return UIAnimation::AttackCategory::SpellMagic;
    }

    static bool IsRestorativeMagic(RE::TESForm* sourceForm) {
        if (!sourceForm) return false;
        auto* magicItem = sourceForm->As<RE::MagicItem>();
        if (!magicItem) return false;

        for (auto* effect : magicItem->effects) {
            if (!effect || !effect->baseEffect) continue;
            auto* base = effect->baseEffect;
            if (base->IsHostile() || base->IsDetrimental()) return false;
            if (base->data.primaryAV == RE::ActorValue::kHealth && effect->effectItem.magnitude > 0.0f) return true;
            if (base->GetMagickSkill() == RE::ActorValue::kRestoration && effect->effectItem.magnitude > 0.0f) return true;
        }
        return false;
    }

    // Buff/utility spells (e.g. Lost Grimoire "Bestow Gift", magnitude 0 script effect)
    static bool IsBeneficialMagic(RE::TESForm* sourceForm) {
        if (!sourceForm) return false;
        auto* magicItem = sourceForm->As<RE::MagicItem>();
        if (!magicItem || magicItem->effects.empty()) return false;

        for (auto* effect : magicItem->effects) {
            if (!effect || !effect->baseEffect) continue;
            if (effect->baseEffect->IsHostile() || effect->baseEffect->IsDetrimental()) return false;
        }
        return true;
    }

    // buff on followers every second (aura/regen scripts, concentration spells) would
    static bool IsFeedbackOnCooldown(RE::FormID id, bool isHeal) {
        constexpr auto kCooldown = std::chrono::milliseconds(3000);
        static std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> s_last;
        const std::uint64_t key = static_cast<std::uint64_t>(id) | (isHeal ? (1ull << 32) : 0ull);
        const auto now = std::chrono::steady_clock::now();
        auto it = s_last.find(key);
        if (it != s_last.end() && now - it->second < kCooldown) return true;
        s_last[key] = now;
        return false;
    }

    // ==================== HIT EVENT SINK ====================
    RE::BSEventNotifyControl HitEventSink::ProcessEvent(
        const RE::TESHitEvent* event,
        RE::BSTEventSource<RE::TESHitEvent>*)
    {
        if (!event || !event->target) return RE::BSEventNotifyControl::kContinue;

        auto* targetActor = event->target->As<RE::Actor>();
        if (!targetActor || targetActor->IsDead()) return RE::BSEventNotifyControl::kContinue;

        // Beneficial spells (heal = emerald, other buffs = orange) never count as damage.
        if (auto* srcForm = RE::TESForm::LookupByID(event->source)) {
            const bool isHeal = IsRestorativeMagic(srcForm);
            if (isHeal || IsBeneficialMagic(srcForm)) {
                auto* settings = Settings::GetSingleton();
                const bool flashEnabled = settings && (isHeal ? settings->qhHealFlash : settings->qhBuffFlash);
                if (flashEnabled && !targetActor->IsPlayerRef() && !IsFeedbackOnCooldown(targetActor->GetFormID(), isHeal)) {
                    const auto allies = FollowerHUD::GetCachedAllies();
                    for (auto* ally : allies) {
                        if (ally && ally->GetFormID() == targetActor->GetFormID()) {
                            std::string key = "portrait_follower_" + std::to_string(targetActor->GetFormID());
                            if (isHeal) UIAnimation::TriggerHealFeedback(key);
                            else        UIAnimation::TriggerBuffFeedback(key);
                            break;
                        }
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        }

        // Suppress hit shake and flash when the actor is already in downed/bleedout state
        auto* avo = targetActor->AsActorValueOwner();
        if ((targetActor->AsActorState() && targetActor->AsActorState()->IsBleedingOut()) ||
            (avo && avo->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        bool isBlocked = event->flags.all(RE::TESHitEvent::Flag::kHitBlocked);

        if (targetActor->IsPlayerRef()) {
            UIAnimation::TriggerHitFeedback("portrait_player", isBlocked);
        } else {
            const auto allies = FollowerHUD::GetCachedAllies();
            for (auto* ally : allies) {
                if (ally && ally->GetFormID() == targetActor->GetFormID()) {
                    std::string key = "portrait_follower_" + std::to_string(targetActor->GetFormID());
                    UIAnimation::TriggerHitFeedback(key, isBlocked);
                    break;
                }
            }
        }

        // Outgoing attack feedback: check if the aggressor is an ally landing a hit
        if (event->cause) {
            auto* aggressor = event->cause->As<RE::Actor>();
            if (aggressor) {
                const auto allies = FollowerHUD::GetCachedAllies();
                for (auto* ally : allies) {
                    if (ally && ally->GetFormID() == aggressor->GetFormID()) {
                        UIAnimation::AttackCategory cat = UIAnimation::AttackCategory::Melee;
                        if (auto* weap = RE::TESForm::LookupByID<RE::TESObjectWEAP>(event->source)) {
                            if (weap->IsBow() || weap->IsCrossbow()) {
                                cat = UIAnimation::AttackCategory::Ranged;
                            }
                        } else if (auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(event->source)) {
                            cat = ResolveSpellCategory(spell);
                        } else if (event->projectile) {
                            cat = UIAnimation::AttackCategory::Ranged;
                        }
                        bool isPower = event->flags.all(RE::TESHitEvent::Flag::kPowerAttack);
                        std::string key = "combat_badge_" + std::to_string(aggressor->GetFormID());
                        UIAnimation::TriggerAttackFeedback(key, cat, isPower);
                        break;
                    }
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void RegisterEventSinks() {
        auto scriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (!scriptEventSource) {
            logger::error("AllyEvents: Failed to get ScriptEventSourceHolder");
            return;
        }
        
        scriptEventSource->AddEventSink(CombatEventSink::GetSingleton());
        
        scriptEventSource->AddEventSink(DeathEventSink::GetSingleton());
        
        scriptEventSource->AddEventSink(SpellCastEventSink::GetSingleton());

        scriptEventSource->AddEventSink(MagicEffectApplySink::GetSingleton());

        scriptEventSource->AddEventSink(EquipEventSink::GetSingleton());

        // Register hit event sink
        scriptEventSource->AddEventSink(HitEventSink::GetSingleton());

        logger::info("AllyEvents: All event sinks registered successfully");
    }
    
}
