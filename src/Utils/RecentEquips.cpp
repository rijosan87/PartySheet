#include "../../includes/Utils/RecentEquips.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace RecentEquips {

    namespace {
        struct Slot {
            RE::FormID    id = 0;
            std::uint64_t lastUsed = 0;
        };

        struct Hand {
            RE::FormID        held = 0;
            std::vector<Slot> slots;
        };

        std::mutex                                          s_lock;
        std::unordered_map<RE::FormID, std::array<Hand, 2>> s_hands;
        std::uint64_t                                       s_clock = 0;
    }

    void Observe(RE::FormID actor, bool leftHand, RE::FormID heldNow) {
        std::lock_guard lock(s_lock);
        Hand& hand = s_hands[actor][leftHand ? 1 : 0];
        if (hand.held == heldNow && (heldNow == 0 || !hand.slots.empty())) return;
        hand.held = heldNow;
        if (heldNow == 0) return;

        auto it = std::find_if(hand.slots.begin(), hand.slots.end(), [&](const Slot& s) { return s.id == heldNow; });
        if (it != hand.slots.end()) {
            it->lastUsed = ++s_clock;
            return;
        }
        if (hand.slots.size() < kMaxRecent) {
            hand.slots.push_back({ heldNow, ++s_clock });
            return;
        }
        auto oldest = std::min_element(hand.slots.begin(), hand.slots.end(),
            [](const Slot& a, const Slot& b) { return a.lastUsed < b.lastUsed; });
        *oldest = { heldNow, ++s_clock };
    }

    std::vector<Entry> Get(RE::FormID actor, bool leftHand) {
        std::lock_guard lock(s_lock);
        std::vector<Entry> out;
        auto it = s_hands.find(actor);
        if (it == s_hands.end()) return out;
        const Hand& hand = it->second[leftHand ? 1 : 0];
        if (hand.slots.size() < 2) return out;
        for (const Slot& s : hand.slots)
            out.push_back({ s.id, s.id == hand.held });
        return out;
    }

    void Clear() {
        std::lock_guard lock(s_lock);
        s_hands.clear();
    }

}
