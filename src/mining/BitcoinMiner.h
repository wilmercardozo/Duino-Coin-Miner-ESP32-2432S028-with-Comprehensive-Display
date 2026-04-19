// SPDX-License-Identifier: GPL-3.0-or-later
// NerdDuino Pro — Bitcoin Stratum V1 miner header
// Ported from NMMiner architecture; mbedTLS SHA-256d (Apache-2.0)
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

    // Stratum job state
    char _jobId[64]         = "";
    char _prevHash[128]     = "";
    char _coinb1[1024]      = "";   // increased: real coinbases exceed 256 raw bytes
    char _coinb2[1024]      = "";   // increased: real coinbases exceed 256 raw bytes
    char _merkle[8][64]     = {};   // up to 8 merkle branches
    int  _merkleCount       = 0;
    char _version[16]       = "";
    char _nbits[16]         = "";
    char _ntime[16]         = "";
    char _extranonce1[32]   = "";
    int  _extranonce2Size   = 4;
    bool _hasJob            = false;

    // Per-job cached values (computed once in _prepareJob, reused every nonce)
    uint8_t  _merkleRoot[32] = {};
    uint32_t _cachedBits     = 0;
    bool     _jobReady       = false;
    // Full 80-byte header template: bytes 0..75 are constant per job,
    // _buildBlockHeader() just copies this and writes the nonce at 76..79.
    uint8_t  _headerTemplate[80] = {};
    // Periodic hashrate report
    uint32_t _lastReportMs   = 0;
    uint32_t _totalHashes    = 0;

    bool _sendSubscribe();
    bool _sendAuthorize();
    bool _readLine(String& out, uint32_t timeoutMs = 5000);
    bool _parseNotify(const String& line);
    bool _parseSubscribeResponse(const String& line);
    void _prepareJob();
    void _buildBlockHeader(uint32_t nonce, uint8_t* header80);
    void _doubleSha256(const uint8_t* data, size_t len, uint8_t* out32);
    bool _meetsTarget(const uint8_t* hash32);
    bool _submitShare(uint32_t nonce);
    void _hexToBytes(const char* hex, uint8_t* out, size_t maxBytes);
    void _bytesToHex(const uint8_t* in, size_t len, char* out);
};
