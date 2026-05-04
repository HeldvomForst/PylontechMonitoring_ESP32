#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
#include "../py_wifimanager.h"
#include "../py_mqtt.h"
#include "../config.h"

extern WebServer server;
extern PyMqtt py_mqtt;

// ---------------------------------------------------------
// WiFi API
// ---------------------------------------------------------
static void apiWifiGet() {
    WifiStatus s = WiFiManagerModule::getStatus();

    DynamicJsonDocument doc(256);
    doc["connected"] = s.connected;
    doc["ssid"]      = s.ssid;
    doc["rssi"]      = s.rssi;
    doc["ip"]        = s.ip;
    doc["mac"]       = s.mac;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void apiWifiPost() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(256);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    String ssid = req["ssid"] | "";
    String pass = req["pass"] | "";

    if (ssid.length() == 0) {
        server.send(400, "text/plain", "SSID missing");
        return;
    }

    WiFiManagerModule::connect(ssid, pass);
    server.send(200, "text/plain", "Connecting…");
}

static void apiWifiScan() {
    String json = WiFiManagerModule::scanJson();
    server.send(200, "application/json", json);
}

// ---------------------------------------------------------
// MQTT API
// ---------------------------------------------------------
static void apiMqttGet() {
    DynamicJsonDocument doc(256);

    doc["enabled"] = config.mqtt.enabled;
    doc["server"]  = config.mqtt.server;
    doc["port"]    = config.mqtt.port;
    doc["user"]    = config.mqtt.user;
    doc["topic"]   = config.mqtt.prefix;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void apiMqttPost() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(256);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.mqtt.enabled = req["enabled"] | false;
    config.mqtt.server  = req["server"]  | "";
    config.mqtt.port    = req["port"]    | 1883;
    config.mqtt.user    = req["user"]    | "";
    String pass         = req["pass"]    | "";
    if (pass.length() > 0) config.mqtt.pass = pass;
    config.mqtt.prefix  = req["topic"]   | "Pylontech";

    config.save();
    py_mqtt.begin();

    server.send(200, "text/plain", "MQTT saved");
}

// ---------------------------------------------------------
// TIME / NTP API
// ---------------------------------------------------------
static void apiTimeGet() {
    DynamicJsonDocument doc(256);

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
    server.send(200, "application/json", out);
}

static void apiTimePost() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(256);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.manual_mode     = req["manual_mode"]     | false;
    config.manual_date     = req["manual_date"]     | "";
    config.manual_time     = req["manual_time"]     | "";
    config.manual_dst      = req["manual_dst"]      | false;

    config.use_gateway_ntp = req["use_gateway_ntp"] | true;
    config.manual_ntp      = req["manual_ntp"]      | false;
    config.ntpServer       = req["server"]          | "pool.ntp.org";

    config.timezone        = req["timezone"]        | "Europe/Berlin";

    config.save();
    server.send(200, "text/plain", "Time saved");
}

// ---------------------------------------------------------
// NETWORK API
// ---------------------------------------------------------
static void apiNetworkGet() {
    DynamicJsonDocument doc(256);

    doc["dhcp"] = !config.useStaticIP;
    doc["ip"]   = config.ipAddr;
    doc["mask"] = config.subnetMask;
    doc["gw"]   = config.gateway;
    doc["dns"]  = config.dns;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void apiNetworkPost() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(256);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.useStaticIP = !(req["dhcp"] | true);
    config.ipAddr      = req["ip"]   | "";
    config.subnetMask  = req["mask"] | "";
    config.gateway     = req["gw"]   | "";
    config.dns         = req["dns"]  | "";

    config.save();
    server.send(200, "text/plain", "Network saved");
}
