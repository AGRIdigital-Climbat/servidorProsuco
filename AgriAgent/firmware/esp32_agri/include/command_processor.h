/*
 * AgriAgents ESP32 - Command Processor
 *
 * Procesa comandos de la API:
 * - actuate: on/off/pwm en pines
 * - restart: reinicio del dispositivo
 * - pin_test: test de pin individual
 * - self_test: test completo de pines
 *
 * ACK de comandos con resultado
 */

#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>
#include <vector>
#include "config.h"

struct Command {
    char command_id[64];
    char kind[32];     // "actuate", "restart", "pin_test", "self_test"
    char action[32];   // "on", "off", "pwm", "read", "write", "adc", "i2c_scan"
    char pin_name[32];
    int8_t gpio = -1;
    int value = 0;
    uint32_t duration_ms = 0;
    int freq_hz = 0;
    int channel = 0;
    char tracking_id[64];
};

struct CommandResult {
    char command_id[64];
    char tracking_id[64];
    bool success = false;
    char error_msg[128] = "";
    int return_value = 0;
    float return_float = 0.0;
};

class CommandProcessor {
public:
    CommandProcessor();
    ~CommandProcessor();

    void init(const DeviceConfig& config);
    void updateConfig(const DeviceConfig& config);

    // Queue command for execution
    void queueCommand(const Command& cmd, const char* tracking_id = "");

    // Process pending commands (call in loop)
    void processPending();

    // Execute single command
    CommandResult executeCommand(const Command& cmd);

    // Pin validation and control
    bool setPinOutput(int8_t gpio, int value);
    bool setPinPWM(int8_t gpio, int value, int freq_hz, int channel);
    int readPin(int8_t gpio);
    float readPinADC(int8_t gpio);

    // Pin test
    CommandResult testPin(const Command& cmd);
    CommandResult selfTest();

    // Restart
    void restartDevice(const char* reason = "remote restart");

    // Get pin capabilities
    bool getPinCapabilities(int8_t gpio, char* buffer, size_t len);

private:
    DeviceConfig _config;
    std::vector<std::pair<Command, String>> _pending_commands;
    uint32_t _last_restart = 0;

    // PWM channels management
    bool _pwm_channels[16] = {false};
    int8_t _pwm_gpios[16] = {-1};

    // Helpers
    int8_t _resolvePin(const char* pin_name);
    bool _validatePinForAction(int8_t gpio, const char* action);
    int _getFreePWMChannel();
    void _releasePWMChannel(int channel);
};

#endif // COMMAND_PROCESSOR_H
