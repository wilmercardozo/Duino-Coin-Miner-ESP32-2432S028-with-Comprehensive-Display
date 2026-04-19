#include "WiFiManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static bool s_needsPortal = false;
static bool s_reconnectStarted = false;

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

        // Some routers hand out their own IP as DNS but then don't actually
        // resolve anything (observed with a MAKKI router — getPool died with
        // hostByName DNS Failed).  Override with public DNS while keeping the
        // DHCP-assigned IP/gateway/subnet so we don't break the local link.
        IPAddress ip   = WiFi.localIP();
        IPAddress gw   = WiFi.gatewayIP();
        IPAddress mask = WiFi.subnetMask();
        IPAddress dns1(1, 1, 1, 1);
        IPAddress dns2(8, 8, 8, 8);
        WiFi.config(ip, gw, mask, dns1, dns2);
        Serial.printf("[wifi] DNS set to %s / %s\n",
                      dns1.toString().c_str(), dns2.toString().c_str());
        return true;
    }

    s_needsPortal = true;
    Serial.println("[wifi] Connection failed — portal required");
    return false;
}

bool WiFiMgr::isConnected() { return WiFi.status() == WL_CONNECTED; }
bool WiFiMgr::needsPortal() { return s_needsPortal; }
void WiFiMgr::clearPortalFlag() { s_needsPortal = false; }

// ---------------------------------------------------------------------------
// Auto-reconnect task — polls WiFi.status() every 2s (cheap) and, when the
// link has been down long enough, attempts a reconnect with exponential
// backoff so we don't hammer a router that's still rebooting.
// ---------------------------------------------------------------------------
static void taskWiFiReconnect(void* param) {
    const Config* cfg = (const Config*)param;
    uint32_t backoffMs      = 10000;   // start at 10 s
    const uint32_t maxBackoff = 120000; // cap at 2 min
    uint32_t nextAttemptAt  = 0;
    bool wasConnected       = (WiFi.status() == WL_CONNECTED);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        bool connectedNow = (WiFi.status() == WL_CONNECTED);

        if (connectedNow) {
            if (!wasConnected) {
                Serial.printf("[wifi] Reconnected — IP %s\n",
                              WiFi.localIP().toString().c_str());
                backoffMs     = 10000;  // reset backoff on success
                nextAttemptAt = 0;
            }
            wasConnected = true;
            continue;
        }

        wasConnected = false;
        uint32_t now = millis();
        if (now < nextAttemptAt) continue;

        if (cfg->wifi_ssid[0] == '\0') continue;   // nothing to reconnect to

        Serial.printf("[wifi] Link down — reconnect attempt (next backoff %lus)\n",
                      (unsigned long)(backoffMs / 1000));
        // Full WiFi.begin() instead of reconnect() — ESP32's reconnect()
        // sometimes gets stuck in a "no-scan" state after the AP disappears.
        WiFi.disconnect(false, false);
        vTaskDelay(pdMS_TO_TICKS(200));
        WiFi.begin(cfg->wifi_ssid, cfg->wifi_pass);

        nextAttemptAt = millis() + backoffMs;
        backoffMs = (backoffMs * 2 > maxBackoff) ? maxBackoff : backoffMs * 2;
    }
}

void WiFiMgr::startAutoReconnect(const Config& cfg) {
    if (s_reconnectStarted) return;
    s_reconnectStarted = true;
    // 4 KB stack is enough; reconnect task is light.  Pin to core 0 so it
    // doesn't contend with the mining task on core 1.
    xTaskCreatePinnedToCore(taskWiFiReconnect, "wifi-reconn",
                            4096, (void*)&cfg, 1, nullptr, 0);
}
