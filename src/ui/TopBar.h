#pragma once
#include <lvgl.h>

// Thin re-usable topbar: orange title on the left, WiFi-icon + dBm on the right.
// Each screen instantiates one of these in its create() and calls update() from
// its own update() so every view gets the same live RSSI indicator.
class TopBar {
public:
    void create(lv_obj_t* parent, const char* title);
    void update();   // reads WiFi.status()/RSSI and refreshes the label

private:
    lv_obj_t* _lblTitle = nullptr;
    lv_obj_t* _lblWifi  = nullptr;
};
