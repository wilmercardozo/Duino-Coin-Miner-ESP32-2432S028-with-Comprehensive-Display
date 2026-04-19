#pragma once
#include <lvgl.h>

// Boot/transition splash: big orange "NerdDuino Pro" title, subtle tagline,
// a spinner that keeps moving, and a status line the caller mutates as
// progress advances ("Conectando WiFi...", "Conectando pool...", etc).
//
// Also used for restart transitions — setStatus("Reiniciando...") + a short
// delay makes the reset feel intentional instead of the screen glitching.
namespace SplashScreen {
    void create();
    void load();                         // show the splash on screen
    void setStatus(const char* text);    // update the status line
}
