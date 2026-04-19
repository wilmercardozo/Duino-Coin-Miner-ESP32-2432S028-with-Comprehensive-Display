#include <Arduino.h>
#include <LittleFS.h>
#include "Config.h"
#include "config/ConfigStore.h"
#include "network/WiFiManager.h"
#include "mining/IMiningAlgorithm.h"
#include "mining/DuinoCoinMiner.h"
#include "mining/BitcoinMiner.h"
#include "ui/UIManager.h"
#include "ui/SplashScreen.h"
#include "ui/DashboardScreen.h"
#include "portal/ConfigPortal.h"
#include "portal/OTAHandler.h"
#include "portal/StatsServer.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ── CYD touch pins (VSPI bus — separate from TFT HSPI) ───────────────────────
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_SCK  25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32

// ── shared state ──────────────────────────────────────────────────────────────
Config gConfig;
volatile bool gPortalRequested = false;   // set by UIManager on long-press
volatile bool gInPortalMode = false;

// ── touch SPI (VSPI) + touchscreen ───────────────────────────────────────────
static SPIClass          s_touchSPI(VSPI);
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);

static void taskUI(void*)
{
    int16_t s_lastX = 0, s_lastY = 0;
    for (;;) {
        if (s_touch.tirqTouched() && s_touch.touched()) {
            TS_Point p = s_touch.getPoint();
            // rotation 3 — invert both axes from raw XPT2046 coords
            s_lastX = (int16_t)map(p.x, 200, 3900, 319, 0);
            s_lastY = (int16_t)map(p.y, 200, 3900, 239, 0);
            UIManager::handleTouch(s_lastX, s_lastY, true);
        } else {
            UIManager::handleTouch(s_lastX, s_lastY, false);
        }
        OTAHandler::handle();
        UIManager::tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void startUI()
{
    UIManager::init();
    s_touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, -1);
    s_touch.begin(s_touchSPI);
    s_touch.setRotation(1);
    // Task NOT created here — setup() drives LVGL manually during boot
    // (splash status transitions) then calls startUITask() at the end.
}

static void startUITask()
{
    xTaskCreatePinnedToCore(taskUI, "ui", 8192, nullptr, 2, nullptr, 0);
}

// ── mining task ───────────────────────────────────────────────────────────────
static IMiningAlgorithm* gMiner = nullptr;

static void taskMining(void* param)
{
    IMiningAlgorithm* miner = (IMiningAlgorithm*)param;
    // Retry connect() forever instead of dying on first failure — routers
    // with flaky DNS, pools briefly down, or a late-booting captive all
    // used to brick the rig until restart.
    while (!miner->connect()) {
        Serial.println("[mining] connect() failed — retry in 10 s");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    for (;;) {
        miner->mine();
    }
}

// Secondary BTC hashing task — runs on core 0 alongside the UI.  Uses
// low priority (1) so the UI task (priority 2) preempts it whenever
// there's a touch event or a render tick.  Only spawned for BTC; DUCO
// is single-core since Duino-Coin is not CPU-bound on an ESP32.
static void taskMiningSecondary(void* param)
{
    BitcoinMiner* btc = (BitcoinMiner*)param;
    // Wait for the primary to finish connect() — secondary shouldn't start
    // hashing against stale state.
    while (!btc) { vTaskDelay(pdMS_TO_TICKS(100)); }
    for (;;) {
        btc->secondaryMine();
    }
}

// Periodic balance refresh — low-priority task on core 0.  Runs forever,
// polling the miner's public balance API (DUCO only; BTC is no-op).  Kept
// separate from the mining task so the TLS round-trip can't stall hashing.
static void taskBalance(void*)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));   // 60 s
        if (gMiner) gMiner->fetchBalance();
    }
}

// NVS persistence task — writes accumulated counters (shares/bestDiff/
// totalHashes/balance) every 60 s so a power cut doesn't zero out the
// user's history.  Cheap: a few u32/u64 NVS puts, takes <5 ms.
static void taskStatsPersist(void*)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        if (gMiner) gMiner->persistStats();
    }
}

void startMining()
{
    if (gConfig.algorithm == Algorithm::DUINOCOIN) {
        if (gConfig.duco_user[0] == '\0') {
            Serial.println("[mining] DUCO selected but duco_user empty — skipping miner start");
            return;
        }
        Serial.printf("[mining] starting DuinoCoinMiner (user=%s)\n", gConfig.duco_user);
        gMiner = new DuinoCoinMiner(gConfig);
    } else {
        if (gConfig.btc_address[0] == '\0') {
            Serial.println("[mining] BTC selected but btc_address empty — skipping miner start");
            return;
        }
        Serial.printf("[mining] starting BitcoinMiner (addr=%s pool=%s:%u)\n",
                      gConfig.btc_address, gConfig.pool_url, (unsigned)gConfig.pool_port);
        gMiner = new BitcoinMiner(gConfig);
    }
    if (gMiner) {
        xTaskCreatePinnedToCore(taskMining, "mining", 8192, gMiner, 5, nullptr, 1);
        // 4 KB is enough for the balance task (Arduino-ESP32's HTTPS stack
        // uses its own heap; the task mostly waits).  Low priority (1) and
        // pinned to core 0 to avoid stealing cycles from mining on core 1.
        xTaskCreatePinnedToCore(taskBalance, "balance", 6144, nullptr, 1, nullptr, 0);
        xTaskCreatePinnedToCore(taskStatsPersist, "persist", 3072, nullptr, 1, nullptr, 0);

        // Dual-core hashing for BTC only.  Secondary runs on core 0 at
        // priority 1 (below UI at 2) so the UI remains responsive.
        if (gConfig.algorithm == Algorithm::BITCOIN) {
            xTaskCreatePinnedToCore(taskMiningSecondary, "mining2", 8192,
                                    gMiner, 1, nullptr, 0);
        }
    }
}

// ── setup / loop ──────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    Serial.println("[boot] NerdDuino Pro starting");

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    startUI();   // init display + build screens; task UI not yet running
    SplashScreen::load();
    SplashScreen::setStatus("Iniciando...");
    UIManager::pumpLvgl(300);

    bool hasConfig   = ConfigStore::load(gConfig);
    bool forcePortal = ConfigStore::isForcePortal();   // one-shot; clears flag

    if (!hasConfig || forcePortal) {
        Serial.printf("[boot] Entering portal (hasConfig=%d forcePortal=%d)\n",
                      hasConfig, forcePortal);
        gInPortalMode = true;
        SplashScreen::setStatus(forcePortal
            ? "Modo portal (solicitado)"
            : "Sin configuracion - portal");
        UIManager::pumpLvgl(200);
        ConfigPortal::start();
        startUITask();
        return;
    }

    SplashScreen::setStatus("Conectando WiFi...");
    UIManager::pumpLvgl(100);
    WiFiMgr::connect(gConfig);

    if (WiFiMgr::needsPortal()) {
        Serial.println("[boot] WiFi failed — entering portal");
        gInPortalMode = true;
        SplashScreen::setStatus("WiFi fallo - portal");
        UIManager::pumpLvgl(200);
        ConfigPortal::start();
        startUITask();
        return;
    }

    if (WiFiMgr::isConnected()) {
        SplashScreen::setStatus("Sincronizando hora...");
        UIManager::pumpLvgl(100);
        configTime((long)gConfig.timezone_offset * 3600, 0,
                   "pool.ntp.org", "time.nist.gov");
        Serial.println("[ntp] Sync started");

        SplashScreen::setStatus("Preparando OTA...");
        UIManager::pumpLvgl(100);
        OTAHandler::init(gConfig.rig_name);
        StatsServer::init(&gMiner);
        WiFiMgr::startAutoReconnect(gConfig);

        SplashScreen::setStatus("Iniciando minero...");
        UIManager::pumpLvgl(100);
        startMining();

        SplashScreen::setStatus("Listo!");
        UIManager::pumpLvgl(400);
    }

    DashboardScreen::load();   // queue transition to dashboard
    UIManager::pumpLvgl(250);  // render the slide animation before UI task takes over
    startUITask();
}

void loop()
{
    if (gInPortalMode) {
        ConfigPortal::handle();
        return;
    }

    if (gPortalRequested) {
        gPortalRequested = false;
        ConfigStore::erase();  // erase config so next boot enters portal cleanly
        UIManager::showRestarting("Abriendo portal...");
    }

    static unsigned long s_lastUpdate = 0;
    unsigned long now = millis();
    if (now - s_lastUpdate >= 1000) {
        s_lastUpdate = now;
        if (gMiner) {
            UIManager::update(gMiner->getStats());
        } else {
            MiningStats idle;
            idle.uptimeSeconds = now / 1000;
            UIManager::update(idle);
        }
    }
}
