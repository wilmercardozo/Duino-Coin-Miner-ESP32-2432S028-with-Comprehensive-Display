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

    // resolved pool host/port (from getPool)
    String   _host;
    int      _port = 0;

    // per-job state
    String   _lastHash;
    String   _expectedHashStr;
    uint8_t  _expectedHash[20];
    uint32_t _jobDiff = 1;

    // chip ID string for submit line
    String   _chipID;

    // time of last successful submit (ms) — reconnect if stale
    uint32_t _lastSubmitMs = 0;

    bool _resolvePool();
    bool _connectToNode();
    bool _waitForData(uint32_t timeoutMs);
    bool _getJob();
    bool _parseJobLine(const String& line);
    uint32_t _dsha1Search();
    bool _submitShare(uint32_t nonce, uint32_t elapsedMs);
    void _updateHashrate(uint32_t hashCount, uint32_t elapsedMs);
    bool _hexToBytes(const String& hex, uint8_t* out, uint32_t outLen);
    void _buildChipID();

    String _clientBuf;

    static constexpr const char* MINER_VER    = "NerdDuino/1.0";
    static constexpr const char* MINER_BANNER = "Official ESP32 Miner";
    static constexpr const char* START_DIFF   = "ESP32";
};
