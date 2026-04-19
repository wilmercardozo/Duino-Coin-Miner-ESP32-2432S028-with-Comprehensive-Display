#pragma once
#include <stdint.h>

enum class Algorithm : uint8_t { DUINOCOIN = 0, BITCOIN = 1 };

struct Config {
    char     wifi_ssid[64]     = "";
    char     wifi_pass[64]     = "";
    Algorithm algorithm        = Algorithm::DUINOCOIN;
    char     duco_user[64]     = "";
    char     duco_key[64]      = "";
    char     btc_address[64]   = "";
    char     pool_url[64]      = "public-pool.io";
    uint16_t pool_port         = 21496;
    // Optional secondary pool — failover target after repeated connect
    // failures on the primary. Leave pool_url2 empty to disable.
    char     pool_url2[64]     = "";
    uint16_t pool_port2        = 0;
    char     rig_name[32]      = "NerdDuino-1";
    int8_t   timezone_offset   = -5;
    bool     valid             = false;
};
