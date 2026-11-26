#pragma once

#include <imgui.h>

namespace PartySheetV2::Condition {

    const char* Title();

    void Draw(ImDrawList* dl, float bodyW, float sc);
}
