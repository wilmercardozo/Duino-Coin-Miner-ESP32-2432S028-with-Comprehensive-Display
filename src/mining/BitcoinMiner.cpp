// SPDX-License-Identifier: GPL-3.0-or-later
// NerdDuino Pro — Bitcoin Stratum V1 miner implementation
// Ported from NMMiner architecture; mbedTLS SHA-256d (Apache-2.0)
#pragma GCC optimize("-O2")
#include "BitcoinMiner.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/sha256.h"

#define MINER_VERSION "NerdDuino/1.0"

BitcoinMiner::BitcoinMiner(const Config& cfg) : _cfg(cfg) {
    strlcpy(_stats.algorithm, "BTC", sizeof(_stats.algorithm));
    snprintf(_stats.poolUrl, sizeof(_stats.poolUrl), "%s:%d", cfg.pool_url, cfg.pool_port);
}

BitcoinMiner::~BitcoinMiner() { disconnect(); }

// ---------------------------------------------------------------------------
// Double-SHA256 using mbedTLS (built into ESP-IDF, Apache-2.0)
// ---------------------------------------------------------------------------
void BitcoinMiner::_doubleSha256(const uint8_t* data, size_t len, uint8_t* out32) {
    uint8_t tmp[32];
    mbedtls_sha256(data, len, tmp, 0);
    mbedtls_sha256(tmp, 32, out32, 0);
}

// ---------------------------------------------------------------------------
// connect() — TCP connect, subscribe, authorize
// ---------------------------------------------------------------------------
bool BitcoinMiner::connect() {
    _startMs = millis();
    Serial.printf("[btc] Connecting to %s:%d\n", _cfg.pool_url, _cfg.pool_port);
    if (!_client.connect(_cfg.pool_url, _cfg.pool_port)) {
        Serial.println("[btc] TCP connect failed");
        return false;
    }
    _client.setTimeout(10);
    return _sendSubscribe() && _sendAuthorize();
}

// ---------------------------------------------------------------------------
// _readLine() — blocking read until '\n' or timeout
// ---------------------------------------------------------------------------
bool BitcoinMiner::_readLine(String& out, uint32_t timeoutMs) {
    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (_client.available()) {
            out = _client.readStringUntil('\n');
            out.trim();
            return out.length() > 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

// ---------------------------------------------------------------------------
// _sendSubscribe() — Stratum mining.subscribe
// ---------------------------------------------------------------------------
bool BitcoinMiner::_sendSubscribe() {
    String req = "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"" MINER_VERSION "\"]}\n";
    uint32_t t0 = millis();
    _client.print(req);
    String resp;
    if (!_readLine(resp, 10000)) { Serial.println("[btc] Subscribe timeout"); return false; }
    _stats.pingMs = millis() - t0;
    if (!_parseSubscribeResponse(resp)) { Serial.println("[btc] Subscribe parse failed"); return false; }
    Serial.printf("[btc] Subscribe OK — extranonce1=%s size=%d ping=%dms\n", _extranonce1, _extranonce2Size, _stats.pingMs);
    return true;
}

// ---------------------------------------------------------------------------
// _parseSubscribeResponse() — extract extranonce1 + extranonce2_size
// {"id":1,"result":[[...],extranonce1,extranonce2_size],"error":null}
// ---------------------------------------------------------------------------
bool BitcoinMiner::_parseSubscribeResponse(const String& line) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return false;
    if (!doc["error"].isNull()) return false;
    JsonArray result = doc["result"];
    if (!result) return false;
    const char* en1 = result[1] | "";
    strlcpy(_extranonce1, en1, sizeof(_extranonce1));
    _extranonce2Size = result[2] | 4;
    return strlen(_extranonce1) > 0;
}

// ---------------------------------------------------------------------------
// _sendAuthorize() — Stratum mining.authorize (worker = address.rig_name)
// ---------------------------------------------------------------------------
bool BitcoinMiner::_sendAuthorize() {
    char worker[128];
    snprintf(worker, sizeof(worker), "%s.%s", _cfg.btc_address, _cfg.rig_name);
    String req = "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"";
    req += worker;
    req += "\",\"\"]}\n";
    _client.print(req);
    String resp;
    if (!_readLine(resp, 5000)) { Serial.println("[btc] Authorize timeout"); return false; }
    if (resp.indexOf("true") < 0) { Serial.println("[btc] Authorize rejected"); return false; }
    Serial.println("[btc] Authorize OK");
    return true;
}

// ---------------------------------------------------------------------------
// _parseNotify() — extract mining.notify job parameters
// {"id":null,"method":"mining.notify","params":[jobId,prevHash,coinb1,coinb2,
//   [merkle],version,nbits,ntime,cleanJobs]}
// ---------------------------------------------------------------------------
bool BitcoinMiner::_parseNotify(const String& line) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return false;
    if (strcmp(doc["method"] | "", "mining.notify") != 0) return false;
    JsonArray p = doc["params"];
    if (!p) return false;
    strlcpy(_jobId,    p[0] | "", sizeof(_jobId));
    strlcpy(_prevHash, p[1] | "", sizeof(_prevHash));
    strlcpy(_coinb1,   p[2] | "", sizeof(_coinb1));
    strlcpy(_coinb2,   p[3] | "", sizeof(_coinb2));
    JsonArray merkleArr = p[4];
    _merkleCount = 0;
    for (JsonVariant m : merkleArr) {
        if (_merkleCount >= 8) break;
        strlcpy(_merkle[_merkleCount++], m | "", sizeof(_merkle[0]));
    }
    strlcpy(_version, p[5] | "", sizeof(_version));
    strlcpy(_nbits,   p[6] | "", sizeof(_nbits));
    strlcpy(_ntime,   p[7] | "", sizeof(_ntime));
    bool cleanJobs = p[8] | false;
    if (cleanJobs) _jobNonce = 0;
    _hasJob = true;
    return true;
}

// ---------------------------------------------------------------------------
// Hex/byte conversion utilities
// ---------------------------------------------------------------------------
void BitcoinMiner::_hexToBytes(const char* hex, uint8_t* out, size_t maxBytes) {
    size_t len = strlen(hex);
    auto fromHex = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
        return 0;
    };
    for (size_t i = 0; i < maxBytes && i * 2 + 1 < len; i++) {
        out[i] = (uint8_t)((fromHex(hex[i*2]) << 4) | fromHex(hex[i*2+1]));
    }
}

void BitcoinMiner::_bytesToHex(const uint8_t* in, size_t len, char* out) {
    for (size_t i = 0; i < len; i++) {
        sprintf(out + i*2, "%02x", in[i]);
    }
    out[len*2] = '\0';
}

// ---------------------------------------------------------------------------
// _buildBlockHeader() — assemble 80-byte Bitcoin block header
// Layout: version(4) + prevHash(32) + merkleRoot(32) + time(4) + bits(4) + nonce(4)
// ---------------------------------------------------------------------------
void BitcoinMiner::_buildBlockHeader(uint32_t nonce, uint8_t* header80) {
    memset(header80, 0, 80);

    // 1. Version — little-endian 32-bit
    uint32_t version = strtoul(_version, nullptr, 16);
    header80[0] = (uint8_t)(version & 0xFF);
    header80[1] = (uint8_t)((version >> 8) & 0xFF);
    header80[2] = (uint8_t)((version >> 16) & 0xFF);
    header80[3] = (uint8_t)((version >> 24) & 0xFF);

    // 2. Previous block hash (32 bytes)
    //    Stratum sends prevHash as 64-char hex where each 4-byte word is
    //    byte-reversed relative to the natural hash order. We reverse each
    //    4-byte chunk to recover the correct network byte order.
    uint8_t prevHashBytes[32] = {};
    _hexToBytes(_prevHash, prevHashBytes, 32);
    for (int i = 0; i < 8; i++) {
        header80[4 + i*4 + 0] = prevHashBytes[i*4 + 3];
        header80[4 + i*4 + 1] = prevHashBytes[i*4 + 2];
        header80[4 + i*4 + 2] = prevHashBytes[i*4 + 1];
        header80[4 + i*4 + 3] = prevHashBytes[i*4 + 0];
    }

    // 3. Merkle root (32 bytes) — computed from coinbase tx + branch hashes
    //    coinbase = coinb1 + extranonce1 + extranonce2(zeros) + coinb2
    char extranonce2[32] = {};
    int en2HexLen = _extranonce2Size * 2;
    if (en2HexLen >= (int)sizeof(extranonce2))
        en2HexLen = (int)sizeof(extranonce2) - 1;
    memset(extranonce2, '0', (size_t)en2HexLen);
    extranonce2[en2HexLen] = '\0';

    String coinbase = String(_coinb1) + String(_extranonce1) + String(extranonce2) + String(_coinb2);
    size_t cbLen = coinbase.length() / 2;
    uint8_t* cbBytes = (uint8_t*)malloc(cbLen);
    if (cbBytes) {
        _hexToBytes(coinbase.c_str(), cbBytes, cbLen);
        uint8_t cbHash[32];
        _doubleSha256(cbBytes, cbLen, cbHash);
        free(cbBytes);

        // Apply merkle branches: root = SHA256d(current || branch)
        for (int i = 0; i < _merkleCount; i++) {
            uint8_t branchBytes[32] = {};
            _hexToBytes(_merkle[i], branchBytes, 32);
            uint8_t concat[64];
            memcpy(concat,      cbHash,      32);
            memcpy(concat + 32, branchBytes, 32);
            _doubleSha256(concat, 64, cbHash);
        }
        memcpy(header80 + 36, cbHash, 32);
    }
    // If malloc failed, merkle root stays zero — the hash won't meet target,
    // which is safe: we just waste this nonce batch.

    // 4. Timestamp — little-endian 32-bit
    uint32_t t = strtoul(_ntime, nullptr, 16);
    header80[68] = (uint8_t)(t & 0xFF);
    header80[69] = (uint8_t)((t >> 8) & 0xFF);
    header80[70] = (uint8_t)((t >> 16) & 0xFF);
    header80[71] = (uint8_t)((t >> 24) & 0xFF);

    // 5. Bits (encoded target) — little-endian 32-bit
    uint32_t bits = strtoul(_nbits, nullptr, 16);
    header80[72] = (uint8_t)(bits & 0xFF);
    header80[73] = (uint8_t)((bits >> 8) & 0xFF);
    header80[74] = (uint8_t)((bits >> 16) & 0xFF);
    header80[75] = (uint8_t)((bits >> 24) & 0xFF);

    // 6. Nonce — little-endian 32-bit
    header80[76] = (uint8_t)(nonce & 0xFF);
    header80[77] = (uint8_t)((nonce >> 8) & 0xFF);
    header80[78] = (uint8_t)((nonce >> 16) & 0xFF);
    header80[79] = (uint8_t)((nonce >> 24) & 0xFF);
}

// ---------------------------------------------------------------------------
// _meetsTarget() — check if hash satisfies the nbits-encoded target
// The hash (SHA256d output) is in internal byte order; Bitcoin valid hashes
// have a value numerically less than the target when interpreted as big-endian.
// We reverse the 32-byte result to compare most-significant byte first.
// ---------------------------------------------------------------------------
bool BitcoinMiner::_meetsTarget(const uint8_t* hash32) {
    // Decode nbits compact target: target = mantissa * 2^(8*(exp-3))
    uint32_t bits     = strtoul(_nbits, nullptr, 16);
    uint8_t  exp      = (uint8_t)(bits >> 24);
    uint32_t mantissa = bits & 0x00FFFFFFu;

    // Build 32-byte target (big-endian)
    // Compact format: the mantissa MSB sits at byte index (32 - exp) in the
    // big-endian 256-bit target, followed by the middle and LSB bytes.
    uint8_t target[32] = {};
    if (exp >= 1 && exp <= 32) {
        int bytePos = 32 - (int)exp;  // most-significant byte position of mantissa
        if (bytePos + 0 >= 0 && bytePos + 0 < 32) target[bytePos + 0] = (uint8_t)((mantissa >> 16) & 0xFF);
        if (bytePos + 1 >= 0 && bytePos + 1 < 32) target[bytePos + 1] = (uint8_t)((mantissa >> 8)  & 0xFF);
        if (bytePos + 2 >= 0 && bytePos + 2 < 32) target[bytePos + 2] = (uint8_t)( mantissa        & 0xFF);
    }

    // SHA256d output is little-endian; reverse to compare as big-endian
    uint8_t rev[32];
    for (int i = 0; i < 32; i++) rev[i] = hash32[31 - i];

    return memcmp(rev, target, 32) < 0;
}

// ---------------------------------------------------------------------------
// _submitShare() — send mining.submit to pool
// ---------------------------------------------------------------------------
bool BitcoinMiner::_submitShare(uint32_t nonce) {
    char nonceBuf[16];
    snprintf(nonceBuf, sizeof(nonceBuf), "%08lx", (unsigned long)nonce);

    char extranonce2[32] = {};
    int en2HexLen = _extranonce2Size * 2;
    if (en2HexLen >= (int)sizeof(extranonce2))
        en2HexLen = (int)sizeof(extranonce2) - 1;
    memset(extranonce2, '0', (size_t)en2HexLen);
    extranonce2[en2HexLen] = '\0';

    char worker[128];
    snprintf(worker, sizeof(worker), "%s.%s", _cfg.btc_address, _cfg.rig_name);

    String req = "{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"";
    req += worker;
    req += "\",\""; req += _jobId;
    req += "\",\""; req += extranonce2;
    req += "\",\""; req += _ntime;
    req += "\",\""; req += nonceBuf;
    req += "\"]}\n";
    _client.print(req);
    Serial.printf("[btc] Share submitted — nonce=%s\n", nonceBuf);

    // Read pool response to determine accept/reject
    String resp;
    if (_readLine(resp, 5000)) {
        // Accepted: {"id":4,"result":true,"error":null}
        // Rejected: {"id":4,"result":null,"error":[21,...]}  or result:false
        bool accepted = (resp.indexOf("\"result\":true") >= 0);
        if (accepted) {
            _stats.sharesAccepted++;
            Serial.println("[btc] Share accepted");
        } else {
            _stats.sharesRejected++;
            Serial.printf("[btc] Share rejected: %s\n", resp.c_str());
        }
    } else {
        // Timeout reading response — count as rejected to avoid silent loss
        _stats.sharesRejected++;
        Serial.println("[btc] Share response timeout — counted as rejected");
    }
    return true;
}

// ---------------------------------------------------------------------------
// mine() — main loop: drain incoming messages then hash a batch
// ---------------------------------------------------------------------------
void BitcoinMiner::mine() {
    // Drain incoming pool messages (new jobs, difficulty updates, responses)
    while (_client.available()) {
        String line;
        if (_readLine(line, 100)) {
            if (line.indexOf("mining.notify") >= 0) {
                if (_parseNotify(line)) {
                    Serial.printf("[btc] New job: %s\n", _jobId);
                }
            }
            // mining.set_difficulty and responses are silently consumed for now
        }
    }

    if (!_hasJob) {
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    if (!_client.connected()) {
        Serial.println("[btc] Disconnected — reconnecting");
        disconnect();
        if (!connect()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        return;
    }

    // Hash a batch of nonces; yield periodically to satisfy RTOS watchdog
    uint8_t   header[80];
    uint32_t  hashCount = 0;
    uint32_t  startMs   = millis();
    const uint32_t BATCH_SIZE = 10000;

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        uint32_t nonce = _jobNonce++;
        _buildBlockHeader(nonce, header);

        uint8_t hash[32];
        _doubleSha256(header, 80, hash);
        hashCount++;

        if (_meetsTarget(hash)) {
            _submitShare(nonce);
        }

        // Yield every 1000 hashes (~1 ms each) to keep the RTOS watchdog happy
        if ((i & 0x3FFu) == 0x3FFu) {  // every 1024 hashes
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // Update hashrate: hashCount H in elapsed ms  →  kH/s (H/ms == kH/s)
    uint32_t elapsed = millis() - startMs;
    if (elapsed > 0) {
        _stats.hashrate     = (float)hashCount / (float)elapsed;
        _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    }
}

// ---------------------------------------------------------------------------
// getStats() / disconnect()
// ---------------------------------------------------------------------------
MiningStats BitcoinMiner::getStats() {
    _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    return _stats;
}

void BitcoinMiner::disconnect() {
    if (_client.connected()) _client.stop();
    _hasJob = false;
}
