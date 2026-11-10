/*
 * AgriAgents ESP32 - HTTP Client Implementation
 */

#include "http_client.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>

HttpClient::HttpClient() : _client(nullptr), _external_client(nullptr), _wifi_secure(nullptr) {}
HttpClient::~HttpClient() { close(); }

void HttpClient::init(const APIConfig& config) {
    _config = config;
    _base_url = config.base_url;
    _hmac_key = config.hmac_key;

    // Cliente por defecto según red
    if (_external_client) {
        _client = _external_client;
    } else if (config.use_https) {
        _wifi_secure = new WiFiClientSecure();
        if (config.ca_cert[0]) {
            _wifi_secure->setCACert(config.ca_cert);
        } else {
            _wifi_secure->setInsecure(); // Solo para testing
        }
        _client = _wifi_secure;
    } else {
        _client = new WiFiClient();
    }

    Serial.println("HTTP Client inicializado");
}

void HttpClient::close() {
    if (_wifi_secure) { delete _wifi_secure; _wifi_secure = nullptr; }
    if (_client && _client != _external_client) { delete _client; _client = nullptr; }
}

Client* HttpClient::getClient() { return _client; }

HttpResponse HttpClient::get(const String& url, const char* hmac_key) {
    return _execute("GET", url, "", hmac_key);
}

HttpResponse HttpClient::post(const String& url, const char* hmac_key, const String& body) {
    return _execute("POST", url, body, hmac_key);
}

HttpResponse HttpClient::put(const String& url, const char* hmac_key, const String& body) {
    return _execute("PUT", url, body, hmac_key);
}

HttpResponse HttpClient::del(const String& url, const char* hmac_key) {
    return _execute("DELETE", url, "", hmac_key);
}

HttpResponse HttpClient::head(const String& url, const char* hmac_key) {
    return _execute("HEAD", url, "", hmac_key);
}

int HttpClient::streamGet(const String& url, const char* hmac_key, std::function<bool(uint8_t*, size_t)> callback) {
    HTTPClient http;
    String full_url = _base_url + url;
    http.begin(*_client, full_url);

    _addCommonHeaders(http, "GET", url, "");
    if (hmac_key && hmac_key[0]) {
        http.addHeader("Authorization", String("HMAC ") + _signRequest("GET", url, "", millis()/1000));
    }

    int httpCode = http.sendRequest("GET");
    if (httpCode != 200) {
        http.end();
        return httpCode;
    }

    int len = http.getSize();
    uint8_t buffer[1024];
    WiFiClient* stream = http.getStreamPtr();

    while (http.connected() && (len > 0 || len == -1)) {
        size_t available = stream->available();
        if (available) {
            size_t read_len = std::min(available, sizeof(buffer));
            int read = stream->readBytes(buffer, read_len);
            if (read > 0) {
                if (!callback(buffer, read)) {
                    http.end();
                    return 0;
                }
                if (len > 0) len -= read;
            }
        }
        delay(1);
    }

    http.end();
    return httpCode;
}

String HttpClient::_signRequest(const String& method, const String& path, const String& body, uint32_t timestamp) {
    // payload = device_id.timestamp.sha256(body)
    String device_id = _config.device_id;
    String body_hash = _sha256(body);
    String payload = device_id + "." + String(timestamp) + "." + body_hash;

    return _hmacSha256(_hmac_key, payload);
}

String HttpClient::_hmacSha256(const String& key, const String& data) {
    unsigned char hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", hmac[i]);
    hex[64] = '\0';
    return String(hex);
}

HttpResponse HttpClient::_execute(const String& method, const String& url, const String& body, const char* hmac_key) {
    HttpResponse resp;
    HTTPClient http;

    String full_url = _base_url + url;
    http.begin(*_client, full_url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    _addCommonHeaders(http, method, url, body);
    if (hmac_key && hmac_key[0]) {
        http.addHeader("Authorization", String("HMAC ") + _signRequest(method, url, body, millis()/1000));
    }

    int httpCode = 0;
    if (method == "GET") httpCode = http.GET();
    else if (method == "POST") httpCode = http.POST(body);
    else if (method == "PUT") httpCode = http.PUT(body);
    else if (method == "DELETE") httpCode = http.sendRequest("DELETE");
    else if (method == "HEAD") httpCode = http.sendRequest("HEAD");

    resp.status = httpCode;
    resp.body = http.getString();
    _parseHeaders(http, resp);

    // Parsear JSON si hay body
    if (resp.body.length() > 0) {
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, resp.body);
        if (!err) resp.json = doc;
    }

    http.end();
    return resp;
}

void HttpClient::_addCommonHeaders(HTTPClient& http, const String& method, const String& path, const String& body) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "AgriAgents-ESP32/" FIRMWARE_VERSION);

    uint32_t timestamp = millis() / 1000;
    String body_hash = _sha256(body);
    String payload = _config.device_id + "." + String(timestamp) + "." + body_hash;
    String signature = _hmacSha256(_hmac_key, payload);

    http.addHeader("X-Device-Id", _config.device_id);
    http.addHeader("X-Timestamp", String(timestamp));
    http.addHeader("X-Signature", signature);
}

void HttpClient::_parseHeaders(HTTPClient& http, HttpResponse& resp) {
    // HTTPClient no expone headers de respuesta fácilmente en Arduino
    // Se parsearían manualmente si se necesita
}

String HttpClient::_sha256(const String& data) {
    unsigned char hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", hash[i]);
    hex[64] = '\0';
    return String(hex);
}
