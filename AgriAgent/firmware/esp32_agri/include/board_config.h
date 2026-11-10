/*
 * AgriAgents ESP32 Firmware - Configuración de placas soportadas
 * Basado en documentación oficial Espressif (ESP-IDF, datasheets, ESP-FAQ)
 *
 * Placas soportadas:
 * - ESP32-DevKitC (WROOM-32E/UE): 30 pines, sin PSRAM
 * - ESP32-WROVER-E/IE: 30 pines, 8MB PSRAM
 * - ESP32-S2 (SAOLA-1/DevKitC-1): 43 pines, sin PSRAM, USB OTG
 * - ESP32-S3 (DevKitC-1): 45 pines, 512KB SRAM + PSRAM opcional
 * - ESP32-C3 (DevKitC-02): 22 pines, RISC-V, sin PSRAM
 * - ESP32-C6 (DevKitC-06): 30 pines, RISC-V, WiFi 6, Thread
 * - ESP32-H2 (DevKitC-07): 28 pines, RISC-V, Thread/Zigbee
 * - ESP32-P4 (DevKitC): 54 pines, dual-core, sin WiFi/BT
 *
 * Detección automática vía `ESP.getChipModel()` y `ESP.getChipRevision()`
 * Forzar modelo definiendo `BOARD_MODEL` en platformio.ini
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>

// ──────────────────────────────────────────────────────────────
// Detección automática de modelo
// ──────────────────────────────────────────────────────────────

enum BoardModel {
    BOARD_UNKNOWN = 0,
    BOARD_ESP32_DEVKIT_V1,      // WROOM-32E/UE
    BOARD_ESP32_WROVER,         // WROVER-E/IE (PSRAM)
    BOARD_ESP32_S2_SAOLA,       // S2 DevKitC-1 / SAOLA-1
    BOARD_ESP32_S3_DEVKITC1,    // S3 DevKitC-1
    BOARD_ESP32_C3_DEVKITC02,   // C3 DevKitC-02
    BOARD_ESP32_C6_DEVKITC06,   // C6 DevKitC-06
    BOARD_ESP32_H2_DEVKITC07,   // H2 DevKitC-07
    BOARD_ESP32_P4_DEVKIT,      // P4 DevKit
};

BoardModel detectBoardModel() {
    const char* chipModel = ESP.getChipModel();
    uint8_t chipRevision = ESP.getChipRevision();

    if (strcmp(chipModel, "ESP32") == 0) {
        if (psramFound()) {
            return BOARD_ESP32_WROVER;
        }
        return BOARD_ESP32_DEVKIT_V1;
    }
    else if (strcmp(chipModel, "ESP32-S2") == 0) {
        return BOARD_ESP32_S2_SAOLA;
    }
    else if (strcmp(chipModel, "ESP32-S3") == 0) {
        return BOARD_ESP32_S3_DEVKITC1;
    }
    else if (strcmp(chipModel, "ESP32-C3") == 0) {
        return BOARD_ESP32_C3_DEVKITC02;
    }
    else if (strcmp(chipModel, "ESP32-C6") == 0) {
        return BOARD_ESP32_C6_DEVKITC06;
    }
    else if (strcmp(chipModel, "ESP32-H2") == 0) {
        return BOARD_ESP32_H2_DEVKITC07;
    }
    else if (strcmp(chipModel, "ESP32-P4") == 0) {
        return BOARD_ESP32_P4_DEVKIT;
    }
    return BOARD_UNKNOWN;
}

// ──────────────────────────────────────────────────────────────
// Estructura de pines por modelo
// ──────────────────────────────────────────────────────────────

struct BoardPins {
    // I2C (pueden remapearse vía GPIO matrix, pero estos son defaults)
    int8_t i2c0_sda = -1;
    int8_t i2c0_scl = -1;
    int8_t i2c1_sda = -1;
    int8_t i2c1_scl = -1;

    // SPI
    int8_t spi_mosi = -1;
    int8_t spi_miso = -1;
    int8_t spi_sclk = -1;
    int8_t spi_cs = -1;

    // UART
    int8_t uart0_tx = -1;
    int8_t uart0_rx = -1;
    int8_t uart1_tx = -1;
    int8_t uart1_rx = -1;
    int8_t uart2_tx = -1;
    int8_t uart2_rx = -1;

    // ADC - GPIOs con capacidad ADC
    int8_t adc1_pins[10] = {-1};
    int8_t adc2_pins[10] = {-1};
    uint8_t adc1_count = 0;
    uint8_t adc2_count = 0;

    // DAC (solo ESP32 original y S2)
    int8_t dac_pins[2] = {-1, -1};
    bool has_dac = false;

    // Touch (capacitivo)
    int8_t touch_pins[14] = {-1};
    uint8_t touch_count = 0;

    // Pines SOLO entrada (sin pull-up/down interno, no pueden ser output)
    bool input_only[54] = {false};

    // Pines strapping (boot mode) - evitar usarlos como salida al arranque
    bool strapping[54] = {false};

    // Pines reservados (flash SPI, PSRAM, USB)
    bool reserved[54] = {false};

    // LED builtin
    int8_t led_builtin = -1;

    // SD Card (SPI mode defaults)
    int8_t sd_cs = -1;
    int8_t sd_mosi = -1;
    int8_t sd_miso = -1;
    int8_t sd_sclk = -1;

    // Capacidad
    bool has_psram = false;
    bool has_wifi = true;
    bool has_bluetooth = true;
    bool has_ethernet = false;
    bool has_usb_otg = false;
    bool has_thread = false;
    bool has_zigbee = false;
    bool has_temperature_sensor = false;

    uint32_t flash_size = 0;
    uint32_t psram_size = 0;
    uint8_t max_gpio = 39;
};

// ──────────────────────────────────────────────────────────────
// Validación de pin según tipo y modelo
// ──────────────────────────────────────────────────────────────

bool validatePin(BoardModel model, int8_t gpio, const char* pin_type) {
    if (gpio < 0) return false;

    BoardPins p = getBoardPins(model);

    if (gpio >= 54) return false;

    // Verificar si es solo entrada
    if (p.input_only[gpio] && (strcmp(pin_type, "output") == 0 || strcmp(pin_type, "pwm") == 0)) {
        return false;
    }

    // Verificar reservados
    if (p.reserved[gpio]) {
        Serial.printf("[WARN] GPIO %d reservado (flash/PSRAM/USB)\n", gpio);
        return false;
    }

    // Verificar strapping
    if (p.strapping[gpio] && (strcmp(pin_type, "output") == 0 || strcmp(pin_type, "pwm") == 0)) {
        Serial.printf("[WARN] GPIO %d es strapping pin, usar con cuidado como %s\n", gpio, pin_type);
    }

    // Verificar capacidades específicas
    if (strcmp(pin_type, "adc") == 0 || strcmp(pin_type, "analog") == 0) {
        bool is_adc = false;
        for (int i = 0; i < p.adc1_count; i++) if (p.adc1_pins[i] == gpio) { is_adc = true; break; }
        for (int i = 0; i < p.adc2_count; i++) if (p.adc2_pins[i] == gpio) { is_adc = true; break; }
        if (!is_adc) return false;
    }

    if (strcmp(pin_type, "dac") == 0) {
        if (!p.has_dac) return false;
        if (p.dac_pins[0] != gpio && p.dac_pins[1] != gpio) return false;
    }

    if (strcmp(pin_type, "touch") == 0) {
        bool is_touch = false;
        for (int i = 0; i < p.touch_count; i++) if (p.touch_pins[i] == gpio) { is_touch = true; break; }
        if (!is_touch) return false;
    }

    if (strcmp(pin_type, "i2c") == 0) {
        // I2C puede ser en casi cualquier GPIO via GPIO matrix
        // Solo verificar que no sea input_only ni reserved
    }

    return true;
}

// ──────────────────────────────────────────────────────────────
// Implementación getBoardPins() con datos oficiales Espressif
// ──────────────────────────────────────────────────────────────

BoardPins getBoardPins(BoardModel model) {
    BoardPins p;

    switch (model) {
        // ─────────────────────────────────────────────────────
        // ESP32-WROOM-32E/UE (DevKitC V4) - 30 pines expuestos
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_DEVKIT_V1: {
            // I2C0 default (GPIO matrix permite remapear)
            p.i2c0_sda = 21; p.i2c0_scl = 22;
            // SPI (VSPI default)
            p.spi_mosi = 23; p.spi_miso = 19; p.spi_sclk = 18; p.spi_cs = 5;
            // UART
            p.uart0_tx = 1; p.uart0_rx = 3;   // USB/Serial
            p.uart1_tx = 17; p.uart1_rx = 16; // Serial2
            p.uart2_tx = 43; p.uart2_rx = 44; // No en WROOM

            // ADC1 (uso con WiFi OK) - GPIOs: 32,33,34,35,36,39
            p.adc1_pins[0] = 32; p.adc1_pins[1] = 33; p.adc1_pins[2] = 34;
            p.adc1_pins[3] = 35; p.adc1_pins[4] = 36; p.adc1_pins[5] = 39;
            p.adc1_count = 6;

            // ADC2 (NO usar con WiFi activo) - GPIOs: 0,2,4,12-15,25-27
            p.adc2_pins[0] = 0; p.adc2_pins[1] = 2; p.adc2_pins[2] = 4;
            p.adc2_pins[3] = 12; p.adc2_pins[4] = 13; p.adc2_pins[5] = 14;
            p.adc2_pins[6] = 15; p.adc2_pins[7] = 25; p.adc2_pins[8] = 26; p.adc2_pins[9] = 27;
            p.adc2_count = 10;

            // DAC (solo ESP32 original)
            p.dac_pins[0] = 25; p.dac_pins[1] = 26;
            p.has_dac = true;

            // Touch (10 canales)
            p.touch_pins[0] = 0; p.touch_pins[1] = 2; p.touch_pins[2] = 4;
            p.touch_pins[3] = 12; p.touch_pins[4] = 13; p.touch_pins[5] = 14;
            p.touch_pins[6] = 15; p.touch_pins[7] = 27; p.touch_pins[8] = 32; p.touch_pins[9] = 33;
            p.touch_count = 10;

            // Input-only (sin pull-up/down, no output)
            for (int i = 34; i <= 39; i++) p.input_only[i] = true;

            // Strapping pins (afectan boot)
            p.strapping[0] = true; p.strapping[2] = true; p.strapping[12] = true; p.strapping[15] = true;
            // GPIO5 también strapping en algunos módulos
            p.strapping[5] = true;

            // Reservados (flash SPI)
            for (int i = 6; i <= 11; i++) p.reserved[i] = true;
            // GPIO20, 24, 28-31, 37, 38 no expuestos en WROOM-32
            p.reserved[20] = p.reserved[24] = true;
            for (int i = 28; i <= 31; i++) p.reserved[i] = true;
            p.reserved[37] = p.reserved[38] = true;

            p.led_builtin = 2;
            p.sd_cs = 5; p.sd_mosi = 23; p.sd_miso = 19; p.sd_sclk = 18;
            p.has_psram = false;
            p.flash_size = 4 * 1024 * 1024;
            p.max_gpio = 39;
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-WROVER-E/IE (PSRAM 8MB) - 30 pines
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_WROVER: {
            BoardPins base = getBoardPins(BOARD_ESP32_DEVKIT_V1);
            p = base;
            p.has_psram = true;
            p.psram_size = 8 * 1024 * 1024;
            // GPIO16 y 17 usados por PSRAM
            p.reserved[16] = p.reserved[17] = true;
            // GPIO12 strapping en WROVER (pull-up interno)
            p.strapping[12] = true;
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-S2 (SAOLA-1 / DevKitC-1) - 43 GPIOs, USB OTG
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_S2_SAOLA: {
            p.i2c0_sda = 12; p.i2c0_scl = 13;  // Default, remapeable
            p.i2c1_sda = 40; p.i2c1_scl = 41;  // Segundo I2C
            p.spi_mosi = 35; p.spi_miso = 37; p.spi_sclk = 36; p.spi_cs = 34;
            p.uart0_tx = 43; p.uart0_rx = 44;  // USB Serial
            p.uart1_tx = 44; p.uart1_rx = 43;  // UART1
            p.uart2_tx = 17; p.uart2_rx = 18;  // UART2

            // ADC: 20 canales (GPIO1-20) - 13-bit SAR ADC
            for (int i = 0; i < 20; i++) p.adc1_pins[i] = i + 1;
            p.adc1_count = 20;
            p.adc2_count = 0;  // S2 no tiene ADC2 separado

            // DAC (2 canales, 8-bit) - GPIO17, 18
            p.dac_pins[0] = 17; p.dac_pins[1] = 18;
            p.has_dac = true;

            // Touch: 14 canales (GPIO1-14)
            for (int i = 0; i < 14; i++) p.touch_pins[i] = i + 1;
            p.touch_count = 14;

            // NO hay input-only en S2
            // Strapping: GPIO0, GPIO46
            p.strapping[0] = true; p.strapping[46] = true;

            // Reservados: SPI flash GPIO26-32 (internos), USB GPIO19,20
            for (int i = 26; i <= 32; i++) p.reserved[i] = true;
            p.reserved[19] = p.reserved[20] = true; // USB D+/D-

            p.led_builtin = 15;  // LED azul en SAOLA
            p.sd_cs = 34; p.sd_mosi = 35; p.sd_miso = 37; p.sd_sclk = 36;
            p.has_psram = false;
            p.has_usb_otg = true;
            p.has_temperature_sensor = true;
            p.flash_size = 4 * 1024 * 1024;
            p.max_gpio = 45;
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-S3 (DevKitC-1) - 45 GPIOs, USB OTG, PSRAM opcional
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_S3_DEVKITC1: {
            p.i2c0_sda = 8; p.i2c0_scl = 9;
            p.i2c1_sda = 18; p.i2c1_scl = 17;
            p.spi_mosi = 11; p.spi_miso = 13; p.spi_sclk = 12; p.spi_cs = 10;
            p.uart0_tx = 43; p.uart0_rx = 44;  // USB Serial
            p.uart1_tx = 19; p.uart1_rx = 20;  // UART1 (también USB D+/D-)
            p.uart2_tx = 44; p.uart2_rx = 43;  // UART2

            // ADC: 20 canales (GPIO1-20) - 12-bit SAR ADC
            for (int i = 0; i < 20; i++) p.adc1_pins[i] = i + 1;
            p.adc1_count = 20;
            p.adc2_count = 0;

            // NO DAC en S3
            p.has_dac = false;

            // Touch: 14 canales (GPIO1-14)
            for (int i = 0; i < 14; i++) p.touch_pins[i] = i + 1;
            p.touch_count = 14;

            // NO input-only en S3
            // Strapping: GPIO0, GPIO46
            p.strapping[0] = true; p.strapping[46] = true;

            // Reservados: SPI flash/PSRAM GPIO26-32, USB GPIO19,20
            for (int i = 26; i <= 32; i++) p.reserved[i] = true;
            p.reserved[19] = p.reserved[20] = true; // USB D+/D-

            p.led_builtin = 48;  // RGB LED en GPIO48
            p.sd_cs = 10; p.sd_mosi = 11; p.sd_miso = 13; p.sd_sclk = 12;
            p.has_psram = psramFound();
            p.flash_size = 8 * 1024 * 1024;
            if (p.has_psram) p.psram_size = 2 * 1024 * 1024; // 2MB u 8MB
            p.max_gpio = 47;
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-C3 (DevKitC-02) - 22 GPIOs, RISC-V, Bluetooth 5
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_C3_DEVKITC02: {
            p.i2c0_sda = 5; p.i2c0_scl = 6;
            p.spi_mosi = 7; p.spi_miso = 2; p.spi_sclk = 6; p.spi_cs = 10;
            p.uart0_tx = 18; p.uart0_rx = 19;  // USB-JTAG
            p.uart1_tx = 20; p.uart1_rx = 21;  // UART1

            // ADC: 6 canales (GPIO0-5) - 12-bit SAR ADC
            for (int i = 0; i < 6; i++) p.adc1_pins[i] = i;
            p.adc1_count = 6;
            p.adc2_count = 0;

            // NO DAC, NO Touch en C3
            p.has_dac = false;
            p.touch_count = 0;

            // Strapping: GPIO0, GPIO2, GPIO8, GPIO9
            p.strapping[0] = p.strapping[2] = p.strapping[8] = p.strapping[9] = true;

            // Reservados: SPI flash GPIO12-17, USB-JTAG GPIO18,19
            for (int i = 12; i <= 17; i++) p.reserved[i] = true;
            p.reserved[18] = p.reserved[19] = true; // USB-JTAG

            p.led_builtin = 8;
            p.sd_cs = 10; p.sd_mosi = 7; p.sd_miso = 2; p.sd_sclk = 6;
            p.has_psram = false;
            p.flash_size = 4 * 1024 * 1024;
            p.has_temperature_sensor = true;
            p.max_gpio = 21;
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-C6 (DevKitC-06) - WiFi 6, Thread, Zigbee
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_C6_DEVKITC06: {
            BoardPins base = getBoardPins(BOARD_ESP32_C3_DEVKITC02);
            p = base;
            p.has_thread = true;
            p.has_zigbee = true;
            p.has_wifi = true; // WiFi 6 (802.11ax)
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-H2 (DevKitC-07) - Thread, Zigbee, BLE (sin WiFi)
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_H2_DEVKITC07: {
            BoardPins base = getBoardPins(BOARD_ESP32_C3_DEVKITC02);
            p = base;
            p.has_wifi = false;
            p.has_bluetooth = true; // BLE 5
            p.has_thread = true;
            p.has_zigbee = true;
            break;
        }

        // ─────────────────────────────────────────────────────
        // ESP32-P4 (DevKitC) - Dual-core, sin WiFi/BT, Ethernet
        // ─────────────────────────────────────────────────────
        case BOARD_ESP32_P4_DEVKIT: {
            p.i2c0_sda = 8; p.i2c0_scl = 9;
            p.spi_mosi = 11; p.spi_miso = 13; p.spi_sclk = 12; p.spi_cs = 10;
            p.uart0_tx = 43; p.uart0_rx = 44;
            p.uart1_tx = 19; p.uart1_rx = 20;
            p.uart2_tx = 44; p.uart2_rx = 43;

            // ADC: 20 canales (GPIO1-20)
            for (int i = 0; i < 20; i++) p.adc1_pins[i] = i + 1;
            p.adc1_count = 20;
            p.adc2_count = 0;

            p.has_dac = false;
            p.touch_count = 0;

            p.strapping[0] = true;
            p.led_builtin = 48;
            p.sd_cs = 10; p.sd_mosi = 11; p.sd_miso = 13; p.sd_sclk = 12;
            p.has_wifi = false;
            p.has_bluetooth = false;
            p.has_ethernet = true;
            p.flash_size = 8 * 1024 * 1024;
            p.max_gpio = 53;
            break;
        }

        default:
            break;
    }

    return p;
}

// ──────────────────────────────────────────────────────────────
// Utilidades
// ──────────────────────────────────────────────────────────────

const char* getBoardName(BoardModel model) {
    switch (model) {
        case BOARD_ESP32_DEVKIT_V1: return "ESP32-DevKitC (WROOM-32E/UE)";
        case BOARD_ESP32_WROVER: return "ESP32-WROVER-E/IE (PSRAM 8MB)";
        case BOARD_ESP32_S2_SAOLA: return "ESP32-S2 DevKitC-1 / SAOLA-1";
        case BOARD_ESP32_S3_DEVKITC1: return "ESP32-S3 DevKitC-1";
        case BOARD_ESP32_C3_DEVKITC02: return "ESP32-C3 DevKitC-02";
        case BOARD_ESP32_C6_DEVKITC06: return "ESP32-C6 DevKitC-06";
        case BOARD_ESP32_H2_DEVKITC07: return "ESP32-H2 DevKitC-07";
        case BOARD_ESP32_P4_DEVKIT: return "ESP32-P4 DevKit";
        default: return "DESCONOCIDO";
    }
}

bool isAdc1Pin(BoardModel model, int8_t gpio) {
    BoardPins p = getBoardPins(model);
    for (int i = 0; i < p.adc1_count; i++) if (p.adc1_pins[i] == gpio) return true;
    return false;
}

bool isAdc2Pin(BoardModel model, int8_t gpio) {
    BoardPins p = getBoardPins(model);
    for (int i = 0; i < p.adc2_count; i++) if (p.adc2_pins[i] == gpio) return true;
    return false;
}

bool canUseAdcWithWifi(BoardModel model, int8_t gpio) {
    // ADC1 siempre usable con WiFi; ADC2 NO usable con WiFi activo en ESP32 original
    if (model == BOARD_ESP32_DEVKIT_V1 || model == BOARD_ESP32_WROVER) {
        return isAdc1Pin(model, gpio);
    }
    // S2, S3, C3, C6, H2, P4 no tienen ADC2 separado
    return true;
}

#endif // BOARD_CONFIG_H
