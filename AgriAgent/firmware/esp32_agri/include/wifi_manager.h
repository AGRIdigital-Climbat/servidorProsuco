/*
 * AgriAgents ESP32 - WiFi Manager Robusto
 *
 * Características:
 * - Conexión automática con credenciales guardadas
 * - Portal cautivo (WiFiManager estilo) para configuración inicial
 * - Reconexión automática con backoff exponencial
 * - Soporte para múltiples redes (prioridad)
 * - Modo AP+STA para configuración sin perder conexión
 * - Heartbeat y detección de desconexión
 * - Compatible con WiFiManager library o implementación nativa
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"

#define WIFI_AP_SSID "AgriAgents-Setup"
#define WIFI_AP_PASSWORD "agriagents123"
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)
#define WIFI_AP_NETMASK IPAddress(255, 255, 255, 0)
#define WIFI_PORTAL_TIMEOUT_MS 300000  // 5 min
#define WIFI_SCAN_INTERVAL_MS 30000
#define WIFI_MAX_RETRY_DELAY_MS 60000

struct WiFiCredential {
    char ssid[64];
    char password[64];
    int32_t priority = 0;  // Mayor = más prioridad
    bool enabled = true;
};

class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();

    void init(const WiFiConfig& config);
    void connect();
    void loop();
    bool isConnected() const;
    int getRSSI() const;
    String getLocalIP() const;
    String getMacAddress() const;

    // Portal cautivo
    void startConfigPortal(uint32_t timeout_ms = WIFI_PORTAL_TIMEOUT_MS);
    void stopConfigPortal();

    // Gestión de credenciales
    bool addCredential(const char* ssid, const char* password, int32_t priority = 0);
    bool removeCredential(const char* ssid);
    void loadCredentials();
    void saveCredentials();

    // Callbacks
    void onConnect(std::function<void()> cb) { _onConnect = cb; }
    void onDisconnect(std::function<void()> cb) { _onDisconnect = cb; }
    void onConfigPortalStart(std::function<void()> cb) { _onPortalStart = cb; }

private:
    WiFiConfig _config;
    WiFiMulti _wifiMulti;
    WebServer* _portalServer = nullptr;
    DNSServer* _dnsServer = nullptr;

    std::vector<WiFiCredential> _credentials;
    uint32_t _lastReconnectAttempt = 0;
    uint32_t _reconnectDelay = 1000;
    uint32_t _lastScan = 0;
    bool _inConfigPortal = false;
    uint32_t _portalStartTime = 0;

    std::function<void()> _onConnect;
    std::function<void()> _onDisconnect;
    std::function<void()> _onPortalStart;

    bool _wasConnected = false;

    // Portal cautivo handlers
    void handleRoot();
    void handleScan();
    void handleSave();
    void handleReboot();

    // Utilidades
    void loadCredentialsFromNVS();
    void saveCredentialsToNVS();
    bool tryConnectSavedNetworks();
    void startAPMode();
    void stopAPMode();
};

#endif // WIFI_MANAGER_H
