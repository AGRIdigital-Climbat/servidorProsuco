/*
 * AgriAgents ESP32 - Watchdog Manager
 *
 * Watchdog de hardware ESP32 (TIMG0/TIMG1)
 * Feed periódico + delay(1) obligatorio después de cada feed
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>

#define DEFAULT_WDT_TIMEOUT_MS 8000
#define DEFAULT_WDT_FEED_INTERVAL_MS 1000

class WatchdogManager {
public:
    WatchdogManager();
    ~WatchdogManager();

    // Inicializar watchdog
    bool init(uint32_t timeout_ms = DEFAULT_WDT_TIMEOUT_MS);

    // Alimentar watchdog (llamar cada loop)
    void feed();

    // Verificar si watchdog está activo
    bool isActive() const { return _active; }

    // Obtener tiempo restante (aprox)
    uint32_t getTimeRemaining() const;

    // Desactivar (solo para testing/OTA)
    void disable();

private:
    bool _active = false;
    uint32_t _timeout_ms = DEFAULT_WDT_TIMEOUT_MS;
    uint32_t _last_feed = 0;
    hw_timer_t* _timer = nullptr;

    // Callback ISR
    static void IRAM_ATTR onTimeout();
    static WatchdogManager* _instance;
};

#endif // WATCHDOG_H
