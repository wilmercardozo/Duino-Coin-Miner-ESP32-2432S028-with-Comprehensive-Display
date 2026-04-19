#pragma once
#include "Config.h"

namespace ConfigStore {
    bool load(Config& out);   // returns false if file missing or parse error
    bool save(const Config& cfg);
    bool exists();
    void erase();
}
