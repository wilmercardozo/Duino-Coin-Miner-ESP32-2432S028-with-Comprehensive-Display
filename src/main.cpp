#include <Arduino.h>
#include "Config.h"

Config gConfig;

void setup() {
    Serial.begin(115200);
    Serial.println("[boot] NerdDuino Pro starting");
}

void loop() {
    delay(1000);
}
