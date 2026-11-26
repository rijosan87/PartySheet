#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <string>
#include <vector>
#include "../EffectCategories.h"

struct ID3D11ShaderResourceView;

namespace EffectAttribution {

    enum class OwnerKind : std::uint8_t {
        Equipment, Blessing, Potion, Disease, Need, Race, Stone, Passive, Spell
    };

    enum class EffectClass : std::uint8_t {
        Timed,
        Condition,
        Gear,
        Permanent
    };

    struct Record {
        RE::ActiveEffect*   effect    = nullptr;
        RE::EffectSetting*  mgef      = nullptr;
        RE::MagicItem*      spell     = nullptr;
        RE::SpellItem*      spellItem = nullptr;
        RE::TESBoundObject* item      = nullptr;
        EffectCategories::EffectSource mechanism = EffectCategories::EffectSource::Unknown;
        OwnerKind      kind        = OwnerKind::Spell;
        RE::ActorValue primaryAV   = RE::ActorValue::kNone;
        RE::ActorValue secondaryAV = RE::ActorValue::kNone;
        float rawMagnitude = 0.0f;
        float magnitude    = 0.0f;
        float duration     = 0.0f;
        float timeLeft     = -1.0f;
        bool  hostile      = false;
        bool  hiddenInUI   = false;
        bool  kye2Armor    = false;
        const char* effectName = "";
        const char* sourceName = "";
        RE::BGSPerk* grantedByPerk = nullptr;
        EffectClass cls = EffectClass::Timed;
    };

    const std::vector<Record>& Get(RE::Actor* actor);

    const Record* Find(RE::Actor* actor, const RE::ActiveEffect* effect);
    EffectClass ClassOf(RE::Actor* actor, const RE::ActiveEffect* effect);
    bool ShowsOnHud(EffectClass cls);

    struct GroupKey {
        const RE::EffectSetting* mgef   = nullptr;
        const void*              source = nullptr;
        bool operator==(const GroupKey&) const = default;
    };
    GroupKey GroupKeyOf(const RE::ActiveEffect* effect);

    std::string OwnerName(const Record& r);
    ID3D11ShaderResourceView* OwnerIcon(const Record& r);
    const std::string& KindLabel(OwnerKind kind);
    std::string OwnerOrigin(const Record& r);
    float DisplayMagnitude(const Record& r, RE::ActorValue av);

    bool IsStandingStoneSource(RE::SpellItem* sp);
    RE::SpellItem* GetStandingStone(RE::Actor* actor);
    bool IsKye2ArmorSource(RE::MagicItem* spell);
    ID3D11ShaderResourceView* ResolveMagicIcon(RE::MagicItem* item);
    ID3D11ShaderResourceView* ResolveBlessingIcon(RE::SpellItem* spell);

    bool HasTimedBuff(RE::Actor* actor, RE::ActorValue stat);
    bool HasStatDebuff(RE::Actor* actor, RE::ActorValue stat);

    struct StatChange {
        RE::ActorValue av    = RE::ActorValue::kNone;
        float          value = 0.0f;
        bool           percent = false;
    };

    std::vector<StatChange> SpellStatChanges(RE::Actor* actor, const RE::MagicItem* spell, bool hiddenOnly = false);

    struct NeedPoolCut {
        const Record* record = nullptr;
        StatChange    change;
    };
    std::vector<NeedPoolCut> NeedPoolCuts(RE::Actor* actor);

    std::vector<const Record*> NeedStages(RE::Actor* actor);

    struct StatLine {
        std::string text;
        bool        cut = false;
    };
    std::vector<StatLine> FormatStatChanges(const std::vector<StatChange>& changes);
}
