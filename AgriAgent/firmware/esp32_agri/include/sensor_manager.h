/*
 * AgriAgents ESP32 - Sensor Manager
 *
 * Características:
 * - Lectura de sensores analógicos (ADC1/ADC2 con muestreo)
 * - Sensores I2C multi-dispositivo (bus compartido, direcciones múltiples)
 * - Auto-detección de sensores I2C conocidos (BME280, ADS1115, SHT31, etc.)
 * - Sensores de pulso (caudalímetros)
 * - Sensores GPIO (contactos, nivel, estado)
 * - Lectura periódica configurable por variable
 * - Validación de rangos y error_value
 * - Calidad de lectura (ok, suspect, error, stale)
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include "config.h"

// ──────────────────────────────────────────────────────────────
// Drivers de sensores I2C soportados
// ──────────────────────────────────────────────────────────────

enum I2CSensorType {
    I2C_UNKNOWN = 0,
    I2C_BME280,       // Temp/Hum/Presión - 0x76/0x77
    I2C_BMP280,       // Temp/Presión - 0x76/0x77
    I2C_SHT31,        // Temp/Hum - 0x44/0x45
    I2C_SHT40,        // Temp/Hum - 0x44/0x45
    I2C_ADS1115,      // ADC 16-bit 4ch - 0x48-0x4B
    I2C_ADS1015,      // ADC 12-bit 4ch - 0x48-0x4B
    I2C_MCP9808,      // Temp alta precisión - 0x18-0x1F
    I2C_TSL2561,      // Luz - 0x29/0x39/0x49
    I2C_BH1750,       // Luz - 0x23/0x5C
    I2C_MPU6050,      // IMU - 0x68/0x69
    I2C_PCA9685,      // PWM 16ch - 0x40-0x7F
    I2C_DS3231,       // RTC - 0x68
    I2C_CUSTOM        // Driver personalizado
};

// Direcciones I2C conocidas por sensor
struct I2CDeviceInfo {
    uint8_t addr;
    I2CSensorType type;
    const char* name;
    uint8_t expected_id_reg = 0;
    uint8_t expected_id_val = 0;
};

const I2CDeviceInfo KNOWN_I2C_DEVICES[] = {
    {0x76, I2C_BME280, "BME280", 0xD0, 0x60},
    {0x77, I2C_BME280, "BME280", 0xD0, 0x60},
    {0x76, I2C_BMP280, "BMP280", 0xD0, 0x58},
    {0x77, I2C_BMP280, "BMP280", 0xD0, 0x58},
    {0x44, I2C_SHT31, "SHT31", 0x00, 0x00},  // ID en 0x00
    {0x45, I2C_SHT31, "SHT31", 0x00, 0x00},
    {0x44, I2C_SHT40, "SHT40", 0x00, 0x00},
    {0x48, I2C_ADS1115, "ADS1115", 0x00, 0x00},
    {0x49, I2C_ADS1115, "ADS1115", 0x00, 0x00},
    {0x4A, I2C_ADS1115, "ADS1115", 0x00, 0x00},
    {0x4B, I2C_ADS1115, "ADS1115", 0x00, 0x00},
    {0x48, I2C_ADS1015, "ADS1015", 0x00, 0x00},
    {0x18, I2C_MCP9808, "MCP9808", 0x06, 0x00},
    {0x29, I2C_TSL2561, "TSL2561", 0x0A, 0x50},
    {0x23, I2C_BH1750, "BH1750", 0x00, 0x00},
    {0x5C, I2C_BH1750, "BH1750", 0x00, 0x00},
    {0x68, I2C_MPU6050, "MPU6050", 0x75, 0x68},
    {0x40, I2C_PCA9685, "PCA9685", 0x00, 0x00},
    {0x68, I2C_DS3231, "DS3231", 0x00, 0x00},
};

// ──────────────────────────────────────────────────────────────
// Estructuras de sensores
// ──────────────────────────────────────────────────────────────

struct SensorInstance {
    char sensor_id[32];
    char kind[32];
    char model[32];
    char port[32];           // "i2c:0x77", "adc:1", "gpio:32", "pulse:27"
    int8_t gpio = -1;
    uint8_t i2c_addr = 0;
    int8_t i2c_bus = 0;      // 0 = Wire, 1 = Wire1
    I2CSensorType i2c_type = I2C_UNKNOWN;
    int depth_cm = 0;
    bool enabled = true;
    uint32_t interval_ms = 60000;
    uint32_t interval_night_ms = 180000;
    uint32_t last_read = 0;
    bool is_night = false;

    // Configuración de variables
    struct Variable {
        char key[32];
        bool enabled = true;
        uint32_t interval_ms = 60000;
        uint32_t interval_night_ms = 180000;
        float range_min = -9999;
        float range_max = 9999;
        float warn_min = -9999;
        float warn_max = 9999;
        float error_value = -9999;
    };
    Variable variables[10];
    uint8_t var_count = 0;

    // Driver específico
    void* driver_data = nullptr;
    bool driver_initialized = false;
};

struct Reading {
    char key[32];
    float value;
    char quality[16];  // "ok", "suspect", "error", "stale"
    char timestamp[32];
    uint32_t seq;
    uint8_t sensor_idx;
};

class SensorManager {
public:
    SensorManager();
    ~SensorManager();

    void init(const DeviceConfig& config);
    void updateConfig(const DeviceConfig& config);

    // Loop periódico
    void loop(uint32_t now);

    // Lectura de todos los sensores
    std::vector<Reading> readAll();

    // Lectura de una variable específica
    bool readVariable(uint8_t sensor_idx, uint8_t var_idx, Reading& out);

    // Escaneo I2C
    void scanI2C();
    void scanI2CBus(TwoWire& wire, int bus_num);

    // Auto-detección I2C
    I2CSensorType detectI2CSensor(TwoWire& wire, uint8_t addr);
    bool initI2CSensor(SensorInstance& sensor);

    // Drivers específicos
    bool readBME280(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out);
    bool readSHT31(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out);
    bool readADS1115(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out);
    bool readMCP9808(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out);

    // Sensores analógicos
    float readAnalog(int8_t gpio, int samples = ADC_SAMPLES);
    float readAnalogCalibrated(int8_t gpio, float slope, float offset, int samples = ADC_SAMPLES);

    // Sensores de pulso (caudalímetros)
    uint32_t getPulseCount(int8_t gpio);
    void resetPulseCount(int8_t gpio);
    float calculateFlow(uint32_t pulses, float k_factor, uint32_t period_ms);

    // Sensores GPIO (contactos, nivel)
    bool readGPIO(int8_t gpio);

    // Utilidades
    bool isNightTime() const;
    void setTimezone(int offset_minutes);
    char* getISOTime(char* buffer, size_t len);

    // Reportar sensores detectados a API
    std::vector<SensorInfo> getDetectedSensors();

private:
    DeviceConfig _config;
    std::vector<SensorInstance> _sensors;
    std::vector<SensorInstance> _i2c_sensors;
    std::vector<SensorInstance> _adc_sensors;
    std::vector<SensorInstance> _pulse_sensors;
    std::vector<SensorInstance> _gpio_sensors;

    int _tz_offset_minutes = -240;
    bool _time_synced = false;

    // Pulse counting (ISR)
    static volatile uint32_t _pulse_counts[8];
    static int8_t _pulse_gpios[8];
    static uint8_t _pulse_count;

    // Helpers
    bool _matchPort(const char* port, int8_t& out_gpio, uint8_t& out_i2c_addr, int8_t& out_i2c_bus);
    void _initSensorFromConfig(SensorInstance& sensor, const SensorConfig& cfg);
    char _evaluateQuality(float value, const SensorInstance::Variable& var);
    void _applyCalibration(float& value, const SensorInstance::Variable& var);
};

// ISR para conteo de pulsos
void IRAM_ATTR pulseISR();

#endif // SENSOR_MANAGER_H
