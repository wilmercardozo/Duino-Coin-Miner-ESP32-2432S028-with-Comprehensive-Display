#include "SystemScreen.h"
#include "TopBar.h"
#include "Config.h"
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_AMBER   lv_color_hex(0xfbbf24)
#define COL_RED     lv_color_hex(0xef4444)
#define COL_SUBTLE  lv_color_hex(0x1a2035)
#define COL_GREY    lv_color_hex(0x94a3b8)
#define COL_LIGHT   lv_color_hex(0xe2e8f0)

#define FW_VERSION  "1.0.0"

extern Config gConfig;

static lv_obj_t* s_scr = nullptr;
static TopBar    s_topBar;

// RED column
static lv_obj_t* s_lblSsid  = nullptr;
static lv_obj_t* s_lblRssi  = nullptr;
static lv_obj_t* s_lblIp    = nullptr;
static lv_obj_t* s_lblGw    = nullptr;

// POOL column
static lv_obj_t* s_lblPool  = nullptr;
static lv_obj_t* s_lblJob   = nullptr;
static lv_obj_t* s_lblDiff  = nullptr;
static lv_obj_t* s_lblPing  = nullptr;

// MINADO
static lv_obj_t* s_lblShares = nullptr;
static lv_obj_t* s_lblBest   = nullptr;
static lv_obj_t* s_lblHashes = nullptr;
static lv_obj_t* s_lblRate   = nullptr;

// SISTEMA
static lv_obj_t* s_lblHeap   = nullptr;
static lv_obj_t* s_lblUp     = nullptr;
static lv_obj_t* s_lblRig    = nullptr;

static lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COL_SUBTLE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y,
                            lv_color_t col, const char* init)
{
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, init);
    lv_obj_set_style_text_color(lbl, col, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, x, y);
    return lbl;
}

void SystemScreen::create()
{
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_topBar.create(s_scr, "Sistema");

    // ── Card 1: RED + POOL (two columns) ────────────────────────────────────
    // Pitch 18 px matches Montserrat 14's real line height (16 px was too
    // tight — descenders bled into the next row). Card taller by 10 px to
    // accommodate; cards 2 and 3 shifted down to match.
    lv_obj_t* c1 = makeCard(s_scr, 8, 24, 304, 100);

    makeLabel(c1, 0,   0, COL_ORANGE, "RED");
    makeLabel(c1, 148, 0, COL_ORANGE, "POOL");

    // Left column
    s_lblSsid = makeLabel(c1, 0, 20, COL_LIGHT, "SSID: --");
    s_lblRssi = makeLabel(c1, 0, 38, COL_GREY,  "RSSI: --");
    s_lblIp   = makeLabel(c1, 0, 56, COL_GREY,  "IP:   --");
    s_lblGw   = makeLabel(c1, 0, 74, COL_GREY,  "GW:   --");

    // Right column
    s_lblPool = makeLabel(c1, 148, 20, COL_LIGHT, "--");
    s_lblJob  = makeLabel(c1, 148, 38, COL_GREY,  "job:  --");
    s_lblDiff = makeLabel(c1, 148, 56, COL_GREY,  "diff: --");
    s_lblPing = makeLabel(c1, 148, 74, COL_GREEN, "ping: --");

    // Every right-column label that can receive variable-length data needs
    // a bounded width + LONG_DOT so Stratum job IDs / long diffs don't run
    // past the card edge and visually collide with adjacent labels.
    for (lv_obj_t* l : {s_lblSsid, s_lblPool, s_lblJob, s_lblDiff}) {
        lv_obj_set_width(l, 140);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }

    // ── Card 2: MINADO ──────────────────────────────────────────────────────
    lv_obj_t* c2 = makeCard(s_scr, 8, 130, 304, 44);
    makeLabel(c2, 0, 0, COL_ORANGE, "MINADO");
    s_lblShares = makeLabel(c2, 90,  0, COL_GREEN, "OK 0 / BAD 0");
    s_lblBest   = makeLabel(c2, 0,  22, COL_LIGHT, "best: --");
    s_lblRate   = makeLabel(c2, 110, 22, COL_ORANGE, "-- kH/s");
    s_lblHashes = makeLabel(c2, 195, 22, COL_GREY,  "h: --");

    // ── Card 3: SISTEMA ─────────────────────────────────────────────────────
    lv_obj_t* c3 = makeCard(s_scr, 8, 178, 304, 44);
    makeLabel(c3, 0, 0, COL_ORANGE, "SISTEMA");
    s_lblHeap = makeLabel(c3, 90,  0, COL_LIGHT, "heap: --");
    s_lblUp   = makeLabel(c3, 0,  22, COL_GREY,  "up: --");
    s_lblRig  = makeLabel(c3, 120, 22, COL_GREY, "rig: --");
    makeLabel(c3, 230, 22, COL_GREEN, "FW " FW_VERSION);

    // ── Nav dots — 4 positions, view 3 (rightmost) active ──────────────────
    static const int DOT_X[4] = {-24, -8, 8, 24};
    for (int i = 0; i < 4; i++) {
        bool active = (i == 3);
        lv_obj_t* d = lv_obj_create(s_scr);
        lv_obj_set_size(d, active ? 14 : 5, 5);
        lv_obj_set_style_bg_color(d, active ? COL_ORANGE : lv_color_hex(0x333333), 0);
        lv_obj_set_style_radius(d, 3, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_align(d, LV_ALIGN_BOTTOM_MID, DOT_X[i], -4);
    }
}

void SystemScreen::load()
{
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

static void formatUptime(uint32_t secs, char* out, size_t outSz)
{
    uint32_t d = secs / 86400U;
    uint32_t h = (secs % 86400U) / 3600U;
    uint32_t m = (secs % 3600U) / 60U;
    if (d > 0)      snprintf(out, outSz, "%lud %luh", (unsigned long)d, (unsigned long)h);
    else if (h > 0) snprintf(out, outSz, "%luh %lum", (unsigned long)h, (unsigned long)m);
    else            snprintf(out, outSz, "%lum",      (unsigned long)m);
}

static void formatHashes(uint64_t h, char* out, size_t outSz)
{
    if      (h >= 1000000000ULL) snprintf(out, outSz, "%.1fG", (double)h / 1e9);
    else if (h >= 1000000ULL)    snprintf(out, outSz, "%.1fM", (double)h / 1e6);
    else if (h >= 1000ULL)       snprintf(out, outSz, "%.1fK", (double)h / 1e3);
    else                          snprintf(out, outSz, "%llu", (unsigned long long)h);
}

void SystemScreen::update(const MiningStats& stats)
{
    if (!s_scr) return;
    char buf[80];

    // ── RED ──────────────────────────────────────────────────────────────────
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "SSID: %s", WiFi.SSID().c_str());
        lv_label_set_text(s_lblSsid, buf);

        int rssi = WiFi.RSSI();
        lv_color_t rcol;
        if      (rssi >= -60) rcol = COL_GREEN;
        else if (rssi >= -75) rcol = COL_AMBER;
        else                   rcol = COL_RED;
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", rssi);
        lv_label_set_text(s_lblRssi, buf);
        lv_obj_set_style_text_color(s_lblRssi, rcol, 0);

        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        lv_label_set_text(s_lblIp, buf);

        snprintf(buf, sizeof(buf), "GW: %s", WiFi.gatewayIP().toString().c_str());
        lv_label_set_text(s_lblGw, buf);
    } else {
        lv_label_set_text(s_lblSsid, "SSID: (sin red)");
        lv_label_set_text(s_lblRssi, "RSSI: --");
        lv_obj_set_style_text_color(s_lblRssi, COL_GREY, 0);
        lv_label_set_text(s_lblIp, "IP: --");
        lv_label_set_text(s_lblGw, "GW: --");
    }

    // ── POOL ─────────────────────────────────────────────────────────────────
    lv_label_set_text(s_lblPool, stats.poolUrl[0] ? stats.poolUrl : "--");

    snprintf(buf, sizeof(buf), "job:  %s", stats.jobId[0] ? stats.jobId : "--");
    lv_label_set_text(s_lblJob, buf);

    if (stats.currentDifficulty > 0) {
        snprintf(buf, sizeof(buf), "diff: %lu", (unsigned long)stats.currentDifficulty);
    } else {
        snprintf(buf, sizeof(buf), "diff: --");
    }
    lv_label_set_text(s_lblDiff, buf);

    if (stats.pingMs > 0) {
        snprintf(buf, sizeof(buf), "ping: %lu ms", (unsigned long)stats.pingMs);
        lv_obj_set_style_text_color(s_lblPing,
            stats.pingMs < 200 ? COL_GREEN : (stats.pingMs < 500 ? COL_AMBER : COL_RED), 0);
    } else {
        snprintf(buf, sizeof(buf), "ping: --");
        lv_obj_set_style_text_color(s_lblPing, COL_GREY, 0);
    }
    lv_label_set_text(s_lblPing, buf);

    // ── MINADO ───────────────────────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "OK %lu / BAD %lu",
             (unsigned long)stats.sharesAccepted,
             (unsigned long)stats.sharesRejected);
    lv_label_set_text(s_lblShares, buf);
    lv_obj_set_style_text_color(s_lblShares,
        stats.sharesRejected > stats.sharesAccepted / 4 ? COL_AMBER : COL_GREEN, 0);

    if (stats.bestDifficulty > 0.0f) {
        if      (stats.bestDifficulty >= 1e6f) snprintf(buf, sizeof(buf), "best: %.1fM", (double)stats.bestDifficulty / 1e6);
        else if (stats.bestDifficulty >= 1e3f) snprintf(buf, sizeof(buf), "best: %.1fK", (double)stats.bestDifficulty / 1e3);
        else                                    snprintf(buf, sizeof(buf), "best: %.1f",  (double)stats.bestDifficulty);
    } else {
        snprintf(buf, sizeof(buf), "best: --");
    }
    lv_label_set_text(s_lblBest, buf);

    snprintf(buf, sizeof(buf), "%.0f kH/s", (double)stats.hashrate);
    lv_label_set_text(s_lblRate, buf);

    char hb[16];
    formatHashes(stats.totalHashes, hb, sizeof(hb));
    snprintf(buf, sizeof(buf), "h: %s", hb);
    lv_label_set_text(s_lblHashes, buf);

    // ── SISTEMA ──────────────────────────────────────────────────────────────
    uint32_t heapKb = (uint32_t)(ESP.getFreeHeap() / 1024);
    snprintf(buf, sizeof(buf), "heap: %lu KB", (unsigned long)heapKb);
    lv_label_set_text(s_lblHeap, buf);
    lv_obj_set_style_text_color(s_lblHeap,
        heapKb > 80 ? COL_LIGHT : (heapKb > 40 ? COL_AMBER : COL_RED), 0);

    char ub[24];
    formatUptime(stats.uptimeSeconds, ub, sizeof(ub));
    snprintf(buf, sizeof(buf), "up: %s", ub);
    lv_label_set_text(s_lblUp, buf);

    snprintf(buf, sizeof(buf), "rig: %s", gConfig.rig_name[0] ? gConfig.rig_name : "--");
    lv_label_set_text(s_lblRig, buf);

    s_topBar.update(stats.algorithm);
}
