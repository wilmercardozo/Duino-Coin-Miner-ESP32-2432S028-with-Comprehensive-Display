#pragma GCC optimize("-Ofast")

#include "DuinoCoinMiner.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>

// ---------------------------------------------------------------------------
// DSHA1 — optimised SHA-1 variant used by DUCO-S1
// Based on ChocDuino/DSHA1.h (MIT licence); see warmup() for one deviation.
// ---------------------------------------------------------------------------
class DSHA1 {
public:
    static const size_t OUTPUT_SIZE = 20;

    DSHA1() { initialize(s); bytes = 0; }

    DSHA1& write(const unsigned char* data, size_t len) {
        size_t bufsize = bytes % 64;
        if (bufsize && bufsize + len >= 64) {
            memcpy(buf + bufsize, data, 64 - bufsize);
            bytes += 64 - bufsize;
            data  += 64 - bufsize;
            len   -= 64 - bufsize;
            transform(s, buf);
            bufsize = 0;
        }
        while (len >= 64) {
            transform(s, data);
            bytes += 64;
            data  += 64;
            len   -= 64;
        }
        if (len > 0) {
            memcpy(buf + bufsize, data, len);
            bytes += len;
        }
        return *this;
    }

    void finalize(unsigned char hash[OUTPUT_SIZE]) {
        const unsigned char pad[64] = {0x80};
        unsigned char sizedesc[8];
        writeBE64(sizedesc, bytes << 3);
        write(pad, 1 + ((119 - (bytes % 64)) % 64));
        write(sizedesc, 8);
        writeBE32(hash,      s[0]);
        writeBE32(hash + 4,  s[1]);
        writeBE32(hash + 8,  s[2]);
        writeBE32(hash + 12, s[3]);
        writeBE32(hash + 16, s[4]);
    }

    DSHA1& reset() {
        bytes = 0;
        initialize(s);
        return *this;
    }

    DSHA1& warmup() {
        uint8_t w[20];
        // Note: original ChocDuino uses length 20 here, which is a bug (string is 14 chars).
        // Using correct length 14 to avoid reading beyond the string literal.
        write((const uint8_t*)"warmupwarmupwa", 14).finalize(w);
        return *this;
    }

private:
    uint32_t s[5];
    unsigned char buf[64];
    uint64_t bytes;

    const uint32_t k1 = 0x5A827999ul;
    const uint32_t k2 = 0x6ED9EBA1ul;
    const uint32_t k3 = 0x8F1BBCDCul;
    const uint32_t k4 = 0xCA62C1D6ul;

    uint32_t inline f1(uint32_t b, uint32_t c, uint32_t d) { return d ^ (b & (c ^ d)); }
    uint32_t inline f2(uint32_t b, uint32_t c, uint32_t d) { return b ^ c ^ d; }
    uint32_t inline f3(uint32_t b, uint32_t c, uint32_t d) { return (b & c) | (d & (b | c)); }
    uint32_t inline left(uint32_t x) { return (x << 1) | (x >> 31); }

    void inline Round(uint32_t a, uint32_t& b, uint32_t c, uint32_t d, uint32_t& e,
                      uint32_t f, uint32_t k, uint32_t w) {
        e += ((a << 5) | (a >> 27)) + f + k + w;
        b  = (b << 30) | (b >> 2);
    }

    void initialize(uint32_t s[5]) {
        s[0] = 0x67452301ul;
        s[1] = 0xEFCDAB89ul;
        s[2] = 0x98BADCFEul;
        s[3] = 0x10325476ul;
        s[4] = 0xC3D2E1F0ul;
    }

    void transform(uint32_t* s, const unsigned char* chunk) {
        uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
        uint32_t w0,w1,w2,w3,w4,w5,w6,w7,w8,w9,w10,w11,w12,w13,w14,w15;

        Round(a,b,c,d,e,f1(b,c,d),k1,w0 =readBE32(chunk+0));
        Round(e,a,b,c,d,f1(a,b,c),k1,w1 =readBE32(chunk+4));
        Round(d,e,a,b,c,f1(e,a,b),k1,w2 =readBE32(chunk+8));
        Round(c,d,e,a,b,f1(d,e,a),k1,w3 =readBE32(chunk+12));
        Round(b,c,d,e,a,f1(c,d,e),k1,w4 =readBE32(chunk+16));
        Round(a,b,c,d,e,f1(b,c,d),k1,w5 =readBE32(chunk+20));
        Round(e,a,b,c,d,f1(a,b,c),k1,w6 =readBE32(chunk+24));
        Round(d,e,a,b,c,f1(e,a,b),k1,w7 =readBE32(chunk+28));
        Round(c,d,e,a,b,f1(d,e,a),k1,w8 =readBE32(chunk+32));
        Round(b,c,d,e,a,f1(c,d,e),k1,w9 =readBE32(chunk+36));
        Round(a,b,c,d,e,f1(b,c,d),k1,w10=readBE32(chunk+40));
        Round(e,a,b,c,d,f1(a,b,c),k1,w11=readBE32(chunk+44));
        Round(d,e,a,b,c,f1(e,a,b),k1,w12=readBE32(chunk+48));
        Round(c,d,e,a,b,f1(d,e,a),k1,w13=readBE32(chunk+52));
        Round(b,c,d,e,a,f1(c,d,e),k1,w14=readBE32(chunk+56));
        Round(a,b,c,d,e,f1(b,c,d),k1,w15=readBE32(chunk+60));

        Round(e,a,b,c,d,f1(a,b,c),k1,w0 =left(w0 ^w13^w8 ^w2));
        Round(d,e,a,b,c,f1(e,a,b),k1,w1 =left(w1 ^w14^w9 ^w3));
        Round(c,d,e,a,b,f1(d,e,a),k1,w2 =left(w2 ^w15^w10^w4));
        Round(b,c,d,e,a,f1(c,d,e),k1,w3 =left(w3 ^w0 ^w11^w5));
        Round(a,b,c,d,e,f2(b,c,d),k2,w4 =left(w4 ^w1 ^w12^w6));
        Round(e,a,b,c,d,f2(a,b,c),k2,w5 =left(w5 ^w2 ^w13^w7));
        Round(d,e,a,b,c,f2(e,a,b),k2,w6 =left(w6 ^w3 ^w14^w8));
        Round(c,d,e,a,b,f2(d,e,a),k2,w7 =left(w7 ^w4 ^w15^w9));
        Round(b,c,d,e,a,f2(c,d,e),k2,w8 =left(w8 ^w5 ^w0 ^w10));
        Round(a,b,c,d,e,f2(b,c,d),k2,w9 =left(w9 ^w6 ^w1 ^w11));
        Round(e,a,b,c,d,f2(a,b,c),k2,w10=left(w10^w7 ^w2 ^w12));
        Round(d,e,a,b,c,f2(e,a,b),k2,w11=left(w11^w8 ^w3 ^w13));
        Round(c,d,e,a,b,f2(d,e,a),k2,w12=left(w12^w9 ^w4 ^w14));
        Round(b,c,d,e,a,f2(c,d,e),k2,w13=left(w13^w10^w5 ^w15));
        Round(a,b,c,d,e,f2(b,c,d),k2,w14=left(w14^w11^w6 ^w0));
        Round(e,a,b,c,d,f2(a,b,c),k2,w15=left(w15^w12^w7 ^w1));

        Round(d,e,a,b,c,f2(e,a,b),k2,w0 =left(w0 ^w13^w8 ^w2));
        Round(c,d,e,a,b,f2(d,e,a),k2,w1 =left(w1 ^w14^w9 ^w3));
        Round(b,c,d,e,a,f2(c,d,e),k2,w2 =left(w2 ^w15^w10^w4));
        Round(a,b,c,d,e,f2(b,c,d),k2,w3 =left(w3 ^w0 ^w11^w5));
        Round(e,a,b,c,d,f2(a,b,c),k2,w4 =left(w4 ^w1 ^w12^w6));
        Round(d,e,a,b,c,f2(e,a,b),k2,w5 =left(w5 ^w2 ^w13^w7));
        Round(c,d,e,a,b,f2(d,e,a),k2,w6 =left(w6 ^w3 ^w14^w8));
        Round(b,c,d,e,a,f2(c,d,e),k2,w7 =left(w7 ^w4 ^w15^w9));
        Round(a,b,c,d,e,f3(b,c,d),k3,w8 =left(w8 ^w5 ^w0 ^w10));
        Round(e,a,b,c,d,f3(a,b,c),k3,w9 =left(w9 ^w6 ^w1 ^w11));
        Round(d,e,a,b,c,f3(e,a,b),k3,w10=left(w10^w7 ^w2 ^w12));
        Round(c,d,e,a,b,f3(d,e,a),k3,w11=left(w11^w8 ^w3 ^w13));
        Round(b,c,d,e,a,f3(c,d,e),k3,w12=left(w12^w9 ^w4 ^w14));
        Round(a,b,c,d,e,f3(b,c,d),k3,w13=left(w13^w10^w5 ^w15));
        Round(e,a,b,c,d,f3(a,b,c),k3,w14=left(w14^w11^w6 ^w0));
        Round(d,e,a,b,c,f3(e,a,b),k3,w15=left(w15^w12^w7 ^w1));

        Round(c,d,e,a,b,f3(d,e,a),k3,w0 =left(w0 ^w13^w8 ^w2));
        Round(b,c,d,e,a,f3(c,d,e),k3,w1 =left(w1 ^w14^w9 ^w3));
        Round(a,b,c,d,e,f3(b,c,d),k3,w2 =left(w2 ^w15^w10^w4));
        Round(e,a,b,c,d,f3(a,b,c),k3,w3 =left(w3 ^w0 ^w11^w5));
        Round(d,e,a,b,c,f3(e,a,b),k3,w4 =left(w4 ^w1 ^w12^w6));
        Round(c,d,e,a,b,f3(d,e,a),k3,w5 =left(w5 ^w2 ^w13^w7));
        Round(b,c,d,e,a,f3(c,d,e),k3,w6 =left(w6 ^w3 ^w14^w8));
        Round(a,b,c,d,e,f3(b,c,d),k3,w7 =left(w7 ^w4 ^w15^w9));
        Round(e,a,b,c,d,f3(a,b,c),k3,w8 =left(w8 ^w5 ^w0 ^w10));
        Round(d,e,a,b,c,f3(e,a,b),k3,w9 =left(w9 ^w6 ^w1 ^w11));
        Round(c,d,e,a,b,f3(d,e,a),k3,w10=left(w10^w7 ^w2 ^w12));
        Round(b,c,d,e,a,f3(c,d,e),k3,w11=left(w11^w8 ^w3 ^w13));
        Round(a,b,c,d,e,f2(b,c,d),k4,w12=left(w12^w9 ^w4 ^w14));
        Round(e,a,b,c,d,f2(a,b,c),k4,w13=left(w13^w10^w5 ^w15));
        Round(d,e,a,b,c,f2(e,a,b),k4,w14=left(w14^w11^w6 ^w0));
        Round(c,d,e,a,b,f2(d,e,a),k4,w15=left(w15^w12^w7 ^w1));

        Round(b,c,d,e,a,f2(c,d,e),k4,w0 =left(w0 ^w13^w8 ^w2));
        Round(a,b,c,d,e,f2(b,c,d),k4,w1 =left(w1 ^w14^w9 ^w3));
        Round(e,a,b,c,d,f2(a,b,c),k4,w2 =left(w2 ^w15^w10^w4));
        Round(d,e,a,b,c,f2(e,a,b),k4,w3 =left(w3 ^w0 ^w11^w5));
        Round(c,d,e,a,b,f2(d,e,a),k4,w4 =left(w4 ^w1 ^w12^w6));
        Round(b,c,d,e,a,f2(c,d,e),k4,w5 =left(w5 ^w2 ^w13^w7));
        Round(a,b,c,d,e,f2(b,c,d),k4,w6 =left(w6 ^w3 ^w14^w8));
        Round(e,a,b,c,d,f2(a,b,c),k4,w7 =left(w7 ^w4 ^w15^w9));
        Round(d,e,a,b,c,f2(e,a,b),k4,w8 =left(w8 ^w5 ^w0 ^w10));
        Round(c,d,e,a,b,f2(d,e,a),k4,w9 =left(w9 ^w6 ^w1 ^w11));
        Round(b,c,d,e,a,f2(c,d,e),k4,w10=left(w10^w7 ^w2 ^w12));
        Round(a,b,c,d,e,f2(b,c,d),k4,w11=left(w11^w8 ^w3 ^w13));
        Round(e,a,b,c,d,f2(a,b,c),k4,w12=left(w12^w9 ^w4 ^w14));
        Round(d,e,a,b,c,f2(e,a,b),k4,   left(w13^w10^w5 ^w15));
        Round(c,d,e,a,b,f2(d,e,a),k4,   left(w14^w11^w6 ^w0));
        Round(b,c,d,e,a,f2(c,d,e),k4,   left(w15^w12^w7 ^w1));

        s[0]+=a; s[1]+=b; s[2]+=c; s[3]+=d; s[4]+=e;
    }

    uint32_t static inline readBE32(const unsigned char* ptr) {
        return __builtin_bswap32(*(const uint32_t*)ptr);
    }
    void static inline writeBE32(unsigned char* ptr, uint32_t x) {
        *(uint32_t*)ptr = __builtin_bswap32(x);
    }
    void static inline writeBE64(unsigned char* ptr, uint64_t x) {
        *(uint64_t*)ptr = __builtin_bswap64(x);
    }
};

// ---------------------------------------------------------------------------
// Counter — decimal string counter for nonce iteration
// Based on ChocDuino/Counter.h (MIT licence)
// ---------------------------------------------------------------------------
template <unsigned int MAX_DIGITS>
class Counter {
public:
    Counter() { reset(); }

    void reset() {
        memset(buffer, '0', MAX_DIGITS);
        buffer[MAX_DIGITS] = '\0';
        val = 0;
        len = 1;
    }

    inline Counter& operator++() {
        incString(buffer + MAX_DIGITS - 1);
        ++val;
        return *this;
    }

    inline operator unsigned int() const { return val; }
    inline const char* c_str() const { return buffer + MAX_DIGITS - len; }
    inline size_t strlen() const { return len; }

    inline bool operator<(uint32_t limit) const { return val < limit; }

private:
    inline void incString(char* c) {
        if (*c < '9') {
            *c += 1;
        } else {
            *c = '0';
            incString(c - 1);
            len = (size_t)max((int)(MAX_DIGITS - (c - buffer) + 1), (int)len);
        }
    }

    char buffer[MAX_DIGITS + 1];
    unsigned int val;
    size_t len;
};

// ---------------------------------------------------------------------------
// Base-36 hex decode table (chars '0'..'z')
// ---------------------------------------------------------------------------
static const uint8_t kBase36Val[75] PROGMEM = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
    0, 0, 0, 0, 0, 0,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35
};

// ===========================================================================
// DuinoCoinMiner implementation
// ===========================================================================

DuinoCoinMiner::DuinoCoinMiner(const Config& cfg)
    : _cfg(cfg)
{
    memset(_expectedHash, 0, sizeof(_expectedHash));
    strncpy(_stats.algorithm, "DUCO", sizeof(_stats.algorithm) - 1);
    _buildChipID();
}

DuinoCoinMiner::~DuinoCoinMiner() {
    disconnect();
}

// ---------------------------------------------------------------------------
// connect() — resolve pool then open TCP connection
// ---------------------------------------------------------------------------
bool DuinoCoinMiner::connect() {
    _startMs = millis();

    if (!_resolvePool()) {
        Serial.println("[DUCO] resolvePool failed");
        return false;
    }

    return _connectToNode();
}

// ---------------------------------------------------------------------------
// mine() — one full job cycle (get job → hash → submit)
// ---------------------------------------------------------------------------
void DuinoCoinMiner::mine() {
    // Reconnect if dropped or stale
    if (!_client.connected()) {
        _client.stop();
        if (!_connectToNode()) {
            delay(2000);
            return;
        }
    }

    // Reconnect if no submit in 5 minutes
    uint32_t now = millis();
    if (_lastSubmitMs && (now - _lastSubmitMs) > 300000UL) {
        Serial.println("[DUCO] no submit in 5m, reconnecting");
        _client.stop();
        if (!_connectToNode()) {
            Serial.println("[duco] Reconnect after stale-submit failed");
            return;
        }
        return;
    }

    if (!_getJob()) return;

    // Pre-hash the last-block-hash prefix (warm the SHA-1 state)
    DSHA1 prefix;
    prefix.warmup();
    prefix.reset();
    prefix.write((const unsigned char*)_lastHash.c_str(), _lastHash.length());
    prefix.write((const unsigned char*)",", 1);

    uint32_t startUs = micros();   // captured once; never reset inside the loop
    uint32_t yieldUs = startUs;    // separate tracker for watchdog yield cadence
    uint8_t  hashOut[20];

    Counter<10> counter;
    uint32_t found = 0;
    bool     solved = false;

    for (; counter < _jobDiff; ++counter) {
        DSHA1 ctx = prefix;
        ctx.write((const unsigned char*)counter.c_str(), counter.strlen())
           .finalize(hashOut);

        if (memcmp(_expectedHash, hashOut, 20) == 0) {
            found  = (uint32_t)(unsigned int)counter;
            solved = true;
            break;
        }

        // Yield to RTOS every ~100ms to prevent watchdog trips
        if ((micros() - yieldUs) > 100000UL) {
            delay(1);
            yieldUs = micros();
            if (!_client.connected()) {
                Serial.println("[DUCO] connection dropped during hash loop");
                _client.stop();
                return;
            }
        }
    }

    if (!solved) {
        // Exhausted range without finding nonce — use last counter value
        found = (uint32_t)(unsigned int)counter;
    }

    uint32_t elapsedMs = (micros() - startUs) / 1000 + 1;

    _updateHashrate(found, elapsedMs);
    _submitShare(found, elapsedMs);
}

// ---------------------------------------------------------------------------
// getStats()
// ---------------------------------------------------------------------------
MiningStats DuinoCoinMiner::getStats() {
    _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    return _stats;
}

// ---------------------------------------------------------------------------
// disconnect()
// ---------------------------------------------------------------------------
void DuinoCoinMiner::disconnect() {
    _client.stop();
}

// ===========================================================================
// Private helpers
// ===========================================================================

void DuinoCoinMiner::_buildChipID() {
    uint64_t mac = ESP.getEfuseMac();
    uint16_t hi  = (uint16_t)(mac >> 32);
    char buf[23];
    snprintf(buf, sizeof(buf), "%04X%08X", hi, (uint32_t)mac);
    _chipID = String(buf);

    // If rig_name is "Auto" we auto-generate from chip ID
    // (The Config field is const here so we just use it as-is;
    //  auto-naming can be added by the caller before constructing.)
}

bool DuinoCoinMiner::_resolvePool() {
    Serial.println("[DUCO] fetching pool from server.duinocoin.com/getPool");

    WiFiClientSecure sc;
    sc.setInsecure();
    sc.setTimeout(3000);

    HTTPClient https;
    https.setTimeout(3000);
    https.setReuse(false);

    if (!https.begin(sc, "https://server.duinocoin.com/getPool")) {
        Serial.println("[DUCO] https.begin failed");
        return false;
    }

    https.addHeader("Accept", "*/*");
    int code = https.GET();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[DUCO] getPool HTTP %d\n", code);
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Serial.println("[DUCO] getPool JSON parse error");
        return false;
    }

    _host = doc["ip"].as<String>();
    _port = doc["port"].as<int>();

    if (_host.length() == 0 || _port == 0) {
        Serial.println("[DUCO] getPool: missing ip/port");
        return false;
    }

    const char* name = doc["name"] | "?";
    snprintf(_stats.poolUrl, sizeof(_stats.poolUrl), "%s:%d (%s)",
             _host.c_str(), _port, name);

    Serial.printf("[DUCO] pool: %s\n", _stats.poolUrl);
    return true;
}

bool DuinoCoinMiner::_connectToNode() {
    if (_client.connected()) return true;

    if (_host.length() == 0) {
        // Pool not resolved yet — try again
        if (!_resolvePool()) return false;
    }

    Serial.printf("[DUCO] connecting to %s:%d\n", _host.c_str(), _port);

    uint32_t t0 = millis();
    while (!_client.connect(_host.c_str(), _port)) {
        if (millis() - t0 > 30000UL) {
            Serial.println("[DUCO] connect timeout");
            _client.stop();
            return false;
        }
        delay(200);
    }

    // Read welcome / version line
    if (!_waitForData(8000)) {
        Serial.println("[DUCO] no welcome from node");
        _client.stop();
        return false;
    }

    Serial.printf("[DUCO] node version: %s\n", _clientBuf.c_str());
    return true;
}

bool DuinoCoinMiner::_waitForData(uint32_t timeoutMs) {
    _clientBuf = "";
    uint32_t t0 = millis();

    while (_client.connected()) {
        if (_client.available()) {
            _clientBuf = _client.readStringUntil('\n');
            _clientBuf.replace("\r", "");
            return true;
        }
        if (millis() - t0 > timeoutMs) {
            Serial.printf("[DUCO] waitForData timeout (%lu ms)\n", (unsigned long)timeoutMs);
            return false;
        }
        delay(1);
    }
    return false;
}

bool DuinoCoinMiner::_getJob() {
    // Request a job
    _client.printf("JOB,%s,%s,%s\n",
                   _cfg.duco_user,
                   START_DIFF,
                   _cfg.duco_key);

    if (!_waitForData(12000)) {
        Serial.println("[DUCO] no job response");
        _client.stop();
        return false;
    }

    if (!_parseJobLine(_clientBuf)) {
        Serial.println("[DUCO] malformed job, reconnecting");
        _client.stop();
        return false;
    }

    return true;
}

bool DuinoCoinMiner::_parseJobLine(const String& line) {
    // Format: lastHash,newHash,difficulty
    char buf[256];
    strncpy(buf, line.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    String tokens[3];
    char* tok = strtok(buf, ",");
    for (int i = 0; tok && i < 3; i++) {
        tokens[i] = tok;
        tok = strtok(nullptr, ",");
    }

    if (tokens[0].length() < 8 || tokens[1].length() < 40 || tokens[2].length() < 1) {
        Serial.printf("[DUCO] job malformed: len0=%u len1=%u len2=%u\n",
                      (unsigned)tokens[0].length(),
                      (unsigned)tokens[1].length(),
                      (unsigned)tokens[2].length());
        return false;
    }

    _lastHash        = tokens[0];
    _expectedHashStr = tokens[1];

    if (!_hexToBytes(_expectedHashStr, _expectedHash, 20)) {
        Serial.println("[DUCO] bad expected hash hex");
        return false;
    }

    int diff = tokens[2].toInt();
    if (diff <= 0) return false;

    _jobDiff = (uint32_t)diff * 100 + 1;
    return true;
}

bool DuinoCoinMiner::_hexToBytes(const String& hex, uint8_t* out, uint32_t outLen) {
    if (!out) return false;
    if ((uint32_t)hex.length() < outLen * 2) {
        Serial.printf("[DUCO] hexToBytes: short hex len=%u need=%u\n",
                      (unsigned)hex.length(), (unsigned)(outLen * 2));
        return false;
    }

    const char* h = hex.c_str();
    for (uint32_t i = 0; i < outLen; i++) {
        char c1 = h[i * 2];
        char c2 = h[i * 2 + 1];

        if (c1 < '0' || c1 > 'z' || c2 < '0' || c2 > 'z') return false;

        uint8_t hi = pgm_read_byte(kBase36Val + (uint8_t)(c1 - '0'));
        uint8_t lo = pgm_read_byte(kBase36Val + (uint8_t)(c2 - '0'));

        if (hi > 15 || lo > 15) return false;
        out[i] = (hi << 4) | lo;
    }
    return true;
}

bool DuinoCoinMiner::_submitShare(uint32_t nonce, uint32_t elapsedMs) {
    if (!_client.connected()) return false;

    float hr = (_stats.hashrate > 0) ? (_stats.hashrate * 1000.0f) : 1.0f;

    _client.printf("%lu,%.0f,%s %s,%s,%s\n",
                   (unsigned long)nonce,
                   hr,
                   MINER_BANNER,
                   MINER_VER,
                   _cfg.rig_name,
                   _cfg.duco_key);

    uint32_t pingStart = millis();
    if (!_waitForData(8000)) {
        _client.stop();
        return false;
    }
    uint32_t pingMs = millis() - pingStart;
    _stats.pingMs = pingMs;
    _lastSubmitMs = millis();

    bool good = (_clientBuf == "GOOD");
    if (good) {
        _stats.sharesAccepted++;
    } else {
        _stats.sharesRejected++;
    }

    Serial.printf("[DUCO] %s nonce=%lu hr=%.2f kH/s ping=%lu ms\n",
                  _clientBuf.c_str(),
                  (unsigned long)nonce,
                  _stats.hashrate,
                  (unsigned long)pingMs);

    return good;
}

void DuinoCoinMiner::_updateHashrate(uint32_t hashCount, uint32_t elapsedMs) {
    if (elapsedMs == 0) return;
    // hashrate in kH/s
    _stats.hashrate = (float)hashCount / (float)elapsedMs;  // H/ms = kH/s
}
