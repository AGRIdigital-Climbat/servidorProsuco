/*
 * AgriAgents ESP32 - Test Standalone MÍNIMO
 *
 * Para test inicial: SOLO WiFi O SIM hardcodeado
 * - Sin WiFiManager, sin portal cautivo
 * - Sin API remota, sin config remota
 * - Solo: conectar → leer sensores → enviar a API hardcodeada
 * - Logs por Serial
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <mbedtls/md.h>
#include <Preferences.h>

// ──────────────────────────────────────────────────────────────
// === EDITA AQUÍ TUS CREDENCIALES ===
// ──────────────────────────────────────────────────────────────

// ---- WIFI (elige uno: WIFI o CELLULAR, no ambos) ----
#define USE_WIFI                 true      // true = WiFi, false = Cellular
#define WIFI_SSID                "TU_SSID"
#define WIFI_PASS                "TU_PASSWORD"

// ---- CELLULAR (si USE_WIFI = false) ----
#define CELL_APN                 "internet.tuoperador.com"
#define CELL_USER                ""
#define CELL_PASS                ""
#define MODEM_TX_PIN             17
#define MODEM_RX_PIN             16
#define MODEM_POWER_PIN          4
#define MODEM_RESET_PIN          5

// ---- API BACKEND ----
#define API_BASE_URL             "http://192.168.1.100:8080/v1"  // O https://tu-dominio.com/v1
#define DEVICE_ID                "test-esp32-01"
#define HMAC_KEY                 "clave_hmac_aqui"  // Debe coincidir con la registrada en API
#define TIMEZONE_OFFSET_MINUTES  -240

// ---- PINOUT I2C ----
#define I2C_SDA_PIN              21
#define I2C_SCL_PIN              22

// ---- INTERVALOS ----
#define READINGS_INTERVAL_MS     60000   // Cada 60s
#define I2C_SCAN_INTERVAL_MS     60000

// ──────────────────────────────────────────────────────────────
// Hardware
// ──────────────────────────────────────────────────────────────

#if USE_WIFI
WiFiClientSecure httpsClient;
#else
// Cellular: requeriría TinyGSM - para test inicial usa WiFi
#error "Test standalone inicial: usa USE_WIFI = true. Cellular requiere TinyGSM."
#endif

Preferences prefs;
uint32_t g_seq = 0;
uint32_t g_last_readings = 0;
uint32_t g_last_i2c_scan = 0;

// ──────────────────────────────────────────────────────────────
// HMAC-SHA256
// ──────────────────────────────────────────────────────────────

String hmacSha256(const String& key, const String& data) {
    unsigned char hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", hmac[i]);
    hex[64] = '\0';
    return String(hex);
}

String sha256(const String& data) {
    unsigned char hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", hash[i]);
    hex[64] = '\0';
    return String(hex);
}

void addAuthHeaders(HTTPClient& http, const String& method, const String& path, const String& body) {
    uint32_t ts = millis() / 1000;
    String bodyHash = sha256(body);
    String payload = DEVICE_ID + "." + String(ts) + "." + bodyHash;
    String signature = hmacSha256(HMAC_KEY, payload);

    http.addHeader("X-Device-Id", DEVICE_ID);
    http.addHeader("X-Timestamp", String(ts));
    http.addHeader("X-Signature", signature);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "AgriAgents-Test/1.0");
}

// ──────────────────────────────────────────────────────────────
// I2C Scan
// ──────────────────────────────────────────────────────────────

void scanI2C() {
    Serial.println("Escaneando I2C...");
    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X encontrado\n", addr);
            found++;
        }
    }
    Serial.printf("Total: %d dispositivos\n", found);
}

// ──────────────────────────────────────────────────────────────
// Leer sensores (ejemplo básico)
// ──────────────────────────────────────────────────────────────

struct Reading {
    String key;
    float value;
    String quality = "ok";
    String timestamp;
};

std::vector<Reading> readSensors() {
    std::vector<Reading> readings;
    char ts[32];
    time_t now = time(nullptr);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    // Ejemplo: leer ADC en GPIO32 (simulando humedad suelo)
    float soil = analogRead(32) * 3.3 / 4095.0 * 100; // 0-100%
    readings.push_back({"soil_moisture", soil, "ok", ts});

    // Ejemplo: temperatura interna
    float temp = (temperature_sens_read() - 32) / 1.8; // ESP32 temp sensor
    readings.push_back({"internal_temp", temp, "ok", ts});

    // I2C: si hay BME280 en 0x76
    Wire.beginTransmission(0x76);
    if (Wire.endTransmission() == 0) {
        // Leer BME280 simplificado
        Wire.beginTransmission(0x76);
        Wire.write(0xF7);
        Wire.endTransmission();
        Wire.requestFrom(0x76, 8);
        if (Wire.available() >= 8) {
            uint32_t press_raw = (Wire.read() << 12) | (Wire.read() << 4) | (Wire.read() >> 4);
            uint32_t temp_raw = (Wire.read() << 12) | (Wire.read() << 4) | (Wire.read() >> 4);
            uint16_t hum_raw = (Wire.read() << 8) | Wire.read();

            float t = temp_raw / 16384.0 * 165.0 - 40.0; // Aprox
            float p = press_raw / 25600.0;
            float h = hum_raw / 1024.0 * 100.0;

            readings.push_back({"temperature", t, "ok", ts});
            readings.push_back({"pressure", p, "ok", ts});
            readings.push_back({"humidity", h, "ok", ts});
        }
    }

    return readings;
}

// ──────────────────────────────────────────────────────────────
// Enviar lote a API
// ──────────────────────────────────────────────────────────────

bool sendReadings(const std::vector<Reading>& readings) {
    if (readings.empty()) return true;

    DynamicJsonDocument doc(2048);
    doc["seq"] = ++g_seq;
    JsonArray arr = doc.createNestedArray("readings");

    for (const auto& r : readings) {
        JsonObject obj = arr.createNestedObject();
        obj["key"] = r.key;
        obj["value"] = r.value;
        obj["quality"] = r.quality;
        obj["ts"] = r.timestamp;
    }

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    String url = String(API_BASE_URL) + "/devices/" + DEVICE_ID + "/readings";

    if (!http.begin(httpsClient, url)) {
        Serial.println("HTTP begin failed");
        return false;
    }

    httpsClient.setInsecure(); // Solo test - en prod usa certificado

    addAuthHeaders(http, "POST", "/devices/" + String(DEVICE_ID) + "/readings", body);

    int httpCode = http.POST(body);
    String resp = http.getString();
    http.end();

    Serial.printf("POST /readings: %d\n", httpCode);
    if (httpCode != 202) {
        Serial.println(resp);
        return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────
// Setup / Loop
// ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== AgriAgents Standalone Test ===");

    // I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    Serial.println("I2C iniciado");

    // ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    analogSetSamples(16);

    // Timezone
    configTime(TIMEZONE_OFFSET_MINUTES * 60, 0, "pool.ntp.org", "time.nist.gov");

    // NVS para secuencia
    prefs.begin("agri_test", false);
    g_seq = prefs.getUInt("seq", 0);

    // WiFi
    Serial.printf("Conectando a %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
        delay(200);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi OK: %s (RSSI: %d)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("\nERROR: WiFi timeout");
        while (true) delay(1000);
    }

    // Sync time
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
        Serial.printf("Hora: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    // I2C scan inicial
    scanI2C();
    g_last_i2c_scan = millis();

    Serial.println("Listo - Loop principal\n");
}

uint32_t g_last_readings = 0;
uint32_t g_last_i2c_scan = 0;
uint32_t g_seq = 0;

void loop() {
    uint32_t now = millis();

    // Watchdog feed simulado
    // ESP.wdtFeed(); // Si usas WDT

    // Reconnect WiFi si se pierde
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi perdido, reconectando...");
        WiFi.reconnect();
        delay(5000);
        return;
    }

    // I2C scan periódico
    if (now - g_last_i2c_scan >= I2C_SCAN_INTERVAL_MS) {
        scanI2C();
        g_last_i2c_scan = now;
    }

    // Enviar lecturas
    if (now - g_last_readings >= READINGS_INTERVAL_MS) {
        auto readings = readSensors();
        if (!readings.empty()) {
            sendReadings(readings);
            prefs.putUInt("seq", g_seq);
        }
        g_last_readings = now;
    }

    delay(100);
}
