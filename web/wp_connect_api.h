#pragma once
#include "api_core.h"
#include <ArduinoJson.h>
#include "../py_wifimanager.h"
#include "../py_mqtt.h"
#include "../config.h"
#include "../py_log.h"

extern PyMqtt py_mqtt;
extern AppConfig config;

// ------------------------------------------------------------
// WiFi GET
// ------------------------------------------------------------
static esp_err_t api_wifi_get(httpd_req_t *req) {

    WifiStatus s = WiFiManagerModule::getStatus();

    StaticJsonDocument<256> doc;
    doc["connected"] = s.connected;
    doc["ssid"]      = s.ssid;
    doc["rssi"]      = s.rssi;
    doc["ip"]        = s.ip;
    doc["mac"]       = s.mac;

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// WiFi POST
// ------------------------------------------------------------
static esp_err_t api_wifi_post(httpd_req_t *req) {

    String body = apiGetBody(req);
    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";

    if (ssid.isEmpty()) {
        apiError(req, 400, "SSID missing");
        return ESP_OK;
    }

    Log(LOG_INFO, "API: wifi POST connect to SSID=" + ssid);
    WiFiManagerModule::connect(ssid, pass);
    apiText(req, "Connecting…");
    return ESP_OK;
}

// ------------------------------------------------------------
// WiFi Scan
// ------------------------------------------------------------
static esp_err_t api_wifi_scan(httpd_req_t *req) {

    Log(LOG_INFO, "API: wifi scan requested");

    WifiStatus before = WiFiManagerModule::getStatus();
    Log(LOG_INFO, "API: wifi scan, mode before = " + before.mode);

    String out = WiFiManagerModule::scanJson();

    Log(LOG_INFO, String("API: wifi scan done, payload length = ") + out.length());

    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// MQTT GET
// ------------------------------------------------------------
static esp_err_t api_mqtt_get(httpd_req_t *req) {

    StaticJsonDocument<256> doc;

    doc["enabled"] = config.mqtt.enabled;
    doc["server"]  = config.mqtt.server;
    doc["port"]    = config.mqtt.port;
    doc["user"]    = config.mqtt.user;
    doc["topic"]   = config.mqtt.prefix;

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// MQTT POST
// ------------------------------------------------------------
static esp_err_t api_mqtt_post(httpd_req_t *req) {

    String body = apiGetBody(req);
    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    config.mqtt.enabled = doc["enabled"] | false;
    config.mqtt.server  = doc["server"]  | "";
    config.mqtt.port    = doc["port"]    | 1883;
    config.mqtt.user    = doc["user"]    | "";

    String pass = doc["pass"] | "";
    if (!pass.isEmpty()) config.mqtt.pass = pass;

    config.mqtt.prefix = doc["topic"] | "Pylontech";

    config.save();
    py_mqtt.begin();

    apiText(req, "MQTT saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// TIME GET
// ------------------------------------------------------------
static esp_err_t api_time_get(httpd_req_t *req) {

    StaticJsonDocument<256> doc;

    doc["manual_mode"]     = config.manual_mode;
    doc["manual_date"]     = config.manual_date;
    doc["manual_time"]     = config.manual_time;
    doc["manual_dst"]      = config.manual_dst;

    doc["use_gateway_ntp"] = config.use_gateway_ntp;
    doc["manual_ntp"]      = config.manual_ntp;
    doc["server"]          = config.ntpServer;

    doc["timezone"]        = config.timezone;

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// TIME POST
// ------------------------------------------------------------
static esp_err_t api_time_post(httpd_req_t *req) {

    String body = apiGetBody(req);
    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    config.manual_mode     = doc["manual_mode"]     | false;
    config.manual_date     = doc["manual_date"]     | "";
    config.manual_time     = doc["manual_time"]     | "";
    config.manual_dst      = doc["manual_dst"]      | false;

    config.use_gateway_ntp = doc["use_gateway_ntp"] | true;
    config.manual_ntp      = doc["manual_ntp"]      | false;
    config.ntpServer       = doc["server"]          | "pool.ntp.org";

    config.timezone        = doc["timezone"]        | "Europe/Berlin";

    config.save();
    apiText(req, "Time saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// NETWORK GET
// ------------------------------------------------------------
static esp_err_t api_network_get(httpd_req_t *req) {

    StaticJsonDocument<256> doc;

    doc["dhcp"] = !config.useStaticIP;
    doc["ip"]   = config.ipAddr;
    doc["mask"] = config.subnetMask;
    doc["gw"]   = config.gateway;
    doc["dns"]  = config.dns;

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// NETWORK POST
// ------------------------------------------------------------
static esp_err_t api_network_post(httpd_req_t *req) {

    String body = apiGetBody(req);
    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    config.useStaticIP = !(doc["dhcp"] | true);
    config.ipAddr      = doc["ip"]   | "";
    config.subnetMask  = doc["mask"] | "";
    config.gateway     = doc["gw"]   | "";
    config.dns         = doc["dns"]  | "";

    config.save();
    WiFiManagerModule::applyNetworkConfig();

    apiText(req, "Network saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerConnectAPI() {

    httpd_uri_t r1 = { "/api/wifi",      HTTP_GET,  api_wifi_get,     NULL };
    httpd_uri_t r2 = { "/api/wifi",      HTTP_POST, api_wifi_post,    NULL };
    httpd_uri_t r3 = { "/api/wifi_scan", HTTP_GET,  api_wifi_scan,    NULL };

    httpd_uri_t r4 = { "/api/mqtt",      HTTP_GET,  api_mqtt_get,     NULL };
    httpd_uri_t r5 = { "/api/mqtt",      HTTP_POST, api_mqtt_post,    NULL };

    httpd_uri_t r6 = { "/api/time",      HTTP_GET,  api_time_get,     NULL };
    httpd_uri_t r7 = { "/api/time",      HTTP_POST, api_time_post,    NULL };

    httpd_uri_t r8 = { "/api/network",   HTTP_GET,  api_network_get,  NULL };
    httpd_uri_t r9 = { "/api/network",   HTTP_POST, api_network_post, NULL };

    httpd_register_uri_handler(server, &r1);
    httpd_register_uri_handler(server, &r2);
    httpd_register_uri_handler(server, &r3);
    httpd_register_uri_handler(server, &r4);
    httpd_register_uri_handler(server, &r5);
    httpd_register_uri_handler(server, &r6);
    httpd_register_uri_handler(server, &r7);
    httpd_register_uri_handler(server, &r8);
    httpd_register_uri_handler(server, &r9);
}
