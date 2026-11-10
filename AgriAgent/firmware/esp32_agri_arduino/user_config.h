/*
 * AgriAgents ESP32 - Configuración de Usuario (EDITAR ANTES DE SUBIR)
 *
 * INSTRUCCIONES:
 * 1. Edita este archivo con tus credenciales REALES
 * 2. NO subas este archivo a GitHub con credenciales reales
 * 3. Guarda una copia segura aparte
 */

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// ──────────────────────────────────────────────────────────────
// === EDITA AQUÍ TUS CREDENCIALES REALES ===
// ──────────────────────────────────────────────────────────────

// ---- WIFI (obligatorio para este test) ----
#define WIFI_SSID                "TU_SSID_AQUI"      // Cambia esto
#define WIFI_PASS                "TU_PASSWORD_AQUI"  // Cambia esto

// ---- API BACKEND (obligatorio) ----
#define API_BASE_URL             "http://192.168.1.100:8080/v1"  // Tu IP local o dominio
// #define API_BASE_URL             "https://api.tu-dominio.com/v1"  // O producción

// Device ID (debe existir en la API y tener HMAC_KEY registrada)
#define DEVICE_ID                "test-esp32-01"

// HMAC Key (debe coincidir con la registrada en la API para este device_id)
#define HMAC_KEY                 "clave_hmac_de_32_chars_minimo"  // Cambia esto

// Timezone offset en minutos
// Bolivia: -240  |  UTC: 0  |  España invierno: +60  |  España verano: +120
#define TIMEZONE_OFFSET_MINUTES  -240

// ──────────────────────────────────────────────────────────────
// PINOUT I2C (ajusta según tu placa)
// ──────────────────────────────────────────────────────────────
#define I2C_SDA_PIN              21
#define I2C_SCL_PIN              22

// ──────────────────────────────────────────────────────────────
// INTERVALOS
// ──────────────────────────────────────────────────────────────
#define READINGS_INTERVAL_MS     60000   // Cada 60 segundos
#define I2C_SCAN_INTERVAL_MS     60000   // Cada 60 segundos

// ──────────────────────────────────────────────────────────────
// NO EDITAR DEBAJO
// ──────────────────────────────────────────────────────────────

#endif // USER_CONFIG_H
