#pragma once
#include "api_core.h"

#include <SPIFFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "../py_log.h"
#include "esp_heap_caps.h"
#include "../py_mqtt.h"
#include "../py_parser.h"
#include "../config.h"

extern AppConfig config;
extern PyMqtt py_mqtt;

// ------------------------------------------------------------
// /api/dashboard
// ------------------------------------------------------------
static esp_err_t api_dashboard(httpd_req_t *req) {

    StaticJsonDocument<1024> doc;

    // WiFi
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["mode"] = "STA";
    wifi["ssid"] = WiFi.SSID();
    wifi["ip"]   = WiFi.localIP().toString();
    wifi["rssi"] = WiFi.RSSI();

    // MQTT
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["connected"] = py_mqtt.isConnected();
    mqtt["server"]    = config.mqtt.server;
    mqtt["port"]      = config.mqtt.port;
    mqtt["last_pwr"]     = config.lastPwrUpdate;
	mqtt["last_bat"]     = config.lastBatUpdate;
	mqtt["last_stat"]    = config.lastStatUpdate;

    // Battery
    JsonObject bat = doc.createNestedObject("battery");
    bat["modules"]     = config.detectedModules;
    bat["last_update"] = config.lastUartUpdate;

    // System
    JsonObject sys = doc.createNestedObject("system");

    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &t);

    sys["time"]    = timeBuf;
    sys["uptime"]  = config.uptimeString();
    sys["version"] = config.firmwareVersion;

    // Senden
    String out;
    serializeJson(doc, out);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out.c_str());

    return ESP_OK;
}

// ------------------------------------------------------------
// /api/log
// ------------------------------------------------------------
static esp_err_t api_log(httpd_req_t *req) {
    apiText(req, WebLogGet());
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/log/level (GET)
// ------------------------------------------------------------
static esp_err_t api_log_level_get(httpd_req_t *req) {

    StaticJsonDocument<128> doc;
    doc["info"]  = config.logInfo;
    doc["warn"]  = config.logWarn;
    doc["error"] = config.logError;
    doc["debug"] = config.logDebug;

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/log/level (POST)
// ------------------------------------------------------------
static esp_err_t api_log_level_post(httpd_req_t *req) {

    String body = apiGetBody(req);
    StaticJsonDocument<128> doc;

    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    config.logInfo  = doc["info"]  | true;
    config.logWarn  = doc["warn"]  | true;
    config.logError = doc["error"] | true;
    config.logDebug = doc["debug"] | false;

    config.save();
    apiText(req, "Log level updated");
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerCombinedAPI() {

    // Dashboard
    httpd_uri_t r1 = { "/api/dashboard", HTTP_GET, api_dashboard, NULL };
    httpd_register_uri_handler(server, &r1);

    // Log
    httpd_uri_t r2 = { "/api/log",        HTTP_GET,  api_log,            NULL };
    httpd_uri_t r3 = { "/api/log/level",  HTTP_GET,  api_log_level_get,  NULL };
    httpd_uri_t r4 = { "/api/log/level",  HTTP_POST, api_log_level_post, NULL };

    httpd_register_uri_handler(server, &r2);
    httpd_register_uri_handler(server, &r3);
    httpd_register_uri_handler(server, &r4);
}
