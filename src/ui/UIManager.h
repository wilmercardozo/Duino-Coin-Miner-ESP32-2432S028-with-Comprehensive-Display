#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <lvgl.h>
#include "mining/IMiningAlgorithm.h"

struct MiningSnapshot {
    MiningStats stats;
    volatile bool dirty;
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
};
extern MiningSnapshot gMiningSnapshot;

namespace UIManager {
    void init();                    // init display + build screens (no task)
    void tick();                    // called from taskUI in main.cpp
    void update(const MiningStats& stats);
    void setTargetFps(uint8_t fps);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Boot sequence helpers:
    // - pumpLvgl() runs lv_timer_handler N times with small delays, used
    //   during setup() to render splash transitions from core 1 before the
    //   UI task exists.
    // - showRestarting() paints "Reiniciando..." over the splash and calls
    //   ESP.restart() after a short delay.  Safe to call from any task.
    void pumpLvgl(uint32_t ms);
    void showRestarting(const char* msg);
}
