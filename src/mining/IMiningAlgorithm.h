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
