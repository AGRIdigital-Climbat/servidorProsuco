/*
 * AgriAgents ESP32 - Config Manager Implementation
 */

#include "config_manager.h"
#include "config.h"
#include <Preferences.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

ConfigManager::ConfigManager() {}
ConfigManager::~ConfigManager() {}

void ConfigManager::init() {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: No se pudo inicializar NVS");
        return;
    }
    _initialized = true;
    Serial.println("ConfigManager inicializado");
}

// ──────────────────────────────────────────────────────────────
// Configuración del dispositivo (NVS)
// ──────────────────────────────────────────────────────────────

bool ConfigManager::loadConfig(DeviceConfig& config) {
    if (!_initialized) return false;
    size_t len = sizeof(DeviceConfig);
    return _getBytes(NVS_KEY_CONFIG, &config, sizeof(DeviceConfig), &len);
}

bool ConfigManager::saveConfig(const DeviceConfig& config) {
    if (!_initialized) return false;
    return _putBytes(NVS_KEY_CONFIG, &config, sizeof(DeviceConfig));
}

bool ConfigManager::clearConfig() {
    if (!_initialized) return false;
    return _removeKey(NVS_KEY_CONFIG);
}

// ──────────────────────────────────────────────────────────────
// Sincronización con API
// ──────────────────────────────────────────────────────────────

bool ConfigManager::fetchDeviceConfig(HttpClient& http, DeviceConfig& config) {
    String url = String(config.api.base_url) + "/devices/" + config.device_id + "/config";
    HttpResponse resp = http.get(url, config.api.hmac_key);

    if (resp.status != 200) {
        Serial.printf("ERROR fetch config: %d %s\n", resp.status, resp.body.c_str());
        return false;
    }

    // Parsear JSON response
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, resp.body);
    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    config.config_version = doc["config_version"] | 0;
    config.tz_offset_minutes = doc["tz_offset_minutes"] | -240;

    // Parsear pines
    if (doc.containsKey("pins")) {
        JsonObject pins = doc["pins"];
        if (pins.containsKey("i2c")) {
            config.pins.i2c0_sda = pins["i2c"]["sda"] | 21;
            config.pins.i2c0_scl = pins["i2c"]["scl"] | 22;
        }
        // ... más pines según necesidad
    }

    // Parsear sensores
    if (doc.containsKey("sensors")) {
        JsonArray sensors = doc["sensors"];
        config.sensor_count = 0;
        for (JsonObject s : sensors) {
            if (config.sensor_count >= MAX_SENSORS) break;
            SensorConfig& sc = config.sensors[config.sensor_count++];
            strncpy(sc.sensor_id, s["sensor_id"] | "", sizeof(sc.sensor_id));
            strncpy(sc.kind, s["kind"] | "", sizeof(sc.kind));
            strncpy(sc.model, s["model"] | "", sizeof(sc.model));
            strncpy(sc.port, s["port"] | "", sizeof(sc.port));
            sc.gpio = s["gpio"] | -1;
            sc.i2c_addr = s["i2c_addr"] | 0;
            sc.depth_cm = s["depth_cm"] | 0;
            sc.enabled = s["enabled"] | true;
            sc.interval_ms = s["interval_seconds"] | 60000;
            sc.interval_ms *= 1000;
            sc.interval_night_ms = s["interval_night_seconds"] | 180000;
            sc.interval_night_ms *= 1000;
        }
    }

    // Parsear variables
    if (doc.containsKey("sensors")) {
        JsonArray sensors = doc["sensors"];
        config.variable_count = 0;
        for (JsonObject s : sensors) {
            if (s.containsKey("variables")) {
                JsonArray vars = s["variables"];
                for (JsonObject v : vars) {
                    if (config.variable_count >= MAX_SENSORS * MAX_VARIABLES_PER_SENSOR) break;
                    VariableConfig& vc = config.variables[config.variable_count++];
                    strncpy(vc.key, v["key"] | "", sizeof(vc.key));
                    vc.enabled = v["enabled"] | true;
                    vc.interval_ms = v["interval_seconds"] | 60000;
                    vc.interval_ms *= 1000;
                    vc.interval_night_ms = v["interval_night_seconds"] | 180000;
                    vc.interval_night_ms *= 1000;
                    vc.range_min = v["range_min"] | -9999;
                    vc.range_max = v["range_max"] | 9999;
                    vc.warn_min = v["warn_min"] | -9999;
                    vc.warn_max = v["warn_max"] | 9999;
                    vc.error_value = v["error_value"] | -9999;
                }
            }
        }
    }

    Serial.printf("Config v%d obtenida: %d sensores, %d variables\n",
                  config.config_version, config.sensor_count, config.variable_count);
    return true;
}

uint32_t ConfigManager::checkConfigVersion(HttpClient& http, const char* device_id) {
    String url = String(g_config.api.base_url) + "/devices/" + device_id + "/config";
    HttpResponse resp = http.head(url, g_config.api.hmac_key);

    if (resp.status == 200 && resp.hasHeader("X-Config-Version")) {
        return atoi(resp.getHeader("X-Config-Version").c_str());
    }
    return 0;
}

bool ConfigManager::rotateHmacKey(HttpClient& http, DeviceConfig& config) {
    String url = String(config.api.base_url) + "/devices/" + config.device_id + "/rotate-key";
    HttpResponse resp = http.post(url, config.api.hmac_key, "{}");

    if (resp.status == 200) {
        const char* new_key = resp.json["auth_key_hmac"] | "";
        if (new_key[0] != '\0') {
            strncpy(config.api.hmac_key, new_key, sizeof(config.api.hmac_key));
            saveHmacKey(new_key);
            saveConfig(config);
            Serial.println("HMAC key rotada");
            return true;
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────
// WiFi credentials (NVS)
// ──────────────────────────────────────────────────────────────

bool ConfigManager::loadWiFiCredentials(std::vector<WiFiCredential>& creds) {
    if (!_initialized) return false;
    size_t count = 0;
    if (!_getUInt("wifi_count", &count)) return false;

    creds.clear();
    for (size_t i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "wifi_%zu", i);
        size_t len = sizeof(WiFiCredential);
        WiFiCredential cred;
        if (_getBytes(key, &cred, sizeof(WiFiCredential), &len)) {
            creds.push_back(cred);
        }
    }
    return true;
}

bool ConfigManager::saveWiFiCredentials(const std::vector<WiFiCredential>& creds) {
    if (!_initialized) return false;
    _putUInt("wifi_count", creds.size());
    for (size_t i = 0; i < creds.size(); i++) {
        char key[32];
        snprintf(key, sizeof(key), "wifi_%zu", i);
        _putBytes(key, &creds[i], sizeof(WiFiCredential));
    }
    return true;
}

bool ConfigManager::addWiFiCredential(const char* ssid, const char* password, int32_t priority) {
    std::vector<WiFiCredential> creds;
    loadWiFiCredentials(creds);

    WiFiCredential cred;
    strncpy(cred.ssid, ssid, sizeof(cred.ssid));
    strncpy(cred.password, password, sizeof(cred.password));
    cred.priority = priority;
    cred.enabled = true;
    creds.push_back(cred);

    return saveWiFiCredentials(creds);
}

bool ConfigManager::removeWiFiCredential(const char* ssid) {
    std::vector<WiFiCredential> creds;
    loadWiFiCredentials(creds);

    auto it = std::remove_if(creds.begin(), creds.end(),
                             [ssid](const WiFiCredential& c) { return strcmp(c.ssid, ssid) == 0; });
    if (it != creds.end()) {
        creds.erase(it, creds.end());
        return saveWiFiCredentials(creds);
    }
    return false;
}

// ──────────────────────────────────────────────────────────────
// HMAC key (NVS - cifrada simple)
// ──────────────────────────────────────────────────────────────

bool ConfigManager::loadHmacKey(char* buffer, size_t len) {
    if (!_initialized) return false;
    return _getString(NVS_KEY_HMAC, buffer, len);
}

bool ConfigManager::saveHmacKey(const char* key) {
    if (!_initialized) return false;
    // Simple XOR obfuscation (mejor usar AES en producción)
    char obfuscated[64];
    for (size_t i = 0; i < strlen(key) && i < sizeof(obfuscated) - 1; i++) {
        obfuscated[i] = key[i] ^ 0x55;
    }
    obfuscated[strlen(key)] = '\0';
    return _putString(NVS_KEY_HMAC, obfuscated);
}

// ──────────────────────────────────────────────────────────────
// Contadores y estado
// ──────────────────────────────────────────────────────────────

uint32_t ConfigManager::getSeqCounter() {
    uint32_t val = 0;
    _getUInt(NVS_KEY_SEQ, &val);
    return val;
}

bool ConfigManager::incrementSeqCounter() {
    uint32_t val = getSeqCounter() + 1;
    return _putUInt(NVS_KEY_SEQ, val);
}

uint32_t ConfigManager::getBootCount() {
    uint32_t val = 0;
    _getUInt(NVS_KEY_BOOT_COUNT, &val);
    return val;
}

bool ConfigManager::incrementBootCount() {
    uint32_t val = getBootCount() + 1;
    return _putUInt(NVS_KEY_BOOT_COUNT, val);
}

uint32_t ConfigManager::getTotalUptime() {
    uint32_t val = 0;
    _getUInt(NVS_KEY_UPTIME, &val);
    return val;
}

bool ConfigManager::addUptime(uint32_t seconds) {
    uint32_t val = getTotalUptime() + seconds;
    return _putUInt(NVS_KEY_UPTIME, val);
}

// ──────────────────────────────────────────────────────────────
// OTA Updates - HTTP/HTTPS con soporte celular
// ──────────────────────────────────────────────────────────────

bool ConfigManager::checkAndPerformOTA(HttpClient& http) {
    String url = String(g_config.api.base_url) + "/devices/" + g_config.device_id + "/firmware/check";
    HttpResponse resp = http.get(url, g_config.api.hmac_key);

    if (resp.status != 200) return false;

    const char* firmware_url = resp.json["firmware_url"] | "";
    const char* version = resp.json["version"] | "";
    const char* sha256 = resp.json["sha256"] | "";

    if (firmware_url[0] == '\0' || strcmp(version, FIRMWARE_VERSION) == 0) {
        return false; // No hay actualización
    }

    Serial.printf("Nueva versión disponible: %s (actual: %s)\n", version, FIRMWARE_VERSION);
    return performOTA(firmware_url, sha256);
}

bool ConfigManager::performOTA(const char* firmware_url, const char* expected_sha256) {
    Serial.printf("Iniciando OTA desde: %s\n", firmware_url);

    // Cliente HTTP (WiFi o Cellular via TinyGSM)
    Client* client = http.getClient();
    if (!client) {
        Serial.println("ERROR: No hay cliente de red");
        return false;
    }

    // Iniciar OTA
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Serial.printf("ERROR: Update.begin() failed: %s\n", Update.errorString());
        return false;
    }

    Serial.println("Descargando firmware...");

    // HTTP GET streaming
    int httpCode = http.streamGet(firmware_url, g_config.api.hmac_key, [&](uint8_t* data, size_t len) {
        if (Update.write(data, len) != len) {
            Serial.printf("ERROR escribiendo OTA: %s\n", Update.errorString());
            return false;
        }
        return true;
    });

    if (httpCode != 200) {
        Serial.printf("ERROR HTTP: %d\n", httpCode);
        Update.abort();
        return false;
    }

    // Verificar SHA256 si se proporciona
    if (expected_sha256 && expected_sha256[0] != '\0') {
        // TODO: Verificar hash del firmware descargado
    }

    if (!Update.end(true)) {
        Serial.printf("ERROR finalizando OTA: %s\n", Update.errorString());
        return false;
    }

    if (Update.hasError()) {
        Serial.printf("ERROR OTA: %s\n", Update.errorString());
        return false;
    }

    Serial.println("OTA completado con éxito - Reiniciando...");
    ESP.restart();
    return true; // Nunca se llega aquí
}

bool ConfigManager::verifyFirmwareSignature(const uint8_t* data, size_t len, const char* sha256) {
    // TODO: Implementar verificación SHA256
    return true;
}

// ──────────────────────────────────────────────────────────────
// Sensor detection reporting
// ──────────────────────────────────────────────────────────────

bool ConfigManager::reportDetectedSensors(HttpClient& http, const std::vector<SensorInfo>& sensors) {
    String url = String(g_config.api.base_url) + "/devices/" + g_config.device_id + "/sensors/report";

    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.createNestedArray("sensors");
    for (const auto& s : sensors) {
        JsonObject obj = arr.createNestedObject();
        obj["sensor_id"] = s.sensor_id;
        obj["kind"] = s.kind;
        obj["model"] = s.model;
        obj["port"] = s.port;
        obj["i2c_addr"] = s.i2c_addr;
        obj["gpio"] = s.gpio;
        obj["driver"] = s.driver;
        obj["auto_detected"] = s.auto_detected;
        obj["range_min"] = s.range_min;
        obj["range_max"] = s.range_max;
        obj["error_value"] = s.error_value;
    }

    String body;
    serializeJson(doc, body);

    HttpResponse resp = http.post(url, g_config.api.hmac_key, body);
    return resp.status == 200 || resp.status == 202;
}

// ──────────────────────────────────────────────────────────────
// Helpers NVS
// ──────────────────────────────────────────────────────────────

bool ConfigManager::_putBytes(const char* key, const void* data, size_t len) {
    return _prefs.putBytes(key, data, len);
}

bool ConfigManager::_getBytes(const char* key, void* data, size_t maxLen, size_t* outLen) {
    size_t len = _prefs.getBytesLength(key);
    if (len == 0 || len > maxLen) return false;
    *outLen = _prefs.getBytes(key, data, maxLen);
    return *outLen == len;
}

bool ConfigManager::_putString(const char* key, const char* value) {
    return _prefs.putString(key, value);
}

bool ConfigManager::_getString(const char* key, char* buffer, size_t maxLen) {
    String val = _prefs.getString(key, "");
    if (val.length() == 0) return false;
    if (val.length() >= maxLen) return false;
    strcpy(buffer, val.c_str());
    // Desofuscar XOR
    for (size_t i = 0; i < strlen(buffer); i++) {
        buffer[i] ^= 0x55;
    }
    return true;
}

bool ConfigManager::_putUInt(const char* key, uint32_t value) {
    return _prefs.putUInt(key, value);
}

bool ConfigManager::_getUInt(const char* key, uint32_t* value) {
    *value = _prefs.getUInt(key, 0);
    return true;
}

bool ConfigManager::_removeKey(const char* key) {
    return _prefs.remove(key);
}

// ──────────────────────────────────────────────────────────────
// OTA Callbacks
// ──────────────────────────────────────────────────────────────

bool ConfigManager::_otaWriteCallback(size_t written, size_t total) {
    static uint32_t last_pct = 0;
    uint32_t pct = total > 0 ? (written * 100 / total) : 0;
    if (pct != last_pct && pct % 10 == 0) {
        Serial.printf("OTA progreso: %d%%\n", pct);
        last_pct = pct;
    }
    return true;
}

void ConfigManager::_otaProgressCallback(size_t written, size_t total) {
    _otaWriteCallback(written, total);
}
