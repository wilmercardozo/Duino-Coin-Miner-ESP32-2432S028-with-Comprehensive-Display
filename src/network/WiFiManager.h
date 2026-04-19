#pragma once
#include "Config.h"

namespace WiFiMgr {
    bool connect(const Config& cfg);  // returns true on success; sets needsPortal on failure
    bool isConnected();
    bool needsPortal();
    void clearPortalFlag();
}
