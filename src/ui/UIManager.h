#pragma once
#include <lvgl.h>
#include "mining/IMiningAlgorithm.h"

namespace UIManager {
    void init();
    void tick();
    void update(const MiningStats& stats);
    void setTargetFps(uint8_t fps);
    void handleTouch(int16_t x, int16_t y, bool pressed);
}
