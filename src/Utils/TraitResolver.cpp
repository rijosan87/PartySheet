// STATUS: Working
// Resolves player and target traits.

#include "../../includes/Utils/TraitResolver.h"
#include "../../includes/LocalizationManager.h"

#include <algorithm>

namespace TraitResolver {
struct TierEntry {
    float minValue;
    const char* key;
};

static constexpr TierEntry kTiers[] = {
    { 100.0f, "Trait.Tier.Master"     },
    {  75.0f, "Trait.Tier.Expert"     },
    {  50.0f, "Trait.Tier.Adept"      },
    {  25.0f, "Trait.Tier.Apprentice" },
};

static const char* GetTierKey(float value) {
    for (const auto& t : kTiers) {
        if (value >= t.minValue) return t.key;
    }
    return nullptr;
}

struct SkillTraitEntry {
    float ActorSkills::* field;
};

struct SkillDef {
    float (*getValue)(const ActorSkills&);
    const char* traitKey;
    const char* skillKey;
};

static const SkillDef kSkillDefs[] = {
    { [](const ActorSkills& s) { return s.oneHanded.value;  }, "Trait.OneHanded",   "Skills.OneHanded"   },
    { [](const ActorSkills& s) { return s.twoHanded.value;  }, "Trait.TwoHanded",   "Skills.TwoHanded"   },
    { [](const ActorSkills& s) { return s.block.value;      }, "Trait.Block",       "Skills.Block"       },
    { [](const ActorSkills& s) { return s.lightArmor.value; }, "Trait.LightArmor",  "Skills.LightArmor"  },
    { [](const ActorSkills& s) { return s.heavyArmor.value; }, "Trait.HeavyArmor",  "Skills.HeavyArmor"  },
    { [](const ActorSkills& s) { return s.smithing.value;   }, "Trait.Smithing",    "Skills.Smithing"    },
    { [](const ActorSkills& s) { return s.destruction.value; }, "Trait.Destruction", "Skills.Destruction" },
    { [](const ActorSkills& s) { return s.restoration.value; }, "Trait.Restoration", "Skills.Restoration" },
    { [](const ActorSkills& s) { return s.conjuration.value; }, "Trait.Conjuration", "Skills.Conjuration" },
    { [](const ActorSkills& s) { return s.alteration.value;  }, "Trait.Alteration",  "Skills.Alteration"  },
    { [](const ActorSkills& s) { return s.illusion.value;    }, "Trait.Illusion",    "Skills.Illusion"    },
    { [](const ActorSkills& s) { return s.enchanting.value;  }, "Trait.Enchanting",  "Skills.Enchanting"  },
    { [](const ActorSkills& s) { return s.sneak.value;       }, "Trait.Sneak",       "Skills.Sneak"       },
    { [](const ActorSkills& s) { return s.lockpicking.value; }, "Trait.Lockpicking", "Skills.Lockpicking" },
    { [](const ActorSkills& s) { return s.pickpocket.value;  }, "Trait.Pickpocket",  "Skills.Pickpocket"  },
    { [](const ActorSkills& s) { return s.archery.value;     }, "Trait.Archery",     "Skills.Archery"     },
    { [](const ActorSkills& s) { return s.alchemy.value;     }, "Trait.Alchemy",     "Skills.Alchemy"     },
    { [](const ActorSkills& s) { return s.speech.value;      }, "Trait.Speech",      "Skills.Speech"      },
};

static constexpr int kSkillCount = static_cast<int>(std::size(kSkillDefs));

std::vector<TraitEntry> Resolve(const ActorSkills& skills, int maxTraits) {
    struct Candidate { float value; int idx; };
    std::vector<Candidate> candidates;
    candidates.reserve(kSkillCount);
    for (int i = 0; i < kSkillCount; ++i) {
        float v = kSkillDefs[i].getValue(skills);
        if (v >= kMinSkillThreshold) {
            candidates.push_back({ v, i });
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.value > b.value; });
    std::vector<TraitEntry> result;
    result.reserve(std::min(maxTraits, (int)candidates.size()));
    for (int i = 0; i < maxTraits && i < (int)candidates.size(); ++i) {
        const auto& c = candidates[i];
        const char* tierKey = GetTierKey(c.value);
        if (!tierKey) continue;
        const char* labelKey = (c.value >= kFlavorThreshold)
            ? kSkillDefs[c.idx].traitKey
            : kSkillDefs[c.idx].skillKey;
        result.push_back({ tierKey, kSkillDefs[c.idx].traitKey, labelKey });
    }
    return result;
}

std::vector<std::string> ResolveLocalized(const ActorSkills& skills, int maxTraits) {
    auto entries = Resolve(skills, maxTraits);
    std::vector<std::string> result;
    result.reserve(entries.size());
    for (const auto& e : entries) {
        std::string tier = LocalizationManager::Get(e.tierKey);
        std::string name = LocalizationManager::Get(e.labelKey);
        result.push_back(tier + " " + name);
    }
    return result;
}
}
