#include "WiFiManager.h"
#include <WiFi.h>

static bool s_needsPortal = false;

bool WiFiMgr::connect(const Config& cfg) {
    if (cfg.wifi_ssid[0] == '\0') {
        s_needsPortal = true;
        return false;
    }

    Serial.printf("[wifi] Connecting to %s ...\n", cfg.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

    uint32_t deadline = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] Connected — IP %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    s_needsPortal = true;
    Serial.println("[wifi] Connection failed — portal required");
    return false;
}

bool WiFiMgr::isConnected() { return WiFi.status() == WL_CONNECTED; }
bool WiFiMgr::needsPortal() { return s_needsPortal; }
void WiFiMgr::clearPortalFlag() { s_needsPortal = false; }
