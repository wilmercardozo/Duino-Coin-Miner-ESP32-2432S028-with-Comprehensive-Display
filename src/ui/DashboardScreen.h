#pragma once
#include "mining/IMiningAlgorithm.h"

namespace DashboardScreen {
    void create();
    void load();
    void update(const MiningStats& stats);
}
