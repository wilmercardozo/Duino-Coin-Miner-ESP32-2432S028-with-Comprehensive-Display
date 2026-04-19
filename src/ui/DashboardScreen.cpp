#include "DashboardScreen.h"
#include "TopBar.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_AMBER   lv_color_hex(0xfbbf24)
#define COL_BLUE    lv_color_hex(0x60a5fa)
#define COL_SUBTLE  lv_color_hex(0x1a2035)

static lv_obj_t* s_scr              = nullptr;
static TopBar    s_topBar;
static lv_obj_t* s_gaugeArc         = nullptr;
static lv_obj_t* s_lblHashrate      = nullptr;
static lv_obj_t* s_lblShares        = nullptr;
static lv_obj_t* s_lblReject        = nullptr;
static lv_obj_t* s_lblBalance       = nullptr;   // value label
static lv_obj_t* s_lblBalanceTitle  = nullptr;   // title label (swaps BTC↔DUCO)
static lv_obj_t* s_lblUptime        = nullptr;
static lv_obj_t* s_lblBtcInfo       = nullptr;   // "Job xxx  D=N  Mh" line
static lv_obj_t* s_chart            = nullptr;
static lv_chart_series_t* s_series  = nullptr;
static float     s_maxHashrate      = 1.0f;

// Static helper: create a 84x48 stat card. Returns the value label; if
// titleOut is non-null, the title label is also written there so callers
// can mutate it (e.g. swap "Balance" → "Best Diff" based on algorithm).
static lv_obj_t* makeCard(lv_obj_t* parent, lv_align_t align, int32_t x, int32_t y,
                           const char* title, lv_color_t valColor,
                           lv_obj_t** titleOut = nullptr)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 84, 48);
    lv_obj_align(card, align, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1a2035), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_t* lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    if (titleOut) *titleOut = lbl;

    lv_obj_t* val = lv_label_create(card);
    lv_label_set_text(val, "0");
    lv_obj_set_style_text_color(val, valColor, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_18, 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return val;
}

void DashboardScreen::create() {
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);

    s_topBar.create(s_scr, "NerdDuino Pro");

    // Gauge arc (130x130, top-left area)
    s_gaugeArc = lv_arc_create(s_scr);
    lv_obj_set_size(s_gaugeArc, 130, 130);
    lv_obj_align(s_gaugeArc, LV_ALIGN_TOP_LEFT, 4, 22);
    lv_arc_set_rotation(s_gaugeArc, 135);
    lv_arc_set_bg_angles(s_gaugeArc, 0, 270);
    lv_arc_set_value(s_gaugeArc, 0);
    lv_arc_set_range(s_gaugeArc, 0, 100);
    lv_obj_set_style_arc_color(s_gaugeArc, COL_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_gaugeArc, COL_SUBTLE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_gaugeArc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_gaugeArc, 10, LV_PART_MAIN);
    lv_obj_remove_flag(s_gaugeArc, LV_OBJ_FLAG_CLICKABLE);
    // Hide knob
    lv_obj_set_style_bg_opa(s_gaugeArc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_gaugeArc, 0, LV_PART_KNOB);

    // Hashrate label centered inside arc
    s_lblHashrate = lv_label_create(s_scr);
    lv_label_set_text(s_lblHashrate, "0\nkH/s");
    lv_obj_set_style_text_color(s_lblHashrate, COL_ORANGE, 0);
    lv_obj_set_style_text_font(s_lblHashrate, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(s_lblHashrate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(s_lblHashrate, s_gaugeArc, LV_ALIGN_CENTER, 0, 0);

    // 2x2 grid of stat cards, right side
    s_lblShares  = makeCard(s_scr, LV_ALIGN_TOP_RIGHT,  -8,   22, "Shares OK", COL_GREEN);
    s_lblReject  = makeCard(s_scr, LV_ALIGN_TOP_RIGHT,  -8,   76, "Rejected",  COL_AMBER);
    s_lblBalance = makeCard(s_scr, LV_ALIGN_TOP_RIGHT, -100,  22, "Balance",   COL_ORANGE,
                            &s_lblBalanceTitle);
    s_lblUptime  = makeCard(s_scr, LV_ALIGN_TOP_RIGHT, -100,  76, "Uptime",    COL_BLUE);

    // Compact info line between arc and chart — only populated for BTC
    // (job id, pool difficulty, cumulative hashes).  Stays empty for DUCO.
    s_lblBtcInfo = lv_label_create(s_scr);
    lv_label_set_text(s_lblBtcInfo, "");
    lv_obj_set_style_text_color(s_lblBtcInfo, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(s_lblBtcInfo, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblBtcInfo, LV_ALIGN_TOP_LEFT, 8, 156);

    // 60-second sparkline chart (full width, bottom area).  Line type + auto
    // Y-scaling keeps the curve visible whether we're at 5 kH/s or 1 MH/s.
    s_chart = lv_chart_create(s_scr);
    lv_obj_set_size(s_chart, 304, 44);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 60);
    lv_chart_set_div_line_count(s_chart, 0, 0);
    lv_obj_set_style_bg_color(s_chart, COL_BG, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);   // hide point dots
    s_series = lv_chart_add_series(s_chart, COL_ORANGE, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 10);

    // Nav dots — 3 positions; this view (Dashboard = 0) is the leftmost pill.
    lv_obj_t* d0 = lv_obj_create(s_scr);
    lv_obj_set_size(d0, 14, 5);
    lv_obj_set_style_bg_color(d0, COL_ORANGE, 0);
    lv_obj_set_style_radius(d0, 3, 0);
    lv_obj_set_style_border_width(d0, 0, 0);
    lv_obj_align(d0, LV_ALIGN_BOTTOM_MID, -18, -2);

    lv_obj_t* d1 = lv_obj_create(s_scr);
    lv_obj_set_size(d1, 5, 5);
    lv_obj_set_style_bg_color(d1, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(d1, 3, 0);
    lv_obj_set_style_border_width(d1, 0, 0);
    lv_obj_align(d1, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_t* d2 = lv_obj_create(s_scr);
    lv_obj_set_size(d2, 5, 5);
    lv_obj_set_style_bg_color(d2, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(d2, 3, 0);
    lv_obj_set_style_border_width(d2, 0, 0);
    lv_obj_align(d2, LV_ALIGN_BOTTOM_MID, 12, -2);
}

void DashboardScreen::load() {
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void DashboardScreen::update(const MiningStats& stats) {
    if (!s_scr) return;

    // Gauge: scale hashrate to 0-100% of historical max
    if (stats.hashrate > s_maxHashrate) s_maxHashrate = stats.hashrate;
    lv_arc_set_value(s_gaugeArc,
        (s_maxHashrate > 0.0f) ? (int32_t)(stats.hashrate / s_maxHashrate * 100.0f) : 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f\nkH/s", (double)stats.hashrate);
    lv_label_set_text(s_lblHashrate, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.sharesAccepted);
    lv_label_set_text(s_lblShares, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.sharesRejected);
    lv_label_set_text(s_lblReject, buf);

    // For BTC the pool balance concept doesn't exist — show best share diff
    // instead.  For DUCO keep the wallet balance.
    bool isBtc = (strcmp(stats.algorithm, "BTC") == 0);
    if (isBtc) {
        lv_label_set_text(s_lblBalanceTitle, "Best Diff");
        if      (stats.bestDifficulty >= 1e6f) snprintf(buf, sizeof(buf), "%.1fM", (double)(stats.bestDifficulty / 1e6f));
        else if (stats.bestDifficulty >= 1e3f) snprintf(buf, sizeof(buf), "%.1fK", (double)(stats.bestDifficulty / 1e3f));
        else                                    snprintf(buf, sizeof(buf), "%.1f",  (double)stats.bestDifficulty);
    } else {
        lv_label_set_text(s_lblBalanceTitle, "Balance");
        snprintf(buf, sizeof(buf), "%.2f", (double)stats.balance);
    }
    lv_label_set_text(s_lblBalance, buf);

    uint32_t d = stats.uptimeSeconds / 86400U;
    uint32_t h = (stats.uptimeSeconds % 86400U) / 3600U;
    snprintf(buf, sizeof(buf), "%ud %uh", (unsigned)d, (unsigned)h);
    lv_label_set_text(s_lblUptime, buf);

    // BTC-only info line: job + pool diff + cumulative hashes (M/G)
    if (s_lblBtcInfo) {
        if (isBtc && stats.jobId[0]) {
            char jobShort[10];
            strncpy(jobShort, stats.jobId, sizeof(jobShort) - 1);
            jobShort[sizeof(jobShort) - 1] = '\0';

            char hashesStr[16];
            if      (stats.totalHashes >= 1000000000ULL) snprintf(hashesStr, sizeof(hashesStr), "%.1fG", (double)stats.totalHashes / 1e9);
            else if (stats.totalHashes >= 1000000ULL)    snprintf(hashesStr, sizeof(hashesStr), "%.1fM", (double)stats.totalHashes / 1e6);
            else                                          snprintf(hashesStr, sizeof(hashesStr), "%lluK", (unsigned long long)(stats.totalHashes / 1000));

            char info[64];
            snprintf(info, sizeof(info), "Job %s  D=%lu  %s",
                     jobShort,
                     (unsigned long)stats.currentDifficulty,
                     hashesStr);
            lv_label_set_text(s_lblBtcInfo, info);
        } else {
            lv_label_set_text(s_lblBtcInfo, "");
        }
    }

    // Sparkline: push new hashrate datapoint every update call and rescale
    // the Y axis so the curve always fills the available height.
    if (s_series) {
        lv_chart_set_next_value(s_chart, s_series, (int32_t)stats.hashrate);
        int32_t yMax = (int32_t)(s_maxHashrate * 1.2f);
        if (yMax < 10) yMax = 10;
        lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, yMax);
    }

    s_topBar.update(stats.algorithm);
}
