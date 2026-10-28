#pragma once

// STATUS: Working
// Installs game input/keyboard hooks.


#include <functional>

namespace InputHooks {
    void InstallInputHooks();

    void SetBlockInput(bool a_block);
}
