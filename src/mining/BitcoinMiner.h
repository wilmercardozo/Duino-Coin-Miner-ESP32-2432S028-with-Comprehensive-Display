// SPDX-License-Identifier: GPL-3.0-or-later
// NerdDuino Pro — Bitcoin Stratum V1 miner header
// Ported from NMMiner architecture; mbedTLS SHA-256d (Apache-2.0)
#pragma once
#include "IMiningAlgorithm.h"
#include "Config.h"
#include "nerdSHA256plus.h"
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class BitcoinMiner : public IMiningAlgorithm {
public:
    explicit BitcoinMiner(const Config& cfg);
    ~BitcoinMiner() override;

    bool connect()         override;
    void mine()            override;   // primary: pool pump + hash
    MiningStats getStats() override;
    void disconnect()      override;
    void persistStats()    override;   // writes counters to NVS (see StatsStore)

    // Secondary task entry point (dual-core BTC hashing).  No network I/O;
    // just hashes against the current job using an atomic nonce dispenser.
    void secondaryMine();

private:
    const Config& _cfg;
    WiFiClient    _client;
    MiningStats   _stats;
    uint32_t      _startMs        = 0;
    volatile uint32_t _nonceHead  = 0;  // atomic nonce dispenser (both tasks)
    // Snapshot of _totalHashes at connect() — hashrate is computed from
    // (totalNow - sessionHashBase)/sessionMs so that a pre-populated counter
    // restored from NVS doesn't inflate the displayed rate.
    uint64_t      _sessionHashBase = 0;

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
    // Pre-baked first 3 SHA-256 rounds over the tail's nonce-independent
    // bytes (W[0..2] = ntime + nbits + 0). Recomputed once per job in
    // _prepareJob() and consumed by nerd_sha256d_baked() in _hashBatch() —
    // skipping 3 round fn calls + 2 message-schedule ops per nonce.
    uint32_t           _bake[15]       = {};
    volatile bool      _jobReady       = false;

    // Mutexes — created lazily in connect().  _jobMutex guards updates to the
    // job-shared state (_midstate, _tailBuf, _cachedBits, _jobReady, jobId).
    // _clientMutex guards _client writes and _pending[] from the secondary task.
    SemaphoreHandle_t _jobMutex    = nullptr;
    SemaphoreHandle_t _clientMutex = nullptr;

    // Per-session counters (both tasks contribute; updated atomically)
    uint32_t _lastReportMs  = 0;
    uint64_t _totalHashes   = 0;     // sum across both cores
    float    _bestDiff      = 0.0f;  // highest share difficulty seen

    // Per-core counters so the 30 s log can prove both cores are hashing
    // (when one goes silent the combined rate halves without warning).
    // Each counter is written only by its own task, read by primary for
    // logging — use __atomic_load_n to avoid 64-bit tearing on read.
    uint64_t _primaryHashes   = 0;
    uint64_t _secondaryHashes = 0;
    uint64_t _primaryHashesLast   = 0;
    uint64_t _secondaryHashesLast = 0;

    // Stratum watchdog — pools occasionally go silent without closing the
    // TCP socket. If no mining.notify arrives for kStaleNotifyMs, reconnect.
    uint32_t _lastNotifyMs = 0;
    static constexpr uint32_t kStaleNotifyMs = 600000UL;   // 10 min

    // Pool failover: 0 = primary (pool_url/port), 1 = secondary.  Increments
    // _connectFails on TCP/subscribe failure; after kMaxConnectFails we swap
    // to the other pool (if configured).  Reset on successful connect.
    uint8_t _poolIndex    = 0;
    uint8_t _connectFails = 0;
    static constexpr uint8_t kMaxConnectFails = 3;

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

    // Hash a fixed batch using a local copy of the job state.  Returns the
    // number of hashes performed.  Both mine() and secondaryMine() use this.
    // coreCounter is incremented by batchSize — each task passes its own
    // counter so we can tell per-core contribution apart.
    uint32_t _hashBatch(uint32_t batchSize, uint64_t& coreCounter);

    // Compute the approximate share difficulty of a 32-byte hash (LE, byte 31=MSB).
    // Used to track the best share seen.  Cheap: only looks at the top 64 bits.
    static double _shareDifficulty(const uint8_t* hash32);
};
