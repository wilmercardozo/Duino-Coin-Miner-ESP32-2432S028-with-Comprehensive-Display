#pragma once
#include "mining/IMiningAlgorithm.h"

// Minimal HTTP server exposing /stats.json for remote monitoring.
// Starts on port 80 in normal (station) mode — do not call during portal mode
// since ConfigPortal owns the same port there.
namespace StatsServer {
    void init(IMiningAlgorithm** miner);
}
