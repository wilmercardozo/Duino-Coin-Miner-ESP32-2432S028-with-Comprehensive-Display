#pragma once
#include "Config.h"

namespace WiFiMgr {
    bool connect(const Config& cfg);  // returns true on success; sets needsPortal on failure
    bool isConnected();
    bool needsPortal();
    void clearPortalFlag();

    // Start a background task that monitors WiFi.status() and reconnects with
    // exponential backoff (10s → 30s → 60s → 120s cap) whenever the link drops.
    // Safe to call multiple times; only the first call spawns the task.
    // Pass the same Config reference used by connect() — the task needs the
    // SSID/pass to call WiFi.reconnect() or WiFi.begin() as a fallback.
    void startAutoReconnect(const Config& cfg);
}
