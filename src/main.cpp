#include <Arduino.h>
#include <LittleFS.h>
#include "Config.h"
#include "config/ConfigStore.h"

Config gConfig;

void setup() {
    Serial.begin(115200);
    Serial.println("[boot] NerdDuino Pro starting");

    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[boot] LittleFS mount failed");
    }

    if (ConfigStore::load(gConfig)) {
        Serial.printf("[boot] Config loaded — algo=%d wifi=%s\n",
                      (int)gConfig.algorithm, gConfig.wifi_ssid);
    } else {
        Serial.println("[boot] No config found — portal needed");
    }
}

void loop() { delay(1000); }
