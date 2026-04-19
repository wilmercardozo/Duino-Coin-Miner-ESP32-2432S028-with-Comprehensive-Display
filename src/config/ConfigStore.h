#pragma once
#include "Config.h"

namespace ConfigStore {
    bool load(Config& out);   // returns false if file missing or parse error
    bool save(const Config& cfg);
    bool exists();
    void erase();

    // "Force portal on next boot" flag — persisted in LittleFS as a sentinel
    // file.  Used by the ConfigScreen "Abrir Portal" button so the user can
    // re-enter the setup portal without losing their saved WiFi/miner config.
    bool isForcePortal();     // also clears the flag if set
    void setForcePortal();
}
