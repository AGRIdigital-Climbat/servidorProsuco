/*
 * AgriAgents ESP32 - Command Processor Implementation
 */

#include "command_processor.h"
#include "config.h"
#include <driver/ledc.h>

CommandProcessor::CommandProcessor() {}
CommandProcessor::~CommandProcessor() {}

void CommandProcessor::init(const DeviceConfig& config) {
    _config = config;
    _pending_commands.clear();

    // Configurar PWM timer
    ledcSetup(0, 5000, 8); // Canal 0, 5kHz, 8-bit (se reconfigura por comando)
    for (int i = 0; i < 16; i++) {
        _pwm_channels[i] = false;
        _pwm_gpios[i] = -1;
    }

    Serial.println("CommandProcessor inicializado");
}

void CommandProcessor::updateConfig(const DeviceConfig& config) {
    _config = config;
}

void CommandProcessor::queueCommand(const Command& cmd, const char* tracking_id) {
    String tid = tracking_id ? tracking_id : "";
    _pending_commands.push_back({cmd, tid});
    Serial.printf("Comando encolado: %s (kind=%s, action=%s)\n",
                  cmd.command_id, cmd.kind, cmd.action);
}

void CommandProcessor::processPending() {
    while (!_pending_commands.empty()) {
        auto [cmd, tracking_id] = _pending_commands.front();
        _pending_commands.erase(_pending_commands.begin());

        CommandResult result = executeCommand(cmd);

        // Enviar ACK a la API
        // http.ackCommand(cmd.command_id, result);

        Serial.printf("Comando %s: %s\n", cmd.command_id, result.success ? "OK" : "FAIL");
        if (!result.success && result.error_msg[0]) {
            Serial.printf("  Error: %s\n", result.error_msg);
        }
    }
}

CommandResult CommandProcessor::executeCommand(const Command& cmd) {
    CommandResult result;
    strncpy(result.command_id, cmd.command_id, sizeof(result.command_id));
    strncpy(result.tracking_id, cmd.tracking_id, sizeof(result.tracking_id));

    if (strcmp(cmd.kind, "actuate") == 0) {
        if (strcmp(cmd.action, "on") == 0) {
            result.success = setPinOutput(cmd.gpio, 1);
        } else if (strcmp(cmd.action, "off") == 0) {
            result.success = setPinOutput(cmd.gpio, 0);
        } else if (strcmp(cmd.action, "pwm") == 0) {
            result.success = setPinPWM(cmd.gpio, cmd.value, cmd.freq_hz, cmd.channel);
        } else {
            snprintf(result.error_msg, sizeof(result.error_msg), "Acción no soportada: %s", cmd.action);
        }
    }
    else if (strcmp(cmd.kind, "restart") == 0) {
        restartDevice("remote restart");
        result.success = true; // No llega aquí
    }
    else if (strcmp(cmd.kind, "pin_test") == 0) {
        result = testPin(cmd);
    }
    else if (strcmp(cmd.kind, "self_test") == 0) {
        result = selfTest();
    }
    else {
        snprintf(result.error_msg, sizeof(result.error_msg), "Kind desconocido: %s", cmd.kind);
    }

    return result;
}

// ──────────────────────────────────────────────────────────────
// Control de pines
// ──────────────────────────────────────────────────────────────

int8_t CommandProcessor::_resolvePin(const char* pin_name) {
    // Buscar en pinout configurado
    for (int i = 0; i < _config.pins.adc1_count; i++) {
        // En producción: mapear pin_name -> gpio desde config.pins
    }
    // Por ahora: pin_name puede ser "gpio:26" o número directo
    if (strncmp(pin_name, "gpio:", 5) == 0) {
        return atoi(pin_name + 5);
    }
    return atoi(pin_name);
}

bool CommandProcessor::_validatePinForAction(int8_t gpio, const char* action) {
    return validatePin(_config.board_model, gpio, action);
}

bool CommandProcessor::setPinOutput(int8_t gpio, int value) {
    if (!_validatePinForAction(gpio, "output")) {
        return false;
    }

    // Liberar PWM si estaba en uso
    for (int i = 0; i < 16; i++) {
        if (_pwm_channels[i] && _pwm_gpios[i] == gpio) {
            ledcDetachPin(gpio);
            _pwm_channels[i] = false;
            _pwm_gpios[i] = -1;
            break;
        }
    }

    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, value ? HIGH : LOW);
    return true;
}

bool CommandProcessor::setPinPWM(int8_t gpio, int value, int freq_hz, int channel) {
    if (!_validatePinForAction(gpio, "pwm")) {
        return false;
    }

    if (channel < 0) channel = _getFreePWMChannel();
    if (channel < 0) return false;

    if (freq_hz <= 0) freq_hz = 5000;
    if (value < 0) value = 0;
    if (value > 255) value = 255;

    ledcSetup(channel, freq_hz, 8);
    ledcAttachPin(gpio, channel);
    ledcWrite(channel, value);

    _pwm_channels[channel] = true;
    _pwm_gpios[channel] = gpio;
    return true;
}

int CommandProcessor::readPin(int8_t gpio) {
    if (!_validatePinForAction(gpio, "input")) return -1;
    pinMode(gpio, INPUT_PULLUP);
    return digitalRead(gpio);
}

float CommandProcessor::readPinADC(int8_t gpio) {
    if (!_validatePinForAction(gpio, "adc")) return NAN;
    return analogRead(gpio) * 3.3 / 4095.0;
}

int CommandProcessor::_getFreePWMChannel() {
    for (int i = 0; i < 16; i++) {
        if (!_pwm_channels[i]) return i;
    }
    return -1;
}

void CommandProcessor::_releasePWMChannel(int channel) {
    if (channel >= 0 && channel < 16 && _pwm_channels[channel]) {
        int8_t gpio = _pwm_gpios[channel];
        if (gpio >= 0) ledcDetachPin(gpio);
        _pwm_channels[channel] = false;
        _pwm_gpios[channel] = -1;
    }
}

// ──────────────────────────────────────────────────────────────
// Pin Test
// ──────────────────────────────────────────────────────────────

CommandResult CommandProcessor::testPin(const Command& cmd) {
    CommandResult result;
    strncpy(result.command_id, cmd.command_id, sizeof(result.command_id));
    strncpy(result.tracking_id, cmd.tracking_id, sizeof(result.tracking_id));

    int8_t gpio = cmd.gpio >= 0 ? cmd.gpio : _resolvePin(cmd.pin_name);
    if (gpio < 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), "Pin no válido");
        return result;
    }

    if (strcmp(cmd.action, "read") == 0) {
        if (!_validatePinForAction(gpio, "input")) {
            snprintf(result.error_msg, sizeof(result.error_msg), "Pin no puede ser input");
            return result;
        }
        pinMode(gpio, INPUT_PULLUP);
        result.return_value = digitalRead(gpio);
        result.success = true;
    }
    else if (strcmp(cmd.action, "write") == 0) {
        if (!_validatePinForAction(gpio, "output")) {
            snprintf(result.error_msg, sizeof(result.error_msg), "Pin no puede ser output");
            return result;
        }
        result.success = setPinOutput(gpio, cmd.value);
    }
    else if (strcmp(cmd.action, "adc") == 0) {
        if (!_validatePinForAction(gpio, "adc")) {
            snprintf(result.error_msg, sizeof(result.error_msg), "Pin no tiene ADC");
            return result;
        }
        result.return_float = readPinADC(gpio);
        result.success = true;
    }
    else if (strcmp(cmd.action, "pwm") == 0) {
        if (!_validatePinForAction(gpio, "pwm")) {
            snprintf(result.error_msg, sizeof(result.error_msg), "Pin no puede ser PWM");
            return result;
        }
        int freq = cmd.freq_hz > 0 ? cmd.freq_hz : 5000;
        result.success = setPinPWM(gpio, cmd.value, freq, cmd.channel);
    }
    else if (strcmp(cmd.action, "i2c_scan") == 0) {
        // Scan I2C bus
        TwoWire& wire = (cmd.channel == 1) ? Wire1 : Wire;
        int found = 0;
        char buffer[256] = "";
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            wire.beginTransmission(addr);
            if (wire.endTransmission() == 0) {
                found++;
                char tmp[16];
                snprintf(tmp, sizeof(tmp), "0x%02X ", addr);
                strncat(buffer, tmp, sizeof(buffer) - strlen(buffer) - 1);
            }
        }
        result.return_value = found;
        // Guardar lista en error_msg como info
        strncpy(result.error_msg, buffer, sizeof(result.error_msg));
        result.success = true;
    }
    else {
        snprintf(result.error_msg, sizeof(result.error_msg), "Test action no soportada: %s", cmd.action);
    }

    return result;
}

CommandResult CommandProcessor::selfTest() {
    CommandResult result;
    strncpy(result.command_id, "self_test", sizeof(result.command_id));
    result.success = true;

    String report = "Self-test:\n";

    // Test I2C buses
    if (_config.pins.i2c0_sda >= 0) {
        int found = 0;
        Wire.beginTransmission(0x08);
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) found++;
        }
        report += "I2C0: " + String(found) + " dispositivos\n";
    }
    if (_config.pins.i2c1_sda >= 0) {
        int found = 0;
        Wire1.beginTransmission(0x08);
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            Wire1.beginTransmission(addr);
            if (Wire1.endTransmission() == 0) found++;
        }
        report += "I2C1: " + String(found) + " dispositivos\n";
    }

    // Test ADC pins
    int adc_ok = 0;
    for (int i = 0; i < _config.pins.adc1_count; i++) {
        int8_t gpio = _config.pins.adc1_pins[i];
        if (gpio >= 0 && validatePin(_config.board_model, gpio, "adc")) {
            float v = analogRead(gpio) * 3.3 / 4095.0;
            if (!isnan(v)) adc_ok++;
        }
    }
    report += "ADC1 OK: " + String(adc_ok) + "/" + String(_config.pins.adc1_count) + "\n";

    // Test output pins (solo los configurados como output en pinout)
    int out_ok = 0, out_total = 0;
    // En producción: iterar pinout y probar outputs

    report += "Outputs OK: " + String(out_ok) + "/" + String(out_total) + "\n";

    strncpy(result.error_msg, report.c_str(), sizeof(result.error_msg));
    return result;
}

// ──────────────────────────────────────────────────────────────
// Restart
// ──────────────────────────────────────────────────────────────

void CommandProcessor::restartDevice(const char* reason) {
    Serial.printf("REINICIO: %s\n", reason);
    Serial.flush();
    delay(100);

    // Guardar estado si necesario
    // nvs.saveConfig...

    ESP.restart();
}

// ──────────────────────────────────────────────────────────────
// Capabilities
// ──────────────────────────────────────────────────────────────

bool CommandProcessor::getPinCapabilities(int8_t gpio, char* buffer, size_t len) {
    if (gpio < 0) return false;

    String caps = "";
    if (validatePin(_config.board_model, gpio, "input")) caps += "input ";
    if (validatePin(_config.board_model, gpio, "output")) caps += "output ";
    if (validatePin(_config.board_model, gpio, "pwm")) caps += "pwm ";
    if (validatePin(_config.board_model, gpio, "adc")) caps += "adc ";
    if (validatePin(_config.board_model, gpio, "dac")) caps += "dac ";
    if (validatePin(_config.board_model, gpio, "touch")) caps += "touch ";

    strncpy(buffer, caps.c_str(), len);
    return true;
}
