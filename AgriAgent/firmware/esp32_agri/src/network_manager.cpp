/*
 * AgriAgents ESP32 - Network Manager Implementation
 */

#include "network_manager.h"
#include "config.h"

#ifdef TINY_GSM_MODEM_SIM7000
#include <TinyGsmClient.h>
#endif

NetworkManager::NetworkManager() : _current(nullptr), _current_index(0), _connected(false) {}
NetworkManager::~NetworkManager() {
    for (auto t : _transports) delete t;
}

void NetworkManager::init(const NetworkConfig& config) {
    _config = config;

    // Crear transportes según prioridad
    _transports.clear();
    _transports.push_back(_createTransport(NET_WIFI));

    if (_config.cellular.modem_type != MODEM_NONE) {
        _transports.push_back(_createTransport(NET_CELLULAR));
    }
    if (_config.ethernet.cs_pin >= 0) {
        _transports.push_back(_createTransport(NET_ETHERNET));
    }
    if (_config.lora.cs_pin >= 0) {
        _transports.push_back(_createTransport(NET_LORA));
    }

    Serial.printf("NetworkManager: %d transportes configurados\n", _transports.size());
}

INetworkTransport* NetworkManager::_createTransport(NetworkType type) {
    switch (type) {
        case NET_WIFI: return new WiFiTransport();
        case NET_CELLULAR:
            #ifdef TINY_GSM_MODEM_SIM7000
            return new CellularTransport();
            #else
            return nullptr;
            #endif
        case NET_ETHERNET:
            #ifdef ETH_H
            return new EthernetTransport();
            #else
            return nullptr;
            #endif
        case NET_LORA: return new LoRaTransport();
        default: return nullptr;
    }
}

void NetworkManager::loop() {
    if (!_current) return;

    _current->loop();

    // Health check
    uint32_t now = millis();
    if (now - _last_health_check >= _config.health_check_interval_ms) {
        _last_health_check = now;
        if (!_current->isConnected() && _config.auto_failover) {
            tryFailover();
        }
    }
}

bool NetworkManager::connect() {
    for (size_t i = 0; i < _transports.size(); i++) {
        if (_transports[i]->begin(_config)) {
            if (_transports[i]->connect()) {
                _current = _transports[i];
                _current_index = i;
                _connected = true;
                Serial.printf("Conectado via %s\n", transportTypeToString(_current->getType()));
                if (_onConnect) _onConnect(_current->getType());
                return true;
            }
        }
    }
    _connected = false;
    return false;
}

void NetworkManager::disconnect() {
    if (_current) {
        _current->disconnect();
        _connected = false;
        if (_onDisconnect) _onDisconnect(_current->getType());
    }
}

bool NetworkManager::isConnected() const {
    return _connected && _current && _current->isConnected();
}

NetworkType NetworkManager::getCurrentTransport() const {
    return _current ? _current->getType() : NET_NONE;
}

bool NetworkManager::tryFailover() {
    if (!_config.auto_failover) return false;

    size_t next = (_current_index + 1) % _transports.size();
    for (size_t i = 0; i < _transports.size(); i++) {
        size_t idx = (next + i) % _transports.size();
        if (idx == _current_index) continue;

        if (_transports[idx]->begin(_config) && _transports[idx]->connect()) {
            NetworkType old = _current->getType();
            if (_current) _current->disconnect();
            _current = _transports[idx];
            _current_index = idx;
            _connected = true;

            Serial.printf("Failover: %s -> %s\n", transportTypeToString(old), transportTypeToString(_current->getType()));
            if (_onFailover) _onFailover(old, _current->getType());
            return true;
        }
    }
    return false;
}

Client* NetworkManager::getClient() {
    return _current ? _current->getClient() : nullptr;
}

int NetworkManager::getSignalQuality() const {
    return _current ? _current->getSignalQuality() : 0;
}

String NetworkManager::getIP() const {
    return _current ? _current->getIP() : "";
}

String NetworkManager::getOperator() const {
    return _current ? _current->getOperator() : "";
}

void NetworkManager::setPrimaryTransport(NetworkType type) {
    for (size_t i = 0; i < _transports.size(); i++) {
        if (_transports[i]->getType() == type) {
            if (i != _current_index) {
                _current_index = i;
                if (_current) _current->disconnect();
                _current = _transports[i];
                if (_current->connect()) {
                    _connected = true;
                }
            }
            break;
        }
    }
}

void NetworkManager::updateCellularConfig(const CellularConfig& config) {
    _config.cellular = config;
    for (auto t : _transports) {
        if (t->getType() == NET_CELLULAR) {
            // Reconfigurar si está conectado
            break;
        }
    }
}

const char* NetworkManager::transportTypeToString(NetworkType type) {
    switch (type) {
        case NET_WIFI: return "WiFi";
        case NET_CELLULAR: return "Cellular";
        case NET_ETHERNET: return "Ethernet";
        case NET_LORA: return "LoRa";
        default: return "None";
    }
}

// ──────────────────────────────────────────────────────────────
// WiFiTransport
// ──────────────────────────────────────────────────────────────

bool WiFiTransport::begin(const NetworkConfig& config) {
    _config = config.wifi;
    _credentials.clear();

    // Cargar credenciales de NVS
    Preferences prefs;
    if (prefs.begin("wifi_cred", true)) {
        size_t count = prefs.getUInt("count", 0);
        for (size_t i = 0; i < count; i++) {
            char key[16];
            snprintf(key, sizeof(key), "cred_%zu", i);
            size_t len = prefs.getBytesLength(key);
            if (len == sizeof(WiFiCredential)) {
                WiFiCredential cred;
                prefs.getBytes(key, &cred, sizeof(WiFiCredential));
                _credentials.push_back(cred);
            }
        }
        prefs.end();
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    return true;
}

bool WiFiTransport::connect() {
    if (_credentials.empty()) return false;

    std::sort(_credentials.begin(), _credentials.end(),
              [](const WiFiCredential& a, const WiFiCredential& b) {
                  return a.priority > b.priority;
    });

    for (const auto& cred : _credentials) {
        if (!cred.enabled) continue;

        Serial.printf("WiFi: conectando a %s...\n", cred.ssid);
        WiFi.begin(cred.ssid, cred.password);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            Serial.printf("WiFi conectado: %s (IP: %s, RSSI: %d)\n",
                          cred.ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }
    }
    return false;
}

void WiFiTransport::disconnect() {
    WiFi.disconnect(true);
    _connected = false;
}

bool WiFiTransport::isConnected() const {
    return _connected && WiFi.status() == WL_CONNECTED;
}

int WiFiTransport::getSignalQuality() const {
    if (!isConnected()) return 0;
    int rssi = WiFi.RSSI();
    // Convertir RSSI a 0-100
    if (rssi >= -50) return 100;
    if (rssi >= -60) return 80;
    if (rssi >= -70) return 60;
    if (rssi >= -80) return 40;
    if (rssi >= -90) return 20;
    return 10;
}

String WiFiTransport::getIP() const {
    return isConnected() ? WiFi.localIP().toString() : "";
}

NetworkType WiFiTransport::getType() const { return NET_WIFI; }

Client* WiFiTransport::getClient() {
    return new WiFiClient();
}

void WiFiTransport::loop() {
    if (!isConnected()) {
        _connected = false;
    }
}

bool WiFiTransport::sleep() { return false; }
bool WiFiTransport::wake() { return true; }

// ──────────────────────────────────────────────────────────────
// CellularTransport (TinyGSM)
// ──────────────────────────────────────────────────────────────

#ifdef TINY_GSM_MODEM_SIM7000

CellularTransport::CellularTransport() : _modem(nullptr), _client(nullptr), _serial(nullptr),
                                         _connected(false), _gprs_connected(false) {}

CellularTransport::~CellularTransport() {
    if (_client) delete _client;
    if (_modem) delete _modem;
    if (_serial) delete _serial;
}

bool CellularTransport::begin(const NetworkConfig& config) {
    _config = config.cellular;

    // Serial para modem
    _serial = new HardwareSerial(1);
    _serial->begin(_config.baud, SERIAL_8N1, _config.rx_pin, _config.tx_pin);

    // Power on
    if (_config.power_pin >= 0) {
        pinMode(_config.power_pin, OUTPUT);
        digitalWrite(_config.power_pin, HIGH);
        delay(1000);
    }

    // Reset
    if (_config.reset_pin >= 0) {
        pinMode(_config.reset_pin, OUTPUT);
        digitalWrite(_config.reset_pin, LOW);
        delay(100);
        digitalWrite(_config.reset_pin, HIGH);
        delay(2000);
    }

    _modem = new TinyGsm(*_serial);
    _client = new TinyGsmClient(*_modem);

    // Detectar modem
    String info = _modem->getModemInfo();
    Serial.printf("Modem: %s\n", info.c_str());

    return true;
}

bool CellularTransport::connect() {
    if (!_modem) return false;

    // Reiniciar modem si es necesario
    if (!_modem->isNetworkConnected()) {
        Serial.println("Cellular: conectando a red...");

        if (!_modem->waitForNetwork(60000)) {
            Serial.println("Cellular: timeout red");
            return false;
        }

        if (!_modem->gprsConnect(_config.apn.c_str(), _config.username.c_str(), _config.password.c_str())) {
            Serial.println("Cellular: GPRS failed");
            return false;
        }
    }

    _connected = _modem->isNetworkConnected();
    _gprs_connected = _modem->isGprsConnected();

    if (_connected) {
        String ip = _modem->getLocalIP();
        Serial.printf("Cellular conectado: IP=%s, Operador=%s, RSSI=%d\n",
                      ip.c_str(), _modem->getOperator().c_str(), _modem->getSignalQuality());
    }

    return _connected;
}

void CellularTransport::disconnect() {
    if (_modem && _gprs_connected) {
        _modem->gprsDisconnect();
    }
    _connected = false;
    _gprs_connected = false;
}

bool CellularTransport::isConnected() const {
    return _connected && _modem && _modem->isNetworkConnected() && _modem->isGprsConnected();
}

int CellularTransport::getSignalQuality() const {
    if (!_modem) return 0;
    int csq = _modem->getSignalQuality();
    // CSQ 0-31 -> 0-100
    return (csq * 100) / 31;
}

String CellularTransport::getIP() const {
    if (!_modem) return "";
    return _modem->getLocalIP();
}

String CellularTransport::getOperator() const {
    if (!_modem) return "";
    return _modem->getOperator();
}

NetworkType CellularTransport::getType() const { return NET_CELLULAR; }

Client* CellularTransport::getClient() {
    return _client;
}

void CellularTransport::loop() {
    if (!_modem) return;

    uint32_t now = millis();
    if (now - _last_network_check > 30000) {
        _last_network_check = now;
        if (!isConnected()) {
            Serial.println("Cellular: conexión perdida, reconectando...");
            connect();
        }
    }
}

bool CellularTransport::sleep() {
    if (_modem && _gprs_connected) {
        _modem->gprsDisconnect();
    }
    if (_modem) {
        _modem->sleepEnable(true);
    }
    return true;
}

bool CellularTransport::wake() {
    if (_modem) {
        _modem->sleepEnable(false);
        delay(1000);
        return connect();
    }
    return false;
}

#endif

// ──────────────────────────────────────────────────────────────
// EthernetTransport
// ──────────────────────────────────────────────────────────────

#ifdef ETH_H

bool EthernetTransport::begin(const NetworkConfig& config) {
    _config = config.ethernet;

    if (_config.cs_pin >= 0) {
        // SPI Ethernet (W5500)
        ETH.begin(_config.cs_pin, _config.int_pin, _config.rst_pin,
                  _config.mosi, _config.miso, _config.sclk);
    } else {
        // RMII (ESP32-P4)
        ETH.begin();
    }
    return true;
}

bool EthernetTransport::connect() {
    _connected = ETH.linkUp();
    if (_connected) {
        Serial.printf("Ethernet: IP=%s\n", ETH.localIP().toString().c_str());
    }
    return _connected;
}

void EthernetTransport::disconnect() { _connected = false; }
bool EthernetTransport::isConnected() const { return _connected && ETH.linkUp(); }
int EthernetTransport::getSignalQuality() const { return isConnected() ? 100 : 0; }
String EthernetTransport::getIP() const { return isConnected() ? ETH.localIP().toString() : ""; }
NetworkType EthernetTransport::getType() const { return NET_ETHERNET; }
Client* EthernetTransport::getClient() { return new EthClient(); }
void EthernetTransport::loop() {}
bool EthernetTransport::sleep() { return false; }
bool EthernetTransport::wake() { return true; }

#endif

// ──────────────────────────────────────────────────────────────
// LoRaTransport
// ──────────────────────────────────────────────────────────────

bool LoRaTransport::begin(const NetworkConfig& config) {
    _config = config.lora;
    // Inicializar SX127x/SX126x con RadioLib
    return true;
}

bool LoRaTransport::connect() { return true; }
void LoRaTransport::disconnect() {}
bool LoRaTransport::isConnected() const { return true; }
int LoRaTransport::getSignalQuality() const { return _rssi; }
String LoRaTransport::getIP() const { return "mesh"; }
NetworkType LoRaTransport::getType() const { return NET_LORA; }
Client* LoRaTransport::getClient() { return nullptr; }

void LoRaTransport::loop() {
    // Mesh networking loop
}

bool LoRaTransport::sendMesh(const uint8_t* data, size_t len, uint32_t dest_addr) { return true; }
bool LoRaTransport::recvMesh(uint8_t* buffer, size_t max_len, uint32_t* src_addr) { return true; }
bool LoRaTransport::sleep() { return true; }
bool LoRaTransport::wake() { return true; }
