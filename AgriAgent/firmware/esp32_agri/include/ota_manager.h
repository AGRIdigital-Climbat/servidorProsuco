/*
 * AgriAgents ESP32 - OTA Manager (Seguro)
 *
 * Principios de seguridad:
 * 1. COMUNICACIÓN PRIMERO: El dispositivo DEBE poder comunicarse desde el primer boot
 * 2. Partición factory inmutable: firmware de recuperación siempre disponible
 * 3. Verificación obligatoria: SHA256 + firma antes de flashear
 * 4. Rollback automático: Si no hay "health check" en X min → revertir
 * 5. OTA opcional: No bloquea operación normal si falla
 * 6. Descarga en background: No interrumpe lectura de sensores
 * 7. Modo recovery: Serial/USB siempre accesible
 *
 * Arquitectura de particiones ESP32:
 * - factory (0x10000): Firmware original, NUNCA se toca
 * - ota_0 (0x110000): Slot A para actualizaciones
 * - ota_1 (0x210000): Slot B para actualizaciones
 * - nvs, ota_data: Metadatos
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "http_client.h"

#define OTA_MAX_RETRIES 3
#define OTA_DOWNLOAD_TIMEOUT_MS 120000  // 2 min
#define OTA_CHUNK_SIZE 1024
#define OTA_HEALTH_CHECK_DELAY_MS 300000  // 5 min para confirmar
#define OTA_MAX_FIRMWARE_SIZE (1024 * 1024)  // 1MB

// Estados OTA
enum OTAState {
    OTA_IDLE = 0,
    OTA_CHECKING_VERSION,
    OTA_DOWNLOADING,
    OTA_VERIFYING,
    OTA_APPLYING,
    OTA_SUCCESS,
    OTA_FAILED,
    OTA_ROLLING_BACK
};

struct OTAInfo {
    OTAState state = OTA_IDLE;
    char version[32] = "";
    char firmware_url[256] = "";
    char sha256[65] = "";
    uint32_t firmware_size = 0;
    uint32_t downloaded = 0;
    uint32_t start_time = 0;
    uint8_t retries = 0;
    char error_msg[128] = "";
    bool health_check_passed = false;
};

// Callbacks
using OTAProgressCallback = std::function<void(uint32_t progress, uint32_t total)>;
using OTAStateCallback = std::function<void(OTAState state, const char* msg)>;

class OTAManager {
public:
    OTAManager();
    ~OTAManager();

    void init(HttpClient* http_client);

    // ──────────────────────────────────────────────────────────────
    // API Principal
    // ──────────────────────────────────────────────────────────────

    // Verificar si hay actualización disponible (no bloqueante)
    bool checkForUpdate();

    // Iniciar descarga e instalación (async, no bloqueante)
    bool startUpdate();

    // Procesar en loop (llamar periódicamente)
    void loop();

    // Confirmar que el nuevo firmware funciona (llamar tras boot exitoso)
    bool confirmUpdate();

    // Forzar rollback a factory
    bool rollbackToFactory();

    // ──────────────────────────────────────────────────────────────
    // Estado
    // ──────────────────────────────────────────────────────────────

    OTAState getState() const { return _info.state; }
    const OTAInfo& getInfo() const { return _info; }
    bool isUpdateAvailable() const { return _update_available; }
    bool isInProgress() const { return _info.state != OTA_IDLE && _info.state != OTA_SUCCESS; }

    // Callbacks
    void onProgress(OTAProgressCallback cb) { _onProgress = cb; }
    void onStateChange(OTAStateCallback cb) { _onStateChange = cb; }

    // ──────────────────────────────────────────────────────────────
    // Configuración
    // ──────────────────────────────────────────────────────────────

    void setAutoConfirm(bool enable) { _auto_confirm = enable; }
    void setHealthCheckDelay(uint32_t ms) { _health_check_delay = ms; }
    void setCurrentVersion(const char* version) { strncpy(_current_version, version, sizeof(_current_version)); }

private:
    HttpClient* _http = nullptr;
    OTAInfo _info;
    bool _update_available = false;
    bool _auto_confirm = false;
    uint32_t _health_check_delay = OTA_HEALTH_CHECK_DELAY_MS;
    char _current_version[32] = FIRMWARE_VERSION;
    uint32_t _last_check = 0;

    // Callbacks
    OTAProgressCallback _onProgress;
    OTAStateCallback _onStateChange;

    // ──────────────────────────────────────────────────────────────
    // Implementación interna
    // ──────────────────────────────────────────────────────────────

    void _setState(OTAState state, const char* msg = "");
    bool _downloadAndVerify();
    bool _verifySHA256(const uint8_t* data, size_t len, const char* expected);
    bool _applyUpdate();
    bool _performRollback();
    bool _checkFactoryFirmware();
    String _getCurrentPartitionLabel();
    bool _markPartitionValid(const char* label);
    bool _markPartitionInvalid(const char* label);

    // HTTP streaming download
    int _streamDownload(const char* url, std::function<bool(uint8_t*, size_t)> writer);
};

#endif // OTA_MANAGER_H
