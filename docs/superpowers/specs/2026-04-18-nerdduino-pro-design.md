# NerdDuino Pro — Design Spec

**Date:** 2026-04-18  
**Status:** Approved  
**Scope:** Option B — Dual-coin (DUCO + BTC) + LVGL UI + Config Portal  
**Hardware:** ESP32-2432S028R (NMTech/Heltec variant) — 520KB SRAM, no PSRAM, 4MB flash, ILI9341 320×240, XPT2046 resistive touch  
**License:** GPL-3.0 (required to reuse NMMiner SHA-256 optimizations)

---

## 1. Goals

- Mine DuinoCoin **or** Bitcoin (one at a time, user-selected via web portal) on the ESP32-2432S028R.
- Provide a polished LVGL UI with two switchable views.
- Zero hardcoded config: all settings (WiFi, wallet, pool, algorithm) via a captive web portal.
- OTA update support for future firmware versions.
- Personal tinkering project — no commercial requirements, no uptime SLAs.

## 2. Non-Goals

- Simultaneous multi-coin mining.
- MQTT / Home Assistant / Telegram integrations (deferred to future iterations).
- Web dashboard beyond the config portal (deferred).
- Achieving NMMiner's 960 kH/s target (use NMMiner's optimized SHA-256 as-is; no additional ASM tuning in scope).

---

## 3. Module Structure

```
src/
├── main.cpp                  ← boot sequence, FreeRTOS task creation
├── config/
│   ├── Config.h              ← runtime settings struct (WiFi, algo, wallet, pool, rig name)
│   └── ConfigStore.h/.cpp    ← LittleFS read/write of /config.json
├── mining/
│   ├── IMiningAlgorithm.h    ← abstract interface: connect(), mine(), getStats(), disconnect()
│   ├── DuinoCoinMiner.h/.cpp ← DUCO-S1 client (ported from ChocDuino, MIT-compatible)
│   └── BitcoinMiner.h/.cpp   ← Stratum V1 SHA-256 client (ported from NMMiner, GPL-3.0)
├── ui/
│   ├── UIManager.h/.cpp      ← LVGL init, partial buffer, screen switching, touch zone handling
│   ├── DashboardScreen.h/.cpp← View 1: gauge arc + 4-stat grid + 60s sparkline chart
│   └── ClockScreen.h/.cpp    ← View 2: large clock + date + hashrate/pool/shares strip
├── portal/
│   ├── ConfigPortal.h/.cpp   ← AP mode, AsyncWebServer, HTML served from flash (PROGMEM)
│   └── OTAHandler.h/.cpp     ← ArduinoOTA init and handle()
└── network/
    └── WiFiManager.h/.cpp    ← connect with retry, failure counting, portal trigger
```

---

## 4. FreeRTOS Task Layout

| Task | Core | Priority | Responsibility |
|------|------|----------|----------------|
| `taskMining` | Core 1 | High (5) | Continuous SHA-256 / DUCO-S1 hashing |
| `taskNetwork` | Core 0 | Medium (4) | Pool connection, Stratum/DUCO protocol, reconnects |
| `taskUI` | Core 0 | Low (2) | LVGL tick (every 5ms), display refresh, touch polling |
| `taskPortal` | Core 0 | Medium (4) | AsyncWebServer — only active in AP mode; suspended during mining |

**FPS throttle:** 15 FPS when mining Bitcoin (both cores busy), 25 FPS when mining DuinoCoin (Core 1 less loaded). UIManager sets the LVGL flush interval dynamically based on active algorithm.

---

## 5. RAM Budget

Total SRAM: 520KB. Available after OS/WiFi stack: ~300KB.

| Component | Estimate |
|-----------|----------|
| LVGL partial buffer (320×24×2 bytes) | ~15KB |
| LVGL object heap | ~40KB |
| FreeRTOS stacks (4 tasks × ~4KB) | ~16KB |
| TCP/TLS stack (WiFi + pool) | ~80KB |
| Stratum rx/tx buffers | ~8KB |
| Ring buffer 60-point sparkline (float×60) | <1KB |
| Config struct + JSON parse buffer | ~4KB |
| **Headroom** | **~136KB** |

No PSRAM required. LVGL partial buffer (1/10 screen height) is sufficient for the cockpit UI style.

---

## 6. Flash Partition Scheme (4MB)

```
Partition     Size    Use
────────────────────────────────────
bootloader    4KB     ESP-IDF
nvs           16KB    WiFi credentials cache
app0 (OTA_0)  1.8MB   Active firmware
app1 (OTA_1)  1.8MB   OTA staging partition
littlefs      384KB   /config.json + future stats ring buffer
```

- Config portal HTML/CSS/JS: hardcoded in firmware as `PROGMEM` string — does NOT consume LittleFS space.
- LittleFS 384KB: `/config.json` (~1KB) leaves ~383KB for future historical data.
- OTA: new firmware writes to `app1`, verified on boot, promoted to `app0`. Rollback if boot fails.

---

## 7. UI Design

### Visual Style
Cockpit Industrial: dark blue-black background (`#080c14`), orange accent (`#ff6b35`), `JetBrains Mono` for numbers, `Inter` for labels. Rounded stat cards with subtle borders.

### View 1 — Dashboard (default on boot)
```
┌─────────────────────────────────────────────────┐
│ ⬢ NerdDuino Pro          ▲ wifi-home · 14:23    │
│                                                  │
│  ┌──────────────┐   ┌────────┐  ┌────────┐      │
│  │  [gauge arc] │   │Shares✓ │  │Rejected│      │
│  │     184      │   │ 1,247  │  │   3    │      │
│  │    KH/S      │   ├────────┤  ├────────┤      │
│  │  QUALITY 97% │   │Balance │  │ Uptime │      │
│  └──────────────┘   │ 42.18  │  │2d 14h  │      │
│                     └────────┘  └────────┘      │
│  ▂▃▅▆▇█▇▆▇█▇▆▇█▇▆▅▆▇█▇█▇▆▇█  ← 60s sparkline  │
│                        ●  ○                     │
└─────────────────────────────────────────────────┘
```
- `lv_arc` for hashrate gauge (0–max historical).
- `lv_chart` bar series for 60-second hashrate ring buffer, updated every second.
- 4 stat cards: Shares accepted (green), Rejected (amber), Balance (orange), Uptime (blue).
- Orange dot = current view indicator.

### View 2 — Clock + Info
```
┌─────────────────────────────────────────────────┐
│ ⬢ NerdDuino Pro          ▲ DUCO · online        │
│                                                  │
│              14:23                               │
│         VIERNES · 18 ABR 2025                    │
│                                                  │
│  ┌──────────────────────────────────────────┐    │
│  │  184 kH/s  │  public-pool.io  │  1,247  │    │
│  │  Hashrate  │  ● 38ms · online │  Shares │    │
│  └──────────────────────────────────────────┘    │
│                        ○  ●                     │
└─────────────────────────────────────────────────┘
```
- Large clock via NTP (timezone configurable in portal, default UTC-5 Colombia).
- Bottom info strip: hashrate + pool name + ping + shares.

### Navigation
Touch zones on left (10% width) and right (10% width) of screen cycle through views. Center 80% is non-interactive for navigation. Long press (3 seconds) anywhere — including lateral zones — triggers config portal mode.

Touch timing disambiguation:
- Press released in < 500ms inside a lateral zone → change view.
- Press held ≥ 3000ms anywhere → enter portal mode (cancel any view change).

---

## 8. Config Portal

### Activation Triggers
1. **First boot** — no `/config.json` found on LittleFS.
2. **WiFi failure** — 3 consecutive connection failures after boot.
3. **Manual** — touch held for 3 seconds anywhere on screen during mining.

### AP Mode Behavior
- SSID: `NerdDuino-Setup` (no password).
- IP: `192.168.4.1`.
- Display shows: `📡 AP: NerdDuino-Setup` + `192.168.4.1`.
- Portal auto-closes and device restarts after saving config.

### Portal Fields
```
WiFi
  SSID          text input + [Scan networks] button
  Password      password input

Algorithm
  ○ DuinoCoin   ○ Bitcoin     (radio — shows relevant block below)

[DuinoCoin block — visible when DuinoCoin selected]
  Username      text input
  Mining Key    text input (optional, leave blank if unused)

[Bitcoin block — visible when Bitcoin selected]
  BTC Address   text input
  Pool URL      text input (default: public-pool.io:21496)

Optional
  Rig Name      text input (default: NerdDuino-1)
  Timezone      dropdown (default: UTC-5, América/Bogotá)

[  Guardar y conectar  ]
```

### Portal Tech Stack
- `ESPAsyncWebServer` serves a single HTML page stored as a `PROGMEM` string in `portal/ConfigPortal.cpp`.
- Dark theme, orange accent, Tailwind CSS via CDN (requires internet on the configuring device, not the ESP32).
- On save: validates non-empty required fields, writes `/config.json`, calls `ESP.restart()`.
- No auth — personal device on local network.

---

## 9. Config Schema (`/config.json`)

```json
{
  "wifi_ssid": "...",
  "wifi_pass": "...",
  "algorithm": "DUCO",
  "duco_user": "...",
  "duco_key": "",
  "btc_address": "",
  "pool_url": "public-pool.io",
  "pool_port": 21496,
  "rig_name": "NerdDuino-1",
  "timezone_offset": -5
}
```

---

## 10. Mining Abstraction

`IMiningAlgorithm` interface:
```cpp
class IMiningAlgorithm {
public:
  virtual bool connect()       = 0;
  virtual void mine()          = 0;  // called in tight loop from taskMining
  virtual MiningStats getStats() = 0;
  virtual void disconnect()    = 0;
};
```

`MiningStats` struct (shared by both implementations):
```cpp
struct MiningStats {
  float    hashrate;        // kH/s
  uint32_t sharesAccepted;
  uint32_t sharesRejected;
  float    balance;         // updated async via API poll
  uint32_t uptimeSeconds;
  uint32_t pingMs;
  char     poolUrl[64];
};
```

`main.cpp` instantiates the correct implementation based on `config.algorithm` at boot. To change algorithm the user re-enters the portal and the device restarts — no hot-swap needed.

---

## 11. OTA

- `ArduinoOTA` initialized at boot (always active when WiFi is connected).
- Flash via Arduino IDE / PlatformIO `pio run -t upload --upload-port <ip>` over the network.
- Partition scheme supports safe OTA rollback.
- Future: check GitHub Releases API and notify on display (not in current scope).

---

## 12. Build System

- **PlatformIO** with `platformio.ini`.
- TFT_eSPI configured via `build_flags` (no manual `User_Setup_Select.h` edits).
- LVGL configured via `lv_conf.h` in `include/`.
- Single environment: `[env:nerdduino-pro]`, board `esp32dev`, 4MB flash, custom partition CSV.
- `pio run -e nerdduino-pro -t upload` to build and flash.

---

## 13. Source Acknowledgements

| Component | Origin | License |
|-----------|--------|---------|
| DuinoCoin client | ChocDuino / Duino-Coin project | MIT |
| SHA-256 miner | NMMiner / NerdMiner_v2 | GPL-3.0 |
| LVGL | LVGL project | MIT |
| ESPAsyncWebServer | me-no-dev | LGPL-3.0 |

**Project license: GPL-3.0** (required by NMMiner dependency).

---

## 14. Out of Scope (Future)

- MQTT / Home Assistant autodiscovery
- Telegram / Discord notifications  
- Web dashboard (beyond config portal)
- Historical charts persisted to LittleFS
- Scrypt / other algorithms
- GitHub Releases OTA auto-check
