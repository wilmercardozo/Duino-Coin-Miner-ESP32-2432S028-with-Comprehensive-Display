#include "OTAHandler.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

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
    // ArduinoOTA.begin() already initializes mDNS with the hostname and
    // registers _arduino._tcp.  Add an _http._tcp record on port 80 so
    // the /stats.json endpoint is discoverable at <hostname>.local too.
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[ota] Ready — hostname: %s.local (http + ota)\n", hostname);
}

void OTAHandler::handle() {
    ArduinoOTA.handle();
}
