#include "ConfigScreen.h"
#include "TopBar.h"
#include "Config.h"
#include "config/ConfigStore.h"
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_BLUE    lv_color_hex(0x2563eb)
#define COL_RED     lv_color_hex(0xdc2626)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_SUBTLE  lv_color_hex(0x1a2035)
#define COL_GREY    lv_color_hex(0x94a3b8)

extern Config gConfig;
extern volatile bool gPortalRequested;

// Button geometry — must not overlap the UIManager edge-nav zones (x<32, x>288)
static constexpr int BTN_X      = 40;
static constexpr int BTN_W      = 240;
static constexpr int BTN_H      = 36;
static constexpr int BTN_Y[3]   = {40, 86, 132};

static lv_obj_t* s_scr            = nullptr;
static TopBar    s_topBar;
static lv_obj_t* s_lblAlgoBtn     = nullptr;   // dynamic: "Algoritmo: BTC"
static lv_obj_t* s_lblInfoSsid    = nullptr;
static lv_obj_t* s_lblInfoIp      = nullptr;
static lv_obj_t* s_lblInfoVer     = nullptr;

static lv_obj_t* makeButton(lv_obj_t* parent, int y, lv_color_t bg,
                             const char* text, lv_obj_t** labelOut)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_pos(btn, BTN_X, y);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);
    if (labelOut) *labelOut = lbl;
    return btn;
}

static void refreshAlgoLabel()
{
    if (!s_lblAlgoBtn) return;
    const char* cur = (gConfig.algorithm == Algorithm::BITCOIN) ? "BTC" : "DUCO";
    const char* nxt = (gConfig.algorithm == Algorithm::BITCOIN) ? "DUCO" : "BTC";
    char buf[40];
    snprintf(buf, sizeof(buf), "Algoritmo: %s  ->  %s", cur, nxt);
    lv_label_set_text(s_lblAlgoBtn, buf);
}

void ConfigScreen::create()
{
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_topBar.create(s_scr, "Configuracion");

    makeButton(s_scr, BTN_Y[0], COL_ORANGE, "",                        &s_lblAlgoBtn);
    refreshAlgoLabel();
    makeButton(s_scr, BTN_Y[1], COL_BLUE,   "Abrir Portal",            nullptr);
    makeButton(s_scr, BTN_Y[2], COL_RED,    "Borrar configuracion",    nullptr);

    // Info strip below the buttons
    lv_obj_t* info = lv_obj_create(s_scr);
    lv_obj_set_size(info, 304, 42);
    lv_obj_set_pos(info, 8, 176);
    lv_obj_set_style_bg_color(info, COL_SUBTLE, 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_radius(info, 4, 0);
    lv_obj_set_style_pad_all(info, 4, 0);
    lv_obj_remove_flag(info, LV_OBJ_FLAG_SCROLLABLE);

    s_lblInfoSsid = lv_label_create(info);
    lv_label_set_text(s_lblInfoSsid, "SSID: --");
    lv_obj_set_style_text_color(s_lblInfoSsid, COL_GREY, 0);
    lv_obj_set_style_text_font(s_lblInfoSsid, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoSsid, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lblInfoIp = lv_label_create(info);
    lv_label_set_text(s_lblInfoIp, "IP: --");
    lv_obj_set_style_text_color(s_lblInfoIp, COL_GREY, 0);
    lv_obj_set_style_text_font(s_lblInfoIp, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoIp, LV_ALIGN_TOP_LEFT, 0, 16);

    s_lblInfoVer = lv_label_create(info);
    lv_label_set_text(s_lblInfoVer, "FW: v1.1");
    lv_obj_set_style_text_color(s_lblInfoVer, COL_GREEN, 0);
    lv_obj_set_style_text_font(s_lblInfoVer, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoVer, LV_ALIGN_TOP_RIGHT, 0, 16);

    // Nav dots — 3 positions, this view is the third (rightmost pill)
    static constexpr int DOT_Y = -2;
    lv_obj_t* d0 = lv_obj_create(s_scr);
    lv_obj_set_size(d0, 5, 5);
    lv_obj_set_style_bg_color(d0, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(d0, 3, 0);
    lv_obj_set_style_border_width(d0, 0, 0);
    lv_obj_align(d0, LV_ALIGN_BOTTOM_MID, -16, DOT_Y);

    lv_obj_t* d1 = lv_obj_create(s_scr);
    lv_obj_set_size(d1, 5, 5);
    lv_obj_set_style_bg_color(d1, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(d1, 3, 0);
    lv_obj_set_style_border_width(d1, 0, 0);
    lv_obj_align(d1, LV_ALIGN_BOTTOM_MID, 0, DOT_Y);

    lv_obj_t* d2 = lv_obj_create(s_scr);
    lv_obj_set_size(d2, 14, 5);
    lv_obj_set_style_bg_color(d2, COL_ORANGE, 0);
    lv_obj_set_style_radius(d2, 3, 0);
    lv_obj_set_style_border_width(d2, 0, 0);
    lv_obj_align(d2, LV_ALIGN_BOTTOM_MID, 16, DOT_Y);
}

void ConfigScreen::load()
{
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void ConfigScreen::update(const MiningStats& stats)
{
    (void)stats;
    if (!s_scr) return;

    refreshAlgoLabel();   // in case gConfig mutated elsewhere

    if (s_lblInfoSsid) {
        char buf[80];
        snprintf(buf, sizeof(buf), "SSID: %s", gConfig.wifi_ssid);
        lv_label_set_text(s_lblInfoSsid, buf);
    }
    if (s_lblInfoIp) {
        char buf[40];
        if (WiFi.status() == WL_CONNECTED) {
            snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        } else {
            snprintf(buf, sizeof(buf), "IP: --");
        }
        lv_label_set_text(s_lblInfoIp, buf);
    }

    s_topBar.update();
}

void ConfigScreen::handleTap(int16_t x, int16_t y)
{
    if (x < BTN_X || x > BTN_X + BTN_W) return;

    for (int i = 0; i < 3; i++) {
        if (y >= BTN_Y[i] && y < BTN_Y[i] + BTN_H) {
            switch (i) {
                case 0: {   // toggle algorithm, save, restart
                    gConfig.algorithm = (gConfig.algorithm == Algorithm::BITCOIN)
                                        ? Algorithm::DUINOCOIN
                                        : Algorithm::BITCOIN;
                    Serial.printf("[config] algorithm -> %s, saving and restarting\n",
                                  gConfig.algorithm == Algorithm::BITCOIN ? "BTC" : "DUCO");
                    ConfigStore::save(gConfig);
                    delay(200);
                    ESP.restart();
                    break;
                }
                case 1: {   // open portal, preserving current config
                    Serial.println("[config] set force-portal flag, restarting");
                    ConfigStore::setForcePortal();
                    delay(200);
                    ESP.restart();
                    break;
                }
                case 2: {   // wipe everything
                    Serial.println("[config] erase + restart requested");
                    ConfigStore::erase();
                    delay(200);
                    ESP.restart();
                    break;
                }
            }
            return;
        }
    }
}
