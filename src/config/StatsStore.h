#pragma once
#include "mining/IMiningAlgorithm.h"

// NVS-backed persistence for the counters we don't want to lose on reboot.
// Separate namespace per algorithm ("stats_DUCO" / "stats_BTC") so swapping
// coins doesn't clobber the other's history.  Call load() in the miner's
// constructor, save() from a periodic task (~every 60 s).
namespace StatsStore {
    void load(const char* algo, MiningStats& stats);
    void save(const char* algo, const MiningStats& stats);
    void erase(const char* algo);
}
