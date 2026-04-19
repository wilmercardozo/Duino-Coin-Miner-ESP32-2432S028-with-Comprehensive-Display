#include <Arduino.h>
#include <LittleFS.h>
#include "Config.h"
#include "config/ConfigStore.h"
#include "network/WiFiManager.h"
#include "mining/IMiningAlgorithm.h"
#include "mining/DuinoCoinMiner.h"
#include "mining/BitcoinMiner.h"
#include "ui/UIManager.h"
#include "portal/ConfigPortal.h"
#include "portal/OTAHandler.h"
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

void startUI()
{
    UIManager::init();
    s_touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, -1);
    s_touch.begin(s_touchSPI);
    s_touch.setRotation(1);
    xTaskCreatePinnedToCore(taskUI, "ui", 8192, nullptr, 2, nullptr, 0);
}

// ── mining task ───────────────────────────────────────────────────────────────
static IMiningAlgorithm* gMiner = nullptr;

static void taskMining(void* param)
{
    IMiningAlgorithm* miner = (IMiningAlgorithm*)param;
    if (!miner->connect()) {
        Serial.println("[mining] connect() failed");
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
        miner->mine();
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

    startUI();  // always init display before any branching

    bool hasConfig = ConfigStore::load(gConfig);

    if (!hasConfig) {
        Serial.println("[boot] No config — entering portal");
        gInPortalMode = true;
        ConfigPortal::start();
        return;
    }

    WiFiMgr::connect(gConfig);

    if (WiFiMgr::needsPortal()) {
        Serial.println("[boot] WiFi failed — entering portal");
        gInPortalMode = true;
        ConfigPortal::start();
        return;
    }

    if (WiFiMgr::isConnected()) {
        configTime((long)gConfig.timezone_offset * 3600, 0,
                   "pool.ntp.org", "time.nist.gov");
        Serial.println("[ntp] Sync started");
        OTAHandler::init(gConfig.rig_name);
        startMining();
    }
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
        delay(100);
        ESP.restart();         // clean reboot — avoids WiFi mode conflict mid-mining
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
