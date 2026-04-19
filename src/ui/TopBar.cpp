#include "TopBar.h"
#include <WiFi.h>
#include <cstdio>
#include <cstring>

#define TB_COL_ORANGE lv_color_hex(0xff6b35)
#define TB_COL_GREEN  lv_color_hex(0x4ade80)
#define TB_COL_AMBER  lv_color_hex(0xfbbf24)
#define TB_COL_RED    lv_color_hex(0xef4444)
#define TB_COL_GREY   lv_color_hex(0x64748b)
#define TB_COL_DARK   lv_color_hex(0x111827)

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

    // Algo pill: rounded colored badge just left of the WiFi indicator.
    _algoPill = lv_obj_create(parent);
    lv_obj_set_size(_algoPill, 44, 18);
    lv_obj_align(_algoPill, LV_ALIGN_TOP_RIGHT, -70, 4);
    lv_obj_set_style_bg_color(_algoPill, TB_COL_DARK, 0);
    lv_obj_set_style_bg_opa(_algoPill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_algoPill, 1, 0);
    lv_obj_set_style_border_color(_algoPill, TB_COL_GREY, 0);
    lv_obj_set_style_radius(_algoPill, 9, 0);
    lv_obj_set_style_pad_all(_algoPill, 0, 0);
    lv_obj_remove_flag(_algoPill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_algoPill, LV_OBJ_FLAG_CLICKABLE);

    _lblAlgo = lv_label_create(_algoPill);
    lv_label_set_text(_lblAlgo, "--");
    lv_obj_set_style_text_color(_lblAlgo, TB_COL_GREY, 0);
    lv_obj_set_style_text_font(_lblAlgo, &lv_font_montserrat_14, 0);
    lv_obj_center(_lblAlgo);
}

void TopBar::update(const char* algo)
{
    // WiFi indicator
    if (_lblWifi) {
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

    // Algo pill
    if (_lblAlgo && _algoPill) {
        lv_color_t fg, border;
        const char* txt;
        if (algo && strcmp(algo, "BTC") == 0) {
            txt = "BTC"; fg = TB_COL_ORANGE; border = TB_COL_ORANGE;
        } else if (algo && strcmp(algo, "DUCO") == 0) {
            txt = "DUCO"; fg = TB_COL_GREEN; border = TB_COL_GREEN;
        } else {
            txt = "--"; fg = TB_COL_GREY; border = TB_COL_GREY;
        }
        lv_label_set_text(_lblAlgo, txt);
        lv_obj_set_style_text_color(_lblAlgo, fg, 0);
        lv_obj_set_style_border_color(_algoPill, border, 0);
    }
}
