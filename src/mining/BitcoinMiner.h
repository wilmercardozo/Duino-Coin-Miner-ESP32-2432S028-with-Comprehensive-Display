// SPDX-License-Identifier: GPL-3.0-or-later
// NerdDuino Pro — Bitcoin Stratum V1 miner header
// Ported from NMMiner architecture; mbedTLS SHA-256d (Apache-2.0)
#pragma once
#include "IMiningAlgorithm.h"
#include "Config.h"
#include "nerdSHA256plus.h"
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

    // Per-job cached values — midstate approach (nerdSHA256plus):
    // nerd_mids() computes SHA256 state after the first 64 bytes of the header
    // (version + prevHash + first 28 B of merkleRoot) once per job.  For every
    // nonce we only run nerd_sha256d() on the 16 remaining bytes.
    nerdSHA256_context _midstate     = {};
    uint8_t            _tailBuf[16]  = {};  // bytes 64..79 of header; nonce in [12..15]
    uint8_t            _merkleRoot[32] = {};
    uint32_t           _cachedBits     = 0;
    bool               _jobReady       = false;

    // Periodic hashrate report
    uint32_t _lastReportMs   = 0;
    uint32_t _totalHashes    = 0;

    // Pending share submissions (non-blocking): we fire-and-forget the submit
    // line, then read the response on a later mine() iteration. A small FIFO
    // avoids losing accept/reject counts when several shares land close together.
    struct PendingShare { uint32_t id; bool inFlight; };
    static constexpr int kMaxPending = 4;
    PendingShare _pending[kMaxPending] = {};
    uint32_t     _nextSubmitId         = 10;   // Stratum ids 1..9 reserved

    bool _sendSubscribe();
    bool _sendAuthorize();
    bool _readLine(String& out, uint32_t timeoutMs = 5000);
    bool _parseNotify(const String& line);
    bool _parseSubscribeResponse(const String& line);
    void _prepareJob();
    bool _meetsTarget(const uint8_t* hash32);
    void _submitShare(uint32_t nonce);        // now async: queues + returns
    void _handleSubmitResponse(const String& line);
    void _hexToBytes(const char* hex, uint8_t* out, size_t maxBytes);
    void _bytesToHex(const uint8_t* in, size_t len, char* out);
};
