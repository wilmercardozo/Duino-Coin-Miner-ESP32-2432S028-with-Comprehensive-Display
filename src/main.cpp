#include <Arduino.h>
#include <LittleFS.h>
#include "Config.h"
#include "config/ConfigStore.h"
#include "network/WiFiManager.h"
#include "mining/IMiningAlgorithm.h"
#include "mining/DuinoCoinMiner.h"
#include "mining/BitcoinMiner.h"

Config gConfig;

static IMiningAlgorithm* gMiner = nullptr;

static void taskMining(void* param) {
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

void startMining() {
    if (gConfig.algorithm == Algorithm::DUINOCOIN) {
        gMiner = new DuinoCoinMiner(gConfig);
    } else {
        gMiner = new BitcoinMiner(gConfig);
    }
    if (gMiner) {
        xTaskCreatePinnedToCore(taskMining, "mining", 8192, gMiner, 5, nullptr, 1);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("[boot] NerdDuino Pro starting");

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    bool hasConfig = ConfigStore::load(gConfig);

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
