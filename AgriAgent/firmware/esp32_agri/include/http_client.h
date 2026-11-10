/*
 * AgriAgents ESP32 - HTTP Client (Unificado WiFi + Cellular)
 *
 * Soporte para:
 * - WiFi (HTTPClient nativo)
 * - Cellular (TinyGSM + HTTPClient)
 * - HTTPS con certificados
 * - HMAC-SHA256 automático
 * - Streaming para OTA
 * - Timeouts configurables
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

#ifdef TINY_GSM_MODEM_SIM7000
#include <TinyGsmClient.h>
#endif

// ──────────────────────────────────────────────────────────────
// Respuesta HTTP
// ──────────────────────────────────────────────────────────────

struct HttpResponse {
    int status = 0;
    String body;
    DynamicJsonDocument json;
    std::map<String, String> headers;

    bool hasHeader(const String& name) const {
        return headers.find(name) != headers.end();
    }
    String getHeader(const String& name) const {
        auto it = headers.find(name);
        return it != headers.end() ? it->second : "";
    }
};

// ──────────────────────────────────────────────────────────────
// HTTP Client unificado
// ──────────────────────────────────────────────────────────────

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    void init(const APIConfig& config);
    void setClient(Client* client) { _external_client = client; }
    Client* getClient() { return _client; }

    // ──────────────────────────────────────────────────────────────
    // Métodos HTTP
    // ──────────────────────────────────────────────────────────────

    HttpResponse get(const String& url, const char* hmac_key = nullptr);
    HttpResponse post(const String& url, const char* hmac_key, const String& body);
    HttpResponse put(const String& url, const char* hmac_key, const String& body);
    HttpResponse del(const String& url, const char* hmac_key);
    HttpResponse head(const String& url, const char* hmac_key);

    // Streaming para OTA
    int streamGet(const String& url, const char* hmac_key, std::function<bool(uint8_t*, size_t)> callback);

    // ──────────────────────────────────────────────────────────────
    // HMAC
    // ──────────────────────────────────────────────────────────────

    void setHmacKey(const char* key) { _hmac_key = key; }
    String getHmacKey() const { return _hmac_key; }

private:
    APIConfig _config;
    Client* _client = nullptr;
    Client* _external_client = nullptr;
    WiFiClientSecure* _wifi_secure = nullptr;
    String _hmac_key;
    String _base_url;

    // HMAC-SHA256
    String _signRequest(const String& method, const String& path, const String& body, uint32_t timestamp);
    String _hmacSha256(const String& key, const String& data);

    // Ejecutar request
    HttpResponse _execute(const String& method, const String& url, const String& body);
    HttpResponse _executeWithClient(HTTPClient& http, const String& method, const String& url, const String& body);

    // Headers
    void _addCommonHeaders(HTTPClient& http, const String& method, const String& path, const String& body);
    void _parseHeaders(HTTPClient& http, HttpResponse& resp);
};

// ──────────────────────────────────────────────────────────────
// Cellular HTTP Client (TinyGSM)
// ──────────────────────────────────────────────────────────────

#ifdef TINY_GSM_MODEM_SIM7000

class CellularHttpClient {
public:
    CellularHttpClient(TinyGsm* modem, TinyGsmClient* client);
    ~CellularHttpClient();

    bool begin(const String& base_url, const char* hmac_key);
    HttpResponse get(const String& path);
    HttpResponse post(const String& path, const String& body);
    int streamGet(const String& path, std::function<bool(uint8_t*, size_t)> callback);

    // OTA via modem HTTP AT commands
    bool performOTA(const String& url, const char* expected_sha256 = nullptr);

private:
    TinyGsm* _modem;
    TinyGsmClient* _client;
    String _base_url;
    String _hmac_key;

    bool _httpInit();
    bool _httpTerm();
    String _buildUrl(const String& path);
    String _signRequest(const String& method, const String& path, const String& body, uint32_t timestamp);
    bool _sendATCommand(const String& cmd, const String& expected, uint32_t timeout);
    int _waitForResponse(String& response, uint32_t timeout);
};

#endif // TINY_GSM_MODEM_SIM7000

#endif // HTTP_CLIENT_H
