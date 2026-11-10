/*
 * AgriAgents ESP32 - Configuración de Usuario (EDITAR ANTES DE SUBIR)
 *
 * INSTRUCCIONES:
 * 1. Copia este archivo a user_config_local.h (no se sube a git)
 * 2. Edita tus credenciales abajo
 * 3. Sube con Arduino IDE o PlatformIO
 *
 * MODO STANDALONE: Si defines WIFI_SSID o CELLULAR_APN → usa config hardcodeada
 * MODO API: Si NO defines nada → obtiene config de API (portal cautivo si no hay WiFi)
 */

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// ──────────────────────────────────────────────────────────────
// === EDITA AQUÍ TUS CREDENCIALES ===
// ──────────────────────────────────────────────────────────────

// ---- WIFI (opcional - si no defines, usa portal cautivo) ----
#define WIFI_SSID              "MI_WIFI_SSID"
#define WIFI_PASSWORD          "MI_WIFI_PASSWORD"
#define WIFI_HOSTNAME          "agri-esp32"       // Opcional

// ---- CELLULAR / SIM (opcional - TinyGSM) ----
// Descomenta y configura si usas modem celular
// #define CELLULAR_APN           "internet.tuoperador.com"
// #define CELLULAR_USER          "usuario"
// #define CELLULAR_PASS          "password"
// #define CELLULAR_PIN           "1234"           // PIN SIM si tiene
// #define CELLULAR_MODEM_TYPE    TINY_GSM_MODEM_SIM7000  // SIM7000, SIM7600, A7670

// Pines del modem (ajusta según tu hardware)
#define MODEM_TX_PIN             17
#define MODEM_RX_PIN             16
#define MODEM_POWER_PIN          4
#define MODEM_RESET_PIN          5
#define MODEM_BAUD               115200

// ---- API BACKEND (obligatorio para modo API) ----
#define API_BASE_URL             "https://tu-dominio.com/v1"
// #define API_BASE_URL             "http://192.168.1.100:8080/v1"  // Local

// Device ID (se usa para HMAC y registro)
// Si vacío, se genera desde MAC: "agri-XXXXXX"
#define DEVICE_ID                ""

// HMAC Key (se obtiene al registrar device en API)
// Si vacío, se obtiene vía API rotation
#define HMAC_KEY                 ""

// Timezone offset en minutos (ej: Bolivia -240, UTC 0, España +120)
#define TIMEZONE_OFFSET_MINUTES  -240

// ──────────────────────────────────────────────────────────────
// PINOUT (opcional - si no defines, usa defaults de tu placa)
// ──────────────────────────────────────────────────────────────

// I2C
#define I2C0_SDA_PIN             21
#define I2C0_SCL_PIN             22
#define I2C1_SDA_PIN             -1    // -1 = no usar segundo bus
#define I2C1_SCL_PIN             -1

// Sensores (ejemplos - ajusta según tu hardware)
// Formato: "tipo:puerto"  |  tipo: i2c/adc/pulse/gpio  |  puerto: 0x76 / 1 / 27 / 32
// #define SENSOR_1_ID             "bme280_1"
// #define SENSOR_1_KIND           "env"
// #define SENSOR_1_PORT           "i2c:0x76"
// #define SENSOR_1_INTERVAL_S       60

// #define SENSOR_2_ID             "soil_1"
// #define SENSOR_2_KIND           "soil"
// #define SENSOR_2_PORT           "adc:32"
// #define SENSOR_2_INTERVAL_S       300

// #define SENSOR_3_ID             "flow_1"
// #define SENSOR_3_KIND           "flow"
// #define SENSOR_3_PORT           "pulse:27"
// #define SENSOR_3_INTERVAL_S       10

// Variables (rangos y error_value)
// #define VAR_TEMP_MIN             -40
// #define VAR_TEMP_MAX             80
// #define VAR_TEMP_ERROR           -999

// ──────────────────────────────────────────────────────────────
// SD CARD (opcional)
// ──────────────────────────────────────────────────────────────
#define SD_CS_PIN                5
#define SD_MOSI_PIN              23
#define SD_MISO_PIN              19
#define SD_SCLK_PIN              18

// ──────────────────────────────────────────────────────────────
// NO EDITAR DEBAJO - Configuración interna
// ──────────────────────────────────────────────────────────────

// Detectar modo: STANDALONE si hay credenciales hardcodeadas
#if defined(WIFI_SSID) && (WIFI_SSID != "MI_WIFI_SSID")
  #define CONFIG_MODE_STANDALONE_WIFI
#elif defined(CELLULAR_APN)
  #define CONFIG_MODE_STANDALONE_CELLULAR
#else
  #define CONFIG_MODE_API
#endif

// Validaciones
#if defined(CONFIG_MODE_API) && !defined(API_BASE_URL)
  #error "Modo API requiere API_BASE_URL definida"
#endif

#endif // USER_CONFIG_H
