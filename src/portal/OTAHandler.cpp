#include "OTAHandler.h"
#include <ArduinoOTA.h>

void OTAHandler::init(const char* hostname) {
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() {
        Serial.println("[ota] Update starting");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("[ota] Done — restarting");
    });
    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("[ota] Error %u\n", err);
    });
    ArduinoOTA.begin();
    Serial.printf("[ota] Ready — hostname: %s.local\n", hostname);
}

void OTAHandler::handle() {
    ArduinoOTA.handle();
}
