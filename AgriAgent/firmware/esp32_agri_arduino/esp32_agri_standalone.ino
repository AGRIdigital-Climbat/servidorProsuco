/*
 * AgriAgents ESP32 - Test Standalone MÍNIMO (Arduino IDE)
 *
 * Para test inicial: SOLO WiFi hardcodeado
 * - Sin WiFiManager, sin portal cautivo
 * - Sin API remota, sin config remota
 * - Solo: conectar WiFi → leer sensores → enviar a API hardcodeada
 * - Logs por Serial
 *
 * INSTRUCCIONES:
 * 1. Edita user_config.h con tus credenciales
 * 2. Abre este .ino en Arduino IDE
 * 3. Selecciona tu placa ESP32 (Tools > Board > ESP32 Arduino > tu placa)
 * 4. Sube y abre Serial Monitor (115200 baud)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include "user_config.h"  // ← EDITAR ESTE ARCHIVO CON TUS CREDENCIALES

// ──────────────────────────────────────────────────────────────
// Hardware / Globales
// ──────────────────────────────────────────────────────────────

WiFiClientSecure httpsClient;
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
    String payload = String(DEVICE_ID) + "." + String(ts) + "." + bodyHash;
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
    Serial.printf("Total I2C: %d dispositivos\n", found);
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

    // 1. ADC ejemplo: GPIO32 (humedad suelo simulada)
    float soil = analogRead(32) * 3.3 / 4095.0 * 100.0; // 0-100%
    readings.push_back({"soil_moisture", soil, "ok", ts});

    // 2. Temperatura interna ESP32
    float temp = (temperature_sens_read() - 32.0) / 1.8; // °C
    readings.push_back({"internal_temp", temp, "ok", ts});

    // 3. I2C: BME280 en 0x76 (si existe)
    Wire.beginTransmission(0x76);
    if (Wire.endTransmission() == 0) {
        Wire.beginTransmission(0x76);
        Wire.write(0xF7); // Data register
        Wire.endTransmission();
        Wire.requestFrom(0x76, 8);
        if (Wire.available() >= 8) {
            uint32_t press_raw = (Wire.read() << 12) | (Wire.read() << 4) | (Wire.read() >> 4);
            uint32_t temp_raw = (Wire.read() << 12) | (Wire.read() << 4) | (Wire.read() >> 4);
            uint16_t hum_raw = (Wire.read() << 8) | Wire.read();

            // Conversión aproximada (usar librería completa en producción)
            float t = temp_raw / 16384.0 * 165.0 - 40.0;
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

    // Solo test: aceptar cualquier certificado (en producción usa CA)
    httpsClient.setInsecure();

    addAuthHeaders(http, "POST", "/devices/" + String(DEVICE_ID) + "/readings", body);

    int httpCode = http.POST(body);
    String resp = http.getString();
    http.end();

    Serial.printf("POST /readings: %d\n", httpCode);
    if (httpCode != 202) {
        Serial.println("Respuesta: " + resp);
        return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────
// Globales
// ──────────────────────────────────────────────────────────────

WiFiClientSecure httpsClient;
Preferences prefs;

uint32_t g_seq = 0;
uint32_t g_last_readings = 0;
uint32_t g_last_i2c_scan = 0;

// ──────────────────────────────────────────────────────────────
// Setup / Loop
// ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== AgriAgents ESP32 Standalone Test ===");
    Serial.println("Firmware: 1.0.0-test");
    Serial.println("Config: user_config.h");

    // I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    Serial.printf("I2C: SDA=%d SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

    // ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    analogSetSamples(16);

    // NVS para secuencia
    prefs.begin("agri_test", false);
    g_seq = prefs.getUInt("seq", 0);

    // WiFi
    Serial.printf("Conectando WiFi: %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
        delay(200);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi OK: %s (RSSI: %d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("ERROR: WiFi timeout - reiniciando en 10s");
        delay(10000);
        ESP.restart();
    }

    // HTTPS: aceptar cualquier cert (solo test)
    httpsClient.setInsecure();

    // Timezone
    configTime(TIMEZONE_OFFSET_MINUTES * 60, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
        Serial.printf("Hora: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println("WARN: NTP timeout");
    }

    // I2C scan inicial
    scanI2C();
    g_last_i2c_scan = millis();

    Serial.println("\n=== Listo - Loop principal ===\n");
}

void loop() {
    uint32_t now = millis();

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
