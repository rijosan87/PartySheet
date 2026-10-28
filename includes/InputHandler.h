#pragma once

// STATUS: Working
// Dispatches keyboard and hotkey inputs.


#include <string>

namespace ImGuiRenderer {
    const char* DIKToKeyName(int dikCode);
    bool IsValidKeyCode(int dikCode);
    int GetNextValidKey(int current);
    int GetPrevValidKey(int current);
    std::string FormatHotkeyWithModifier(int dikCode, int modifierKey);
    uint32_t GetHeldModifier();
    uint32_t GetHeldModifierSided();
    bool IsModifierScancode(uint32_t scancode);
}
