#include "ConfigStore.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char* PATH              = "/config.json";
static const char* PATH_FORCE_PORTAL = "/force_portal";

bool ConfigStore::exists() {
    return LittleFS.exists(PATH);
}

void ConfigStore::erase() {
    LittleFS.remove(PATH);
}

bool ConfigStore::load(Config& out) {
    if (!LittleFS.exists(PATH)) return false;
    File f = LittleFS.open(PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err != DeserializationError::Ok) return false;

    strlcpy(out.wifi_ssid,    doc["wifi_ssid"]    | "", sizeof(out.wifi_ssid));
    strlcpy(out.wifi_pass,    doc["wifi_pass"]    | "", sizeof(out.wifi_pass));
    strlcpy(out.duco_user,    doc["duco_user"]    | "", sizeof(out.duco_user));
    strlcpy(out.duco_key,     doc["duco_key"]     | "", sizeof(out.duco_key));
    strlcpy(out.btc_address,  doc["btc_address"]  | "", sizeof(out.btc_address));
    strlcpy(out.pool_url,     doc["pool_url"]     | "public-pool.io", sizeof(out.pool_url));
    strlcpy(out.pool_url2,    doc["pool_url2"]    | "", sizeof(out.pool_url2));
    strlcpy(out.rig_name,     doc["rig_name"]     | "NerdDuino-1", sizeof(out.rig_name));
    out.pool_port       = doc["pool_port"]       | 21496;
    out.pool_port2      = doc["pool_port2"]      | 0;
    out.timezone_offset = doc["timezone_offset"] | -5;
    uint8_t algo = doc["algorithm"] | 0;
    out.algorithm = (algo == static_cast<uint8_t>(Algorithm::BITCOIN))
                    ? Algorithm::BITCOIN : Algorithm::DUINOCOIN;
    out.valid = true;
    return true;
}

bool ConfigStore::isForcePortal() {
    if (!LittleFS.exists(PATH_FORCE_PORTAL)) return false;
    LittleFS.remove(PATH_FORCE_PORTAL);   // one-shot: clear after reading
    return true;
}

void ConfigStore::setForcePortal() {
    File f = LittleFS.open(PATH_FORCE_PORTAL, "w");
    if (f) { f.print("1"); f.close(); }
}

bool ConfigStore::save(const Config& cfg) {
    JsonDocument doc;
    doc["wifi_ssid"]        = cfg.wifi_ssid;
    doc["wifi_pass"]        = cfg.wifi_pass;
    doc["algorithm"]        = (uint8_t)cfg.algorithm;
    doc["duco_user"]        = cfg.duco_user;
    doc["duco_key"]         = cfg.duco_key;
    doc["btc_address"]      = cfg.btc_address;
    doc["pool_url"]         = cfg.pool_url;
    doc["pool_port"]        = cfg.pool_port;
    doc["pool_url2"]        = cfg.pool_url2;
    doc["pool_port2"]       = cfg.pool_port2;
    doc["rig_name"]         = cfg.rig_name;
    doc["timezone_offset"]  = cfg.timezone_offset;

    File f = LittleFS.open(PATH, "w");
    if (!f) return false;
    size_t written = serializeJson(doc, f);
    f.close();
    return written > 0;
}
