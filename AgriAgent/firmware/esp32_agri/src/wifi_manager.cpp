/*
 * AgriAgents ESP32 - WiFi Manager Implementation
 */

#include "wifi_manager.h"
#include "config.h"
#include <Preferences.h>

WiFiManager::WiFiManager() : _inConfigPortal(false), _wasConnected(false) {}

WiFiManager::~WiFiManager() {
    stopConfigPortal();
    if (_portalServer) delete _portalServer;
    if (_dnsServer) delete _dnsServer;
}

void WiFiManager::init(const WiFiConfig& config) {
    _config = config;

    // Cargar credenciales guardadas
    loadCredentials();

    // Configurar hostname
    if (_config.hostname[0]) {
        WiFi.setHostname(_config.hostname);
    } else {
        char hn[32];
        snprintf(hn, sizeof(hn), "agri-%06X", ESP.getEfuseMac() & 0xFFFFFF);
        WiFi.setHostname(hn);
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Desactivar sleep para mejor recepción
    WiFi.onEvent([this](WiFiEvent_t event) { _onWiFiEvent(event); });

    Serial.println("WiFiManager inicializado");
}

void WiFiManager::connect() {
    if (_credentials.empty()) {
        Serial.println("No hay credenciales WiFi guardadas");
        return;
    }

    // Ordenar por prioridad
    std::sort(_credentials.begin(), _credentials.end(),
              [](const WiFiCredential& a, const WiFiCredential& b) {
                  return a.priority > b.priority;
    });

    for (const auto& cred : _credentials) {
        if (!cred.enabled) continue;

        Serial.printf("Conectando a %s...\n", cred.ssid);
        WiFi.begin(cred.ssid, cred.password);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("Conectado a %s (IP: %s, RSSI: %d dBm)\n",
                          cred.ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
            if (_onConnect) _onConnect();
            return;
        }
        Serial.printf("Falló conexión a %s\n", cred.ssid);
    }

    Serial.println("No se pudo conectar a ninguna red guardada");
}

void WiFiManager::loop() {
    if (!_inConfigPortal) {
        // Verificar conexión
        bool connected = (WiFi.status() == WL_CONNECTED);

        if (!connected && _wasConnected) {
            Serial.println("WiFi desconectado");
            if (_onDisconnect) _onDisconnect();
            _wasConnected = false;
        }

        if (!connected) {
            uint32_t now = millis();
            if (now - _lastReconnectAttempt >= _reconnectDelay) {
                _lastReconnectAttempt = now;
                _reconnectDelay = min(_reconnectDelay * 2, (uint32_t)WIFI_MAX_RETRY_DELAY_MS);
                connect();
            }
        } else {
            _wasConnected = true;
            _reconnectDelay = 1000;
        }
    } else {
        // Portal cautivo
        _dnsServer->processNextRequest();
        _portalServer->handleClient();

        if (millis() - _portalStartTime >= WIFI_PORTAL_TIMEOUT_MS) {
            Serial.println("Portal cautivo timeout");
            stopConfigPortal();
            ESP.restart();
        }
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

int WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

String WiFiManager::getLocalIP() const {
    return WiFi.localIP().toString();
}

String WiFiManager::getMacAddress() const {
    return WiFi.macAddress();
}

void WiFiManager::startConfigPortal(uint32_t timeout_ms) {
    if (_inConfigPortal) return;

    Serial.println("Iniciando portal cautivo...");

    // AP mode
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, 4);
    WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_IP, WIFI_AP_NETMASK);

    _dnsServer = new DNSServer();
    _dnsServer->start(53, "*", WIFI_AP_IP);

    _portalServer = new WebServer(80);
    _portalServer->on("/", [this]() { handleRoot(); });
    _portalServer->on("/scan", [this]() { handleScan(); });
    _portalServer->on("/save", [this]() { handleSave(); });
    _portalServer->on("/reboot", [this]() { handleReboot(); });
    _portalServer->begin();

    _inConfigPortal = true;
    _portalStartTime = millis();

    if (_onPortalStart) _onPortalStart();

    Serial.printf("Portal cautivo en http://%s\n", WiFi.softAPIP().toString().c_str());
}

void WiFiManager::stopConfigPortal() {
    if (!_inConfigPortal) return;

    if (_portalServer) {
        _portalServer->stop();
        delete _portalServer;
        _portalServer = nullptr;
    }
    if (_dnsServer) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }

    WiFi.softAPdisconnect(true);
    _inConfigPortal = false;
}

bool WiFiManager::addCredential(const char* ssid, const char* password, int32_t priority) {
    // Verificar duplicado
    for (auto& c : _credentials) {
        if (strcmp(c.ssid, ssid) == 0) {
            strncpy(c.password, password, sizeof(c.password));
            c.priority = priority;
            c.enabled = true;
            saveCredentials();
            return true;
        }
    }

    WiFiCredential cred;
    strncpy(cred.ssid, ssid, sizeof(cred.ssid));
    strncpy(cred.password, password, sizeof(cred.password));
    cred.priority = priority;
    cred.enabled = true;
    _credentials.push_back(cred);

    return saveCredentials();
}

bool WiFiManager::removeCredential(const char* ssid) {
    auto it = std::remove_if(_credentials.begin(), _credentials.end(),
                             [ssid](const WiFiCredential& c) { return strcmp(c.ssid, ssid) == 0; });
    if (it != _credentials.end()) {
        _credentials.erase(it, _credentials.end());
        return saveCredentials();
    }
    return false;
}

void WiFiManager::loadCredentials() {
    Preferences prefs;
    if (!prefs.begin("wifi_cred", true)) return;

    size_t count = prefs.getUInt("count", 0);
    _credentials.clear();

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

    Serial.printf("Credenciales WiFi cargadas: %zu\n", _credentials.size());
}

bool WiFiManager::saveCredentials() {
    Preferences prefs;
    if (!prefs.begin("wifi_cred", false)) return false;

    prefs.putUInt("count", _credentials.size());
    for (size_t i = 0; i < _credentials.size(); i++) {
        char key[16];
        snprintf(key, sizeof(key), "cred_%zu", i);
        prefs.putBytes(key, &_credentials[i], sizeof(WiFiCredential));
    }
    prefs.end();
    return true;
}

void WiFiManager::_onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("WiFi: Conectado a AP");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("WiFi: Desconectado de AP");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.printf("WiFi: IP obtenida: %s\n", WiFi.localIP().toString().c_str());
            break;
        default:
            break;
    }
}

// ──────────────────────────────────────────────────────────────
// Portal cautivo handlers
// ──────────────────────────────────────────────────────────────

void WiFiManager::handleRoot() {
    String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>AgriAgents WiFi Setup</title>
    <style>
        body { font-family: sans-serif; max-width: 400px; margin: 2rem auto; padding: 1rem; }
        .network { border: 1px solid #ddd; padding: 1rem; margin: 0.5rem 0; }
        .network.connected { border-color: #4CAF50; background: #f1f8e9; }
        input, button { padding: 0.5rem; margin: 0.25rem; }
        button { background: #2196F3; color: white; border: none; cursor: pointer; }
        button:hover { background: #1976D2; }
        .signal { display: inline-block; width: 1rem; height: 1rem; margin-right: 0.5rem; }
    </style>
</head>
<body>
    <h1>AgriAgents WiFi Setup</h1>
    <p>Dispositivo: <strong>%DEVICE_ID%</strong></p>

    <h2>Redes disponibles</h2>
    <div id="networks">Escaneando...</div>

    <h2>Agregar red manual</h2>
    <form id="manualForm">
        <input type="text" name="ssid" placeholder="SSID" required>
        <input type="password" name="password" placeholder="Contraseña">
        <input type="number" name="priority" placeholder="Prioridad (0-100)" value="0" min="0" max="100">
        <button type="submit">Guardar</button>
    </form>

    <button onclick="reboot()">Reiniciar dispositivo</button>

    <script>
        async function loadNetworks() {
            const resp = await fetch('/scan');
            const data = await resp.json();
            const div = document.getElementById('networks');
            div.innerHTML = data.networks.map(n => `
                <div class="network ${n.saved ? 'connected' : ''}">
                    <span class="signal" style="background:${signalColor(n.rssi)}"></span>
                    <strong>${n.ssid}</strong> (${n.rssi} dBm)
                    ${n.saved ? '✓ Guardada' : `<button onclick="connect('${n.ssid}')">Conectar</button>`}
                </div>
            `).join('');
        }

        function signalColor(rssi) {
            if (rssi > -50) return '#4CAF50';
            if (rssi > -70) return '#FFC107';
            return '#F44336';
        }

        async function connect(ssid) {
            const pwd = prompt('Contraseña para ' + ssid);
            if (!pwd) return;
            await fetch('/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ssid, password: pwd, priority: 50})
            });
            loadNetworks();
        }

        document.getElementById('manualForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const fd = new FormData(e.target);
            await fetch('/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    ssid: fd.get('ssid'),
                    password: fd.get('password'),
                    priority: parseInt(fd.get('priority'))
                })
            });
            loadNetworks();
        });

        function reboot() {
            if (confirm('¿Reiniciar dispositivo?')) fetch('/reboot');
        }

        loadNetworks();
        setInterval(loadNetworks, 10000);
    </script>
</body>
</html>
)HTML";

    // Reemplazar placeholder
    String device_id = WiFi.macAddress();
    html.replace("%DEVICE_ID%", device_id);

    _portalServer->send(200, "text/html", html);
}

void WiFiManager::handleScan() {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(4096);
    JsonArray networks = doc.createNestedArray("networks");

    Preferences prefs;
    prefs.begin("wifi_cred", true);
    size_t count = prefs.getUInt("count", 0);
    std::set<String> saved_ssids;
    for (size_t i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "cred_%zu", i);
        size_t len = prefs.getBytesLength(key);
        if (len == sizeof(WiFiCredential)) {
            WiFiCredential cred;
            prefs.getBytes(key, &cred, sizeof(WiFiCredential));
            saved_ssids.insert(cred.ssid);
        }
    }
    prefs.end();

    for (int i = 0; i < n; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["encryption"] = WiFi.encryptionType(i);
        net["saved"] = saved_ssids.count(WiFi.SSID(i)) > 0;
    }

    String output;
    serializeJson(doc, output);
    _portalServer->send(200, "application/json", output);
}

void WiFiManager::handleSave() {
    if (!_portalServer->hasArg("plain")) {
        _portalServer->send(400, "application/json", "{\"error\":\"No data\"}");
        return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, _portalServer->arg("plain"));
    if (err) {
        _portalServer->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* ssid = doc["ssid"] | "";
    const char* password = doc["password"] | "";
    int priority = doc["priority"] | 0;

    if (!ssid[0]) {
        _portalServer->send(400, "application/json", "{\"error\":\"SSID requerido\"}");
        return;
    }

    bool ok = addCredential(ssid, password, priority);

    String output = ok ? "{\"success\":true}" : "{\"error\":\"Failed to save\"}";
    _portalServer->send(ok ? 200 : 500, "application/json", output);
}

void WiFiManager::handleReboot() {
    _portalServer->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
    delay(500);
    ESP.restart();
}
