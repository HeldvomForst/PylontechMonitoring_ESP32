#pragma once
#include <WebServer.h>
#include <WiFi.h>

// KORREKTE Pfade aus dem Unterordner:
#include "../py_mqtt.h"
#include "../py_parser_pwr.h"

extern AppConfig config;
extern PyMqtt py_mqtt;
extern BatteryStack lastParsedStack;

void registerDashboardAPI(WebServer &server) {

    server.on("/api/dashboard", HTTP_GET, [&]() {

        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "application/json", "");
        server.sendContent("{");

        // WiFi
        server.sendContent("\"wifi\":{");
        server.sendContent("\"mode\":\"STA\",");
        server.sendContent("\"ssid\":\"" + WiFi.SSID() + "\",");
        server.sendContent("\"ip\":\"" + WiFi.localIP().toString() + "\",");
        server.sendContent("\"rssi\":" + String(WiFi.RSSI()));
        server.sendContent("},");

        // MQTT
        server.sendContent("\"mqtt\":{");
        server.sendContent("\"connected\":" + String(py_mqtt.isConnected() ? "true" : "false") + ",");
        server.sendContent("\"server\":\"" + config.mqtt.server + "\",");
        server.sendContent("\"port\":" + String(config.mqtt.port) + ",");
        server.sendContent("\"last_contact\":\"" + config.lastMqttContact + "\"");
        server.sendContent("},");

        // Battery
        server.sendContent("\"battery\":{");
        server.sendContent("\"modules\":" + String(lastParsedStack.batteryCount) + ",");
        server.sendContent("\"last_update\":\"" + config.lastPwrUpdate + "\"");
        server.sendContent("},");

        // System
        time_t now;
        time(&now);
        struct tm t;
        localtime_r(&now, &t);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);

        server.sendContent("\"system\":{");
        server.sendContent("\"time\":\"" + String(buf) + "\",");
        server.sendContent("\"uptime\":\"" + config.uptimeString() + "\",");
        server.sendContent("\"version\":\"" + config.firmwareVersion + "\"");
        server.sendContent("}");

        server.sendContent("}");
        server.sendContent("");
    });
}
