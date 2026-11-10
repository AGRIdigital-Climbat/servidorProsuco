/*
 * AgriAgents ESP32 Firmware - Configuración principal
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "board_config.h"

// ──────────────────────────────────────────────────────────────
// Versión del firmware
// ──────────────────────────────────────────────────────────────
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_BUILD_DATE __DATE__ " " __TIME__

// ──────────────────────────────────────────────────────────────
// Configuración por defecto (sobrescribible por API)
// ──────────────────────────────────────────────────────────────

// API
#define DEFAULT_API_BASE_URL "http://192.168.1.100:8080/v1"
#define DEFAULT_DEVICE_ID "agri-esp32"
#define DEFAULT_HMAC_KEY ""  // Se obtiene vía API al registrar

// Timeouts
#define HTTP_TIMEOUT_MS 15000
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RECONNECT_INTERVAL_MS 60000
#define CONFIG_POLL_INTERVAL_MS 300000  // 5 min
#define READINGS_BATCH_INTERVAL_MS 60000  // 1 min default
#define TASK_POLL_INTERVAL_MS 10000
#define COMMAND_POLL_INTERVAL_MS 5000

// Watchdog
#define WDT_TIMEOUT_MS 8000
#define WDT_FEED_INTERVAL_MS 1000

// Buffer offline
#define MAX_OFFLINE_READINGS 500
#define MAX_OFFLINE_EVENTS 100
#define OFFLINE_FLUSH_INTERVAL_MS 30000

// Sensores
#define MAX_SENSORS 20
#define MAX_VARIABLES_PER_SENSOR 10
#define I2C_SCAN_INTERVAL_MS 60000
#define ADC_SAMPLES 16
#define ADC_ATTENUATION ADC_11db  // 0-3.3V

// Logging
#define LOG_BUFFER_SIZE 2048
#define MAX_LOG_ENTRIES 50

// ──────────────────────────────────────────────────────────────
// Estructuras de configuración runtime
// ──────────────────────────────────────────────────────────────

struct WiFiConfig {
    char ssid[64];
    char password[64];
    char hostname[32];
    bool dhcp = true;
    IPAddress static_ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
};

struct APIConfig {
    char base_url[128];
    char device_id[64];
    char hmac_key[64];
    bool use_https = false;
    char ca_cert[1024];
};

struct SensorConfig {
    char sensor_id[32];
    char kind[32];
    char model[32];
    char port[32];       // "i2c:0x77", "adc:1", "gpio:32", "pulse:27"
    int8_t gpio = -1;
    int8_t i2c_addr = -1;
    int depth_cm = 0;
    bool enabled = true;
    uint32_t interval_ms = 60000;
    uint32_t interval_night_ms = 180000;
};

struct VariableConfig {
    char key[32];
    bool enabled = true;
    uint32_t interval_ms = 60000;
    uint32_t interval_night_ms = 180000;
    float range_min = -9999;
    float range_max = 9999;
    float warn_min = -9999;
    float warn_max = 9999;
    float error_value = -9999;
};

struct DeviceConfig {
    uint32_t config_version = 0;
    int tz_offset_minutes = -240;  // BOT UTC-4
    WiFiConfig wifi;
    APIConfig api;
    SensorConfig sensors[MAX_SENSORS];
    uint8_t sensor_count = 0;
    VariableConfig variables[MAX_SENSORS * MAX_VARIABLES_PER_SENSOR];
    uint8_t variable_count = 0;
    BoardPins pins;
    BoardModel board_model = BOARD_UNKNOWN;
};

struct Reading {
    char key[32];
    float value;
    char quality[16];  // "ok", "suspect", "error", "stale"
    char timestamp[32];
    uint32_t seq;
};

struct Event {
    char kind[32];
    char level[16];    // "debug", "info", "warning", "error", "critical"
    char message[256];
    char code[32];
    char timestamp[32];
};

struct Command {
    char command_id[64];
    char kind[32];     // "actuate", "restart", "pin_test", "self_test"
    char action[32];   // "on", "off", "pwm", "read", "write", "adc", "i2c_scan"
    char pin_name[32];
    int8_t gpio = -1;
    int value = 0;
    uint32_t duration_ms = 0;
    int freq_hz = 0;
    int channel = 0;
    char tracking_id[64];
};

extern DeviceConfig g_config;

#endif // CONFIG_H
