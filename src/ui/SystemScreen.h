#pragma once
#include "mining/IMiningAlgorithm.h"

// Fourth view — diagnostics / system info.  Read-only, shows network,
// pool, mining counters and system stats in one dense panel so the user
// can verify connectivity and health without USB logs.
namespace SystemScreen {
    void create();
    void load();
    void update(const MiningStats& stats);
}
