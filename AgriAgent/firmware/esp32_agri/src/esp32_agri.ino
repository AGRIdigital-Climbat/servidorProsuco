/*
 * AgriAgents ESP32 Firmware v1.0.0
 *
 * Firmware único, configurable 100% vía API O hardcodeado en user_config.h
 * Multi-modelo ESP32 (WROOM, WROVER, S2, S3, C3, C6, H2, P4)
 * Comunicación: WiFi + Cellular (TinyGSM) + Ethernet + LoRa
 * Sin OTA → comunicación garantizada desde primer boot
 */

#include <Arduino.h>
#include "config.h"
#include "board_config.h"
#include "user_config.h"           // ← CONFIGURACIÓN DE USUARIO (EDITAR ANTES DE SUBIR)
#include "wifi_manager.h"
#include "network_manager.h"
#include "http_client.h"
#include "sensor_manager.h"
#include "command_processor.h"
#include "persistence.h"
#include "watchdog.h"

// ──────────────────────────────────────────────────────────────
// Instancias globales
// ──────────────────────────────────────────────────────────────
DeviceConfig g_config;
WiFiManager g_wifi;
NetworkManager g_network;
HttpClient g_http;
SensorManager g_sensors;
CommandProcessor g_commands;
PersistenceManager g_persistence;
WatchdogManager g_wdt;

// ──────────────────────────────────────────────────────────────
// Timing
// ──────────────────────────────────────────────────────────────
uint32_t g_last_readings_batch = 0;
uint32_t g_last_config_poll = 0;
uint32_t g_last_task_poll = 0;
uint32_t g_last_command_poll = 0;
uint32_t g_last_i2c_scan = 0;
uint32_t g_last_offline_flush = 0;
uint32_t g_last_wdt_feed = 0;
uint32_t g_boot_time = 0;
uint32_t g_seq_counter = 0;

// Estado
bool g_config_loaded = false;
bool g_time_synced = false;
bool g_initial_sync_done = false;
bool g_network_ready = false;

// ──────────────────────────────────────────────────────────────
// Setup
// ──────────────────────────────────────────────────────────────
void setup() {
    g_boot_time = millis();

    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  AgriAgents ESP32 Firmware v" FIRMWARE_VERSION);
    Serial.println("  Build: " FIRMWARE_BUILD_DATE);
    Serial.println("========================================");

    // 1. Detectar placa y pines
    g_config.board_model = detectBoardModel();
    g_config.pins = getBoardPins(g_config.board_model);
    Serial.printf("Placa: %s\n", getBoardName(g_config.board_model));

    // 2. Watchdog PRIMERO
    g_wdt.init(WDT_TIMEOUT_MS);
    g_wdt.feed();
    delay(1);  // CRÍTICO: delay(1) después de cada wdt feed

    // 3. Aplicar configuración de usuario (user_config.h)
    applyUserConfig();

    // 4. Persistencia (NVS + SD/SPIFFS)
    g_persistence.init(g_config);
    if (g_persistence.loadConfig(g_config)) {
        g_config_loaded = true;
        Serial.printf("Config local v%u cargada\n", g_config.config_version);
    }

    // 5. Network Manager (WiFi/Cellular/Ethernet/LoRa)
    g_network.init(g_config);
    g_network.onConnect([](NetworkType t) {
        Serial.printf("Red conectada: %d\n", t);
        g_network_ready = true;
    });
    g_network.onDisconnect([](NetworkType t) {
        Serial.printf("Red desconectada: %d\n", t);
        g_network_ready = false;
    });

    // Conectar según modo
    if (!connectNetwork()) {
        Serial.println("ERROR: No se pudo conectar a ninguna red");
        #if defined(CONFIG_MODE_API)
        // En modo API, portal cautivo intentará en loop
        Serial.println("Modo API: esperando portal cautivo...");
        #else
        // En standalone, reiniciar y reintentar
        Serial.println("Reiniciando en 30s...");
        delay(30000);
        ESP.restart();
        #endif
    }
    g_wdt.feed(); delay(1);

    // 6. HTTP Client
    g_http.init(g_config.api);
    g_wdt.feed(); delay(1);

    // 7. Sensores
    g_sensors.init(g_config);
    g_wdt.feed(); delay(1);

    // 8. Comandos
    g_commands.init(g_config);
    g_wdt.feed(); delay(1);

    // 9. Sincronizar tiempo (NTP)
    syncTime();
    g_wdt.feed(); delay(1);

    // 10. Primera sincronización completa (solo modo API)
    #if defined(CONFIG_MODE_API)
    if (!g_initial_sync_done) {
        fullInitialSync();
        g_initial_sync_done = true;
    }
    #endif

    // 11. Escanear I2C inicial
    g_sensors.scanI2C();
    g_last_i2c_scan = millis();

    // Incrementar contador de boots
    g_persistence.nvs.incrementBootCount();

    Serial.println("Setup completado - Loop principal");
    Serial.println("========================================");
}

// ──────────────────────────────────────────────────────────────
// Aplicar configuración de user_config.h
// ──────────────────────────────────────────────────────────────
void applyUserConfig() {
    // API
    if (strlen(API_BASE_URL) > 0) {
        strncpy(g_config.api.base_url, API_BASE_URL, sizeof(g_config.api.base_url));
    }
    if (strlen(HMAC_KEY) > 0) {
        strncpy(g_config.api.hmac_key, HMAC_KEY, sizeof(g_config.api.hmac_key));
    }

    // Device ID
    if (strlen(DEVICE_ID) > 0) {
        strncpy(g_config.device_id, DEVICE_ID, sizeof(g_config.device_id));
    } else {
        // Generar desde MAC
        uint64_t mac = ESP.getEfuseMac();
        char id[32];
        snprintf(id, sizeof(id), "agri-%06X", (uint32_t)(mac & 0xFFFFFF));
        strncpy(g_config.device_id, id, sizeof(g_config.device_id));
    }

    // Timezone
    g_config.tz_offset_minutes = TIMEZONE_OFFSET_MINUTES;

    // WiFi
    #ifdef CONFIG_MODE_STANDALONE_WIFI
    if (strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "MI_WIFI_SSID") != 0) {
        WiFiCredential cred;
        strncpy(cred.ssid, WIFI_SSID, sizeof(cred.ssid));
        strncpy(cred.password, WIFI_PASSWORD, sizeof(cred.password));
        cred.priority = 100;
        cred.enabled = true;
        g_config.wifi = cred; // Sobrescribe primera credencial
    }
    if (strlen(WIFI_HOSTNAME) > 0) {
        strncpy(g_config.wifi.hostname, WIFI_HOSTNAME, sizeof(g_config.wifi.hostname));
    }
    #endif

    // Cellular
    #ifdef CONFIG_MODE_STANDALONE_CELLULAR
    g_config.cellular.modem_type = CELLULAR_MODEM_TYPE;
    strncpy(g_config.cellular.apn, CELLULAR_APN, sizeof(g_config.cellular.apn));
    strncpy(g_config.cellular.username, CELLULAR_USER, sizeof(g_config.cellular.username));
    strncpy(g_config.cellular.password, CELLULAR_PASS, sizeof(g_config.cellular.password));
    strncpy(g_config.cellular.pin, CELLULAR_PIN, sizeof(g_config.cellular.pin));
    g_config.cellular.tx_pin = MODEM_TX_PIN;
    g_config.cellular.rx_pin = MODEM_RX_PIN;
    g_config.cellular.power_pin = MODEM_POWER_PIN;
    g_config.cellular.reset_pin = MODEM_RESET_PIN;
    g_config.cellular.baud = MODEM_BAUD;
    #endif

    // Pinout I2C
    if (I2C0_SDA_PIN >= 0) g_config.pins.i2c0_sda = I2C0_SDA_PIN;
    if (I2C0_SCL_PIN >= 0) g_config.pins.i2c0_scl = I2C0_SCL_PIN;
    if (I2C1_SDA_PIN >= 0) g_config.pins.i2c1_sda = I2C1_SDA_PIN;
    if (I2C1_SCL_PIN >= 0) g_config.pins.i2c1_scl = I2C1_SCL_PIN;

    // SD Card
    if (SD_CS_PIN >= 0) {
        g_config.pins.sd_cs = SD_CS_PIN;
        g_config.pins.sd_mosi = SD_MOSI_PIN;
        g_config.pins.sd_miso = SD_MISO_PIN;
        g_config.pins.sd_sclk = SD_SCLK_PIN;
    }

    // Sensores hardcodeados (ejemplos)
    #ifdef SENSOR_1_ID
    if (g_config.sensor_count < MAX_SENSORS) {
        SensorConfig& s = g_config.sensors[g_config.sensor_count++];
        strncpy(s.sensor_id, SENSOR_1_ID, sizeof(s.sensor_id));
        strncpy(s.kind, SENSOR_1_KIND, sizeof(s.kind));
        strncpy(s.port, SENSOR_1_PORT, sizeof(s.port));
        s.enabled = true;
        s.interval_ms = SENSOR_1_INTERVAL_S * 1000;
        s.interval_night_ms = SENSOR_1_INTERVAL_S * 1000 * 3;
    }
    #endif

    #ifdef SENSOR_2_ID
    if (g_config.sensor_count < MAX_SENSORS) {
        SensorConfig& s = g_config.sensors[g_config.sensor_count++];
        strncpy(s.sensor_id, SENSOR_2_ID, sizeof(s.sensor_id));
        strncpy(s.kind, SENSOR_2_KIND, sizeof(s.kind));
        strncpy(s.port, SENSOR_2_PORT, sizeof(s.port));
        s.enabled = true;
        s.interval_ms = SENSOR_2_INTERVAL_S * 1000;
        s.interval_night_ms = SENSOR_2_INTERVAL_S * 1000 * 3;
    }
    #endif

    #ifdef SENSOR_3_ID
    if (g_config.sensor_count < MAX_SENSORS) {
        SensorConfig& s = g_config.sensors[g_config.sensor_count++];
        strncpy(s.sensor_id, SENSOR_3_ID, sizeof(s.sensor_id));
        strncpy(s.kind, SENSOR_3_KIND, sizeof(s.kind));
        strncpy(s.port, SENSOR_3_PORT, sizeof(s.port));
        s.enabled = true;
        s.interval_ms = SENSOR_3_INTERVAL_S * 1000;
        s.interval_night_ms = SENSOR_3_INTERVAL_S * 1000 * 3;
    }
    #endif

    // Variables hardcodeadas
    #ifdef VAR_TEMP_MIN
    if (g_config.variable_count < MAX_SENSORS * MAX_VARIABLES_PER_SENSOR) {
        VariableConfig& v = g_config.variables[g_config.variable_count++];
        strncpy(v.key, "temperature", sizeof(v.key));
        v.enabled = true;
        v.interval_ms = 60000;
        v.range_min = VAR_TEMP_MIN;
        v.range_max = VAR_TEMP_MAX;
        v.error_value = VAR_TEMP_ERROR;
    }
    #endif

    Serial.println("User config aplicada");
}

// ──────────────────────────────────────────────────────────────
// Conectar red según modo
// ──────────────────────────────────────────────────────────────
bool connectNetwork() {
    #if defined(CONFIG_MODE_STANDALONE_WIFI)
    Serial.println("Modo: WiFi Standalone");
    g_network.setPrimaryTransport(NET_WIFI);
    return g_network.connect();

    #elif defined(CONFIG_MODE_STANDALONE_CELLULAR)
    Serial.println("Modo: Cellular Standalone");
    g_network.setPrimaryTransport(NET_CELLULAR);
    return g_network.connect();

    #else
    Serial.println("Modo: API (WiFi + failover Cellular)");
    // En modo API, NetworkManager maneja todo con failover
    g_network.onConnect([](NetworkType t) {
        g_network_ready = true;
    });
    return g_network.connect(); // Intenta WiFi primero, luego cellular
    #endif
}

// ──────────────────────────────────────────────────────────────
// Loop principal
// ──────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // 1. Watchdog feed (CRÍTICO - cada 1s)
    if (now - g_last_wdt_feed >= WDT_FEED_INTERVAL_MS) {
        g_wdt.feed();
        delay(1);  // CRÍTICO
        g_last_wdt_feed = now;
    }

    // 2. Network manager
    g_network.loop();

    if (!g_network.isConnected()) {
        return;
    }

    // 3. Sync config desde API (solo modo API, cada 5 min)
    #if defined(CONFIG_MODE_API)
    if (now - g_last_config_poll >= CONFIG_POLL_INTERVAL_MS) {
        pollConfigUpdate();
        g_last_config_poll = now;
        g_wdt.feed(); delay(1);
    }
    #endif

    // 4. Lectura sensores y envío lote
    if (now - g_last_readings_batch >= READINGS_BATCH_INTERVAL_MS) {
        readAndSendBatch();
        g_last_readings_batch = now;
        g_wdt.feed(); delay(1);
    }

    // 5. Poll tareas asíncronas (solo modo API)
    #if defined(CONFIG_MODE_API)
    if (now - g_last_task_poll >= TASK_POLL_INTERVAL_MS) {
        pollTasks();
        g_last_task_poll = now;
        g_wdt.feed(); delay(1);
    }
    #endif

    // 6. Poll comandos rápidos (solo modo API)
    #if defined(CONFIG_MODE_API)
    if (now - g_last_command_poll >= COMMAND_POLL_INTERVAL_MS) {
        pollCommands();
        g_last_command_poll = now;
        g_wdt.feed(); delay(1);
    }
    #endif

    // 7. Escaneo I2C periódico (cada 1 min)
    if (now - g_last_i2c_scan >= I2C_SCAN_INTERVAL_MS) {
        g_sensors.scanI2C();
        g_last_i2c_scan = now;
        g_wdt.feed(); delay(1);
    }

    // 8. Flush buffer offline (cada 30s)
    if (now - g_last_offline_flush >= OFFLINE_FLUSH_INTERVAL_MS) {
        g_persistence.flushOfflineBuffers(g_http);
        g_last_offline_flush = now;
        g_wdt.feed(); delay(1);
    }

    // 9. Procesar comandos pendientes (solo modo API)
    #if defined(CONFIG_MODE_API)
    g_commands.processPending();
    #endif

    // 10. Sensor manager loop
    g_sensors.loop(now);

    delay(10);
}

// ──────────────────────────────────────────────────────────────
// Funciones auxiliares (solo modo API)
// ──────────────────────────────────────────────────────────────

#if defined(CONFIG_MODE_API)

void syncTime() {
    if (g_time_synced) return;
    configTime(g_config.tz_offset_minutes * 60, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
        g_time_synced = true;
        Serial.printf("Hora sincronizada: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println("WARN: No se pudo sincronizar NTP");
    }
}

void fullInitialSync() {
    Serial.println("=== Sincronización inicial completa ===");

    if (g_network.isConnected()) {
        if (g_persistence.nvs.fetchDeviceConfig(g_http, g_config)) {
            Serial.printf("Config remota v%u obtenida\n", g_config.config_version);
            g_config_loaded = true;
            g_persistence.saveConfig(g_config);
            g_sensors.updateConfig(g_config);
            g_commands.updateConfig(g_config);
        }

        auto detected = g_sensors.getDetectedSensors();
        if (!detected.empty()) {
            g_persistence.nvs.reportDetectedSensors(g_http, detected);
        }

        requestInitialTask();
    }
    g_initial_sync_done = true;
    Serial.println("=== Sincronización inicial completada ===");
}

void pollConfigUpdate() {
    if (!g_network.isConnected()) return;
    uint32_t new_version = g_persistence.nvs.checkConfigVersion(g_http, g_config.device_id);
    if (new_version > g_config.config_version) {
        Serial.printf("Nueva config v%u disponible\n", new_version);
        if (g_persistence.nvs.fetchDeviceConfig(g_http, g_config)) {
            g_persistence.saveConfig(g_config);
            g_sensors.updateConfig(g_config);
            g_commands.updateConfig(g_config);
            Serial.println("Config actualizada");
        }
    }
}

void readAndSendBatch() {
    if (!g_network.isConnected()) {
        auto readings = g_sensors.readAll();
        for (const auto& r : readings) g_persistence.queueOfflineReading(r);
        return;
    }

    auto readings = g_sensors.readAll();
    if (readings.empty()) return;

    uint32_t seq = ++g_seq_counter;
    HttpResponse resp = g_http.postReadings(g_config.device_id, g_config.api.hmac_key, seq, readings);

    if (resp.status == 202) {
        Serial.printf("Lote seq=%u: %u lecturas\n", seq, readings.size());
        if (resp.hasHeader("X-Config-Version")) {
            uint32_t new_ver = atoi(resp.getHeader("X-Config-Version").c_str());
            if (new_ver > g_config.config_version) pollConfigUpdate();
        }
    } else if (resp.status == 401) {
        Serial.println("HMAC inválido - rotando clave");
        g_persistence.nvs.rotateHmacKey(g_http, g_config);
    } else {
        Serial.printf("ERROR lote: %d %s\n", resp.status, resp.body.c_str());
        for (const auto& r : readings) g_persistence.queueOfflineReading(r);
    }
}

void pollTasks() {
    if (!g_network.isConnected()) return;
    HttpResponse resp = g_http.getTaskResult(g_config.device_id, g_config.api.hmac_key);
    if (resp.status == 200 && resp.json.containsKey("command")) {
        Command cmd = parseCommand(resp.json["command"]);
        g_commands.queueCommand(cmd, resp.json["tracking_id"] | "");
    }
}

void pollCommands() {
    if (!g_network.isConnected()) return;
    HttpResponse resp = g_http.getNextCommand(g_config.device_id, g_config.api.hmac_key);
    if (resp.status == 200) {
        Command cmd = parseCommand(resp.json);
        g_commands.queueCommand(cmd, "");
    }
}

void requestInitialTask() {
    HttpResponse resp = g_http.createTask(g_config.device_id, g_config.api.hmac_key,
                                          "irrigation_analysis", "{\"reason\":\"startup\"}");
    if (resp.status == 202 && resp.json.containsKey("tracking_id")) {
        g_commands.setCurrentTaskId(resp.json["tracking_id"]);
    }
}

Command parseCommand(JsonObject json) {
    Command cmd;
    strncpy(cmd.command_id, json["command_id"] | "", sizeof(cmd.command_id));
    strncpy(cmd.kind, json["kind"] | "actuate", sizeof(cmd.kind));
    strncpy(cmd.action, json["action"] | "", sizeof(cmd.action));
    strncpy(cmd.pin_name, json["pin"] | "", sizeof(cmd.pin_name));
    cmd.gpio = json["gpio"] | -1;
    cmd.value = json["value"] | 0;
    cmd.duration_ms = json["duration_ms"] | 0;
    cmd.freq_hz = json["freq_hz"] | 0;
    cmd.channel = json["channel"] | 0;
    strncpy(cmd.tracking_id, json["tracking_id"] | "", sizeof(cmd.tracking_id));
    return cmd;
}

#endif // CONFIG_MODE_API

// ──────────────────────────────────────────────────────────────
// Funciones comunes (ambos modos)
// ──────────────────────────────────────────────────────────────

// En modo standalone, readAndSendBatch simplificado
#if !defined(CONFIG_MODE_API)
void readAndSendBatch() {
    if (!g_network.isConnected()) {
        auto readings = g_sensors.readAll();
        for (const auto& r : readings) g_persistence.queueOfflineReading(r);
        return;
    }

    auto readings = g_sensors.readAll();
    if (readings.empty()) return;

    uint32_t seq = ++g_seq_counter;
    HttpResponse resp = g_http.postReadings(g_config.device_id, g_config.api.hmac_key, seq, readings);

    if (resp.status == 202) {
        Serial.printf("Lote seq=%u: %u lecturas\n", seq, readings.size());
    } else {
        Serial.printf("ERROR lote: %d\n", resp.status);
        for (const auto& r : readings) g_persistence.queueOfflineReading(r);
    }
}
#endif

// Dummy para compilar en ambos modos
#if defined(CONFIG_MODE_API)
void pollTasks() {}
void pollCommands() {}
void requestInitialTask() {}
Command parseCommand(JsonObject json) { Command c; return c; }
void syncTime() { configTime(g_config.tz_offset_minutes * 60, 0, "pool.ntp.org", "time.nist.gov"); }
#else
void syncTime() { configTime(TIMEZONE_OFFSET_MINUTES * 60, 0, "pool.ntp.org", "time.nist.gov"); }
#endif
