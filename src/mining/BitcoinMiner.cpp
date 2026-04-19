// SPDX-License-Identifier: GPL-3.0-or-later
// NerdDuino Pro — Bitcoin Stratum V1 miner implementation
// Hash core: nerdSHA256plus (Apache-2.0, NerdMiner_v2 / Blockstream Jade lineage)
#pragma GCC optimize("-O2")
#include "BitcoinMiner.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/sha256.h"

#define MINER_VERSION "NerdDuino/1.0"

// ---------------------------------------------------------------------------
// Local one-shot double-SHA256 used only by _prepareJob() (merkle root).
// The mining hot path uses nerd_sha256d() with a cached midstate instead.
// ---------------------------------------------------------------------------
static void doubleSha256Once(const uint8_t* data, size_t len, uint8_t* out32) {
    uint8_t tmp[32];
    mbedtls_sha256(data, len, tmp, 0);
    mbedtls_sha256(tmp, 32, out32, 0);
}

BitcoinMiner::BitcoinMiner(const Config& cfg) : _cfg(cfg) {
    strlcpy(_stats.algorithm, "BTC", sizeof(_stats.algorithm));
    // Issue 4: pool_url can be long; truncate to 57 chars before formatting
    // so that "%s:%d" never exceeds the 64-byte poolUrl buffer.
    char shortUrl[58];
    strlcpy(shortUrl, cfg.pool_url, sizeof(shortUrl));
    snprintf(_stats.poolUrl, sizeof(_stats.poolUrl), "%s:%d", shortUrl, cfg.pool_port);
}

BitcoinMiner::~BitcoinMiner() { disconnect(); }

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

    if (strlen(_coinb1) >= 1023 || strlen(_coinb2) >= 1023) {
        Serial.println("[btc] WARNING: coinbase too large — job skipped");
        _jobReady = false;
        return false;
    }

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

    _prepareJob();
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
// _prepareJob() — build merkleRoot, compute midstate of bytes 0..63, and
// cache bytes 64..75 into _tailBuf so the inner loop only writes the nonce.
// ---------------------------------------------------------------------------
void BitcoinMiner::_prepareJob() {
    _jobReady = false;
    _cachedBits = strtoul(_nbits, nullptr, 16);

    // --- Build coinbase hex: coinb1 + extranonce1 + extranonce2(zeros) + coinb2
    char extranonce2[32] = {};
    int en2HexLen = _extranonce2Size * 2;
    if (en2HexLen >= (int)sizeof(extranonce2))
        en2HexLen = (int)sizeof(extranonce2) - 1;
    memset(extranonce2, '0', (size_t)en2HexLen);
    extranonce2[en2HexLen] = '\0';

    size_t cbHexLen = strlen(_coinb1) + strlen(_extranonce1)
                    + (size_t)en2HexLen + strlen(_coinb2);
    size_t cbLen    = cbHexLen / 2;

    uint8_t* cbBytes = (uint8_t*)malloc(cbLen);
    if (!cbBytes) {
        Serial.println("[btc] _prepareJob: malloc failed — job skipped");
        return;
    }
    size_t offset = 0;
    size_t c1Len  = strlen(_coinb1)       / 2;
    size_t en1Len = strlen(_extranonce1)  / 2;
    size_t en2Len = (size_t)en2HexLen     / 2;
    size_t c2Len  = strlen(_coinb2)       / 2;

    _hexToBytes(_coinb1,      cbBytes + offset, c1Len);  offset += c1Len;
    _hexToBytes(_extranonce1, cbBytes + offset, en1Len); offset += en1Len;
    _hexToBytes(extranonce2,  cbBytes + offset, en2Len); offset += en2Len;
    _hexToBytes(_coinb2,      cbBytes + offset, c2Len);

    uint8_t cbHash[32];
    doubleSha256Once(cbBytes, cbLen, cbHash);
    free(cbBytes);

    for (int i = 0; i < _merkleCount; i++) {
        uint8_t branchBytes[32] = {};
        _hexToBytes(_merkle[i], branchBytes, 32);
        uint8_t concat[64];
        memcpy(concat,      cbHash,      32);
        memcpy(concat + 32, branchBytes, 32);
        doubleSha256Once(concat, 64, cbHash);
    }
    memcpy(_merkleRoot, cbHash, 32);

    // --- Assemble the full 80-byte block header in a temp buffer
    uint8_t header[80] = {};
    uint32_t version = strtoul(_version, nullptr, 16);
    header[0] = (uint8_t)(version & 0xFF);
    header[1] = (uint8_t)((version >> 8)  & 0xFF);
    header[2] = (uint8_t)((version >> 16) & 0xFF);
    header[3] = (uint8_t)((version >> 24) & 0xFF);

    uint8_t prevHashBytes[32] = {};
    _hexToBytes(_prevHash, prevHashBytes, 32);
    for (int i = 0; i < 8; i++) {
        header[4 + i*4 + 0] = prevHashBytes[i*4 + 3];
        header[4 + i*4 + 1] = prevHashBytes[i*4 + 2];
        header[4 + i*4 + 2] = prevHashBytes[i*4 + 1];
        header[4 + i*4 + 3] = prevHashBytes[i*4 + 0];
    }
    memcpy(header + 36, _merkleRoot, 32);

    uint32_t t = strtoul(_ntime, nullptr, 16);
    header[68] = (uint8_t)(t & 0xFF);
    header[69] = (uint8_t)((t >> 8)  & 0xFF);
    header[70] = (uint8_t)((t >> 16) & 0xFF);
    header[71] = (uint8_t)((t >> 24) & 0xFF);

    header[72] = (uint8_t)(_cachedBits & 0xFF);
    header[73] = (uint8_t)((_cachedBits >> 8)  & 0xFF);
    header[74] = (uint8_t)((_cachedBits >> 16) & 0xFF);
    header[75] = (uint8_t)((_cachedBits >> 24) & 0xFF);
    // bytes 76..79 = nonce (zero here; filled by inner loop via _tailBuf)

    // --- Compute midstate over bytes 0..63 once; cache tail 64..79 for reuse
    nerd_mids(_midstate.digest, header);
    memcpy(_tailBuf, header + 64, 16);   // _tailBuf[12..15] will carry the nonce

    _jobReady = true;
}

// ---------------------------------------------------------------------------
// _meetsTarget() — check whether the double-SHA256 hash satisfies the
// nbits-encoded target.  Bitcoin's hash-vs-target comparison treats the
// 32-byte digest as a 256-bit little-endian integer: byte 31 is the MSByte
// and byte 0 is the LSByte.  The nerd_sha256d() early-exit check at A[7]
// already filters bytes 30..31 = 0 (diff ≥ 1), so any hash that reaches
// this function is a real candidate — we do the full numeric compare here
// to avoid submitting low-diff shares that the pool would just reject.
// ---------------------------------------------------------------------------
bool BitcoinMiner::_meetsTarget(const uint8_t* hash32) {
    uint8_t  exp      = (uint8_t)(_cachedBits >> 24);
    uint32_t mantissa = _cachedBits & 0x00FFFFFFu;

    // Build target in LE byte order: target_int = mantissa << (8 * (exp - 3))
    uint8_t target[32] = {};
    if (exp >= 3 && exp <= 32) {
        target[exp - 3] = (uint8_t)( mantissa        & 0xFF);
        target[exp - 2] = (uint8_t)((mantissa >>  8) & 0xFF);
        target[exp - 1] = (uint8_t)((mantissa >> 16) & 0xFF);
    }

    // Compare as LE 256-bit integers: walk from MSByte (index 31) down.
    for (int i = 31; i >= 0; i--) {
        if (hash32[i] < target[i]) return true;
        if (hash32[i] > target[i]) return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// _submitShare() — fire-and-forget: queue id, write request, return.  The
// pool's accept/reject line is consumed by _handleSubmitResponse() on the
// next mine() iteration.  This removes the 5-second blocking read that
// used to freeze the hash loop right after every share.
// ---------------------------------------------------------------------------
void BitcoinMiner::_submitShare(uint32_t nonce) {
    // find a free slot (drop oldest silently if full — pool will just re-reject)
    int slot = -1;
    for (int i = 0; i < kMaxPending; i++) {
        if (!_pending[i].inFlight) { slot = i; break; }
    }
    if (slot < 0) slot = 0;   // overwrite slot 0 rather than lose the submit

    uint32_t id = _nextSubmitId++;
    _pending[slot].id       = id;
    _pending[slot].inFlight = true;

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

    char req[320];
    snprintf(req, sizeof(req),
        "{\"id\":%lu,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}\n",
        (unsigned long)id, worker, _jobId, extranonce2, _ntime, nonceBuf);
    _client.print(req);

    Serial.printf("[btc] Share submitted — id=%lu nonce=%s\n", (unsigned long)id, nonceBuf);
}

// ---------------------------------------------------------------------------
// _handleSubmitResponse() — match a {"id":N,"result":...} line to an in-flight
// share and update accepted/rejected counters accordingly.
// ---------------------------------------------------------------------------
void BitcoinMiner::_handleSubmitResponse(const String& line) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return;
    if (doc["id"].isNull()) return;
    uint32_t id = doc["id"] | 0u;
    if (id < 10) return;   // not one of our submits

    int slot = -1;
    for (int i = 0; i < kMaxPending; i++) {
        if (_pending[i].inFlight && _pending[i].id == id) { slot = i; break; }
    }
    if (slot < 0) return;  // response for an already-forgotten id

    bool accepted = doc["result"].is<bool>() && (doc["result"].as<bool>());
    if (accepted) {
        _stats.sharesAccepted++;
        Serial.printf("[btc] Share %lu accepted\n", (unsigned long)id);
    } else {
        _stats.sharesRejected++;
        Serial.printf("[btc] Share %lu rejected: %s\n", (unsigned long)id, line.c_str());
    }
    _pending[slot].inFlight = false;
}

// ---------------------------------------------------------------------------
// mine() — drain pool messages, then hash a batch using midstate
// ---------------------------------------------------------------------------
void BitcoinMiner::mine() {
    // Drain incoming pool messages: new jobs, difficulty updates, share responses.
    while (_client.available()) {
        String line;
        if (!_readLine(line, 100)) break;
        if (line.indexOf("mining.notify") >= 0) {
            if (_parseNotify(line)) Serial.printf("[btc] New job: %s\n", _jobId);
        } else if (line.indexOf("\"result\"") >= 0) {
            _handleSubmitResponse(line);
        }
    }

    if (!_hasJob || !_jobReady) {
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

    // --- hash loop: yield every 1024 nonces and update hashrate on each yield
    uint32_t startMs   = millis();
    uint32_t hashCount = 0;
    const uint32_t BATCH_SIZE = 10000;
    uint8_t hash[32];

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        uint32_t nonce = _jobNonce++;
        _tailBuf[12] = (uint8_t)(nonce & 0xFF);
        _tailBuf[13] = (uint8_t)((nonce >> 8)  & 0xFF);
        _tailBuf[14] = (uint8_t)((nonce >> 16) & 0xFF);
        _tailBuf[15] = (uint8_t)((nonce >> 24) & 0xFF);

        bool maybe = nerd_sha256d(&_midstate, _tailBuf, hash);
        hashCount++;

        if (maybe && _meetsTarget(hash)) {
            _submitShare(nonce);
        }

        if ((i & 0x3FFu) == 0x3FFu) {
            // Update stats every 1024 hashes so the UI doesn't see a stale
            // value mid-batch when a share or a difficulty change lands.
            uint32_t elapsed = millis() - startMs;
            if (elapsed > 0) _stats.hashrate = (float)hashCount / (float)elapsed;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    uint32_t elapsed = millis() - startMs;
    if (elapsed > 0) _stats.hashrate = (float)hashCount / (float)elapsed;
    _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    _totalHashes += hashCount;

    uint32_t now = millis();
    if (now - _lastReportMs > 30000UL) {
        Serial.printf("[btc] hr=%.1f kH/s total=%lu nonce=%lu accepted=%lu rejected=%lu\n",
                      (double)_stats.hashrate,
                      (unsigned long)_totalHashes,
                      (unsigned long)_jobNonce,
                      (unsigned long)_stats.sharesAccepted,
                      (unsigned long)_stats.sharesRejected);
        _lastReportMs = now;
    }
}

MiningStats BitcoinMiner::getStats() {
    _stats.uptimeSeconds = (millis() - _startMs) / 1000;
    return _stats;
}

void BitcoinMiner::disconnect() {
    if (_client.connected()) _client.stop();
    _hasJob   = false;
    _jobReady = false;
    for (int i = 0; i < kMaxPending; i++) _pending[i].inFlight = false;
}
