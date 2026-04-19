#include "TopBar.h"
#include <WiFi.h>
#include <cstdio>

#define TB_COL_ORANGE lv_color_hex(0xff6b35)
#define TB_COL_GREEN  lv_color_hex(0x4ade80)
#define TB_COL_AMBER  lv_color_hex(0xfbbf24)
#define TB_COL_RED    lv_color_hex(0xef4444)
#define TB_COL_GREY   lv_color_hex(0x64748b)

void TopBar::create(lv_obj_t* parent, const char* title)
{
    _lblTitle = lv_label_create(parent);
    lv_label_set_text(_lblTitle, title);
    lv_obj_set_style_text_color(_lblTitle, TB_COL_ORANGE, 0);
    lv_obj_set_style_text_font(_lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(_lblTitle, LV_ALIGN_TOP_LEFT, 8, 4);

    _lblWifi = lv_label_create(parent);
    lv_label_set_text(_lblWifi, LV_SYMBOL_WIFI " --");
    lv_obj_set_style_text_color(_lblWifi, TB_COL_GREY, 0);
    lv_obj_set_style_text_font(_lblWifi, &lv_font_montserrat_14, 0);
    lv_obj_align(_lblWifi, LV_ALIGN_TOP_RIGHT, -8, 4);
}

void TopBar::update()
{
    if (!_lblWifi) return;
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        lv_color_t col;
        if      (rssi >= -60) col = TB_COL_GREEN;
        else if (rssi >= -75) col = TB_COL_AMBER;
        else                  col = TB_COL_RED;
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %d", rssi);
        lv_obj_set_style_text_color(_lblWifi, col, 0);
        lv_label_set_text(_lblWifi, buf);
    } else {
        lv_obj_set_style_text_color(_lblWifi, TB_COL_RED, 0);
        lv_label_set_text(_lblWifi, LV_SYMBOL_WIFI " --");
    }
}
