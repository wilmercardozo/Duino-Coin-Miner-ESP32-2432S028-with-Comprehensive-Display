#include "ConfigScreen.h"
#include "TopBar.h"
#include "UIManager.h"
#include "Config.h"
#include "config/ConfigStore.h"
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_BLUE    lv_color_hex(0x2563eb)
#define COL_RED     lv_color_hex(0xdc2626)
#define COL_AMBER   lv_color_hex(0xfbbf24)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_SUBTLE  lv_color_hex(0x1a2035)
#define COL_GREY    lv_color_hex(0x94a3b8)

extern Config gConfig;

// Button geometry — must not overlap the UIManager edge-nav zones (x<32, x>288)
static constexpr int BTN_X      = 40;
static constexpr int BTN_W      = 240;
static constexpr int BTN_H      = 32;
static constexpr int BTN_Y[3]   = {30, 70, 110};

// Confirmation window for the destructive "wipe" button.  First tap arms;
// a second tap within this many ms actually erases.  Single tap auto-reverts.
static constexpr uint32_t WIPE_CONFIRM_MS = 5000;

static lv_obj_t* s_scr            = nullptr;
static TopBar    s_topBar;
static lv_obj_t* s_wipeBtn        = nullptr;   // red button (for color flip)
static lv_obj_t* s_lblAlgoBtn     = nullptr;
static lv_obj_t* s_lblWipeBtn     = nullptr;
static lv_obj_t* s_lblInfoSsid    = nullptr;
static lv_obj_t* s_lblInfoIp      = nullptr;
static lv_obj_t* s_lblInfoVer     = nullptr;
static lv_obj_t* s_lblInfoDuco    = nullptr;
static lv_obj_t* s_lblInfoBtc     = nullptr;

static uint32_t  s_wipeArmedAt    = 0;         // 0 = disarmed

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void maskSecret(const char* in, char* out, size_t outSz)
{
    if (!in || in[0] == '\0') { strncpy(out, "--", outSz); out[outSz-1] = 0; return; }
    size_t len = strlen(in);
    if (len <= 8) { strncpy(out, in, outSz); out[outSz-1] = 0; return; }
    snprintf(out, outSz, "%.5s...%s", in, in + len - 3);
}

static void setWipeLabel(bool armed)
{
    if (!s_lblWipeBtn || !s_wipeBtn) return;
    if (armed) {
        lv_label_set_text(s_lblWipeBtn, LV_SYMBOL_WARNING "  Confirmar borrar?");
        lv_obj_set_style_bg_color(s_wipeBtn, COL_AMBER, 0);
    } else {
        lv_label_set_text(s_lblWipeBtn, LV_SYMBOL_TRASH "  Borrar todo");
        lv_obj_set_style_bg_color(s_wipeBtn, COL_RED, 0);
    }
}

static void refreshAlgoLabel()
{
    if (!s_lblAlgoBtn) return;
    const char* nxt = (gConfig.algorithm == Algorithm::BITCOIN) ? "DUCO" : "BTC";
    char buf[40];
    snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH "  Cambiar a %s", nxt);
    lv_label_set_text(s_lblAlgoBtn, buf);
}

static lv_obj_t* makeButton(lv_obj_t* parent, int y, lv_color_t bg,
                             const char* text, lv_obj_t** labelOut,
                             lv_obj_t** btnOut = nullptr)
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
    if (btnOut)   *btnOut   = btn;
    return btn;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void ConfigScreen::create()
{
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_topBar.create(s_scr, "Configuracion");

    makeButton(s_scr, BTN_Y[0], COL_ORANGE, "", &s_lblAlgoBtn);
    refreshAlgoLabel();

    makeButton(s_scr, BTN_Y[1], COL_BLUE,
               LV_SYMBOL_WIFI "  Abrir Portal", nullptr);

    makeButton(s_scr, BTN_Y[2], COL_RED, "", &s_lblWipeBtn, &s_wipeBtn);
    setWipeLabel(false);

    // Info strip: bigger now (4 lines).  SSID + FW on top row, IP on second,
    // DUCO user + BTC address (masked) on the last two so the user sees at
    // a glance which credentials are stored.
    lv_obj_t* info = lv_obj_create(s_scr);
    lv_obj_set_size(info, 304, 66);
    lv_obj_set_pos(info, 8, 152);
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

    s_lblInfoVer = lv_label_create(info);
    lv_label_set_text(s_lblInfoVer, "FW v1.2");
    lv_obj_set_style_text_color(s_lblInfoVer, COL_GREEN, 0);
    lv_obj_set_style_text_font(s_lblInfoVer, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoVer, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_lblInfoIp = lv_label_create(info);
    lv_label_set_text(s_lblInfoIp, "IP: --");
    lv_obj_set_style_text_color(s_lblInfoIp, COL_GREY, 0);
    lv_obj_set_style_text_font(s_lblInfoIp, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoIp, LV_ALIGN_TOP_LEFT, 0, 16);

    s_lblInfoDuco = lv_label_create(info);
    lv_label_set_text(s_lblInfoDuco, "DUCO: --");
    lv_obj_set_style_text_color(s_lblInfoDuco, COL_GREY, 0);
    lv_obj_set_style_text_font(s_lblInfoDuco, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoDuco, LV_ALIGN_TOP_LEFT, 0, 32);

    s_lblInfoBtc = lv_label_create(info);
    lv_label_set_text(s_lblInfoBtc, "BTC: --");
    lv_obj_set_style_text_color(s_lblInfoBtc, COL_GREY, 0);
    lv_obj_set_style_text_font(s_lblInfoBtc, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblInfoBtc, LV_ALIGN_TOP_LEFT, 0, 48);

    // Nav dots — 4 positions; Config (view 2) is the third pill.
    static constexpr int DOT_Y = -4;
    static const int DOT_X[4] = {-24, -8, 8, 24};
    for (int i = 0; i < 4; i++) {
        bool active = (i == 2);
        lv_obj_t* d = lv_obj_create(s_scr);
        lv_obj_set_size(d, active ? 14 : 5, 5);
        lv_obj_set_style_bg_color(d, active ? COL_ORANGE : lv_color_hex(0x333333), 0);
        lv_obj_set_style_radius(d, 3, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_align(d, LV_ALIGN_BOTTOM_MID, DOT_X[i], DOT_Y);
    }
}

void ConfigScreen::load()
{
    // Re-enter the screen clears any pending wipe confirmation
    s_wipeArmedAt = 0;
    setWipeLabel(false);
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void ConfigScreen::update(const MiningStats& stats)
{
    (void)stats;
    if (!s_scr) return;

    refreshAlgoLabel();

    // Auto-disarm the wipe confirmation after the timeout
    if (s_wipeArmedAt != 0 && millis() > s_wipeArmedAt + WIPE_CONFIRM_MS) {
        s_wipeArmedAt = 0;
        setWipeLabel(false);
    }

    if (s_lblInfoSsid) {
        char buf[80];
        snprintf(buf, sizeof(buf), "SSID: %s",
                 gConfig.wifi_ssid[0] ? gConfig.wifi_ssid : "--");
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
    if (s_lblInfoDuco) {
        char m[24], buf[40];
        maskSecret(gConfig.duco_user, m, sizeof(m));
        snprintf(buf, sizeof(buf), "DUCO: %s", m);
        lv_label_set_text(s_lblInfoDuco, buf);
    }
    if (s_lblInfoBtc) {
        char m[24], buf[40];
        maskSecret(gConfig.btc_address, m, sizeof(m));
        snprintf(buf, sizeof(buf), "BTC:  %s", m);
        lv_label_set_text(s_lblInfoBtc, buf);
    }

    const char* algo = (gConfig.algorithm == Algorithm::BITCOIN) ? "BTC" : "DUCO";
    s_topBar.update(algo);
}

void ConfigScreen::handleTap(int16_t x, int16_t y)
{
    if (x < BTN_X || x > BTN_X + BTN_W) return;

    for (int i = 0; i < 3; i++) {
        if (y >= BTN_Y[i] && y < BTN_Y[i] + BTN_H) {
            switch (i) {
                case 0: {   // toggle algorithm
                    gConfig.algorithm = (gConfig.algorithm == Algorithm::BITCOIN)
                                        ? Algorithm::DUINOCOIN
                                        : Algorithm::BITCOIN;
                    Serial.printf("[config] algorithm -> %s\n",
                                  gConfig.algorithm == Algorithm::BITCOIN ? "BTC" : "DUCO");
                    ConfigStore::save(gConfig);
                    UIManager::showRestarting(gConfig.algorithm == Algorithm::BITCOIN
                                              ? "Cambiando a BTC..."
                                              : "Cambiando a DUCO...");
                    break;
                }
                case 1: {   // open portal, keeping config
                    Serial.println("[config] force-portal flag set");
                    ConfigStore::setForcePortal();
                    UIManager::showRestarting("Abriendo portal...");
                    break;
                }
                case 2: {   // wipe — two-tap with timeout
                    uint32_t now = millis();
                    if (s_wipeArmedAt == 0 || now > s_wipeArmedAt + WIPE_CONFIRM_MS) {
                        s_wipeArmedAt = now;
                        setWipeLabel(true);
                        Serial.println("[config] wipe armed — tap again to confirm");
                    } else {
                        Serial.println("[config] wipe confirmed — erasing");
                        ConfigStore::erase();
                        UIManager::showRestarting("Borrando configuracion...");
                    }
                    break;
                }
            }
            return;
        }
    }
}
