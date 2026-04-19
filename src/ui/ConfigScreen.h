#pragma once
#include "mining/IMiningAlgorithm.h"
#include <stdint.h>

// Third view — on-device configuration.  Lets the user:
//   • toggle mining algorithm (BTC ↔ DUCO) and restart
//   • reopen the setup portal (preserves saved config)
//   • erase everything and return to a blank portal
// Taps land here via UIManager::handleTouch when the current view is Config.
namespace ConfigScreen {
    void create();
    void load();
    void update(const MiningStats& stats);
    void handleTap(int16_t x, int16_t y);
}
