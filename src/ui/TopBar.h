#pragma once
#include <lvgl.h>

// Thin re-usable topbar. Layout (left → right):
//   [orange title]  ...  [algo pill]  [WiFi icon + dBm]
// Each screen instantiates one of these in its create() and calls update()
// (passing the current algorithm name) from its own update() so every view
// gets the same live RSSI indicator and active-coin badge.
class TopBar {
public:
    void create(lv_obj_t* parent, const char* title);
    // Pass "BTC", "DUCO", or nullptr/"" if the algo is not yet known.
    void update(const char* algo = nullptr);

private:
    lv_obj_t* _lblTitle = nullptr;
    lv_obj_t* _lblWifi  = nullptr;
    lv_obj_t* _algoPill = nullptr;
    lv_obj_t* _lblAlgo  = nullptr;
};
