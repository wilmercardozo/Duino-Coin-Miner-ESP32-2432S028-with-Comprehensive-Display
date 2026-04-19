#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cstddef>
#include "Config.h"

// strlcpy is not available on Windows MinGW — provide a portable shim
#ifndef __APPLE__
static size_t strlcpy(char* dst, const char* src, size_t size) {
    if (size == 0) return strlen(src);
    size_t len = strlen(src);
    size_t copy = len < size - 1 ? len : size - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return len;
}
#endif

static bool parseConfigJson(const char* json, Config& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    strlcpy(out.wifi_ssid,   doc["wifi_ssid"]   | "", sizeof(out.wifi_ssid));
    strlcpy(out.wifi_pass,   doc["wifi_pass"]   | "", sizeof(out.wifi_pass));
    strlcpy(out.duco_user,   doc["duco_user"]   | "", sizeof(out.duco_user));
    strlcpy(out.duco_key,    doc["duco_key"]    | "", sizeof(out.duco_key));
    strlcpy(out.btc_address, doc["btc_address"] | "", sizeof(out.btc_address));
    strlcpy(out.pool_url,    doc["pool_url"]    | "public-pool.io", sizeof(out.pool_url));
    out.pool_port       = doc["pool_port"]       | 21496;
    out.timezone_offset = doc["timezone_offset"] | -5;
    out.algorithm = (doc["algorithm"] | 0) == 1 ? Algorithm::BITCOIN : Algorithm::DUINOCOIN;
    out.valid = true;
    return true;
}

void test_parse_duco_config() {
    const char* json = R"({"wifi_ssid":"MyNet","wifi_pass":"pass123","algorithm":0,"duco_user":"will","duco_key":"","pool_url":"public-pool.io","pool_port":21496,"rig_name":"NerdDuino-1","timezone_offset":-5})";
    Config cfg;
    TEST_ASSERT_TRUE(parseConfigJson(json, cfg));
    TEST_ASSERT_EQUAL_STRING("MyNet", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("will", cfg.duco_user);
    TEST_ASSERT_EQUAL(Algorithm::DUINOCOIN, cfg.algorithm);
    TEST_ASSERT_EQUAL(21496, cfg.pool_port);
    TEST_ASSERT_EQUAL(-5, cfg.timezone_offset);
    TEST_ASSERT_TRUE(cfg.valid);
}

void test_parse_bitcoin_config() {
    const char* json = R"({"wifi_ssid":"MyNet","wifi_pass":"pass","algorithm":1,"btc_address":"bc1qxxx","pool_url":"public-pool.io","pool_port":21496,"rig_name":"rig1","timezone_offset":-5})";
    Config cfg;
    TEST_ASSERT_TRUE(parseConfigJson(json, cfg));
    TEST_ASSERT_EQUAL(Algorithm::BITCOIN, cfg.algorithm);
    TEST_ASSERT_EQUAL_STRING("bc1qxxx", cfg.btc_address);
}

void test_defaults_on_missing_fields() {
    const char* json = R"({"wifi_ssid":"x","wifi_pass":"y"})";
    Config cfg;
    TEST_ASSERT_TRUE(parseConfigJson(json, cfg));
    TEST_ASSERT_EQUAL_STRING("public-pool.io", cfg.pool_url);
    TEST_ASSERT_EQUAL(21496, cfg.pool_port);
    TEST_ASSERT_EQUAL(-5, cfg.timezone_offset);
    TEST_ASSERT_EQUAL(Algorithm::DUINOCOIN, cfg.algorithm);
}

void test_invalid_json_returns_false() {
    Config cfg;
    TEST_ASSERT_FALSE(parseConfigJson("{not valid json", cfg));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_duco_config);
    RUN_TEST(test_parse_bitcoin_config);
    RUN_TEST(test_defaults_on_missing_fields);
    RUN_TEST(test_invalid_json_returns_false);
    return UNITY_END();
}
