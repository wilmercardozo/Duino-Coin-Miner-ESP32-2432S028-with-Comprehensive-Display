#pragma once
#include "mining/IMiningAlgorithm.h"

namespace ClockScreen {
    void create();
    void load();
    void update(const MiningStats& stats);
}
