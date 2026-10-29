#pragma once

// STATUS: Working

#include "PartySheetStateAPI.h"

namespace PartySheetAPI
{
    void DispatchInterface();
    void PublishIfChanged();
    void SetPendingReason(Reason a_reason);

    // Checked in the hotkey match so a suppressed press falls through.
    bool IsHotkeySuppressedInternal(Panel a_panel);

    // Implemented in plugin.cpp, where the Inspect Card input state lives. Main thread only.
    APIResult OpenInspectCardFromAPI();
    void      CloseInspectCardFromAPI();
    void      PlayPanelSound(bool a_opening);
}
