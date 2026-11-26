#include "../../includes/Utils/SpellTags.h"

#include "IntentModel/IntentModel.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace SpellTags {

    namespace {
        struct WordTag { const char* word; std::uint32_t tag; };
        constexpr WordTag kWords[] = {
            { "intent:damage",  kDamage  },
            { "intent:heal",    kHeal    },
            { "intent:buff",    kBuff    },
            { "intent:debuff",  kDebuff  },
            { "intent:resist",  kResist  },
            { "intent:control", kControl },
            { "intent:summon",  kSummon  },
            { "intent:bound",   kBound   },
            { "intent:stealth", kStealth },
            { "fire damage",    kFire    }, { "fire resist",   kFire   },
            { "frost damage",   kFrost   }, { "frost resist",  kFrost  },
            { "shock damage",   kShock   }, { "shock resist",  kShock  },
            { "poison damage",  kPoison  }, { "poison resist", kPoison },
        };

        std::uint32_t FromIntentModel(const RE::SpellItem* spell) {
            std::vector<std::string> words;
            IntentModel::AddSpellIntentFields(words, spell);
            const auto delivery = spell->GetDelivery();
            for (const auto* effect : spell->effects)
                IntentModel::AddEffectIntentFields(words, effect, delivery);

            std::uint32_t tags = 0;
            for (const auto& w : words)
                for (const auto& wt : kWords)
                    if (w == wt.word) tags |= wt.tag;
            return tags;
        }

        std::uint32_t Extras(const RE::SpellItem* spell, std::uint32_t tags) {
            using A = RE::EffectArchetypes::ArchetypeID;
            for (const auto* effect : spell->effects)
                if (effect && effect->baseEffect && effect->baseEffect->GetArchetype() == A::kCloak) tags |= kCloak;

            if (spell->GetDelivery() == RE::MagicSystem::Delivery::kTargetLocation &&
                (tags & (kDamage | kControl)) && !(tags & kSummon))
                tags |= kRune;

            if (const_cast<RE::SpellItem*>(spell)->GetLargestArea() > 0) tags |= kAoE;
            if (spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) tags |= kConcentration;
            return tags;
        }

        const RE::SpellItem* SpellOf(const RE::TESForm* form) {
            if (auto* spell = form->As<RE::SpellItem>()) return spell;
            if (auto* shout = form->As<RE::TESShout>()) return shout->variations[0].spell;
            return nullptr;
        }
    }

    namespace {
        std::unordered_map<RE::FormID, std::uint32_t> cache;
    }

    std::size_t CachedCount() { return cache.size(); }

    std::uint32_t Of(const RE::TESForm* form) {
        if (!form) return 0;
        if (auto it = cache.find(form->GetFormID()); it != cache.end()) return it->second;

        std::uint32_t tags = 0;
        if (const auto* spell = SpellOf(form)) tags = Extras(spell, FromIntentModel(spell));
        cache.emplace(form->GetFormID(), tags);
        return tags;
    }

    const char* LabelKey(int i) {
        static constexpr const char* kKeys[kTagCount] = {
            "PartySheetV2.SpellTagFire",    "PartySheetV2.SpellTagFrost",   "PartySheetV2.SpellTagShock",
            "PartySheetV2.SpellTagPoison",  "PartySheetV2.SpellTagDamage",  "PartySheetV2.SpellTagHeal",
            "PartySheetV2.SpellTagBuff",    "PartySheetV2.SpellTagDebuff",  "PartySheetV2.SpellTagResist",
            "PartySheetV2.SpellTagControl", "PartySheetV2.SpellTagSummon",  "PartySheetV2.SpellTagBound",
            "PartySheetV2.SpellTagStealth", "PartySheetV2.SpellTagCloak",   "PartySheetV2.SpellTagRune",
            "PartySheetV2.SpellTagAoE",     "PartySheetV2.SpellTagConcentration",
        };
        return (i >= 0 && i < kTagCount) ? kKeys[i] : "";
    }
}
