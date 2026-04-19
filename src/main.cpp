#include <Arduino.h>
#include <LittleFS.h>
#include "Config.h"
#include "config/ConfigStore.h"
#include "network/WiFiManager.h"
#include "mining/IMiningAlgorithm.h"
#include "mining/DuinoCoinMiner.h"
#include "mining/BitcoinMiner.h"
#include "ui/UIManager.h"
#include <XPT2046_Touchscreen.h>

// ── CYD touch pins ────────────────────────────────────────────────────────────
#define TOUCH_CS  33
#define TOUCH_IRQ 36

// ── shared state ──────────────────────────────────────────────────────────────
Config gConfig;
volatile bool gPortalRequested = false;   // set by UIManager on long-press

// ── touch + UI task ───────────────────────────────────────────────────────────
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);

static void taskUI(void*)
{
    for (;;) {
        if (s_touch.tirqTouched() && s_touch.touched()) {
            TS_Point p = s_touch.getPoint();
            int16_t sx = (int16_t)map(p.x, 200, 3900, 0, 319);
            int16_t sy = (int16_t)map(p.y, 200, 3900, 0, 239);
            UIManager::handleTouch(sx, sy, true);
        } else {
            UIManager::handleTouch(0, 0, false);
        }
        UIManager::tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void startUI()
{
    UIManager::init();
    s_touch.begin();
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
        gMiner = new DuinoCoinMiner(gConfig);
    } else {
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

    bool hasConfig = ConfigStore::load(gConfig);

    // Display comes up first so the user always sees something
    startUI();

    if (!hasConfig) {
        Serial.println("[boot] No config — entering portal");
        WiFiMgr::clearPortalFlag();
        return;
    }

    WiFiMgr::connect(gConfig);

    if (WiFiMgr::needsPortal()) {
        Serial.println("[boot] WiFi failed — entering portal");
        // Portal start placeholder
    }

    if (WiFiMgr::isConnected() && !WiFiMgr::needsPortal()) {
        startMining();
    }
}

void loop() { delay(1000); }
