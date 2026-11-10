/*
 * AgriAgents ESP32 - Sensor Manager Implementation
 *
 * I2C Multi-dispositivo:
 * - 2 buses I2C (I2C0 e I2C1) en paralelo
 * - Escaneo automático al boot
 * - Drivers para BME280, SHT31, ADS1115, MCP9808, etc.
 * - Mux TCA9548A para colisiones de dirección
 *
 * Persistencia:
 * - Config en NVS (Preferences)
 * - Buffer offline en NVS/SD
 */

#include "sensor_manager.h"
#include "config.h"
#include <Preferences.h>
#include <Wire.h>

// ──────────────────────────────────────────────────────────────
// Variables estáticas para ISR pulsos
// ──────────────────────────────────────────────────────────────
volatile uint32_t SensorManager::_pulse_counts[8] = {0};
int8_t SensorManager::_pulse_gpios[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
uint8_t SensorManager::_pulse_count = 0;

// ──────────────────────────────────────────────────────────────
// SensorManager
// ──────────────────────────────────────────────────────────────

SensorManager::SensorManager() {}
SensorManager::~SensorManager() {}

void SensorManager::init(const DeviceConfig& config) {
    _config = config;
    _tz_offset_minutes = config.tz_offset_minutes;

    // Inicializar buses I2C
    if (config.pins.i2c0_sda >= 0 && config.pins.i2c0_scl >= 0) {
        Wire.begin(config.pins.i2c0_sda, config.pins.i2c0_scl, 100000);
        Serial.printf("I2C0 iniciado: SDA=%d SCL=%d\n", config.pins.i2c0_sda, config.pins.i2c0_scl);
    }
    if (config.pins.i2c1_sda >= 0 && config.pins.i2c1_scl >= 0) {
        Wire1.begin(config.pins.i2c1_sda, config.pins.i2c1_scl, 100000);
        Serial.printf("I2C1 iniciado: SDA=%d SCL=%d\n", config.pins.i2c1_sda, config.pins.i2c1_scl);
    }

    // Configurar ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_ATTENUATION);
    analogSetSamples(ADC_SAMPLES);

    // Parsear sensores de config
    _sensors.clear();
    _i2c_sensors.clear();
    _adc_sensors.clear();
    _pulse_sensors.clear();
    _gpio_sensors.clear();

    for (uint8_t i = 0; i < config.sensor_count; i++) {
        SensorInstance sensor;
        _initSensorFromConfig(sensor, config.sensors[i]);
        _sensors.push_back(sensor);

        // Clasificar por tipo de puerto
        int8_t gpio; uint8_t i2c_addr; int8_t i2c_bus;
        if (_matchPort(sensor.port, gpio, i2c_addr, i2c_bus)) {
            if (strncmp(sensor.port, "i2c:", 4) == 0) {
                sensor.i2c_addr = i2c_addr;
                sensor.i2c_bus = i2c_bus;
                sensor.i2c_type = detectI2CSensor(i2c_bus == 0 ? Wire : Wire1, i2c_addr);
                if (sensor.i2c_type != I2C_UNKNOWN) {
                    initI2CSensor(sensor);
                }
                _i2c_sensors.push_back(sensor);
            }
            else if (strncmp(sensor.port, "adc:", 4) == 0) {
                sensor.gpio = gpio;
                if (validatePin(config.board_model, gpio, "adc")) {
                    _adc_sensors.push_back(sensor);
                }
            }
            else if (strncmp(sensor.port, "pulse:", 6) == 0) {
                sensor.gpio = gpio;
                pinMode(gpio, INPUT_PULLUP);
                attachInterrupt(digitalPinToInterrupt(gpio), pulseISR, RISING);
                _pulse_sensors.push_back(sensor);
            }
            else if (strncmp(sensor.port, "gpio:", 5) == 0) {
                sensor.gpio = gpio;
                pinMode(gpio, INPUT_PULLUP);
                _gpio_sensors.push_back(sensor);
            }
        }
    }

    // Escaneo I2C inicial
    scanI2C();

    Serial.printf("Sensores: %d total (%d I2C, %d ADC, %d Pulse, %d GPIO)\n",
                  _sensors.size(), _i2c_sensors.size(), _adc_sensors.size(),
                  _pulse_sensors.size(), _gpio_sensors.size());
}

void SensorManager::updateConfig(const DeviceConfig& config) {
    _config = config;
    _tz_offset_minutes = config.tz_offset_minutes;
    // Re-inicializar sensores si cambió config
    init(config);
}

void SensorManager::loop(uint32_t now) {
    // Lectura periódica por sensor
    for (auto& sensor : _sensors) {
        if (!sensor.enabled) continue;
        uint32_t interval = sensor.is_night ? sensor.interval_night_ms : sensor.interval_ms;
        if (now - sensor.last_read >= interval) {
            sensor.last_read = now;
            // La lectura real se hace en readAll()
        }
    }

    // Detectar día/noche
    static bool last_night = false;
    bool now_night = isNightTime();
    if (now_night != last_night) {
        for (auto& sensor : _sensors) sensor.is_night = now_night;
        last_night = now_night;
    }
}

std::vector<Reading> SensorManager::readAll() {
    std::vector<Reading> readings;
    uint32_t seq = g_config_mgr.incrementSeqCounter();
    char timestamp[32];
    getISOTime(timestamp, sizeof(timestamp));

    // I2C sensors
    for (auto& sensor : _i2c_sensors) {
        if (!sensor.enabled) continue;
        TwoWire& wire = (sensor.i2c_bus == 0) ? Wire : Wire1;
        for (auto& var : sensor.variables) {
            if (!var.enabled) continue;
            Reading r;
            strncpy(r.key, var.key, sizeof(r.key));
            r.seq = seq;
            strncpy(r.timestamp, timestamp, sizeof(r.timestamp));
            r.sensor_idx = &sensor - &_i2c_sensors[0];

            bool ok = false;
            switch (sensor.i2c_type) {
                case I2C_BME280: ok = readBME280(wire, sensor.i2c_addr, sensor, r); break;
                case I2C_SHT31: ok = readSHT31(wire, sensor.i2c_addr, sensor, r); break;
                case I2C_ADS1115: ok = readADS1115(wire, sensor.i2c_addr, sensor, r); break;
                case I2C_MCP9808: ok = readMCP9808(wire, sensor.i2c_addr, sensor, r); break;
                default: ok = false;
            }
            if (!ok) {
                r.value = var.error_value;
                strncpy(r.quality, "error", sizeof(r.quality));
            } else {
                r.quality[0] = '\0'; // se evalúa abajo
            }
            char q = _evaluateQuality(r.value, var);
            r.quality[0] = q; r.quality[1] = '\0';
            readings.push_back(r);
        }
    }

    // ADC sensors
    for (auto& sensor : _adc_sensors) {
        if (!sensor.enabled) continue;
        for (auto& var : sensor.variables) {
            if (!var.enabled) continue;
            Reading r;
            strncpy(r.key, var.key, sizeof(r.key));
            r.seq = seq;
            strncpy(r.timestamp, timestamp, sizeof(r.timestamp));
            r.sensor_idx = &sensor - &_adc_sensors[0] + _i2c_sensors.size();

            float val = readAnalog(sensor.gpio);
            _applyCalibration(val, var);
            r.value = val;
            char q = _evaluateQuality(val, var);
            r.quality[0] = q; r.quality[1] = '\0';
            readings.push_back(r);
        }
    }

    // Pulse sensors (caudalímetros)
    for (auto& sensor : _pulse_sensors) {
        if (!sensor.enabled) continue;
        for (auto& var : sensor.variables) {
            if (!var.enabled) continue;
            Reading r;
            strncpy(r.key, var.key, sizeof(r.key));
            r.seq = seq;
            strncpy(r.timestamp, timestamp, sizeof(r.timestamp));
            r.sensor_idx = &sensor - &_pulse_sensors[0] + _i2c_sensors.size() + _adc_sensors.size();

            uint32_t pulses = getPulseCount(sensor.gpio);
            float flow = calculateFlow(pulses, 7.5, sensor.interval_ms); // K-factor ejemplo
            _applyCalibration(flow, var);
            r.value = flow;
            char q = _evaluateQuality(flow, var);
            r.quality[0] = q; r.quality[1] = '\0';
            readings.push_back(r);
        }
    }

    // GPIO sensors (contactos, nivel)
    for (auto& sensor : _gpio_sensors) {
        if (!sensor.enabled) continue;
        for (auto& var : sensor.variables) {
            if (!var.enabled) continue;
            Reading r;
            strncpy(r.key, var.key, sizeof(r.key));
            r.seq = seq;
            strncpy(r.timestamp, timestamp, sizeof(r.timestamp));
            r.sensor_idx = &sensor - &_gpio_sensors[0] + _i2c_sensors.size() + _adc_sensors.size() + _pulse_sensors.size();

            bool val = readGPIO(sensor.gpio);
            r.value = val ? 1.0 : 0.0;
            char q = _evaluateQuality(r.value, var);
            r.quality[0] = q; r.quality[1] = '\0';
            readings.push_back(r);
        }
    }

    return readings;
}

bool SensorManager::readVariable(uint8_t sensor_idx, uint8_t var_idx, Reading& out) {
    // TODO: Implementar lectura individual
    return false;
}

// ──────────────────────────────────────────────────────────────
// Escaneo I2C multi-bus
// ──────────────────────────────────────────────────────────────

void SensorManager::scanI2C() {
    if (_config.pins.i2c0_sda >= 0) {
        scanI2CBus(Wire, 0);
    }
    if (_config.pins.i2c1_sda >= 0) {
        scanI2CBus(Wire1, 1);
    }
}

void SensorManager::scanI2CBus(TwoWire& wire, int bus_num) {
    Serial.printf("Escaneando I2C bus %d...\n", bus_num);
    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        wire.beginTransmission(addr);
        if (wire.endTransmission() == 0) {
            I2CSensorType type = detectI2CSensor(wire, addr);
            const char* name = "DESCONOCIDO";
            for (const auto& d : KNOWN_I2C_DEVICES) {
                if (d.addr == addr && d.type == type) { name = d.name; break; }
            }
            Serial.printf("  Bus %d: 0x%02X -> %s\n", bus_num, addr, name);
            found++;
        }
    }
    Serial.printf("  Encontrados: %d dispositivos\n", found);
}

I2CSensorType SensorManager::detectI2CSensor(TwoWire& wire, uint8_t addr) {
    for (const auto& d : KNOWN_I2C_DEVICES) {
        if (d.addr == addr) {
            // Verificar ID register si existe
            if (d.expected_id_reg != 0) {
                wire.beginTransmission(addr);
                wire.write(d.expected_id_reg);
                if (wire.endTransmission() == 0) {
                    wire.requestFrom(addr, 1);
                    if (wire.available()) {
                        uint8_t id = wire.read();
                        if (id == d.expected_id_val) return d.type;
                    }
                }
            } else {
                return d.type; // Sin verificación de ID
            }
        }
    }
    return I2C_CUSTOM;
}

bool SensorManager::initI2CSensor(SensorInstance& sensor) {
    TwoWire& wire = (sensor.i2c_bus == 0) ? Wire : Wire1;
    switch (sensor.i2c_type) {
        case I2C_BME280: {
            // Soft reset
            wire.beginTransmission(sensor.i2c_addr);
            wire.write(0xE0); wire.write(0xB6);
            wire.endTransmission();
            delay(10);
            sensor.driver_initialized = true;
            strncpy(sensor.driver, "bme280", sizeof(sensor.driver));
            return true;
        }
        case I2C_SHT31: {
            // High repeatability measurement
            wire.beginTransmission(sensor.i2c_addr);
            wire.write(0x2C); wire.write(0x06);
            wire.endTransmission();
            sensor.driver_initialized = true;
            strncpy(sensor.driver, "sht31", sizeof(sensor.driver));
            return true;
        }
        case I2C_ADS1115: {
            // Config: AIN0, 4.096V, 128 SPS, single-shot
            wire.beginTransmission(sensor.i2c_addr);
            wire.write(0x01); // Config register
            wire.write(0xC3); wire.write(0x83);
            wire.endTransmission();
            sensor.driver_initialized = true;
            strncpy(sensor.driver, "ads1115", sizeof(sensor.driver));
            return true;
        }
        case I2C_MCP9808: {
            // Set resolution 0.0625°C
            wire.beginTransmission(sensor.i2c_addr);
            wire.write(0x08); wire.write(0x03);
            wire.endTransmission();
            sensor.driver_initialized = true;
            strncpy(sensor.driver, "mcp9808", sizeof(sensor.driver));
            return true;
        }
        default:
            return false;
    }
}

// ──────────────────────────────────────────────────────────────
// Drivers I2C específicos
// ──────────────────────────────────────────────────────────────

bool SensorManager::readBME280(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out) {
    // Trigger measurement
    wire.beginTransmission(addr);
    wire.write(0xF4); wire.write(0x27); // Normal mode
    wire.endTransmission();
    delay(10);

    // Read 8 bytes: P[20:0] T[20:0] H[15:0]
    wire.beginTransmission(addr);
    wire.write(0xF7);
    if (wire.endTransmission() != 0) return false;
    wire.requestFrom(addr, 8);
    if (wire.available() < 8) return false;

    uint32_t press_raw = (wire.read() << 12) | (wire.read() << 4) | (wire.read() >> 4);
    uint32_t temp_raw = (wire.read() << 12) | (wire.read() << 4) | (wire.read() >> 4);
    uint16_t hum_raw = (wire.read() << 8) | wire.read();

    // Compensación simplificada (en producción usar librería completa)
    float temp = (temp_raw / 16384.0 - 25.0); // Aproximado
    float press = press_raw / 25600.0; // hPa aprox
    float hum = hum_raw / 1024.0 * 100.0;

    if (strstr(out.key, "temp")) out.value = temp;
    else if (strstr(out.key, "press")) out.value = press;
    else if (strstr(out.key, "hum")) out.value = hum;
    return true;
}

bool SensorManager::readSHT31(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out) {
    wire.beginTransmission(addr);
    wire.write(0x2C); wire.write(0x06); // High repeatability
    wire.endTransmission();
    delay(16);

    wire.requestFrom(addr, 6);
    if (wire.available() < 6) return false;

    uint16_t temp_raw = (wire.read() << 8) | wire.read(); wire.read(); // CRC
    uint16_t hum_raw = (wire.read() << 8) | wire.read(); // CRC

    float temp = -45 + 175 * (temp_raw / 65535.0);
    float hum = 100 * (hum_raw / 65535.0);

    if (strstr(out.key, "temp")) out.value = temp;
    else if (strstr(out.key, "hum")) out.value = hum;
    return true;
}

bool SensorManager::readADS1115(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out) {
    // Leer canal 0 (AIN0-GND)
    wire.beginTransmission(addr);
    wire.write(0x00); // Conversion register
    wire.endTransmission();

    wire.requestFrom(addr, 2);
    if (wire.available() < 2) return false;

    int16_t raw = (wire.read() << 8) | wire.read();
    float voltage = raw * 0.125 / 1000.0; // 4.096V range, 16-bit

    out.value = voltage;
    return true;
}

bool SensorManager::readMCP9808(TwoWire& wire, uint8_t addr, SensorInstance& sensor, Reading& out) {
    wire.beginTransmission(addr);
    wire.write(0x05); // Temp register
    if (wire.endTransmission() != 0) return false;

    wire.requestFrom(addr, 2);
    if (wire.available() < 2) return false;

    uint16_t raw = (wire.read() << 8) | wire.read();
    float temp = (raw & 0x0FFF) / 16.0;
    if (raw & 0x1000) temp -= 256; // Sign bit

    out.value = temp;
    return true;
}

// ──────────────────────────────────────────────────────────────
// Sensores analógicos
// ──────────────────────────────────────────────────────────────

float SensorManager::readAnalog(int8_t gpio, int samples) {
    if (!validatePin(g_config.board_model, gpio, "adc")) return NAN;

    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(gpio);
        delayMicroseconds(100);
    }
    float avg = sum / (float)samples;
    // Convertir a voltaje (ESP32 ADC 12-bit, 3.3V ref con atten 11dB)
    return avg * 3.3 / 4095.0;
}

float SensorManager::readAnalogCalibrated(int8_t gpio, float slope, float offset, int samples) {
    float v = readAnalog(gpio, samples);
    return v * slope + offset;
}

// ──────────────────────────────────────────────────────────────
// Sensores de pulso (caudalímetros)
// ──────────────────────────────────────────────────────────────

void IRAM_ATTR pulseISR() {
    // Buscar qué GPIO disparó
    for (uint8_t i = 0; i < SensorManager::_pulse_count; i++) {
        if (digitalRead(SensorManager::_pulse_gpios[i]) == HIGH) {
            SensorManager::_pulse_counts[i]++;
            break;
        }
    }
}

uint32_t SensorManager::getPulseCount(int8_t gpio) {
    for (uint8_t i = 0; i < _pulse_count; i++) {
        if (_pulse_gpios[i] == gpio) {
            uint32_t count = _pulse_counts[i];
            _pulse_counts[i] = 0; // Reset para próxima ventana
            return count;
        }
    }
    return 0;
}

void SensorManager::resetPulseCount(int8_t gpio) {
    for (uint8_t i = 0; i < _pulse_count; i++) {
        if (_pulse_gpios[i] == gpio) {
            _pulse_counts[i] = 0;
            break;
        }
    }
}

float SensorManager::calculateFlow(uint32_t pulses, float k_factor, uint32_t period_ms) {
    // K-factor: pulsos por litro (ej. 7.5 para YF-S201)
    float liters = pulses / k_factor;
    float flow_l_min = liters / (period_ms / 60000.0);
    return flow_l_min;
}

// ──────────────────────────────────────────────────────────────
// Sensores GPIO
// ──────────────────────────────────────────────────────────────

bool SensorManager::readGPIO(int8_t gpio) {
    return digitalRead(gpio) == HIGH;
}

// ──────────────────────────────────────────────────────────────
// Utilidades
// ──────────────────────────────────────────────────────────────

bool SensorManager::_matchPort(const char* port, int8_t& out_gpio, uint8_t& out_i2c_addr, int8_t& out_i2c_bus) {
    if (strncmp(port, "i2c:", 4) == 0) {
        // i2c:0x77 o i2c:0x77:1 (bus 1)
        char* end;
        out_i2c_addr = strtoul(port + 4, &end, 0);
        out_i2c_bus = (*end == ':') ? atoi(end + 1) : 0;
        return true;
    }
    else if (strncmp(port, "adc:", 4) == 0) {
        out_gpio = atoi(port + 4);
        return true;
    }
    else if (strncmp(port, "pulse:", 6) == 0) {
        out_gpio = atoi(port + 6);
        return true;
    }
    else if (strncmp(port, "gpio:", 5) == 0) {
        out_gpio = atoi(port + 5);
        return true;
    }
    return false;
}

void SensorManager::_initSensorFromConfig(SensorInstance& sensor, const SensorConfig& cfg) {
    strncpy(sensor.sensor_id, cfg.sensor_id, sizeof(sensor.sensor_id));
    strncpy(sensor.kind, cfg.kind, sizeof(sensor.kind));
    strncpy(sensor.model, cfg.model, sizeof(sensor.model));
    strncpy(sensor.port, cfg.port, sizeof(sensor.port));
    sensor.gpio = cfg.gpio;
    sensor.depth_cm = cfg.depth_cm;
    sensor.enabled = cfg.enabled;
    sensor.interval_ms = cfg.interval_ms;
    sensor.interval_night_ms = cfg.interval_night_ms;
    sensor.var_count = 0;
    sensor.last_read = 0;
    sensor.is_night = false;

    // Buscar variables correspondientes en config
    for (uint8_t i = 0; i < _config.variable_count && sensor.var_count < 10; i++) {
        VariableConfig& vc = _config.variables[i];
        // Asumir que variable pertenece a este sensor si key coincide con kind
        // En producción: mapeo explícito sensor_id -> variables
        SensorInstance::Variable& sv = sensor.variables[sensor.var_count++];
        strncpy(sv.key, vc.key, sizeof(sv.key));
        sv.enabled = vc.enabled;
        sv.interval_ms = vc.interval_ms;
        sv.interval_night_ms = vc.interval_night_ms;
        sv.range_min = vc.range_min;
        sv.range_max = vc.range_max;
        sv.warn_min = vc.warn_min;
        sv.warn_max = vc.warn_max;
        sv.error_value = vc.error_value;
    }
}

char SensorManager::_evaluateQuality(float value, const Variable& var) {
    if (value == var.error_value) return 'e'; // error
    if (value < var.range_min || value > var.range_max) return 's'; // suspect
    if (value < var.warn_min || value > var.warn_max) return 'w'; // warning
    return 'o'; // ok
}

void SensorManager::_applyCalibration(float& value, const Variable& var) {
    // Placeholder para calibración lineal: value = value * slope + offset
    // Se podría añadir slope/offset en VariableConfig
}

bool SensorManager::isNightTime() const {
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    int hour = tm->tm_hour;
    // Noche: 20:00 - 06:00
    return hour >= 20 || hour < 6;
}

void SensorManager::setTimezone(int offset_minutes) {
    _tz_offset_minutes = offset_minutes;
    setenv("TZ", "UTC", 1); // Simplificado
    tzset();
}

char* SensorManager::getISOTime(char* buffer, size_t len) {
    time_t now = time(nullptr);
    struct tm* tm = gmtime(&now);
    strftime(buffer, len, "%Y-%m-%dT%H:%M:%SZ", tm);
    return buffer;
}

std::vector<SensorInfo> SensorManager::getDetectedSensors() {
    std::vector<SensorInfo> infos;
    for (const auto& s : _i2c_sensors) {
        SensorInfo info;
        strncpy(info.sensor_id, s.sensor_id, sizeof(info.sensor_id));
        strncpy(info.kind, s.kind, sizeof(info.kind));
        strncpy(info.model, s.model, sizeof(info.model));
        snprintf(info.port, sizeof(info.port), "i2c:0x%02X:%d", s.i2c_addr, s.i2c_bus);
        info.i2c_addr = s.i2c_addr;
        info.gpio = -1;
        strncpy(info.driver, s.driver, sizeof(info.driver));
        info.auto_detected = true;
        info.range_min = -9999;
        info.range_max = 9999;
        info.error_value = -9999;
        infos.push_back(info);
    }
    // ADC, pulse, GPIO sensors también
    return infos;
}
