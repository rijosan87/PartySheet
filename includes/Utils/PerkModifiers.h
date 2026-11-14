#pragma once

#include <RE/Skyrim.h>

namespace PerkModifiers {

    struct HitEstimate {
        float perkMult = 1.0f;
        float physicalDamage = -1.0f;
    };

    HitEstimate GetHitEstimate(RE::Actor* actor, RE::TESObjectWEAP* weapon);
}
