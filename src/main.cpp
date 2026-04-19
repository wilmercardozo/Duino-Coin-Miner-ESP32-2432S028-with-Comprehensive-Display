#include <Arduino.h>
#include <LittleFS.h>
#include "Config.h"
#include "config/ConfigStore.h"
#include "network/WiFiManager.h"

Config gConfig;

void setup() {
    Serial.begin(115200);
    Serial.println("[boot] NerdDuino Pro starting");

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    bool hasConfig = ConfigStore::load(gConfig);

    if (!hasConfig) {
        Serial.println("[boot] No config — entering portal");
        // Portal will be started in Task 10; placeholder for now
        return;
    }

    WiFiMgr::connect(gConfig);

    if (WiFiMgr::needsPortal()) {
        Serial.println("[boot] WiFi failed — entering portal");
        // Portal start placeholder
    }
}

void loop() { delay(1000); }
