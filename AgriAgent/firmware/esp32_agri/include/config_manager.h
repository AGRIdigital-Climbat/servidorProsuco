/*
 * AgriAgents ESP32 - Config Manager
 *
 * Persistencia de configuración en NVS (Non-Volatile Storage)
 * - Configuración del dispositivo (pins, sensores, variables, intervalos)
 * - Credenciales WiFi
 * - Clave HMAC
 * - Estado de sensores
 * - Contadores (seq, uptime, etc.)
 *
 * OTA (Over-The-Air) updates:
 * - Verificación de versión desde API
 * - Descarga y flasheo seguro
 * - Rollback automático en caso de fallo
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Update.h>
#include "config.h"

#define NVS_NAMESPACE "agri_cfg"
#define NVS_KEY_CONFIG "dev_cfg"
#define NVS_KEY_WIFI "wifi_cred"
#define NVS_KEY_HMAC "hmac_key"
#define NVS_KEY_SEQ "seq_cnt"
#define NVS_KEY_UPTIME "uptime"
#define NVS_KEY_BOOT_COUNT "boot_cnt"

#define OTA_CHECK_INTERVAL_MS 3600000  // 1 hora
#define OTA_MAX_SIZE (1024 * 1024)     // 1MB max firmware

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Inicialización
    void init();

    // ──────────────────────────────────────────────────────────────
    // Configuración del dispositivo (NVS)
    // ──────────────────────────────────────────────────────────────
    bool loadConfig(DeviceConfig& config);
    bool saveConfig(const DeviceConfig& config);
    bool clearConfig();

    // ──────────────────────────────────────────────────────────────
    // Sincronización con API
    // ──────────────────────────────────────────────────────────────
    bool fetchDeviceConfig(HttpClient& http, DeviceConfig& config);
    uint32_t checkConfigVersion(HttpClient& http, const char* device_id);
    bool rotateHmacKey(HttpClient& http, DeviceConfig& config);

    // ──────────────────────────────────────────────────────────────
    // WiFi credentials (NVS)
    // ──────────────────────────────────────────────────────────────
    bool loadWiFiCredentials(std::vector<WiFiCredential>& creds);
    bool saveWiFiCredentials(const std::vector<WiFiCredential>& creds);
    bool addWiFiCredential(const char* ssid, const char* password, int32_t priority);
    bool removeWiFiCredential(const char* ssid);

    // ──────────────────────────────────────────────────────────────
    // HMAC key (NVS - cifrada)
    // ──────────────────────────────────────────────────────────────
    bool loadHmacKey(char* buffer, size_t len);
    bool saveHmacKey(const char* key);

    // ──────────────────────────────────────────────────────────────
    // Contadores y estado
    // ──────────────────────────────────────────────────────────────
    uint32_t getSeqCounter();
    bool incrementSeqCounter();
    uint32_t getBootCount();
    bool incrementBootCount();
    uint32_t getTotalUptime();
    bool addUptime(uint32_t seconds);

    // ──────────────────────────────────────────────────────────────
    // OTA Updates
    // ──────────────────────────────────────────────────────────────
    bool checkAndPerformOTA(HttpClient& http);
    bool performOTA(const char* firmware_url, const char* expected_sha256 = nullptr);
    bool verifyFirmwareSignature(const uint8_t* data, size_t len, const char* sha256);

    // ──────────────────────────────────────────────────────────────
    // Sensor detection reporting
    // ──────────────────────────────────────────────────────────────
    bool reportDetectedSensors(HttpClient& http, const std::vector<SensorInfo>& sensors);

private:
    Preferences _prefs;
    bool _initialized = false;

    // Helpers NVS
    bool _putBytes(const char* key, const void* data, size_t len);
    bool _getBytes(const char* key, void* data, size_t maxLen, size_t* outLen);
    bool _putString(const char* key, const char* value);
    bool _getString(const char* key, char* buffer, size_t maxLen);
    bool _putUInt(const char* key, uint32_t value);
    bool _getUInt(const char* key, uint32_t* value);
    bool _removeKey(const char* key);

    // OTA helpers
    static bool _otaWriteCallback(size_t written, size_t total);
    static void _otaProgressCallback(size_t written, size_t total);
};

// ──────────────────────────────────────────────────────────────
// Estructura para reporte de sensores detectados
// ──────────────────────────────────────────────────────────────

struct SensorInfo {
    char sensor_id[32];
    char kind[32];
    char model[32];
    char port[32];
    uint8_t i2c_addr = 0;
    int8_t gpio = -1;
    char driver[32];      // "bme280", "ads1115", "sht31", etc.
    bool auto_detected = true;
    float range_min = -9999;
    float range_max = 9999;
    float error_value = -9999;
};

#endif // CONFIG_MANAGER_H
