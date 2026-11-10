/*
 * AgriAgents ESP32 - Network Manager (Unificado WiFi + Cellular)
 *
 * Abstracción de transporte de red:
 * - WiFi (ESP32 nativo, ESP32-C3/S3/C6/H2)
 * - Cellular: SIM7000G (LTE-M/NB-IoT), SIM7600 (LTE Cat-1/4)
 *   - LilyGo T-A7670 (SIM7600G-H)
 *   - LilyGo T-SIM7000G
 *   - M5Stack SIM7000G Module
 *   - TTGO T-Call (SIM800L - 2G)
 * - Ethernet (ESP32-P4, ESP32 con W5500/ENC28J60)
 * - LoRa (SX1276/SX1262) para meshing
 *
 * TinyGSM para modems AT-command
 * Fallback automático: WiFi -> Cellular -> Ethernet -> LoRa
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <Client.h>
#include "config.h"

// ──────────────────────────────────────────────────────────────
// Tipos de red soportados
// ──────────────────────────────────────────────────────────────

enum NetworkType {
    NET_NONE = 0,
    NET_WIFI,
    NET_CELLULAR,
    NET_ETHERNET,
    NET_LORA,
};

enum CellularModem {
    MODEM_NONE = 0,
    MODEM_SIM7000,   // SIM7000G/A/C/E - LTE-M/NB-IoT
    MODEM_SIM7600,   // SIM7600G/H/A/E - LTE Cat-1/4
    MODEM_SIM800,    // SIM800L - 2G/GPRS
    MODEM_BG96,      // Quectel BG96 - LTE-M/NB-IoT
    MODEM_UBLOX,     // u-blox SARA-R4/N4
    MODEM_GENERIC,   // AT commands genéricos
};

struct CellularConfig {
    CellularModem modem_type = MODEM_NONE;
    char apn[64] = "";
    char username[32] = "";
    char password[32] = "";
    char pin[16] = "";
    int8_t power_pin = -1;      // Pin para encender modem
    int8_t reset_pin = -1;      // Pin de reset
    int8_t tx_pin = -1;         // UART TX
    int8_t rx_pin = -1;         // UART RX
    int8_t dtr_pin = -1;        // DTR (sleep)
    int8_t ri_pin = -1;         // RI (ring indicator)
    uint32_t baud = 115200;
    bool auto_baud = true;
    // Bandas soportadas (bitmask)
    uint32_t bands = 0;
    // Modo: 0=auto, 1=LTE-M, 2=NB-IoT, 3=GSM
    uint8_t network_mode = 0;
};

struct EthernetConfig {
    int8_t cs_pin = -1;         // SPI CS
    int8_t int_pin = -1;        // Interrupt
    int8_t rst_pin = -1;        // Reset
    int8_t mosi = -1;
    int8_t miso = -1;
    int8_t sclk = -1;
    // Para ESP32-P4 RMII
    int8_t rmii_ref_clk = -1;
    int8_t rmii_crs_dv = -1;
    int8_t rmii_rxd0 = -1;
    int8_t rmii_rxd1 = -1;
    int8_t rmii_tx_en = -1;
    int8_t rmii_txd0 = -1;
    int8_t rmii_txd1 = -1;
    int8_t rmii_mdc = -1;
    int8_t rmii_mdio = -1;
};

struct LoRaConfig {
    int8_t cs_pin = -1;
    int8_t rst_pin = -1;
    int8_t dio0_pin = -1;
    int8_t dio1_pin = -1;
    int8_t dio2_pin = -1;
    int8_t sck = -1;
    int8_t miso = -1;
    int8_t mosi = -1;
    float frequency = 868.0;    // MHz (EU868/US915)
    uint8_t spreading_factor = 7;
    uint8_t bandwidth = 125;    // kHz
    uint8_t coding_rate = 5;
    uint8_t sync_word = 0x12;
    int8_t tx_power = 17;       // dBm
};

struct NetworkConfig {
    NetworkType primary = NET_WIFI;
    NetworkType fallback1 = NET_CELLULAR;
    NetworkType fallback2 = NET_ETHERNET;
    NetworkType fallback3 = NET_LORA;

    WiFiConfig wifi;
    CellularConfig cellular;
    EthernetConfig ethernet;
    LoRaConfig lora;

    // Prioridad y timeouts
    uint32_t connect_timeout_ms = 30000;
    uint32_t reconnect_interval_ms = 60000;
    uint32_t health_check_interval_ms = 300000;
    bool auto_failover = true;
};

// ──────────────────────────────────────────────────────────────
// Interfaz común de red
// ──────────────────────────────────────────────────────────────

class INetworkTransport {
public:
    virtual ~INetworkTransport() {}
    virtual bool begin(const NetworkConfig& config) = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual int getSignalQuality() const = 0;  // 0-100
    virtual String getIP() const = 0;
    virtual String getOperator() const { return ""; }
    virtual NetworkType getType() const = 0;
    virtual Client* getClient() = 0;
    virtual void loop() = 0;
    virtual bool sleep() = 0;
    virtual bool wake() = 0;
};

// ──────────────────────────────────────────────────────────────
// Network Manager - gestor de transporte con failover
// ──────────────────────────────────────────────────────────────

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    void init(const NetworkConfig& config);
    void loop();

    // Conexión
    bool connect();
    void disconnect();
    bool isConnected() const;
    NetworkType getCurrentTransport() const;

    // Failover
    bool tryFailover();

    // Cliente HTTP
    Client* getClient();

    // Estado
    int getSignalQuality() const;
    String getIP() const;
    String getOperator() const;

    // Callbacks
    void onConnect(std::function<void(NetworkType)> cb) { _onConnect = cb; }
    void onDisconnect(std::function<void(NetworkType)> cb) { _onDisconnect = cb; }
    void onFailover(std::function<void(NetworkType, NetworkType)> cb) { _onFailover = cb; }

    // Configuración dinámica
    void setPrimaryTransport(NetworkType type);
    void updateCellularConfig(const CellularConfig& config);

private:
    NetworkConfig _config;
    std::vector<INetworkTransport*> _transports;
    INetworkTransport* _current = nullptr;
    uint8_t _current_index = 0;

    uint32_t _last_health_check = 0;
    uint32_t _last_reconnect = 0;
    bool _connected = false;

    std::function<void(NetworkType)> _onConnect;
    std::function<void(NetworkType)> _onDisconnect;
    std::function<void(NetworkType, NetworkType)> _onFailover;

    INetworkTransport* _createTransport(NetworkType type);
    bool _tryConnectTransport(INetworkTransport* transport);
};

// ──────────────────────────────────────────────────────────────
// Implementaciones de transporte (forward declarations)
// ──────────────────────────────────────────────────────────────

class WiFiTransport : public INetworkTransport {
public:
    bool begin(const NetworkConfig& config) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    int getSignalQuality() const override;
    String getIP() const override;
    NetworkType getType() const override { return NET_WIFI; }
    Client* getClient() override;
    void loop() override;
    bool sleep() override;
    bool wake() override;

private:
    WiFiConfig _config;
    WiFiMulti _wifiMulti;
    std::vector<WiFiCredential> _credentials;
    bool _connected = false;
};

#ifdef TINY_GSM_MODEM_SIM7000
class CellularTransport : public INetworkTransport {
public:
    CellularTransport();
    ~CellularTransport();
    bool begin(const NetworkConfig& config) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    int getSignalQuality() const override;
    String getIP() const override;
    String getOperator() const override;
    NetworkType getType() const override { return NET_CELLULAR; }
    Client* getClient() override;
    void loop() override;
    bool sleep() override;
    bool wake() override;

private:
    CellularConfig _config;
    TinyGsm* _modem = nullptr;
    TinyGsmClient* _client = nullptr;
    HardwareSerial* _serial = nullptr;
    bool _connected = false;
    bool _gprs_connected = false;
    uint32_t _last_network_check = 0;
};
#endif

#ifdef ETH_H
class EthernetTransport : public INetworkTransport {
public:
    bool begin(const NetworkConfig& config) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    int getSignalQuality() const override { return 100; }
    String getIP() const override;
    NetworkType getType() const override { return NET_ETHERNET; }
    Client* getClient() override;
    void loop() override;
    bool sleep() override { return false; }
    bool wake() override { return true; }

private:
    EthernetConfig _config;
    bool _connected = false;
};
#endif

class LoRaTransport : public INetworkTransport {
public:
    bool begin(const NetworkConfig& config) override;
    bool connect() override { return true; }  // Siempre "conectado" en mesh
    void disconnect() override {}
    bool isConnected() const override { return true; }
    int getSignalQuality() const override { return _rssi; }
    String getIP() const override { return "mesh"; }
    NetworkType getType() const override { return NET_LORA; }
    Client* getClient() override { return nullptr; }  // No HTTP directo
    void loop() override;
    bool sleep() override;
    bool wake() override;

    // Mesh networking
    bool sendMesh(const uint8_t* data, size_t len, uint32_t dest_addr = 0xFFFFFFFF);
    bool recvMesh(uint8_t* buffer, size_t max_len, uint32_t* src_addr);

private:
    LoRaConfig _config;
    int _rssi = 0;
    // SX127x/SX126x driver
    void* _radio = nullptr;
};

#endif // NETWORK_MANAGER_H
