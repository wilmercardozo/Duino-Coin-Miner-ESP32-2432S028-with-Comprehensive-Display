#pragma once
#include <stdint.h>

struct MiningStats {
    float    hashrate          = 0.0f;   // kH/s (sum across cores for BTC)
    uint32_t sharesAccepted    = 0;
    uint32_t sharesRejected    = 0;
    float    balance           = 0.0f;
    uint32_t uptimeSeconds     = 0;
    uint32_t pingMs            = 0;
    char     poolUrl[64]       = "";
    char     algorithm[16]     = "";     // "DUCO" or "BTC"

    // BTC-only extras (zero for DUCO)
    float    bestDifficulty    = 0.0f;   // highest share diff seen in this session
    uint32_t currentDifficulty = 0;      // pool target diff (derived from nbits)
    uint64_t totalHashes       = 0;      // cumulative hashes since connect
    char     jobId[16]         = "";     // current Stratum job id (truncated)
};

class IMiningAlgorithm {
public:
    virtual ~IMiningAlgorithm() = default;
    virtual bool connect()            = 0;
    virtual void mine()               = 0;   // called in tight loop
    virtual MiningStats getStats()    = 0;
    virtual void disconnect()         = 0;

    // Optional: fetch current wallet balance from the coin's public API.
    // Default no-op (e.g. Stratum BTC pools don't expose a balance concept).
    virtual void fetchBalance() {}
};
