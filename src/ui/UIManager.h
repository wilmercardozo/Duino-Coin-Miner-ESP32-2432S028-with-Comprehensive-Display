#pragma once
#include <lvgl.h>
#include "mining/IMiningAlgorithm.h"

struct MiningSnapshot {
    MiningStats stats;
    volatile bool dirty;
};
extern MiningSnapshot gMiningSnapshot;

namespace UIManager {
    void init();
    void tick();
    void update(const MiningStats& stats);
    void setTargetFps(uint8_t fps);
    void handleTouch(int16_t x, int16_t y, bool pressed);
}
