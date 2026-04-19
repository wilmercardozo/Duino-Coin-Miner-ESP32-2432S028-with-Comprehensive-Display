#include "UIManager.h"
#include "DashboardScreen.h"
#include "ClockScreen.h"

#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Arduino.h>

// ── portal flag — defined in main.cpp, set here on long-press ────────────────
extern volatile bool gPortalRequested;

// ── cross-task snapshot (written by Core 1, consumed by Core 0) ──────────────
MiningSnapshot gMiningSnapshot = {};

// ── static state ─────────────────────────────────────────────────────────────
static TFT_eSPI        s_tft;
static lv_display_t*   s_disp        = nullptr;
// 16 rows × 320 px × 2 B ≈ 10 KB — kept in DRAM (no PSRAM on CYD)
static lv_color_t      s_lvBuf[320 * 16];

static uint8_t         s_targetFps   = 30;
static uint32_t        s_lastFlush   = 0;

// touch state
static bool     s_pressing   = false;
static uint32_t s_pressStart = 0;

// view management (0 = Dashboard, 1 = Clock)
static constexpr uint8_t kViewCount = 2;
static uint8_t s_curView = 0;

// ── TFT_eSPI flush callback ───────────────────────────────────────────────────
static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1, w, h);
    s_tft.pushColors(reinterpret_cast<uint16_t*>(px_map), w * h, true);
    s_tft.endWrite();

    lv_display_flush_ready(disp);
}

// ── internal helpers ─────────────────────────────────────────────────────────
static void switchView(uint8_t view)
{
    s_curView = view;
    if (s_curView == 0) {
        DashboardScreen::load();
    } else {
        ClockScreen::load();
    }
}

// ── public API ───────────────────────────────────────────────────────────────
namespace UIManager {

void init()
{
    // 1. Hardware init
    s_tft.init();
    s_tft.setRotation(1);   // landscape — ESP32-2432S028R with TFT_INVERSION_ON

    // 2. LVGL init
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // 3. Create display driver
    s_disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(s_disp, flushCb);
    lv_display_set_buffers(s_disp,
                           s_lvBuf,
                           nullptr,
                           sizeof(s_lvBuf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 4. Build screens (stubs for now; real content in Task 8/9)
    DashboardScreen::create();
    ClockScreen::create();
    DashboardScreen::load();
}

void tick()
{
    uint32_t now = millis();
    uint32_t period = (s_targetFps > 0) ? (1000u / s_targetFps) : 33u;
    if (now - s_lastFlush >= period) {
        if (gMiningSnapshot.dirty) {
            MiningStats snapshot;
            taskENTER_CRITICAL(&gMiningSnapshot.mux);
            snapshot = gMiningSnapshot.stats;
            gMiningSnapshot.dirty = false;
            taskEXIT_CRITICAL(&gMiningSnapshot.mux);
            DashboardScreen::update(snapshot);
            ClockScreen::update(snapshot);
        }
        lv_timer_handler();
        s_lastFlush = now;
    }
}

void update(const MiningStats& stats)
{
    taskENTER_CRITICAL(&gMiningSnapshot.mux);
    gMiningSnapshot.stats = stats;
    gMiningSnapshot.dirty = true;
    taskEXIT_CRITICAL(&gMiningSnapshot.mux);
}

void setTargetFps(uint8_t fps)
{
    s_targetFps = (fps > 0) ? fps : 1;
}

void handleTouch(int16_t x, int16_t y, bool pressed)
{
    uint32_t now = millis();

    if (pressed && !s_pressing) {
        // press start
        s_pressing   = true;
        s_pressStart = now;
    } else if (!pressed && s_pressing) {
        // release
        s_pressing = false;
        uint32_t dur = now - s_pressStart;

        if (dur >= 3000u) {
            // long-press → request captive portal
            gPortalRequested = true;
        } else if (dur < 500u) {
            // short tap — left/right zone navigation
            if (x < 32) {
                // previous view
                uint8_t next = (s_curView == 0) ? (kViewCount - 1) : (s_curView - 1);
                switchView(next);
            } else if (x > 288) {
                // next view
                uint8_t next = (s_curView + 1) % kViewCount;
                switchView(next);
            }
        }
    }
}

} // namespace UIManager
