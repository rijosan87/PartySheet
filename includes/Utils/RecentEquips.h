#pragma once

#include <RE/Skyrim.h>

#include <vector>

namespace RecentEquips {

    constexpr std::size_t kMaxRecent = 5;

    struct Entry {
        RE::FormID id = 0;
        bool       held = false;
    };

    void Observe(RE::FormID actor, bool leftHand, RE::FormID heldNow);

    std::vector<Entry> Get(RE::FormID actor, bool leftHand);

    void Clear();

}
