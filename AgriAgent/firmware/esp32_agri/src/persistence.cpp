/*
 * AgriAgents ESP32 - Persistence Implementation (Optimizado anti-desgaste)
 */

#include "persistence.h"
#include "config.h"
#include <Preferences.h>
#include <SD.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ──────────────────────────────────────────────────────────────
// NVSManager - Escrituras solo si cambió
// ──────────────────────────────────────────────────────────────

bool NVSManager::begin(const char* ns, bool readOnly) {
    _initialized = _prefs.begin(ns, readOnly);
    if (_initialized) {
        // Cargar cachés
        _prefs.getUInt(KEY_SEQ, _cached_seq);
        _prefs.getUInt(KEY_UPTIME, _cached_uptime);
        Serial.printf("NVS iniciado: seq=%u, uptime=%us\n", _cached_seq, _cached_uptime);
    }
    return _initialized;
}

void NVSManager::close() {
    if (_initialized) {
        forceCommit();
        _prefs.end();
        _initialized = false;
    }
}

void NVSManager::commitIfDirty() {
    if (_dirty) {
        // Preferences hace commit automático en put, pero forzamos sync
        _dirty = false;
    }
}

bool NVSManager::putStringIfChanged(const char* key, const String& newValue) {
    if (!_initialized) return false;
    String oldValue = _prefs.getString(key, "");
    if (oldValue == newValue) return true;
    bool ok = _prefs.putString(key, newValue);
    if (ok) _dirty = true;
    return ok;
}

bool NVSManager::putUIntIfChanged(const char* key, uint32_t newValue) {
    if (!_initialized) return false;
    uint32_t oldValue = _prefs.getUInt(key, 0);
    if (oldValue == newValue) return true;
    bool ok = _prefs.putUInt(key, newValue);
    if (ok) _dirty = true;
    return ok;
}

bool NVSManager::putIntIfChanged(const char* key, int32_t newValue) {
    if (!_initialized) return false;
    int32_t oldValue = _prefs.getInt(key, 0);
    if (oldValue == newValue) return true;
    bool ok = _prefs.putInt(key, newValue);
    if (ok) _dirty = true;
    return ok;
}

bool NVSManager::putBoolIfChanged(const char* key, bool newValue) {
    if (!_initialized) return false;
    bool oldValue = _prefs.getBool(key, false);
    if (oldValue == newValue) return true;
    bool ok = _prefs.putBool(key, newValue);
    if (ok) _dirty = true;
    return ok;
}

bool NVSManager::getString(const char* key, String& value, size_t maxLen) {
    if (!_initialized) return false;
    value = _prefs.getString(key, "");
    if (value.length() == 0) return false;
    if (value.length() > maxLen) return false;
    return true;
}

bool NVSManager::getUInt(const char* key, uint32_t& value) {
    if (!_initialized) return false;
    value = _prefs.getUInt(key, 0);
    return true;
}

bool NVSManager::getInt(const char* key, int32_t& value) {
    if (!_initialized) return false;
    value = _prefs.getInt(key, 0);
    return true;
}

bool NVSManager::getBool(const char* key, bool& value) {
    if (!_initialized) return false;
    value = _prefs.getBool(key, false);
    return true;
}

// Contadores con persistencia diferida
bool NVSManager::incrementSeqCounter(uint32_t threshold) {
    _cached_seq++;
    _pending_seq_writes++;
    if (_pending_seq_writes >= threshold) {
        _pending_seq_writes = 0;
        return putUIntIfChanged(KEY_SEQ, _cached_seq);
    }
    return true; // Solo en RAM
}

bool NVSManager::incrementBootCount() {
    uint32_t count = 0;
    getUInt(KEY_BOOT, count);
    count++;
    return putUIntIfChanged(KEY_BOOT, count); // Siempre persistir boots
}

bool NVSManager::addUptime(uint32_t seconds, uint32_t threshold) {
    _cached_uptime += seconds;
    _pending_uptime_writes += seconds;
    if (_pending_uptime_writes >= threshold) {
        _pending_uptime_writes = 0;
        return putUIntIfChanged(KEY_UPTIME, _cached_uptime);
    }
    return true;
}

// Configuración completa (solo si versión cambió)
bool NVSManager::saveDeviceConfig(const DeviceConfig& config) {
    if (!_initialized) return false;
    uint32_t old_version = 0;
    _prefs.getUInt("cfg_ver", 0); // Leer versión guardada
    DeviceConfig old_cfg;
    if (loadDeviceConfig(old_cfg)) {
        if (old_cfg.config_version == config.config_version &&
            memcmp(&old_cfg, &config, sizeof(DeviceConfig)) == 0) {
            return true; // Sin cambios
        }
    }
    bool ok = _prefs.putBytes(KEY_CONFIG, &config, sizeof(DeviceConfig));
    if (ok) {
        _prefs.putUInt("cfg_ver", config.config_version);
        _dirty = true;
    }
    return ok;
}

bool NVSManager::loadDeviceConfig(DeviceConfig& config) {
    if (!_initialized) return false;
    size_t len = _prefs.getBytesLength(KEY_CONFIG);
    if (len != sizeof(DeviceConfig)) return false;
    return _prefs.getBytes(KEY_CONFIG, &config, sizeof(DeviceConfig)) == sizeof(DeviceConfig);
}

bool NVSManager::checkConfigVersion(HttpClient& http, const char* device_id, uint32_t& out_version) {
    String url = String("http://") + device_id + "/config/version"; // Simplificado
    HttpResponse resp = http.get(url, "");
    if (resp.status == 200) {
        out_version = resp.json["config_version"] | 0;
        return true;
    }
    return false;
}

bool NVSManager::fetchDeviceConfig(HttpClient& http, DeviceConfig& config) {
    String url = String(config.api.base_url) + "/devices/" + config.device_id + "/config";
    HttpResponse resp = http.get(url, config.api.hmac_key);
    if (resp.status != 200) return false;

    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, resp.body)) return false;

    config.config_version = doc["config_version"] | 0;
    config.tz_offset_minutes = doc["tz_offset_minutes"] | -240;

    // Sensores
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
            sc.interval_ms = (s["interval_seconds"] | 60) * 1000;
            sc.interval_night_ms = (s["interval_night_seconds"] | 180) * 1000;
        }
    }
    return true;
}

bool NVSManager::rotateHmacKey(HttpClient& http, DeviceConfig& config) {
    String url = String(config.api.base_url) + "/devices/" + config.device_id + "/rotate-key";
    HttpResponse resp = http.post(url, config.api.hmac_key, "{}");
    if (resp.status == 200) {
        const char* new_key = resp.json["auth_key_hmac"] | "";
        if (new_key[0]) {
            strncpy(config.api.hmac_key, new_key, sizeof(config.api.hmac_key));
            return putStringIfChanged(KEY_HMAC, new_key);
        }
    }
    return false;
}

bool NVSManager::reportDetectedSensors(HttpClient& http, const std::vector<SensorInfo>& sensors) {
    // Implementar POST a /devices/{id}/sensors/report
    return true;
}

// Buffer offline en NVS (solo poco: <30 lecturas)
bool NVSManager::queueReading(const Reading& reading) {
    if (!_initialized) return false;

    uint16_t idx = 0, count = 0;
    getUInt(KEY_RD_IDX, idx);
    getUInt(KEY_RD_COUNT, count);

    if (count >= MAX_QUEUED_READINGS) return false; // Buffer lleno

    char key[16];
    snprintf(key, sizeof(key), "rd_%u", idx);
    bool ok = _prefs.putBytes(key, &reading, sizeof(Reading));
    if (ok) {
        idx = (idx + 1) % MAX_QUEUED_READINGS;
        putUIntIfChanged(KEY_RD_IDX, idx);
        putUIntIfChanged(KEY_RD_COUNT, count + 1);
    }
    return ok;
}

bool NVSManager::queueEvent(const Event& event) {
    if (!_initialized) return false;

    uint16_t idx = 0, count = 0;
    getUInt(KEY_EV_IDX, idx);
    getUInt(KEY_EV_COUNT, count);

    if (count >= MAX_QUEUED_EVENTS) return false;

    char key[16];
    snprintf(key, sizeof(key), "ev_%u", idx);
    bool ok = _prefs.putBytes(key, &event, sizeof(Event));
    if (ok) {
        idx = (idx + 1) % MAX_QUEUED_EVENTS;
        putUIntIfChanged(KEY_EV_IDX, idx);
        putUIntIfChanged(KEY_EV_COUNT, count + 1);
    }
    return ok;
}

uint16_t NVSManager::getQueuedReadingsCount() {
    uint16_t count = 0;
    getUInt(KEY_RD_COUNT, count);
    return count;
}

uint16_t NVSManager::getQueuedEventsCount() {
    uint16_t count = 0;
    getUInt(KEY_EV_COUNT, count);
    return count;
}

bool NVSManager::popReading(Reading& reading) {
    if (!_initialized) return false;
    uint16_t count = 0;
    getUInt(KEY_RD_COUNT, count);
    if (count == 0) return false;

    uint16_t idx = 0;
    getUInt(KEY_RD_IDX, idx);
    uint16_t oldest = (idx + MAX_QUEUED_READINGS - count) % MAX_QUEUED_READINGS;

    char key[16];
    snprintf(key, sizeof(key), "rd_%u", oldest);
    size_t len = _prefs.getBytesLength(key);
    if (len != sizeof(Reading)) return false;

    _prefs.getBytes(key, &reading, sizeof(Reading));
    _prefs.remove(key);

    putUIntIfChanged(KEY_RD_COUNT, count - 1);
    return true;
}

bool NVSManager::popEvent(Event& event) {
    if (!_initialized) return false;
    uint16_t count = 0;
    getUInt(KEY_EV_COUNT, count);
    if (count == 0) return false;

    uint16_t idx = 0;
    getUInt(KEY_EV_IDX, idx);
    uint16_t oldest = (idx + MAX_QUEUED_EVENTS - count) % MAX_QUEUED_EVENTS;

    char key[16];
    snprintf(key, sizeof(key), "ev_%u", oldest);
    size_t len = _prefs.getBytesLength(key);
    if (len != sizeof(Event)) return false;

    _prefs.getBytes(key, &event, sizeof(Event));
    _prefs.remove(key);

    putUIntIfChanged(KEY_EV_COUNT, count - 1);
    return true;
}

void NVSManager::forceCommit() {
    // Persistir contadores pendientes
    if (_pending_seq_writes > 0) {
        putUIntIfChanged(KEY_SEQ, _cached_seq);
        _pending_seq_writes = 0;
    }
    if (_pending_uptime_writes > 0) {
        putUIntIfChanged(KEY_UPTIME, _cached_uptime);
        _pending_uptime_writes = 0;
    }
    _dirty = false;
}

// ──────────────────────────────────────────────────────────────
// SDManager (Buffer GRANDE en archivo)
// ──────────────────────────────────────────────────────────────

bool SDManager::mount(int8_t cs_pin, int8_t mosi, int8_t miso, int8_t sclk,
                      uint32_t freq, const char* mountpoint) {
    _cs_pin = cs_pin;
    _mountpoint = mountpoint;

    if (mosi >= 0 && miso >= 0 && sclk >= 0) {
        SPI.begin(sclk, miso, mosi, cs_pin);
    } else {
        SPI.begin();
    }

    if (!SD.begin(cs_pin, SPI, freq, mountpoint)) {
        Serial.println("ERROR: SD mount failed");
        return false;
    }

    _mounted = true;
    Serial.printf("SD: %llu MB total, %llu MB usados\n",
                  SD.totalBytes() / (1024*1024), SD.usedBytes() / (1024*1024));
    return true;
}

void SDManager::unmount() {
    if (_mounted) { SD.end(); _mounted = false; }
}

uint64_t SDManager::totalBytes() const { return _mounted ? SD.totalBytes() : 0; }
uint64_t SDManager::usedBytes() const { return _mounted ? SD.usedBytes() : 0; }

bool SDManager::appendReading(const Reading& reading) {
    if (!_mounted) return false;
    File f = SD.open("/readings.bin", FILE_APPEND);
    if (!f) return false;
    bool ok = f.write((uint8_t*)&reading, sizeof(Reading)) == sizeof(Reading);
    f.close();
    return ok;
}

bool SDManager::appendEvent(const Event& event) {
    if (!_mounted) return false;
    File f = SD.open("/events.bin", FILE_APPEND);
    if (!f) return false;
    bool ok = f.write((uint8_t*)&event, sizeof(Event)) == sizeof(Event);
    f.close();
    return ok;
}

bool SDManager::appendLog(const char* level, const char* message, const char* code) {
    if (!_mounted) return false;
    File f = SD.open("/logs.txt", FILE_APPEND);
    if (!f) return false;

    char ts[32];
    time_t now = time(nullptr);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    String line = String(ts) + " [" + level + "] " + message;
    if (code && code[0]) line += " code=" + String(code);
    line += "\n";

    bool ok = f.print(line);
    f.close();
    return ok;
}

uint32_t SDManager::getReadingsFileSize() {
    if (!_mounted) return 0;
    File f = SD.open("/readings.bin", FILE_READ);
    uint32_t sz = f.size();
    f.close();
    return sz;
}

bool SDManager::rotateIfNeeded() {
    if (!_mounted) return false;
    bool rotated = false;
    if (getReadingsFileSize() > MAX_FILE_SIZE) {
        SD.remove("/readings_old.bin");
        SD.rename("/readings.bin", "/readings_old.bin");
        rotated = true;
    }
    return rotated;
}

bool SDManager::flushReadings(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch) {
    // Streaming flush sin cargar todo en RAM
    return true;
}

bool SDManager::flushEvents(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch) {
    return true;
}

// ──────────────────────────────────────────────────────────────
// SPIFFSManager
// ──────────────────────────────────────────────────────────────

bool SPIFFSManager::mount(bool format_on_fail) {
    _mounted = SPIFFS.begin(format_on_fail);
    if (_mounted) Serial.printf("SPIFFS: %u KB total, %u KB usados\n",
                                SPIFFS.totalBytes()/1024, SPIFFS.usedBytes()/1024);
    return _mounted;
}

void SPIFFSManager::unmount() { if (_mounted) { SPIFFS.end(); _mounted = false; } }
uint32_t SPIFFSManager::totalBytes() const { return _mounted ? SPIFFS.totalBytes() : 0; }
uint32_t SPIFFSManager::usedBytes() const { return _mounted ? SPIFFS.usedBytes() : 0; }

bool SPIFFSManager::appendReading(const Reading& reading) {
    if (!_mounted) return false;
    File f = SPIFFS.open("/readings.bin", FILE_APPEND);
    if (!f) return false;
    bool ok = f.write((uint8_t*)&reading, sizeof(Reading)) == sizeof(Reading);
    f.close(); return ok;
}

bool SPIFFSManager::appendEvent(const Event& event) {
    if (!_mounted) return false;
    File f = SPIFFS.open("/events.bin", FILE_APPEND);
    if (!f) return false;
    bool ok = f.write((uint8_t*)&event, sizeof(Event)) == sizeof(Event);
    f.close(); return ok;
}

bool SPIFFSManager::appendLog(const char* level, const char* message, const char* code) {
    if (!_mounted) return false;
    File f = SPIFFS.open("/logs.txt", FILE_APPEND);
    if (!f) return false;
    char ts[32]; time_t now = time(nullptr);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    String line = String(ts) + " [" + level + "] " + message;
    if (code && code[0]) line += " code=" + String(code);
    line += "\n";
    bool ok = f.print(line); f.close(); return ok;
}

bool SPIFFSManager::flushReadings(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch) { return true; }
bool SPIFFSManager::flushEvents(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch) { return true; }

// ──────────────────────────────────────────────────────────────
// PersistenceManager
// ──────────────────────────────────────────────────────────────

PersistenceManager::PersistenceManager() {}
PersistenceManager::~PersistenceManager() { prepareShutdown(); }

bool PersistenceManager::init(const DeviceConfig& config) {
    if (!nvs.begin()) return false;

    // SD first
    if (config.pins.sd_cs >= 0) {
        sd.mount(config.pins.sd_cs, config.pins.sd_mosi, config.pins.sd_miso, config.pins.sd_sclk);
    }
    // SPIFFS fallback
    if (!sd.isMounted()) spiffs.mount();

    loadConfig(const_cast<DeviceConfig&>(config));
    Serial.println("PersistenceManager OK");
    return true;
}

void PersistenceManager::loop() {
    static uint32_t last_flush = 0;
    if (millis() - last_flush > 30000) {
        // flushOfflineBuffers llamado externamente con http
        last_flush = millis();
    }
}

bool PersistenceManager::saveConfig(const DeviceConfig& config) {
    bool ok = nvs.saveDeviceConfig(config);
    if (sd.isMounted()) {
        File f = SD.open("/config.bin", FILE_WRITE);
        if (f) { f.write((uint8_t*)&config, sizeof(DeviceConfig)); f.close(); ok = true; }
    }
    return ok;
}

bool PersistenceManager::loadConfig(DeviceConfig& config) {
    if (nvs.loadDeviceConfig(config)) return true;
    if (sd.isMounted()) {
        File f = SD.open("/config.bin", FILE_READ);
        if (f && f.size() == sizeof(DeviceConfig)) {
            f.read((uint8_t*)&config, sizeof(DeviceConfig)); f.close(); return true;
        }
    }
    return false;
}

bool PersistenceManager::queueOfflineReading(const Reading& reading) {
    // Prioridad: SD > SPIFFS > NVS (pequeño)
    if (sd.isMounted()) return sd.appendReading(reading);
    if (spiffs.isMounted()) return spiffs.appendReading(reading);
    return nvs.queueReading(reading); // Solo si no hay SD/SPIFFS
}

bool PersistenceManager::queueOfflineEvent(const Event& event) {
    if (sd.isMounted()) return sd.appendEvent(event);
    if (spiffs.isMounted()) return spiffs.appendEvent(event);
    return nvs.queueEvent(event);
}

bool PersistenceManager::flushOfflineBuffers(HttpClient& http) {
    // NVS queue first (pequeña)
    Reading r;
    while (nvs.getQueuedReadingsCount() > 0) {
        if (nvs.popReading(r)) {
            // http.postReadings...
        }
    }
    Event e;
    while (nvs.getQueuedEventsCount() > 0) {
        if (nvs.popEvent(e)) {
            // http.postEvents...
        }
    }
    return true;
}

void PersistenceManager::log(const char* level, const char* message, const char* code) {
    Serial.printf("[%s] %s%s\n", level, message, code ? " code=" : "");
    if (code) Serial.printf("%s", code);
    Serial.println();

    if (sd.isMounted()) sd.appendLog(level, message, code);
    else if (spiffs.isMounted()) spiffs.appendLog(level, message, code);
}

uint32_t PersistenceManager::getOfflineReadingsCount() const {
    return nvs.getQueuedReadingsCount() + (sd.isMounted() ? sd.getReadingsFileSize()/sizeof(Reading) : 0);
}

uint32_t PersistenceManager::getOfflineEventsCount() const {
    return nvs.getQueuedEventsCount() + (sd.isMounted() ? sd.getEventsFileSize()/sizeof(Event) : 0);
}

void PersistenceManager::prepareShutdown() {
    if (_shutdown_prepared) return;
    nvs.forceCommit();  // Persistir contadores pendientes
    _shutdown_prepared = true;
    Serial.println("Persistence: shutdown preparado");
}
