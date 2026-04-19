#include "ClockScreen.h"
#include <lvgl.h>
#include <Arduino.h>
#include <time.h>
#include <cstdio>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_SUBTLE  lv_color_hex(0x1a2035)

static lv_obj_t* s_scr        = nullptr;
static lv_obj_t* s_lblTime    = nullptr;
static lv_obj_t* s_lblDate    = nullptr;
static lv_obj_t* s_lblHash    = nullptr;
static lv_obj_t* s_lblPool    = nullptr;
static lv_obj_t* s_lblPing    = nullptr;
static lv_obj_t* s_lblShares  = nullptr;

static lv_obj_t* makeCell(lv_obj_t* parent, int32_t w,
                           const char* initVal, lv_color_t valColor,
                           const char* caption)
{
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_size(cell, w, 50);
    lv_obj_set_style_pad_all(cell, 2, 0);

    lv_obj_t* val = lv_label_create(cell);
    lv_label_set_text(val, initVal);
    lv_obj_set_style_text_color(val, valColor, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* cap = lv_label_create(cell);
    lv_label_set_text(cap, caption);
    lv_obj_set_style_text_color(cap, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_align(cap, LV_ALIGN_CENTER, 0, 10);

    return val;
}

void ClockScreen::create()
{
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);

    // ── Topbar ────────────────────────────────────────────────────────────────
    lv_obj_t* top = lv_label_create(s_scr);
    lv_label_set_text(top, LV_SYMBOL_WIFI "  NerdDuino Pro");
    lv_obj_set_style_text_color(top, COL_ORANGE, 0);
    lv_obj_set_style_text_font(top, &lv_font_montserrat_14, 0);
    lv_obj_align(top, LV_ALIGN_TOP_LEFT, 8, 6);

    // ── Large clock (Montserrat 36, white) ────────────────────────────────────
    s_lblTime = lv_label_create(s_scr);
    lv_label_set_text(s_lblTime, "00:00");
    lv_obj_set_style_text_color(s_lblTime, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s_lblTime, &lv_font_montserrat_36, 0);
    lv_obj_align(s_lblTime, LV_ALIGN_CENTER, 0, -30);

    // ── Date line (Montserrat 14, gray) ───────────────────────────────────────
    s_lblDate = lv_label_create(s_scr);
    lv_label_set_text(s_lblDate, "");
    lv_obj_set_style_text_color(s_lblDate, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(s_lblDate, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblDate, LV_ALIGN_CENTER, 0, 10);

    // ── Info strip container (flex row) ───────────────────────────────────────
    lv_obj_t* strip = lv_obj_create(s_scr);
    lv_obj_set_size(strip, 304, 56);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_bg_color(strip, COL_SUBTLE, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, 8, 0);
    lv_obj_set_style_pad_all(strip, 4, 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_layout(strip, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_lblHash   = makeCell(strip,  80, "0 kH/s", COL_ORANGE,             "Hashrate");
    s_lblPool   = makeCell(strip, 110, "pool...", lv_color_hex(0xe2e8f0), "Pool");
    s_lblPing   = makeCell(strip,  70, "--ms",    COL_GREEN,              "Ping");
    s_lblShares = makeCell(strip,  60, "0",       lv_color_hex(0xe2e8f0), "Shares");

    // ── Nav dots: left (Dashboard) = gray, right (this view) = orange pill ───
    lv_obj_t* dot1 = lv_obj_create(s_scr);
    lv_obj_set_size(dot1, 5, 5);
    lv_obj_set_style_bg_color(dot1, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(dot1, 3, 0);
    lv_obj_set_style_border_width(dot1, 0, 0);
    lv_obj_align(dot1, LV_ALIGN_BOTTOM_MID, -12, -2);

    lv_obj_t* dot2 = lv_obj_create(s_scr);
    lv_obj_set_size(dot2, 14, 5);
    lv_obj_set_style_bg_color(dot2, COL_ORANGE, 0);
    lv_obj_set_style_radius(dot2, 3, 0);
    lv_obj_set_style_border_width(dot2, 0, 0);
    lv_obj_align(dot2, LV_ALIGN_BOTTOM_MID, 6, -2);
}

void ClockScreen::load()
{
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

static const char* DAYS[]   = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
static const char* MONTHS[] = {"Ene","Feb","Mar","Abr","May","Jun",
                                "Jul","Ago","Sep","Oct","Nov","Dic"};

void ClockScreen::update(const MiningStats& stats)
{
    if (!s_scr) return;

    // ── Clock / date ─────────────────────────────────────────────────────────
    struct tm t;
    if (getLocalTime(&t)) {
        char timeBuf[8];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
        lv_label_set_text(s_lblTime, timeBuf);

        char dateBuf[32];
        snprintf(dateBuf, sizeof(dateBuf), "%s · %d %s %d",
                 DAYS[t.tm_wday], t.tm_mday,
                 MONTHS[t.tm_mon], 1900 + t.tm_year);
        lv_label_set_text(s_lblDate, dateBuf);
    }

    // ── Mining stats ─────────────────────────────────────────────────────────
    char buf[32];

    snprintf(buf, sizeof(buf), "%.0f kH/s", (double)stats.hashrate);
    lv_label_set_text(s_lblHash, buf);

    lv_label_set_text(s_lblPool, stats.poolUrl);

    snprintf(buf, sizeof(buf), "%u ms", (unsigned)stats.pingMs);
    lv_label_set_text(s_lblPing, buf);

    snprintf(buf, sizeof(buf), "%u", (unsigned)stats.sharesAccepted);
    lv_label_set_text(s_lblShares, buf);
}
