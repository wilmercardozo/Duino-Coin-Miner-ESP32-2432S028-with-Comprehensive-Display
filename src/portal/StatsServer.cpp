#include "StatsServer.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static AsyncWebServer       s_server(80);
static IMiningAlgorithm**   s_minerRef = nullptr;

void StatsServer::init(IMiningAlgorithm** miner) {
    s_minerRef = miner;

    s_server.on("/stats.json", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        MiningStats stats;
        if (s_minerRef && *s_minerRef) stats = (*s_minerRef)->getStats();

        doc["algorithm"]         = stats.algorithm;
        doc["hashrate_khs"]      = stats.hashrate;
        doc["shares_accepted"]   = stats.sharesAccepted;
        doc["shares_rejected"]   = stats.sharesRejected;
        doc["balance"]           = stats.balance;
        doc["uptime_s"]          = stats.uptimeSeconds;
        doc["ping_ms"]           = stats.pingMs;
        doc["pool"]              = stats.poolUrl;
        doc["best_difficulty"]   = stats.bestDifficulty;
        doc["current_difficulty"]= stats.currentDifficulty;
        doc["total_hashes"]      = stats.totalHashes;
        doc["job_id"]            = stats.jobId;
        doc["rssi"]              = (int)WiFi.RSSI();
        doc["ip"]                = WiFi.localIP().toString();
        doc["free_heap"]         = (uint32_t)ESP.getFreeHeap();

        String out;
        serializeJson(doc, out);
        AsyncWebServerResponse* resp =
            req->beginResponse(200, "application/json", out);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        req->send(resp);
    });

    s_server.begin();
    Serial.println("[stats] /stats.json listening on :80");
}
