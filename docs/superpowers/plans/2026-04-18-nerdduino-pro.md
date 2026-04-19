# NerdDuino Pro Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dual-coin (DuinoCoin + Bitcoin) ESP32 miner firmware for the ESP32-2432S028R with a cockpit-style LVGL UI, two switchable views, and a captive web config portal — no hardcoded settings.

**Architecture:** PlatformIO project using Arduino framework. Config stored as JSON in LittleFS. Mining runs on Core 1 (FreeRTOS), UI + network on Core 0. LVGL uses a partial buffer (no PSRAM required). Config portal activates in AP mode via AsyncWebServer with HTML in PROGMEM.

**Tech Stack:** PlatformIO · ESP32 Arduino · LVGL 9.x · TFT_eSPI (ILI9341) · ArduinoJson · ESPAsyncWebServer · LittleFS · ArduinoOTA · mbedTLS SHA-256 (from NMMiner, GPL-3.0)

**License:** GPL-3.0 (required by NMMiner SHA-256 code)

**Spec:** `docs/superpowers/specs/2026-04-18-nerdduino-pro-design.md`

---

## File Map

```
/
├── platformio.ini
├── partitions.csv
├── include/
│   ├── lv_conf.h                    ← LVGL compile-time config
│   └── Config.h                     ← runtime settings struct + Algorithm enum
├── src/
│   ├── main.cpp                     ← boot, task creation, top-level wiring
│   ├── config/
│   │   └── ConfigStore.cpp/.h       ← LittleFS JSON read/write
│   ├── network/
│   │   └── WiFiManager.cpp/.h       ← connect, retry counter, portal trigger flag
│   ├── mining/
│   │   ├── IMiningAlgorithm.h       ← abstract interface + MiningStats struct
│   │   ├── DuinoCoinMiner.cpp/.h    ← DUCO-S1 client (from ChocDuino)
│   │   └── BitcoinMiner.cpp/.h      ← Stratum V1 SHA-256 (from NMMiner)
│   ├── ui/
│   │   ├── UIManager.cpp/.h         ← LVGL init, partial buffer, screen switch, touch
│   │   ├── DashboardScreen.cpp/.h   ← View 1: gauge arc + stats + sparkline
│   │   └── ClockScreen.cpp/.h       ← View 2: large clock + info strip
│   └── portal/
│       ├── ConfigPortal.cpp/.h      ← AP mode + AsyncWebServer + PROGMEM HTML
│       └── OTAHandler.cpp/.h        ← ArduinoOTA init + handle()
└── test/
    └── native/
        └── test_config/
            └── test_config_store.cpp ← unit tests for Config JSON parse/serialize
```

---

## Phase 1 — Foundation

### Task 1: PlatformIO project skeleton

**Files:**
- Create: `platformio.ini`
- Create: `partitions.csv`
- Create: `src/main.cpp`
- Create: `include/Config.h`

- [ ] **Step 1: Create `platformio.ini`**

```ini
[platformio]
default_envs = nerdduino-pro

[env:nerdduino-pro]
platform = espressif32
board = esp32dev
framework = arduino
board_build.flash_size = 4MB
board_build.partitions = partitions.csv
monitor_speed = 115200
upload_speed = 921600

lib_deps =
    lvgl/lvgl@^9.2.0
    bodmer/TFT_eSPI@^2.5.43
    bblanchon/ArduinoJson@^7.0.0
    me-no-dev/ESP Async WebServer@^1.2.3
    me-no-dev/AsyncTCP@^1.1.1

build_flags =
    ; TFT_eSPI — CYD ESP32-2432S028 pins (avoids editing User_Setup_Select.h)
    -DUSER_SETUP_LOADED
    -DILI9341_DRIVER
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_MISO=-1
    -DTFT_BL=21
    -DTFT_BACKLIGHT_ON=1
    -DSPI_FREQUENCY=16000000
    -DLOAD_GLCD
    -DLOAD_FONT2
    -DLOAD_FONT4
    -DLOAD_GFXFF
    -DSMOOTH_FONT
    ; LVGL — use include/lv_conf.h
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_LVGL_H_INCLUDE_SIMPLE

[env:native]
platform = native
test_build_src = no
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
```

- [ ] **Step 2: Create `partitions.csv`**

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1c0000,
app1,     app,  ota_1,   0x1d0000, 0x1c0000,
littlefs, data, spiffs,  0x390000, 0x70000,
```

- [ ] **Step 3: Create `include/Config.h`**

```cpp
#pragma once
#include <stdint.h>

enum class Algorithm : uint8_t { DUINOCOIN = 0, BITCOIN = 1 };

struct Config {
    char     wifi_ssid[64]     = "";
    char     wifi_pass[64]     = "";
    Algorithm algorithm        = Algorithm::DUINOCOIN;
    char     duco_user[64]     = "";
    char     duco_key[64]      = "";
    char     btc_address[64]   = "";
    char     pool_url[64]      = "public-pool.io";
    uint16_t pool_port         = 21496;
    char     rig_name[32]      = "NerdDuino-1";
    int8_t   timezone_offset   = -5;
    bool     valid             = false;
};
```

- [ ] **Step 4: Create minimal `src/main.cpp`**

```cpp
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
```

- [ ] **Step 5: Verify it compiles**

```bash
pio run -e nerdduino-pro
```

Expected: `SUCCESS` with no errors. Warnings about unused variables are OK.

- [ ] **Step 6: Commit**

```bash
git add platformio.ini partitions.csv include/Config.h src/main.cpp
git commit -m "feat: PlatformIO skeleton for nerdduino-pro"
```

---

### Task 2: Config store (LittleFS JSON)

**Files:**
- Create: `src/config/ConfigStore.h`
- Create: `src/config/ConfigStore.cpp`
- Create: `test/native/test_config/test_config_store.cpp`

- [ ] **Step 1: Create `src/config/ConfigStore.h`**

```cpp
#pragma once
#include "Config.h"

namespace ConfigStore {
    bool load(Config& out);   // returns false if file missing or parse error
    bool save(const Config& cfg);
    bool exists();
    void erase();
}
```

- [ ] **Step 2: Create `src/config/ConfigStore.cpp`**

```cpp
#include "ConfigStore.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char* PATH = "/config.json";

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
    if (deserializeJson(doc, f) != DeserializationError::Ok) {
        f.close();
        return false;
    }
    f.close();

    strlcpy(out.wifi_ssid,    doc["wifi_ssid"]    | "", sizeof(out.wifi_ssid));
    strlcpy(out.wifi_pass,    doc["wifi_pass"]    | "", sizeof(out.wifi_pass));
    strlcpy(out.duco_user,    doc["duco_user"]    | "", sizeof(out.duco_user));
    strlcpy(out.duco_key,     doc["duco_key"]     | "", sizeof(out.duco_key));
    strlcpy(out.btc_address,  doc["btc_address"]  | "", sizeof(out.btc_address));
    strlcpy(out.pool_url,     doc["pool_url"]     | "public-pool.io", sizeof(out.pool_url));
    strlcpy(out.rig_name,     doc["rig_name"]     | "NerdDuino-1", sizeof(out.rig_name));
    out.pool_port       = doc["pool_port"]       | 21496;
    out.timezone_offset = doc["timezone_offset"] | -5;
    out.algorithm = (doc["algorithm"] | 0) == 1 ? Algorithm::BITCOIN : Algorithm::DUINOCOIN;
    out.valid = true;
    return true;
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
    doc["rig_name"]         = cfg.rig_name;
    doc["timezone_offset"]  = cfg.timezone_offset;

    File f = LittleFS.open(PATH, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}
```

- [ ] **Step 3: Write native unit test `test/native/test_config/test_config_store.cpp`**

This test validates JSON serialization logic without hardware (runs on your PC via PlatformIO native env):

```cpp
#include <unity.h>
#include <ArduinoJson.h>
#include "Config.h"

// Replicate the serialize/deserialize logic inline for native testing
// (LittleFS not available natively — we test the JSON logic only)

static bool parseConfigJson(const char* json, Config& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    strlcpy(out.wifi_ssid,   doc["wifi_ssid"]   | "", sizeof(out.wifi_ssid));
    strlcpy(out.wifi_pass,   doc["wifi_pass"]   | "", sizeof(out.wifi_pass));
    strlcpy(out.duco_user,   doc["duco_user"]   | "", sizeof(out.duco_user));
    strlcpy(out.pool_url,    doc["pool_url"]    | "public-pool.io", sizeof(out.pool_url));
    out.pool_port       = doc["pool_port"]       | 21496;
    out.timezone_offset = doc["timezone_offset"] | -5;
    out.algorithm = (doc["algorithm"] | 0) == 1 ? Algorithm::BITCOIN : Algorithm::DUINOCOIN;
    out.valid = true;
    return true;
}

void test_parse_duco_config() {
    const char* json = R"({
        "wifi_ssid":"MyNet","wifi_pass":"pass123",
        "algorithm":0,"duco_user":"will","duco_key":"",
        "pool_url":"public-pool.io","pool_port":21496,
        "rig_name":"NerdDuino-1","timezone_offset":-5
    })";
    Config cfg;
    TEST_ASSERT_TRUE(parseConfigJson(json, cfg));
    TEST_ASSERT_EQUAL_STRING("MyNet", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("will", cfg.duco_user);
    TEST_ASSERT_EQUAL(Algorithm::DUINOCOIN, cfg.algorithm);
    TEST_ASSERT_EQUAL(21496, cfg.pool_port);
    TEST_ASSERT_EQUAL(-5, cfg.timezone_offset);
    TEST_ASSERT_TRUE(cfg.valid);
}

void test_parse_bitcoin_config() {
    const char* json = R"({
        "wifi_ssid":"MyNet","wifi_pass":"pass",
        "algorithm":1,"btc_address":"bc1qxxx",
        "pool_url":"public-pool.io","pool_port":21496,
        "rig_name":"rig1","timezone_offset":-5
    })";
    Config cfg;
    TEST_ASSERT_TRUE(parseConfigJson(json, cfg));
    TEST_ASSERT_EQUAL(Algorithm::BITCOIN, cfg.algorithm);
    TEST_ASSERT_EQUAL_STRING("bc1qxxx", cfg.btc_address);
}

void test_defaults_on_missing_fields() {
    const char* json = R"({"wifi_ssid":"x","wifi_pass":"y"})";
    Config cfg;
    TEST_ASSERT_TRUE(parseConfigJson(json, cfg));
    TEST_ASSERT_EQUAL_STRING("public-pool.io", cfg.pool_url);
    TEST_ASSERT_EQUAL(21496, cfg.pool_port);
    TEST_ASSERT_EQUAL(-5, cfg.timezone_offset);
    TEST_ASSERT_EQUAL(Algorithm::DUINOCOIN, cfg.algorithm);
}

void test_invalid_json_returns_false() {
    Config cfg;
    TEST_ASSERT_FALSE(parseConfigJson("{not valid json", cfg));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_duco_config);
    RUN_TEST(test_parse_bitcoin_config);
    RUN_TEST(test_defaults_on_missing_fields);
    RUN_TEST(test_invalid_json_returns_false);
    return UNITY_END();
}
```

- [ ] **Step 4: Run native tests**

```bash
pio test -e native
```

Expected:
```
test/native/test_config/test_config_store.cpp:4 tests ran.
OK
```

- [ ] **Step 5: Initialize LittleFS in `main.cpp`**

Replace `src/main.cpp` setup():

```cpp
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
```

- [ ] **Step 6: Flash and verify serial output**

```bash
pio run -e nerdduino-pro -t upload && pio device monitor
```

Expected serial output (fresh flash, no config file):
```
[boot] NerdDuino Pro starting
[boot] No config found — portal needed
```

- [ ] **Step 7: Commit**

```bash
git add src/config/ test/ src/main.cpp
git commit -m "feat: ConfigStore — LittleFS JSON load/save with native tests"
```

---

### Task 3: WiFi manager with retry + portal trigger flag

**Files:**
- Create: `src/network/WiFiManager.h`
- Create: `src/network/WiFiManager.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create `src/network/WiFiManager.h`**

```cpp
#pragma once
#include "Config.h"

namespace WiFiMgr {
    // Attempt WiFi connection. Returns true on success.
    // On 3rd consecutive failure sets needsPortal = true.
    bool connect(const Config& cfg);

    bool isConnected();
    bool needsPortal();       // true when portal should activate
    void clearPortalFlag();   // call after portal saves new config
}
```

- [ ] **Step 2: Create `src/network/WiFiManager.cpp`**

```cpp
#include "WiFiManager.h"
#include <WiFi.h>

static uint8_t  s_failCount   = 0;
static bool     s_needsPortal = false;

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
        s_failCount = 0;
        Serial.printf("[wifi] Connected — IP %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    s_failCount++;
    Serial.printf("[wifi] Failed (attempt %d/3)\n", s_failCount);
    if (s_failCount >= 3) {
        s_needsPortal = true;
        Serial.println("[wifi] 3 failures — portal required");
    }
    return false;
}

bool WiFiMgr::isConnected() { return WiFi.status() == WL_CONNECTED; }
bool WiFiMgr::needsPortal() { return s_needsPortal; }
void WiFiMgr::clearPortalFlag() { s_needsPortal = false; s_failCount = 0; }
```

- [ ] **Step 3: Wire into `src/main.cpp` setup()**

```cpp
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
        WiFiMgr::clearPortalFlag();
        // Portal will be started in Task 12; placeholder for now
        return;
    }

    WiFiMgr::connect(gConfig);

    if (WiFiMgr::needsPortal()) {
        Serial.println("[boot] WiFi failed — entering portal");
        // Portal start placeholder
    }
}

void loop() { delay(1000); }
```

- [ ] **Step 4: Flash and test with wrong WiFi credentials**

Edit a temporary config: open serial monitor, the output should show:
```
[wifi] Connecting to <ssid> ...
.......
[wifi] Failed (attempt 1/3)
```

(Only 1 attempt on boot. The 3-attempt logic activates across reboots via `s_failCount` which resets on restart — this is intentional; failing once per boot is fine for a personal device.)

> Note: `s_failCount` resets on restart. For the 3-failure trigger to work across boots, persist fail count in NVS or simply trigger portal after 1 failure on first boot. For this personal project, triggering portal immediately on any WiFi failure at boot is acceptable — simplify to: if `!connect()` → needsPortal = true.

- [ ] **Step 5: Simplify — trigger portal on first WiFi failure**

Update `WiFiManager.cpp`:

```cpp
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
```

- [ ] **Step 6: Commit**

```bash
git add src/network/ src/main.cpp
git commit -m "feat: WiFiManager with connection retry and portal trigger"
```

---

## Phase 2 — Mining Core

### Task 4: IMiningAlgorithm interface + MiningStats

**Files:**
- Create: `src/mining/IMiningAlgorithm.h`

- [ ] **Step 1: Create `src/mining/IMiningAlgorithm.h`**

```cpp
#pragma once
#include <stdint.h>

struct MiningStats {
    float    hashrate       = 0.0f;   // kH/s
    uint32_t sharesAccepted = 0;
    uint32_t sharesRejected = 0;
    float    balance        = 0.0f;
    uint32_t uptimeSeconds  = 0;
    uint32_t pingMs         = 0;
    char     poolUrl[64]    = "";
    char     algorithm[16]  = "";     // "DUCO" or "BTC"
};

class IMiningAlgorithm {
public:
    virtual ~IMiningAlgorithm() = default;
    virtual bool connect()            = 0;
    virtual void mine()               = 0;   // called in tight loop
    virtual MiningStats getStats()    = 0;
    virtual void disconnect()         = 0;
};
```

- [ ] **Step 2: Verify compile**

```bash
pio run -e nerdduino-pro
```

Expected: `SUCCESS`

- [ ] **Step 3: Commit**

```bash
git add src/mining/IMiningAlgorithm.h
git commit -m "feat: IMiningAlgorithm interface and MiningStats struct"
```

---

### Task 5: DuinoCoin miner

**Files:**
- Create: `src/mining/DuinoCoinMiner.h`
- Create: `src/mining/DuinoCoinMiner.cpp`

This ports the ChocDuino logic from `MiningJob.h`, `DSHA1.h`, `Counter.h` into the new class structure.

- [ ] **Step 1: Copy original algorithm files as reference**

```bash
# These are read-only references — do NOT include them in the build
cp MiningJob.h src/mining/_ref_MiningJob.h
cp DSHA1.h src/mining/_ref_DSHA1.h
cp Counter.h src/mining/_ref_Counter.h
```

- [ ] **Step 2: Create `src/mining/DuinoCoinMiner.h`**

```cpp
#pragma once
#include "IMiningAlgorithm.h"
#include "Config.h"
#include <WiFiClient.h>

class DuinoCoinMiner : public IMiningAlgorithm {
public:
    explicit DuinoCoinMiner(const Config& cfg);
    ~DuinoCoinMiner() override;

    bool connect()          override;
    void mine()             override;
    MiningStats getStats()  override;
    void disconnect()       override;

private:
    const Config& _cfg;
    WiFiClient    _client;
    MiningStats   _stats;
    uint32_t      _startMs = 0;

    // Internal helpers
    bool _getJob(String& lastHash, String& newHash, uint32_t& diff);
    uint32_t _dsha1(const String& base, const String& target, uint32_t maxDiff);
    bool _submitShare(uint32_t result, uint32_t elapsed);
    void _updateHashrate(uint32_t hashCount, uint32_t elapsedMs);
    bool _resolvePool();  // DNS lookup for server.duinocoin.com pool list
};
```

- [ ] **Step 3: Create `src/mining/DuinoCoinMiner.cpp`**

Port from `_ref_MiningJob.h`. Key sections to migrate:

```cpp
#include "DuinoCoinMiner.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- SHA-1 based DUCO-S1 hash (ported from DSHA1.h) ---
// Keep the original optimized implementation unchanged.
// Paste the full content of _ref_DSHA1.h's hash function here.

DuinoCoinMiner::DuinoCoinMiner(const Config& cfg) : _cfg(cfg) {
    strlcpy(_stats.algorithm, "DUCO", sizeof(_stats.algorithm));
    strlcpy(_stats.poolUrl, "server.duinocoin.com", sizeof(_stats.poolUrl));
}

DuinoCoinMiner::~DuinoCoinMiner() { disconnect(); }

bool DuinoCoinMiner::_resolvePool() {
    HTTPClient http;
    http.begin("https://server.duinocoin.com/getPool");
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    deserializeJson(doc, http.getString());
    http.end();

    const char* ip   = doc["ip"]   | "server.duinocoin.com";
    uint16_t    port = doc["port"] | 2812;

    return _client.connect(ip, port);
}

bool DuinoCoinMiner::connect() {
    _startMs = millis();
    return _resolvePool();
}

void DuinoCoinMiner::disconnect() {
    if (_client.connected()) _client.stop();
}

MiningStats DuinoCoinMiner::getStats() {
    _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    return _stats;
}

// _getJob, _dsha1, _submitShare, mine() — port verbatim from _ref_MiningJob.h
// replacing MiningConfig references with _cfg fields.
// Replace references to DUCO_USER with _cfg.duco_user, etc.
void DuinoCoinMiner::mine() {
    // Port the main mining loop from MiningJob.h loop() function here
    // The logic: getJob → hash → submitShare → repeat
    // On reconnect needed: call connect() again
}
```

> **Port guide:** Open `_ref_MiningJob.h` and find the main loop inside `MiningJob::loop()`. Move that logic here. Replace `MiningConfig.DUCO_USER` → `_cfg.duco_user`, `MiningConfig.MINER_KEY` → `_cfg.duco_key`, `MiningConfig.RIG_IDENTIFIER` → `_cfg.rig_name`. Keep DSHA1 hash function unchanged.

- [ ] **Step 4: Add a mining task stub to `main.cpp`**

```cpp
#include "mining/DuinoCoinMiner.h"
#include "mining/BitcoinMiner.h"   // stub — Task 6

static IMiningAlgorithm* gMiner = nullptr;

static void taskMining(void* param) {
    IMiningAlgorithm* miner = (IMiningAlgorithm*)param;
    if (!miner->connect()) {
        Serial.println("[mining] connect() failed");
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
        miner->mine();
    }
}

// In setup(), after WiFi connects:
void startMining() {
    if (gConfig.algorithm == Algorithm::DUINOCOIN) {
        gMiner = new DuinoCoinMiner(gConfig);
    }
    // Bitcoin case added in Task 6
    if (gMiner) {
        xTaskCreatePinnedToCore(taskMining, "mining", 8192, gMiner, 5, nullptr, 1);
    }
}
```

- [ ] **Step 5: Flash and verify serial shows DUCO mining activity**

```bash
pio run -e nerdduino-pro -t upload && pio device monitor
```

Expected serial lines:
```
[mining] connect() succeeded
[mining] Job received: lastHash=... diff=...
[mining] Share found: result=... in Xms
[mining] Share accepted
```

- [ ] **Step 6: Delete reference files**

```bash
rm src/mining/_ref_MiningJob.h src/mining/_ref_DSHA1.h src/mining/_ref_Counter.h
```

- [ ] **Step 7: Commit**

```bash
git add src/mining/DuinoCoinMiner.h src/mining/DuinoCoinMiner.cpp src/main.cpp
git commit -m "feat: DuinoCoinMiner — DUCO-S1 client ported from ChocDuino"
```

---

### Task 6: Bitcoin miner (NMMiner Stratum port)

**Files:**
- Create: `src/mining/BitcoinMiner.h`
- Create: `src/mining/BitcoinMiner.cpp`

> **Source:** Clone NMMiner locally for reference: `git clone https://github.com/BitMaker-hub/NerdMiner_v2 /tmp/nerdminer-ref`
> Key files: `src/mining/mining.cpp`, `src/mining/utils/crypto/sha256.h`.
> License: GPL-3.0 — attribution required.

- [ ] **Step 1: Create `src/mining/BitcoinMiner.h`**

```cpp
#pragma once
#include "IMiningAlgorithm.h"
#include "Config.h"
#include <WiFiClient.h>

class BitcoinMiner : public IMiningAlgorithm {
public:
    explicit BitcoinMiner(const Config& cfg);
    ~BitcoinMiner() override;

    bool connect()         override;
    void mine()            override;
    MiningStats getStats() override;
    void disconnect()      override;

private:
    const Config& _cfg;
    WiFiClient    _client;
    MiningStats   _stats;
    uint32_t      _startMs        = 0;
    uint32_t      _jobNonce       = 0;
    char          _jobId[64]      = "";
    char          _prevHash[128]  = "";
    char          _coinb1[256]    = "";
    char          _coinb2[256]    = "";
    char          _version[16]    = "";
    char          _nbits[16]      = "";
    char          _ntime[16]      = "";
    char          _extranonce1[32]= "";
    int           _extranonce2Size= 4;

    bool _sendSubscribe();
    bool _sendAuthorize();
    bool _parseJob(const String& line);
    bool _submitShare(uint32_t nonce);
    void _doubleSha256(const uint8_t* data, size_t len, uint8_t* out);
    bool _checkHash(const uint8_t* hash, const char* target);
};
```

- [ ] **Step 2: Create `src/mining/BitcoinMiner.cpp`**

Port Stratum V1 logic from NMMiner. Key flow:

```cpp
#include "BitcoinMiner.h"
#include <Arduino.h>
#include "mbedtls/sha256.h"   // included in ESP-IDF, no extra dep needed

// SHA-256d (double SHA-256) using mbedTLS
void BitcoinMiner::_doubleSha256(const uint8_t* data, size_t len, uint8_t* out) {
    uint8_t tmp[32];
    mbedtls_sha256(data, len, tmp, 0);
    mbedtls_sha256(tmp, 32, out, 0);
}

BitcoinMiner::BitcoinMiner(const Config& cfg) : _cfg(cfg) {
    strlcpy(_stats.algorithm, "BTC", sizeof(_stats.algorithm));
    snprintf(_stats.poolUrl, sizeof(_stats.poolUrl), "%s:%d", cfg.pool_url, cfg.pool_port);
}

BitcoinMiner::~BitcoinMiner() { disconnect(); }

bool BitcoinMiner::connect() {
    _startMs = millis();
    Serial.printf("[btc] Connecting to %s:%d\n", _cfg.pool_url, _cfg.pool_port);
    if (!_client.connect(_cfg.pool_url, _cfg.pool_port)) {
        Serial.println("[btc] TCP connect failed");
        return false;
    }
    return _sendSubscribe() && _sendAuthorize();
}

bool BitcoinMiner::_sendSubscribe() {
    // Stratum: mining.subscribe
    String req = "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"NerdDuinoPro/1.0\"]}\n";
    _client.print(req);
    // Read response: {"id":1,"result":[[...],extranonce1,extranonce2_size],"error":null}
    String resp = _client.readStringUntil('\n');
    // Parse extranonce1 and extranonce2Size from resp
    // (port parsing from NMMiner src/mining/mining.cpp stratum_subscribe())
    return resp.length() > 0;
}

bool BitcoinMiner::_sendAuthorize() {
    // Stratum: mining.authorize
    // worker = btc_address.rig_name
    char worker[128];
    snprintf(worker, sizeof(worker), "%s.%s", _cfg.btc_address, _cfg.rig_name);
    String req = "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"";
    req += worker;
    req += "\",\"\"]}\n";
    _client.print(req);
    String resp = _client.readStringUntil('\n');
    return resp.indexOf("true") >= 0;
}

void BitcoinMiner::mine() {
    // 1. Read any incoming lines from pool (new job or response)
    while (_client.available()) {
        String line = _client.readStringUntil('\n');
        line.trim();
        if (line.indexOf("mining.notify") >= 0) _parseJob(line);
    }

    // 2. Hash loop — try next nonce batch
    // (port the SHA-256 mining loop from NMMiner, using _doubleSha256)
    // Increment _jobNonce, build block header, hash, check difficulty
    // If share found: _submitShare(nonce)
    // Update _stats.hashrate every 5 seconds
}

bool BitcoinMiner::_submitShare(uint32_t nonce) {
    // Stratum: mining.submit
    char msg[256];
    snprintf(msg, sizeof(msg),
        "{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"%s.%s\",\"%s\",\"%08x\",\"%s\",\"%08lx\"]}\n",
        _cfg.btc_address, _cfg.rig_name, _jobId, 0, _ntime, (unsigned long)nonce);
    _client.print(msg);
    _stats.sharesAccepted++;
    return true;
}

MiningStats BitcoinMiner::getStats() {
    _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    return _stats;
}

void BitcoinMiner::disconnect() {
    if (_client.connected()) _client.stop();
}
```

- [ ] **Step 3: Wire Bitcoin case into `main.cpp` startMining()**

```cpp
void startMining() {
    if (gConfig.algorithm == Algorithm::DUINOCOIN) {
        gMiner = new DuinoCoinMiner(gConfig);
    } else {
        gMiner = new BitcoinMiner(gConfig);
    }
    if (gMiner) {
        xTaskCreatePinnedToCore(taskMining, "mining", 8192, gMiner, 5, nullptr, 1);
    }
}
```

- [ ] **Step 4: Test Bitcoin mode — set config manually via serial**

Temporarily hard-code a test config in `main.cpp` setup() to verify BTC connects:

```cpp
// TEMP: manual test config (remove after verification)
strlcpy(gConfig.btc_address, "YOUR_BTC_ADDRESS", sizeof(gConfig.btc_address));
strlcpy(gConfig.pool_url, "public-pool.io", sizeof(gConfig.pool_url));
gConfig.pool_port = 21496;
gConfig.algorithm = Algorithm::BITCOIN;
gConfig.valid = true;
```

Expected serial:
```
[btc] Connecting to public-pool.io:21496
[btc] Subscribe OK
[btc] Authorize OK
[btc] Job received
```

- [ ] **Step 5: Remove temp config, commit**

```bash
git add src/mining/BitcoinMiner.h src/mining/BitcoinMiner.cpp src/main.cpp
git commit -m "feat: BitcoinMiner — Stratum V1 SHA-256 client (GPL-3.0, NMMiner port)"
```

---

## Phase 3 — LVGL UI

### Task 7: LVGL init + UIManager skeleton

**Files:**
- Create: `include/lv_conf.h`
- Create: `src/ui/UIManager.h`
- Create: `src/ui/UIManager.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create `include/lv_conf.h`**

```c
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH          16
#define LV_MEM_SIZE             (48 * 1024)
#define LV_MEM_POOL_INCLUDE     <stdlib.h>
#define LV_MEM_POOL_ALLOC       malloc
#define LV_MEM_POOL_FREE        free

/* Display */
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN

/* Fonts */
#define LV_FONT_DEFAULT         &lv_font_montserrat_14
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_36   1

/* Widgets */
#define LV_USE_ARC              1
#define LV_USE_CHART            1
#define LV_USE_LABEL            1
#define LV_USE_BTN              1
#define LV_USE_IMG              0
#define LV_USE_LINE             1
#define LV_USE_SPINNER          0
#define LV_USE_BAR              1

/* Animation */
#define LV_USE_ANIM             1

#endif /* LV_CONF_H */
```

- [ ] **Step 2: Create `src/ui/UIManager.h`**

```cpp
#pragma once
#include <lvgl.h>
#include "mining/IMiningAlgorithm.h"

namespace UIManager {
    void init();                              // call once in setup()
    void tick();                              // call from UI task loop
    void update(const MiningStats& stats);    // push new stats to active screen
    void setTargetFps(uint8_t fps);           // called by mining task (15 BTC, 25 DUCO)

    // Internal: called by touch ISR equivalent (polled)
    void handleTouch(int16_t x, int16_t y, bool pressed);
}
```

- [ ] **Step 3: Create `src/ui/UIManager.cpp`**

```cpp
#include "UIManager.h"
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "DashboardScreen.h"
#include "ClockScreen.h"

static TFT_eSPI        s_tft;
static lv_display_t*   s_disp      = nullptr;
static uint8_t         s_curView   = 0;      // 0 = Dashboard, 1 = Clock
static uint8_t         s_targetFps = 25;
static uint32_t        s_lastFlush = 0;

// Partial buffer: 1/10 of screen height = 32 rows × 320 px × 2 bytes = 20KB
static const int BUF_ROWS = 32;
static lv_color_t s_lvBuf[320 * BUF_ROWS];

static void flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1,
                        area->x2 - area->x1 + 1,
                        area->y2 - area->y1 + 1);
    s_tft.pushColors((uint16_t*)px_map,
                     (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1),
                     true);
    s_tft.endWrite();
    lv_display_flush_ready(disp);
}

void UIManager::init() {
    s_tft.init();
    s_tft.setRotation(1);           // landscape: 320×240
    s_tft.fillScreen(TFT_BLACK);

    lv_init();

    s_disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(s_disp, flushCallback);
    lv_display_set_buffers(s_disp, s_lvBuf, nullptr,
                           sizeof(s_lvBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    DashboardScreen::create();
    ClockScreen::create();
    DashboardScreen::load();  // boot on dashboard
}

void UIManager::tick() {
    uint32_t now = millis();
    uint32_t interval = 1000 / s_targetFps;
    if (now - s_lastFlush >= interval) {
        lv_timer_handler();
        s_lastFlush = now;
    }
}

void UIManager::setTargetFps(uint8_t fps) { s_targetFps = fps; }

void UIManager::update(const MiningStats& stats) {
    DashboardScreen::update(stats);
    ClockScreen::update(stats);
}

// Touch zone detection: left 10% or right 10% of 320px = x < 32 or x > 288
static uint32_t s_pressStart = 0;
static bool     s_pressing   = false;

void UIManager::handleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed && !s_pressing) {
        s_pressing   = true;
        s_pressStart = millis();
    }

    if (!pressed && s_pressing) {
        s_pressing = false;
        uint32_t duration = millis() - s_pressStart;

        if (duration >= 3000) {
            // Long press → signal portal activation (set a global flag)
            extern volatile bool gPortalRequested;
            gPortalRequested = true;
            return;
        }

        if (duration < 500) {
            if (x < 32) {
                // Left zone → previous view
                s_curView = (s_curView == 0) ? 1 : s_curView - 1;
            } else if (x > 288) {
                // Right zone → next view
                s_curView = (s_curView + 1) % 2;
            }
            if (s_curView == 0) DashboardScreen::load();
            else                ClockScreen::load();
        }
    }
}
```

- [ ] **Step 4: Add UI task and touch polling to `main.cpp`**

```cpp
#include "ui/UIManager.h"
#include <XPT2046_Touchscreen.h>   // add to lib_deps: paulstoffregen/XPT2046_Touchscreen

// CYD touch pins
#define TOUCH_CS  33
#define TOUCH_IRQ 36

volatile bool gPortalRequested = false;
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);

static void taskUI(void* param) {
    for (;;) {
        if (s_touch.tirqTouched() && s_touch.touched()) {
            TS_Point p = s_touch.getPoint();
            // Map raw touch coords (0–4095) to display coords (0–319, 0–239)
            int16_t sx = map(p.x, 200, 3900, 0, 319);
            int16_t sy = map(p.y, 200, 3900, 0, 239);
            UIManager::handleTouch(sx, sy, true);
        } else {
            UIManager::handleTouch(0, 0, false);
        }
        UIManager::tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// In setup():
void startUI() {
    UIManager::init();
    s_touch.begin();
    s_touch.setRotation(1);
    xTaskCreatePinnedToCore(taskUI, "ui", 8192, nullptr, 2, nullptr, 0);
}
```

Add `paulstoffregen/XPT2046_Touchscreen@^1.4` to `lib_deps` in `platformio.ini`.

- [ ] **Step 5: Flash — verify blank LVGL screen appears (black + no crash)**

```bash
pio run -e nerdduino-pro -t upload && pio device monitor
```

Expected: display lights up, no serial panics, LVGL initialized.

- [ ] **Step 6: Commit**

```bash
git add include/lv_conf.h src/ui/UIManager.h src/ui/UIManager.cpp src/main.cpp platformio.ini
git commit -m "feat: UIManager — LVGL init with partial buffer and touch zone detection"
```

---

### Task 8: DashboardScreen (View 1)

**Files:**
- Create: `src/ui/DashboardScreen.h`
- Create: `src/ui/DashboardScreen.cpp`

- [ ] **Step 1: Create `src/ui/DashboardScreen.h`**

```cpp
#pragma once
#include "mining/IMiningAlgorithm.h"

namespace DashboardScreen {
    void create();                         // build LVGL objects (call once)
    void load();                           // lv_scr_load() this screen
    void update(const MiningStats& stats); // refresh all labels and chart
}
```

- [ ] **Step 2: Create `src/ui/DashboardScreen.cpp`**

```cpp
#include "DashboardScreen.h"
#include <lvgl.h>

// Colors
#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_AMBER   lv_color_hex(0xfbbf24)
#define COL_BLUE    lv_color_hex(0x60a5fa)
#define COL_SUBTLE  lv_color_hex(0x1a2035)

static lv_obj_t* s_scr         = nullptr;
static lv_obj_t* s_gaugeArc    = nullptr;
static lv_obj_t* s_lblHashrate = nullptr;
static lv_obj_t* s_lblShares   = nullptr;
static lv_obj_t* s_lblReject   = nullptr;
static lv_obj_t* s_lblBalance  = nullptr;
static lv_obj_t* s_lblUptime   = nullptr;
static lv_obj_t* s_chart       = nullptr;
static lv_chart_series_t* s_series = nullptr;
static lv_obj_t* s_lblPool     = nullptr;
static float     s_maxHashrate = 1.0f;

void DashboardScreen::create() {
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);

    // --- Topbar ---
    lv_obj_t* topbar = lv_label_create(s_scr);
    lv_label_set_text(topbar, LV_SYMBOL_WIFI " NerdDuino Pro");
    lv_obj_set_style_text_color(topbar, COL_ORANGE, 0);
    lv_obj_set_style_text_font(topbar, &lv_font_montserrat_14, 0);
    lv_obj_align(topbar, LV_ALIGN_TOP_LEFT, 8, 6);

    // --- Gauge arc (left side, 130×130) ---
    s_gaugeArc = lv_arc_create(s_scr);
    lv_obj_set_size(s_gaugeArc, 130, 130);
    lv_obj_align(s_gaugeArc, LV_ALIGN_TOP_LEFT, 8, 26);
    lv_arc_set_rotation(s_gaugeArc, 135);
    lv_arc_set_bg_angles(s_gaugeArc, 0, 270);
    lv_arc_set_value(s_gaugeArc, 0);
    lv_obj_set_style_arc_color(s_gaugeArc, COL_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_gaugeArc, COL_SUBTLE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_gaugeArc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_gaugeArc, 10, LV_PART_MAIN);
    lv_arc_set_mode(s_gaugeArc, LV_ARC_MODE_NORMAL);
    lv_obj_remove_flag(s_gaugeArc, LV_OBJ_FLAG_CLICKABLE);

    // Hashrate label inside arc
    s_lblHashrate = lv_label_create(s_scr);
    lv_label_set_text(s_lblHashrate, "0\nkH/s");
    lv_obj_set_style_text_color(s_lblHashrate, COL_ORANGE, 0);
    lv_obj_set_style_text_font(s_lblHashrate, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(s_lblHashrate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lblHashrate, LV_ALIGN_TOP_LEFT, 27, 66);

    // --- Stat cards (right side 2×2 grid) ---
    // Shares accepted
    lv_obj_t* c1 = lv_obj_create(s_scr);
    lv_obj_set_size(c1, 84, 44);
    lv_obj_align(c1, LV_ALIGN_TOP_RIGHT, -8, 26);
    lv_obj_set_style_bg_color(c1, COL_SUBTLE, 0);
    lv_obj_set_style_border_width(c1, 0, 0);
    lv_obj_set_style_pad_all(c1, 4, 0);

    lv_obj_t* l1t = lv_label_create(c1);
    lv_label_set_text(l1t, "Shares OK");
    lv_obj_set_style_text_color(l1t, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(l1t, &lv_font_montserrat_14, 0);
    lv_obj_align(l1t, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lblShares = lv_label_create(c1);
    lv_label_set_text(s_lblShares, "0");
    lv_obj_set_style_text_color(s_lblShares, COL_GREEN, 0);
    lv_obj_set_style_text_font(s_lblShares, &lv_font_montserrat_18, 0);
    lv_obj_align(s_lblShares, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Rejected
    lv_obj_t* c2 = lv_obj_create(s_scr);
    lv_obj_set_size(c2, 84, 44);
    lv_obj_align(c2, LV_ALIGN_TOP_RIGHT, -8, 76);
    lv_obj_set_style_bg_color(c2, COL_SUBTLE, 0);
    lv_obj_set_style_border_width(c2, 0, 0);
    lv_obj_set_style_pad_all(c2, 4, 0);

    lv_obj_t* l2t = lv_label_create(c2);
    lv_label_set_text(l2t, "Rejected");
    lv_obj_set_style_text_color(l2t, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(l2t, &lv_font_montserrat_14, 0);
    lv_obj_align(l2t, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lblReject = lv_label_create(c2);
    lv_label_set_text(s_lblReject, "0");
    lv_obj_set_style_text_color(s_lblReject, COL_AMBER, 0);
    lv_obj_set_style_text_font(s_lblReject, &lv_font_montserrat_18, 0);
    lv_obj_align(s_lblReject, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Balance
    lv_obj_t* c3 = lv_obj_create(s_scr);
    lv_obj_set_size(c3, 84, 44);
    lv_obj_align(c3, LV_ALIGN_TOP_RIGHT, -100, 26);
    lv_obj_set_style_bg_color(c3, COL_SUBTLE, 0);
    lv_obj_set_style_border_width(c3, 0, 0);
    lv_obj_set_style_pad_all(c3, 4, 0);

    lv_obj_t* l3t = lv_label_create(c3);
    lv_label_set_text(l3t, "Balance");
    lv_obj_set_style_text_color(l3t, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(l3t, &lv_font_montserrat_14, 0);
    lv_obj_align(l3t, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lblBalance = lv_label_create(c3);
    lv_label_set_text(s_lblBalance, "0.00");
    lv_obj_set_style_text_color(s_lblBalance, COL_ORANGE, 0);
    lv_obj_set_style_text_font(s_lblBalance, &lv_font_montserrat_18, 0);
    lv_obj_align(s_lblBalance, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Uptime
    lv_obj_t* c4 = lv_obj_create(s_scr);
    lv_obj_set_size(c4, 84, 44);
    lv_obj_align(c4, LV_ALIGN_TOP_RIGHT, -100, 76);
    lv_obj_set_style_bg_color(c4, COL_SUBTLE, 0);
    lv_obj_set_style_border_width(c4, 0, 0);
    lv_obj_set_style_pad_all(c4, 4, 0);

    lv_obj_t* l4t = lv_label_create(c4);
    lv_label_set_text(l4t, "Uptime");
    lv_obj_set_style_text_color(l4t, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(l4t, &lv_font_montserrat_14, 0);
    lv_obj_align(l4t, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lblUptime = lv_label_create(c4);
    lv_label_set_text(s_lblUptime, "0d 0h");
    lv_obj_set_style_text_color(s_lblUptime, COL_BLUE, 0);
    lv_obj_set_style_text_font(s_lblUptime, &lv_font_montserrat_18, 0);
    lv_obj_align(s_lblUptime, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // --- Sparkline chart (bottom, full width) ---
    s_chart = lv_chart_create(s_scr);
    lv_obj_set_size(s_chart, 304, 44);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(s_chart, 60);
    lv_chart_set_div_line_count(s_chart, 0, 0);
    lv_obj_set_style_bg_color(s_chart, COL_BG, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);

    s_series = lv_chart_add_series(s_chart, COL_ORANGE, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);

    // Chart label
    s_lblPool = lv_label_create(s_scr);
    lv_label_set_text(s_lblPool, "— 60s —");
    lv_obj_set_style_text_color(s_lblPool, lv_color_hex(0x3a4568), 0);
    lv_obj_set_style_text_font(s_lblPool, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblPool, LV_ALIGN_BOTTOM_MID, 0, -2);

    // Nav dots
    lv_obj_t* dot1 = lv_obj_create(s_scr);
    lv_obj_set_size(dot1, 14, 5);
    lv_obj_set_style_bg_color(dot1, COL_ORANGE, 0);
    lv_obj_set_style_radius(dot1, 3, 0);
    lv_obj_set_style_border_width(dot1, 0, 0);
    lv_obj_align(dot1, LV_ALIGN_BOTTOM_MID, -12, -2);

    lv_obj_t* dot2 = lv_obj_create(s_scr);
    lv_obj_set_size(dot2, 5, 5);
    lv_obj_set_style_bg_color(dot2, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(dot2, 3, 0);
    lv_obj_set_style_border_width(dot2, 0, 0);
    lv_obj_align(dot2, LV_ALIGN_BOTTOM_MID, 6, -2);
}

void DashboardScreen::load() {
    lv_scr_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void DashboardScreen::update(const MiningStats& stats) {
    if (!s_scr) return;

    // Update hashrate gauge
    if (stats.hashrate > s_maxHashrate) s_maxHashrate = stats.hashrate;
    int32_t gaugeVal = (s_maxHashrate > 0)
        ? (int32_t)(stats.hashrate / s_maxHashrate * 100)
        : 0;
    lv_arc_set_value(s_gaugeArc, gaugeVal);

    // Hashrate label
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f\nkH/s", stats.hashrate);
    lv_label_set_text(s_lblHashrate, buf);

    // Stats
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.sharesAccepted);
    lv_label_set_text(s_lblShares, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.sharesRejected);
    lv_label_set_text(s_lblReject, buf);

    snprintf(buf, sizeof(buf), "%.2f", stats.balance);
    lv_label_set_text(s_lblBalance, buf);

    uint32_t d = stats.uptimeSeconds / 86400;
    uint32_t h = (stats.uptimeSeconds % 86400) / 3600;
    snprintf(buf, sizeof(buf), "%lud %luh", (unsigned long)d, (unsigned long)h);
    lv_label_set_text(s_lblUptime, buf);

    // Sparkline — push new datapoint
    lv_chart_set_next_value(s_chart, s_series, (int32_t)stats.hashrate);
    lv_chart_refresh(s_chart);
}
```

- [ ] **Step 3: Flash and verify dashboard renders with live stats**

```bash
pio run -e nerdduino-pro -t upload && pio device monitor
```

Expected: cockpit dashboard visible on display with stats updating as mining runs.

- [ ] **Step 4: Commit**

```bash
git add src/ui/DashboardScreen.h src/ui/DashboardScreen.cpp
git commit -m "feat: DashboardScreen — cockpit gauge, stats cards, 60s sparkline"
```

---

### Task 9: ClockScreen (View 2)

**Files:**
- Create: `src/ui/ClockScreen.h`
- Create: `src/ui/ClockScreen.cpp`
- Modify: `src/main.cpp` (add NTP sync)

- [ ] **Step 1: Create `src/ui/ClockScreen.h`**

```cpp
#pragma once
#include "mining/IMiningAlgorithm.h"

namespace ClockScreen {
    void create();
    void load();
    void update(const MiningStats& stats);
}
```

- [ ] **Step 2: Create `src/ui/ClockScreen.cpp`**

```cpp
#include "ClockScreen.h"
#include <lvgl.h>
#include <time.h>

#define COL_BG      lv_color_hex(0x080c14)
#define COL_ORANGE  lv_color_hex(0xff6b35)
#define COL_GREEN   lv_color_hex(0x4ade80)
#define COL_SUBTLE  lv_color_hex(0x1a2035)

static lv_obj_t* s_scr        = nullptr;
static lv_obj_t* s_lblTime    = nullptr;
static lv_obj_t* s_lblDate    = nullptr;
static lv_obj_t* s_lblHash    = nullptr;
static lv_obj_t* s_lblPool    = nullptr;
static lv_obj_t* s_lblPing    = nullptr;
static lv_obj_t* s_lblShares  = nullptr;

void ClockScreen::create() {
    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);

    // Topbar
    lv_obj_t* top = lv_label_create(s_scr);
    lv_label_set_text(top, LV_SYMBOL_WIFI "  NerdDuino Pro");
    lv_obj_set_style_text_color(top, COL_ORANGE, 0);
    lv_obj_set_style_text_font(top, &lv_font_montserrat_14, 0);
    lv_obj_align(top, LV_ALIGN_TOP_LEFT, 8, 6);

    // Large clock
    s_lblTime = lv_label_create(s_scr);
    lv_label_set_text(s_lblTime, "00:00");
    lv_obj_set_style_text_color(s_lblTime, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(s_lblTime, &lv_font_montserrat_36, 0);
    lv_obj_align(s_lblTime, LV_ALIGN_CENTER, 0, -30);

    // Date
    s_lblDate = lv_label_create(s_scr);
    lv_label_set_text(s_lblDate, "");
    lv_obj_set_style_text_color(s_lblDate, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(s_lblDate, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblDate, LV_ALIGN_CENTER, 0, 10);

    // Info strip container
    lv_obj_t* strip = lv_obj_create(s_scr);
    lv_obj_set_size(strip, 304, 56);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_bg_color(strip, COL_SUBTLE, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, 8, 0);
    lv_obj_set_layout(strip, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Hashrate cell
    lv_obj_t* cellHash = lv_obj_create(strip);
    lv_obj_set_style_bg_opa(cellHash, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cellHash, 0, 0);
    lv_obj_set_size(cellHash, 90, 50);

    s_lblHash = lv_label_create(cellHash);
    lv_label_set_text(s_lblHash, "0 kH/s");
    lv_obj_set_style_text_color(s_lblHash, COL_ORANGE, 0);
    lv_obj_set_style_text_font(s_lblHash, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblHash, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t* lHash = lv_label_create(cellHash);
    lv_label_set_text(lHash, "Hashrate");
    lv_obj_set_style_text_color(lHash, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(lHash, &lv_font_montserrat_14, 0);
    lv_obj_align(lHash, LV_ALIGN_CENTER, 0, 10);

    // Pool cell
    lv_obj_t* cellPool = lv_obj_create(strip);
    lv_obj_set_style_bg_opa(cellPool, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cellPool, 0, 0);
    lv_obj_set_size(cellPool, 110, 50);

    s_lblPool = lv_label_create(cellPool);
    lv_label_set_text(s_lblPool, "pool...");
    lv_obj_set_style_text_color(s_lblPool, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_text_font(s_lblPool, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblPool, LV_ALIGN_CENTER, 0, -8);

    s_lblPing = lv_label_create(cellPool);
    lv_label_set_text(s_lblPing, "-- ms");
    lv_obj_set_style_text_color(s_lblPing, COL_GREEN, 0);
    lv_obj_set_style_text_font(s_lblPing, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lblPing, LV_ALIGN_CENTER, 0, 8);

    // Shares cell
    lv_obj_t* cellShares = lv_obj_create(strip);
    lv_obj_set_style_bg_opa(cellShares, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cellShares, 0, 0);
    lv_obj_set_size(cellShares, 80, 50);

    s_lblShares = lv_label_create(cellShares);
    lv_label_set_text(s_lblShares, "0");
    lv_obj_set_style_text_color(s_lblShares, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_text_font(s_lblShares, &lv_font_montserrat_18, 0);
    lv_obj_align(s_lblShares, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t* lShares = lv_label_create(cellShares);
    lv_label_set_text(lShares, "Shares");
    lv_obj_set_style_text_color(lShares, lv_color_hex(0x4a5568), 0);
    lv_obj_set_style_text_font(lShares, &lv_font_montserrat_14, 0);
    lv_obj_align(lShares, LV_ALIGN_CENTER, 0, 10);

    // Nav dots
    lv_obj_t* dot1 = lv_obj_create(s_scr);
    lv_obj_set_size(dot1, 5, 5);
    lv_obj_set_style_bg_color(dot1, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(dot1, 3, 0);
    lv_obj_set_style_border_width(dot1, 0, 0);
    lv_obj_align(dot1, LV_ALIGN_BOTTOM_MID, -12, -2);

    lv_obj_t* dot2 = lv_obj_create(s_scr);
    lv_obj_set_size(dot2, 14, 5);
    lv_obj_set_style_bg_color(dot2, COL_ORANGE, 0);
    lv_obj_set_style_radius(dot2, 3, 0);
    lv_obj_set_style_border_width(dot2, 0, 0);
    lv_obj_align(dot2, LV_ALIGN_BOTTOM_MID, 6, -2);
}

void ClockScreen::load() {
    lv_scr_load_anim(s_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

static const char* DAYS[]   = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
static const char* MONTHS[] = {"Ene","Feb","Mar","Abr","May","Jun",
                                "Jul","Ago","Sep","Oct","Nov","Dic"};

void ClockScreen::update(const MiningStats& stats) {
    if (!s_scr) return;

    // Update clock via NTP
    struct tm t;
    if (getLocalTime(&t)) {
        char timeBuf[8];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
        lv_label_set_text(s_lblTime, timeBuf);

        char dateBuf[32];
        snprintf(dateBuf, sizeof(dateBuf), "%s · %d %s %d",
                 DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon], 1900 + t.tm_year);
        lv_label_set_text(s_lblDate, dateBuf);
    }

    // Stats
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f kH/s", stats.hashrate);
    lv_label_set_text(s_lblHash, buf);

    lv_label_set_text(s_lblPool, stats.poolUrl);

    snprintf(buf, sizeof(buf), "%lu ms", (unsigned long)stats.pingMs);
    lv_label_set_text(s_lblPing, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.sharesAccepted);
    lv_label_set_text(s_lblShares, buf);
}
```

- [ ] **Step 3: Add NTP sync to `main.cpp` setup()**

After WiFi connects:

```cpp
// After WiFiMgr::connect() succeeds:
configTime(gConfig.timezone_offset * 3600, 0,
           "pool.ntp.org", "time.nist.gov");
Serial.println("[ntp] Sync started");
```

- [ ] **Step 4: Flash and test view switching**

Tap right edge of screen → should switch to ClockScreen with time + info strip.
Tap left edge → back to DashboardScreen.
Hold 3s → serial should print `[portal] requested` (flag set).

- [ ] **Step 5: Commit**

```bash
git add src/ui/ClockScreen.h src/ui/ClockScreen.cpp src/main.cpp
git commit -m "feat: ClockScreen — NTP clock, info strip, view navigation"
```

---

## Phase 4 — Config Portal + OTA

### Task 10: Config portal (AP mode + web form)

**Files:**
- Create: `src/portal/ConfigPortal.h`
- Create: `src/portal/ConfigPortal.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create `src/portal/ConfigPortal.h`**

```cpp
#pragma once

namespace ConfigPortal {
    void start();    // enter AP mode and start web server
    void handle();   // call in loop while in portal mode
    void stop();     // disconnect AP, restart
    bool isDone();   // true after config saved → caller should restart
}
```

- [ ] **Step 2: Create `src/portal/ConfigPortal.cpp`**

```cpp
#include "ConfigPortal.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config/ConfigStore.h"
#include "Config.h"

static AsyncWebServer s_server(80);
static bool s_done = false;

// HTML stored in flash — not LittleFS
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
  <h1 class="text-2xl font-bold accent mb-1">⬢ NerdDuino Pro</h1>
  <p class="text-gray-400 text-sm mb-6">Configuración inicial</p>
  <form id="form">
    <p class="text-xs uppercase tracking-widest text-gray-500 mb-2">WiFi</p>
    <div class="flex gap-2 mb-2">
      <input id="ssid" name="wifi_ssid" placeholder="Red WiFi" class="flex-1 rounded p-2 text-sm" required>
      <button type="button" onclick="scanWifi()" class="px-3 py-2 text-xs rounded border border-gray-600 text-gray-300">Buscar</button>
    </div>
    <input id="pass" name="wifi_pass" type="password" placeholder="Contraseña" class="w-full rounded p-2 text-sm mb-4">

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
      <input name="btc_address" placeholder="Dirección Bitcoin (bc1q...)" class="w-full rounded p-2 text-sm mb-2">
      <input name="pool_url"    placeholder="Pool URL" value="public-pool.io" class="w-full rounded p-2 text-sm mb-2">
      <input name="pool_port"   placeholder="Puerto" value="21496" type="number" class="w-full rounded p-2 text-sm">
    </div>

    <p class="text-xs uppercase tracking-widest text-gray-500 mb-2">Opcional</p>
    <input name="rig_name" placeholder="Nombre del rig" value="NerdDuino-1" class="w-full rounded p-2 text-sm mb-2">
    <select name="timezone_offset" class="w-full rounded p-2 text-sm mb-6">
      <option value="-5" selected>UTC-5 (Colombia/Ecuador/Perú)</option>
      <option value="-4">UTC-4 (Venezuela/Bolivia)</option>
      <option value="-3">UTC-3 (Argentina/Brasil)</option>
      <option value="-6">UTC-6 (México)</option>
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
      m.textContent=d.ok?'✓ Guardado — reiniciando...':'Error: '+d.error;
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
    Serial.printf("[portal] AP started — connect to NerdDuino-Setup, open 192.168.4.1\n");

    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", PORTAL_HTML);
    });

    s_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n && i < 10; i++) {
            if (i) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
        }
        json += "]";
        req->send(200, "application/json", json);
    });

    s_server.on("/save", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"JSON inválido\"}");
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
            cfg.timezone_offset = doc["timezone_offset"] | -5;
            cfg.algorithm = (int)(doc["algorithm"] | 0) == 1 ? Algorithm::BITCOIN : Algorithm::DUINOCOIN;

            if (cfg.wifi_ssid[0] == '\0') {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"SSID requerido\"}");
                return;
            }

            ConfigStore::save(cfg);
            req->send(200, "application/json", "{\"ok\":true}");
            s_done = true;
        }
    );

    s_server.begin();
}

void ConfigPortal::handle() {
    if (s_done) {
        delay(1000);
        ESP.restart();
    }
}

void ConfigPortal::stop() { s_server.end(); }

bool ConfigPortal::isDone() { return s_done; }
```

- [ ] **Step 3: Wire portal into `main.cpp`**

```cpp
#include "portal/ConfigPortal.h"

extern volatile bool gPortalRequested;
static bool gInPortalMode = false;

void setup() {
    // ... (existing init) ...

    bool hasConfig = ConfigStore::load(gConfig);
    bool wifiOk = hasConfig && WiFiMgr::connect(gConfig);

    if (!hasConfig || WiFiMgr::needsPortal()) {
        gInPortalMode = true;
        ConfigPortal::start();
        return;  // skip mining/UI start
    }

    // NTP after WiFi
    configTime(gConfig.timezone_offset * 3600, 0, "pool.ntp.org");

    startUI();
    startMining();
}

void loop() {
    if (gInPortalMode) {
        ConfigPortal::handle();
        return;
    }
    if (gPortalRequested) {
        gPortalRequested = false;
        gInPortalMode = true;
        ConfigPortal::start();
        return;
    }
    // Push mining stats to UI every second
    static uint32_t lastUpdate = 0;
    if (gMiner && millis() - lastUpdate > 1000) {
        UIManager::update(gMiner->getStats());
        lastUpdate = millis();
    }
}
```

- [ ] **Step 4: Test portal end-to-end**

1. Erase flash: `pio run -t erase`
2. Flash firmware: `pio run -e nerdduino-pro -t upload`
3. Connect phone/PC to WiFi `NerdDuino-Setup`
4. Open `http://192.168.4.1`
5. Fill in form, submit
6. Verify device restarts and connects to your WiFi

Expected serial after save:
```
[portal] Config saved — restarting
[boot] Config loaded — algo=0 wifi=YourSSID
[wifi] Connected — IP 192.168.x.x
[mining] connect() succeeded
```

- [ ] **Step 5: Commit**

```bash
git add src/portal/ConfigPortal.h src/portal/ConfigPortal.cpp src/main.cpp
git commit -m "feat: ConfigPortal — AP mode web config with PROGMEM HTML"
```

---

### Task 11: OTA handler

**Files:**
- Create: `src/portal/OTAHandler.h`
- Create: `src/portal/OTAHandler.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create `src/portal/OTAHandler.h`**

```cpp
#pragma once

namespace OTAHandler {
    void init(const char* hostname);  // call after WiFi connects
    void handle();                    // call from loop() (Core 0 task)
}
```

- [ ] **Step 2: Create `src/portal/OTAHandler.cpp`**

```cpp
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
```

- [ ] **Step 3: Add OTA to `main.cpp` and add to UI task**

After WiFi connects in `setup()`:
```cpp
OTAHandler::init(gConfig.rig_name);
```

In `taskUI` loop, add before `UIManager::tick()`:
```cpp
OTAHandler::handle();
```

- [ ] **Step 4: Test OTA upload**

```bash
# Find device IP from serial monitor, then:
pio run -e nerdduino-pro -t upload --upload-port 192.168.x.x
```

Expected: firmware uploads wirelessly, device restarts, mining resumes.

- [ ] **Step 5: Commit**

```bash
git add src/portal/OTAHandler.h src/portal/OTAHandler.cpp src/main.cpp
git commit -m "feat: OTAHandler — ArduinoOTA wireless firmware update"
```

---

## Phase 5 — Integration & Verification

### Task 12: End-to-end smoke test

- [ ] **Step 1: Full erase and fresh flash**

```bash
pio run -t erase && pio run -e nerdduino-pro -t upload
```

- [ ] **Step 2: Verify DuinoCoin path**

1. Connect to `NerdDuino-Setup` AP
2. Open `http://192.168.4.1`
3. Configure: your WiFi, algorithm=DuinoCoin, duco_user=your username
4. Save → device restarts
5. Open serial monitor. Verify:
   - WiFi connects
   - Mining starts (shares appear)
   - Dashboard shows hashrate > 0, shares incrementing
   - Clock screen shows correct time
   - Touch left/right edges switches views
   - Balance label shows DUCO balance (may take a few minutes to appear)

- [ ] **Step 3: Verify Bitcoin path**

1. Long press 3s on screen → display shows portal message
2. Connect to `NerdDuino-Setup` AP
3. Switch to Bitcoin, enter BTC address and pool
4. Save → restarts
5. Verify:
   - Connects to pool (serial: `[btc] Authorize OK`)
   - Dashboard shows BTC hashrate
   - Pool name appears on ClockScreen

- [ ] **Step 4: Verify OTA**

```bash
# Bump version string in main.cpp, then:
pio run -e nerdduino-pro -t upload --upload-port <device-ip>
```

Verify device reboots and resumes mining with new firmware.

- [ ] **Step 5: Final commit + tag**

```bash
git add -A
git commit -m "feat: NerdDuino Pro v1.0 — dual-coin miner with LVGL UI and config portal"
git tag v1.0.0
```

---

## Self-Review Notes

**Spec coverage check:**
- ✅ Dual-coin DUCO + BTC via IMiningAlgorithm — Tasks 4–6
- ✅ LVGL cockpit UI, View 1 gauge + sparkline — Task 8
- ✅ View 2 clock + info strip — Task 9
- ✅ Lateral touch zone navigation — Task 7 (UIManager)
- ✅ Long press 3s → portal — Task 7 (UIManager)
- ✅ Config portal AP mode, all fields, HTML in PROGMEM — Task 10
- ✅ First boot → portal, WiFi fail → portal — Task 10
- ✅ LittleFS /config.json — Task 2
- ✅ Partition scheme 4MB with OTA — Task 1
- ✅ ArduinoOTA wireless update — Task 11
- ✅ FreeRTOS: taskMining Core 1, taskUI Core 0 — Tasks 6, 7
- ✅ LVGL partial buffer, no PSRAM — Task 7
- ✅ GPL-3.0 license (NMMiner) — noted in Task 6
- ✅ NTP timezone from config — Task 9
- ✅ PlatformIO with TFT_eSPI build_flags — Task 1

**Not in scope (per spec Section 2):**
- MQTT, Home Assistant, Telegram, Discord
- Web dashboard beyond config portal
- Historical charts to LittleFS
- GitHub Releases OTA auto-check
