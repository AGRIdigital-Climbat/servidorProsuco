/*
 * AgriAgents ESP32 - Watchdog Implementation
 */

#include "watchdog.h"

WatchdogManager* WatchdogManager::_instance = nullptr;

WatchdogManager::WatchdogManager() {}
WatchdogManager::~WatchdogManager() {
    disable();
}

bool WatchdogManager::init(uint32_t timeout_ms) {
    _timeout_ms = timeout_ms;
    _instance = this;

    // Timer 0, div 80 (80MHz/80 = 1MHz = 1us por tick)
    _timer = timerBegin(0, 80, true);
    if (!_timer) {
        Serial.println("ERROR: No se pudo crear timer watchdog");
        return false;
    }

    timerAttachInterrupt(_timer, &onTimeout, true);
    timerAlarmWrite(_timer, _timeout_ms * 1000, false); // microsegundos
    timerAlarmEnable(_timer);

    _active = true;
    feed(); // Primera alimentación

    Serial.printf("Watchdog iniciado: timeout=%u ms\n", _timeout_ms);
    return true;
}

void WatchdogManager::feed() {
    if (!_active) return;

    timerWrite(_timer, 0); // Reset contador
    _last_feed = millis();

    // CRÍTICO: delay(1) después de feed para asegurar escritura en hardware
    // El hardware necesita tiempo para registrar el reset
    delay(1);
}

uint32_t WatchdogManager::getTimeRemaining() const {
    if (!_active) return 0;
    uint32_t elapsed = millis() - _last_feed;
    return (elapsed < _timeout_ms) ? _timeout_ms - elapsed : 0;
}

void WatchdogManager::disable() {
    if (_timer) {
        timerAlarmDisable(_timer);
        timerDetachInterrupt(_timer);
        timerEnd(_timer);
        _timer = nullptr;
    }
    _active = false;
}

void IRAM_ATTR WatchdogManager::onTimeout() {
    // Watchdog expiró - reiniciar sistema
    // No usar Serial aquí (ISR)
    esp_restart();
}
