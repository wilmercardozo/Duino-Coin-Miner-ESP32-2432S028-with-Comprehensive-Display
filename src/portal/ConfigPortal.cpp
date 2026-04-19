#include "ConfigPortal.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config/ConfigStore.h"
#include "Config.h"

static AsyncWebServer s_server(80);
static bool s_done = false;

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NerdDuino Pro — Setup</title>
<link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
<style>
  body { background:#0f172a; color:#e2e8f0; }
  input,select { background:#1e293b; border:1px solid #334155; color:#e2e8f0; }
  .card { background:#1e293b; border-radius:12px; }
  .accent { color:#ff6b35; }
  .btn { background:#ff6b35; color:white; }
  .btn:hover { background:#ea580c; }
  #duco-fields, #btc-fields { display:none; }
</style>
</head>
<body class="min-h-screen flex items-center justify-center p-4">
<div class="card p-8 w-full max-w-md shadow-2xl">
  <h1 class="text-2xl font-bold accent mb-1">NerdDuino Pro</h1>
  <p class="text-gray-400 text-sm mb-6">Configuracion inicial</p>
  <form id="form">
    <p class="text-xs uppercase tracking-widest text-gray-500 mb-2">WiFi</p>
    <div class="flex gap-2 mb-2">
      <input id="ssid" name="wifi_ssid" placeholder="Red WiFi" class="flex-1 rounded p-2 text-sm" required>
      <button type="button" onclick="scanWifi()" class="px-3 py-2 text-xs rounded border border-gray-600 text-gray-300">Buscar</button>
    </div>
    <input id="pass" name="wifi_pass" type="password" placeholder="Contrasena" class="w-full rounded p-2 text-sm mb-4">

    <p class="text-xs uppercase tracking-widest text-gray-500 mb-2">Algoritmo</p>
    <div class="flex gap-4 mb-4 text-sm">
      <label><input type="radio" name="algorithm" value="0" onchange="showAlgo(0)" checked class="mr-1">DuinoCoin</label>
      <label><input type="radio" name="algorithm" value="1" onchange="showAlgo(1)" class="mr-1">Bitcoin</label>
    </div>

    <div id="duco-fields" class="mb-4">
      <input name="duco_user" placeholder="Usuario DuinoCoin" class="w-full rounded p-2 text-sm mb-2">
      <input name="duco_key"  placeholder="Mining Key (opcional)" class="w-full rounded p-2 text-sm">
    </div>
    <div id="btc-fields" class="mb-4">
      <input name="btc_address" placeholder="Direccion Bitcoin (bc1q...)" class="w-full rounded p-2 text-sm mb-2">
      <input name="pool_url"    placeholder="Pool URL" value="public-pool.io" class="w-full rounded p-2 text-sm mb-2">
      <input name="pool_port"   placeholder="Puerto" value="21496" type="number" class="w-full rounded p-2 text-sm">
    </div>

    <p class="text-xs uppercase tracking-widest text-gray-500 mb-2">Opcional</p>
    <input name="rig_name" placeholder="Nombre del rig" value="NerdDuino-1" class="w-full rounded p-2 text-sm mb-2">
    <select name="timezone_offset" class="w-full rounded p-2 text-sm mb-6">
      <option value="-5" selected>UTC-5 (Colombia/Ecuador/Peru)</option>
      <option value="-4">UTC-4 (Venezuela/Bolivia)</option>
      <option value="-3">UTC-3 (Argentina/Brasil)</option>
      <option value="-6">UTC-6 (Mexico)</option>
      <option value="0">UTC+0</option>
    </select>

    <button type="submit" class="btn w-full py-3 rounded font-semibold">Guardar y conectar</button>
  </form>
  <p id="msg" class="text-center text-sm mt-4 hidden"></p>
</div>
<script>
  showAlgo(0);
  function showAlgo(v){
    document.getElementById('duco-fields').style.display = v==0?'block':'none';
    document.getElementById('btc-fields').style.display  = v==1?'block':'none';
  }
  function scanWifi(){
    fetch('/scan').then(r=>r.json()).then(nets=>{
      const s=document.getElementById('ssid');
      s.value = nets[0]?.ssid || s.value;
      alert('Redes:\n'+nets.map(n=>n.ssid+' ('+n.rssi+'dBm)').join('\n'));
    });
  }
  document.getElementById('form').onsubmit=function(e){
    e.preventDefault();
    const data=Object.fromEntries(new FormData(this));
    fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
    .then(r=>r.json()).then(d=>{
      const m=document.getElementById('msg');
      m.textContent=d.ok?'Guardado - reiniciando...':'Error: '+d.error;
      m.className='text-center text-sm mt-4 '+(d.ok?'text-green-400':'text-red-400');
      m.classList.remove('hidden');
    });
  };
</script>
</html>
)rawliteral";

void ConfigPortal::start() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("NerdDuino-Setup");
    Serial.println("[portal] AP started — connect to NerdDuino-Setup, open 192.168.4.1");

    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", PORTAL_HTML);
    });

    s_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        int n = WiFi.scanNetworks();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n && i < 10; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["ssid"] = WiFi.SSID(i);
            obj["rssi"] = (int)WiFi.RSSI(i);
        }
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on("/save", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"JSON invalido\"}");
                return;
            }

            Config cfg;
            strlcpy(cfg.wifi_ssid,   doc["wifi_ssid"]   | "", sizeof(cfg.wifi_ssid));
            strlcpy(cfg.wifi_pass,   doc["wifi_pass"]   | "", sizeof(cfg.wifi_pass));
            strlcpy(cfg.duco_user,   doc["duco_user"]   | "", sizeof(cfg.duco_user));
            strlcpy(cfg.duco_key,    doc["duco_key"]    | "", sizeof(cfg.duco_key));
            strlcpy(cfg.btc_address, doc["btc_address"] | "", sizeof(cfg.btc_address));
            strlcpy(cfg.pool_url,    doc["pool_url"]    | "public-pool.io", sizeof(cfg.pool_url));
            strlcpy(cfg.rig_name,    doc["rig_name"]    | "NerdDuino-1", sizeof(cfg.rig_name));
            cfg.pool_port       = doc["pool_port"]       | 21496;
            cfg.timezone_offset = (int8_t)((int)(doc["timezone_offset"] | -5));
            cfg.algorithm = ((int)(doc["algorithm"] | 0) == 1) ? Algorithm::BITCOIN : Algorithm::DUINOCOIN;

            if (cfg.wifi_ssid[0] == '\0') {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"SSID requerido\"}");
                return;
            }

            cfg.valid = true;
            ConfigStore::save(cfg);
            Serial.println("[portal] Config saved — restarting");
            req->send(200, "application/json", "{\"ok\":true}");
            s_done = true;
        }
    );

    s_server.begin();
}

void ConfigPortal::handle() {
    if (s_done) {
        vTaskSuspendAll();   // suspend all tasks before restart
        delay(500);
        ESP.restart();
    }
}

void ConfigPortal::stop() { s_server.end(); }
bool ConfigPortal::isDone() { return s_done; }
