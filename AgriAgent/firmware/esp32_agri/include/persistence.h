/*
 * AgriAgents ESP32 - Persistencia Optimizada (NVS + SD/SPIFFS)
 *
 * Principios anti-desgaste NVS:
 * 1. SOLO escribir si el valor CAMBIÓ (comparar antes de escribir)
 * 2. Agrupar escrituras relacionadas (batch)
 * 3. Contadores: solo persistir cada N incrementos o en shutdown
 * 4. Buffer offline: NVS para poco (<50), SD/SPIFFS para mucho
 * 5. Config completa: solo guardar si versión cambió
 */

#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <Arduino.h>
#include <Preferences.h>
#include <SD.h>
#include <SPIFFS.h>
#include "config.h"

// ──────────────────────────────────────────────────────────────
// NVS Manager (Escrituras controladas)
// ──────────────────────────────────────────────────────────────

class NVSManager {
public:
    NVSManager() : _initialized(false), _dirty(false) {}
    ~NVSManager() { close(); }

    bool begin(const char* ns = "agri_cfg", bool readOnly = false);
    void close();
    void markDirty() { _dirty = true; }
    void commitIfDirty();  // Solo escribe si hay cambios

    // Key-Value con verificación de cambio
    template<typename T>
    bool putIfChanged(const char* key, const T& newValue) {
        if (!_initialized) return false;
        T oldValue;
        if (get(key, oldValue)) {
            if (memcmp(&oldValue, &newValue, sizeof(T)) == 0) return true; // Sin cambios
        }
        bool ok = _prefs.putBytes(key, &newValue, sizeof(T));
        if (ok) _dirty = true;
        return ok;
    }

    bool putStringIfChanged(const char* key, const String& newValue);
    bool putUIntIfChanged(const char* key, uint32_t newValue);
    bool putIntIfChanged(const char* key, int32_t newValue);
    bool putBoolIfChanged(const char* key, bool newValue);

    // Lectura (cache en RAM opcional)
    template<typename T>
    bool get(const char* key, T& value) {
        if (!_initialized) return false;
        size_t len = _prefs.getBytesLength(key);
        if (len != sizeof(T)) return false;
        return _prefs.getBytes(key, &value, sizeof(T)) == sizeof(T);
    }

    bool getString(const char* key, String& value, size_t maxLen = 256);
    bool getUInt(const char* key, uint32_t& value);
    bool getInt(const char* key, int32_t& value);
    bool getBool(const char* key, bool& value);

    // Contadores con persistencia diferida (cada N o en shutdown)
    bool incrementSeqCounter(uint32_t threshold = 10);  // Persistir cada 10
    bool incrementBootCount();  // Siempre persistir
    bool addUptime(uint32_t seconds, uint32_t threshold = 60); // Cada 60s

    // Configuración completa (solo si versión cambió)
    bool saveDeviceConfig(const DeviceConfig& config);
    bool loadDeviceConfig(DeviceConfig& config);
    bool checkConfigVersion(HttpClient& http, const char* device_id, uint32_t& out_version);
    bool fetchDeviceConfig(HttpClient& http, DeviceConfig& config);
    bool rotateHmacKey(HttpClient& http, DeviceConfig& config);
    bool reportDetectedSensors(HttpClient& http, const std::vector<SensorInfo>& sensors);

    // Buffer offline (NVS solo para poco: <50 items)
    bool queueReading(const Reading& reading);
    bool queueEvent(const Event& event);
    uint16_t getQueuedReadingsCount();
    uint16_t getQueuedEventsCount();
    bool popReading(Reading& reading);
    bool popEvent(Event& event);

    // Forzar commit (shutdown)
    void forceCommit();

private:
    Preferences _prefs;
    bool _initialized = false;
    bool _dirty = false;

    // Cache de contadores en RAM
    uint32_t _cached_seq = 0;
    uint32_t _cached_uptime = 0;
    uint32_t _pending_seq_writes = 0;
    uint32_t _pending_uptime_writes = 0;

    static constexpr const char* NS = "agri_cfg";
    static constexpr const char* KEY_CONFIG = "dev_cfg";
    static constexpr const char* KEY_HMAC = "hmac_key";
    static constexpr const char* KEY_SEQ = "seq_cnt";
    static constexpr const char* KEY_BOOT = "boot_cnt";
    static constexpr const char* KEY_UPTIME = "uptime_s";
    static constexpr const char* KEY_RD_IDX = "rd_idx";
    static constexpr const char* KEY_RD_COUNT = "rd_cnt";
    static constexpr const char* KEY_EV_IDX = "ev_idx";
    static constexpr const char* KEY_EV_COUNT = "ev_cnt";
    static constexpr uint16_t MAX_QUEUED_READINGS = 30;  // Reducido para NVS
    static constexpr uint16_t MAX_QUEUED_EVENTS = 20;
    static constexpr uint32_t SEQ_WRITE_THRESHOLD = 10;
    static constexpr uint32_t UPTIME_WRITE_THRESHOLD = 60;
};

// ──────────────────────────────────────────────────────────────
// SD Card Manager (Buffer offline GRANDE)
// ──────────────────────────────────────────────────────────────

class SDManager {
public:
    SDManager() : _mounted(false) {}
    ~SDManager() { unmount(); }

    bool mount(int8_t cs_pin, int8_t mosi = -1, int8_t miso = -1, int8_t sclk = -1,
               uint32_t freq = 4000000, const char* mountpoint = "/sd");
    void unmount();
    bool isMounted() const { return _mounted; }
    uint64_t totalBytes() const;
    uint64_t usedBytes() const;

    // Buffer offline en archivos binarios (append-only, rotación)
    bool appendReading(const Reading& reading);
    bool appendEvent(const Event& event);
    bool appendLog(const char* level, const char* message, const char* code = nullptr);
    uint32_t getReadingsFileSize();
    uint32_t getEventsFileSize();
    uint32_t getLogsFileSize();

    // Flush a API (streaming, sin cargar todo en RAM)
    bool flushReadings(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch = 100);
    bool flushEvents(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch = 50);

    // Rotación de archivos (max 1MB)
    bool rotateIfNeeded();

private:
    bool _mounted = false;
    int8_t _cs_pin = -1;
    String _mountpoint;
    static constexpr const char* READINGS_FILE = "/readings.bin";
    static constexpr const char* EVENTS_FILE = "/events.bin";
    static constexpr const char* LOGS_FILE = "/logs.txt";
    static constexpr uint32_t MAX_FILE_SIZE = 1024 * 1024; // 1MB
};

// ──────────────────────────────────────────────────────────────
// SPIFFS Manager (Fallback sin SD)
// ──────────────────────────────────────────────────────────────

class SPIFFSManager {
public:
    SPIFFSManager() : _mounted(false) {}
    ~SPIFFSManager() { unmount(); }

    bool mount(bool format_on_fail = true);
    void unmount();
    bool isMounted() const { return _mounted; }
    uint32_t totalBytes() const;
    uint32_t usedBytes() const;

    bool appendReading(const Reading& reading);
    bool appendEvent(const Event& event);
    bool appendLog(const char* level, const char* message, const char* code = nullptr);
    bool flushReadings(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch = 100);
    bool flushEvents(HttpClient& http, const char* device_id, const char* hmac_key, uint32_t max_batch = 50);

private:
    bool _mounted = false;
};

// ──────────────────────────────────────────────────────────────
// Unified Persistence Manager
// ──────────────────────────────────────────────────────────────

class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();

    bool init(const DeviceConfig& config);
    void loop();

    // NVS (escrituras controladas)
    NVSManager nvs;

    // Storage grande (SD o SPIFFS)
    SDManager sd;
    SPIFFSManager spiffs;

    // API unificada
    bool saveConfig(const DeviceConfig& config);
    bool loadConfig(DeviceConfig& config);

    // Buffer offline (prioridad: SD > SPIFFS > NVS)
    bool queueOfflineReading(const Reading& reading);
    bool queueOfflineEvent(const Event& event);
    bool flushOfflineBuffers(HttpClient& http);

    // Logs
    void log(const char* level, const char* message, const char* code = nullptr);

    // Estado
    bool hasSD() const { return sd.isMounted(); }
    bool hasSPIFFS() const { return spiffs.isMounted(); }
    uint32_t getOfflineReadingsCount() const;
    uint32_t getOfflineEventsCount() const;

    // Shutdown seguro
    void prepareShutdown();

private:
    bool _shutdown_prepared = false;
};

#endif // PERSISTENCE_H
