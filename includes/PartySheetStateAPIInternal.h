#pragma once

// STATUS: Working

#include "PartySheetStateAPI.h"

namespace PartySheetAPI
{
    void DispatchInterface();
    void PublishIfChanged();
    void SetPendingReason(Reason a_reason);
}
