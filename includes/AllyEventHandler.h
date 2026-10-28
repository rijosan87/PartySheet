#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace AllyEvents {
    
    class CombatEventSink : public RE::BSTEventSink<RE::TESCombatEvent> {
    public:
        static CombatEventSink* GetSingleton() {
            static CombatEventSink singleton;
            return &singleton;
        }
        
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESCombatEvent* event,
            RE::BSTEventSource<RE::TESCombatEvent>*) override;
        
    private:
        CombatEventSink() = default;
        CombatEventSink(const CombatEventSink&) = delete;
        CombatEventSink& operator=(const CombatEventSink&) = delete;
    };
    
    class DeathEventSink : public RE::BSTEventSink<RE::TESDeathEvent> {
    public:
        static DeathEventSink* GetSingleton() {
            static DeathEventSink singleton;
            return &singleton;
        }
        
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESDeathEvent* event,
            RE::BSTEventSource<RE::TESDeathEvent>*) override;
        
    private:
        DeathEventSink() = default;
        DeathEventSink(const DeathEventSink&) = delete;
        DeathEventSink& operator=(const DeathEventSink&) = delete;
    };
    
    class SpellCastEventSink : public RE::BSTEventSink<RE::TESSpellCastEvent> {
    public:
        static SpellCastEventSink* GetSingleton() {
            static SpellCastEventSink singleton;
            return &singleton;
        }
        
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSpellCastEvent* event,
            RE::BSTEventSource<RE::TESSpellCastEvent>*) override;
        
    private:
        SpellCastEventSink() = default;
        SpellCastEventSink(const SpellCastEventSink&) = delete;
        SpellCastEventSink& operator=(const SpellCastEventSink&) = delete;
    };
    
    class MagicEffectApplySink : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent> {
    public:
        static MagicEffectApplySink* GetSingleton() {
            static MagicEffectApplySink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESMagicEffectApplyEvent* event,
            RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override;

    private:
        MagicEffectApplySink() = default;
        MagicEffectApplySink(const MagicEffectApplySink&) = delete;
        MagicEffectApplySink& operator=(const MagicEffectApplySink&) = delete;
    };

    class EquipEventSink : public RE::BSTEventSink<RE::TESEquipEvent> {
    public:
        static EquipEventSink* GetSingleton() {
            static EquipEventSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESEquipEvent* event,
            RE::BSTEventSource<RE::TESEquipEvent>*) override;

    private:
        EquipEventSink() = default;
        EquipEventSink(const EquipEventSink&) = delete;
        EquipEventSink& operator=(const EquipEventSink&) = delete;
    };

    // Hit event sink - triggers discrete portrait hit feedback for player and active followers
    class HitEventSink : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        static HitEventSink* GetSingleton() {
            static HitEventSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESHitEvent* event,
            RE::BSTEventSource<RE::TESHitEvent>*) override;

    private:
        HitEventSink() = default;
        HitEventSink(const HitEventSink&) = delete;
        HitEventSink& operator=(const HitEventSink&) = delete;
    };

    void RegisterEventSinks();
    
}
