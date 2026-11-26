#pragma once

#include <optional>
#include <string>

namespace SurvivalSupplies {

    struct Totals {
        int food = 0, drinks = 0, firewood = 0;
        int dirtyWater = 0;
        int tents = 0;
        int waterskinSips = 0;
        int torches = 0;
    };

    Totals Count();
}
