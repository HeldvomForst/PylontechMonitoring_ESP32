#include "wp_routes.h"
#include "wp_webserver.h"

// Pages
#include "web/wp_index.h"
#include "web/wp_battery.h"
#include "web/wp_log.h"
#include "web/wp_connect.h"
#include "web/wp_pwr_setting.h"

// API / Modules
#include "py_wifimanager.h"
#include "config.h"
#include "py_uart.h"
#include "py_parser_pwr.h"   // <-- needed for lastParsedStack, lastParserHeader, lastParserValues
#include "py_scheduler.h"
#include "py_mqtt.h"
#include "py_log.h"

#include <ArduinoJson.h>

extern PyScheduler py_scheduler;
extern PyUart py_uart;
extern PyMqtt py_mqtt;

// ---------------------------------------------------------
//  API: LOG
// ---------------------------------------------------------
static void handleApiLog() {
    server.send(200, "text/plain", WebLogGet());
}

static void handleApiLogLevelGet() {
    DynamicJsonDocument doc(128);

    doc["info"]  = config.logInfo;
    doc["warn"]  = config.logWarn;
    doc["error"] = config.logError;
    doc["debug"] = config.logDebug;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}
static void handleApiLogLevelSet() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(128);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.logInfo  = req["info"]  | true;
    config.logWarn  = req["warn"]  | true;
    config.logError = req["error"] | true;
    config.logDebug = req["debug"] | false;

    config.save();
    server.send(200, "text/plain", "Log level updated");
}

// ---------------------------------------------------------
//  API: UART / Scheduler
// ---------------------------------------------------------
static void handleReq() {
    String cmd = server.arg("code");
    py_scheduler.enqueueRaw(cmd);
    server.send(200, "text/plain", "OK");
}

static void handleApiLastFrame() {
    server.send(200, "text/plain", py_uart.getLastRawFrame());
}

// ---------------------------------------------------------
//  API: WiFi
// ---------------------------------------------------------
static void handleApiWifiStatus() {
    server.send(200, "application/json", WiFiManagerModule::getStatusJson());
}

static void handleApiWifiScan() {
    server.send(200, "application/json", WiFiManagerModule::scanJson());
}

static void handleApiWifiConnect() {
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

// ---------------------------------------------------------
//  API: Network Config
// ---------------------------------------------------------
static void handleApiNetGet() {
    DynamicJsonDocument doc(256);

    doc["static"] = config.useStaticIP;
    doc["ip"]     = config.ipAddr;
    doc["mask"]   = config.subnetMask;
    doc["gw"]     = config.gateway;
    doc["dns"]    = config.dns;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleApiNetSet() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(512);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.useStaticIP = req["static"] | false;
    config.ipAddr      = req["ip"]     | "";
    config.subnetMask  = req["mask"]   | "";
    config.gateway     = req["gw"]     | "";
    config.dns         = req["dns"]    | "";

    config.save();
    server.send(200, "text/plain", "Network settings saved");
}

// ---------------------------------------------------------
//  API: NTP Server
// ---------------------------------------------------------
static void handleApiNtpGet() {
    DynamicJsonDocument doc(128);
    doc["server"] = config.ntpServer;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleApiNtpSet() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(128);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.ntpServer = req["server"] | "pool.ntp.org";
    config.save();

    server.send(200, "text/plain", "NTP server saved");
}
// ---------------------------------------------------------
//  API: Time Settings (NTP + Timezone + DST)
// ---------------------------------------------------------
static void handleApiTimeGet() {
    DynamicJsonDocument doc(256);

    doc["server"]   = config.ntpServer;
    doc["timezone"] = config.timezone;
    doc["dst"]      = config.daylightSaving;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleApiTimeSet() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(256);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    config.ntpServer      = req["server"]   | config.ntpServer;
    config.timezone       = req["timezone"] | config.timezone;
    config.daylightSaving = req["dst"]      | config.daylightSaving;

    config.save();
    server.send(200, "text/plain", "Time settings saved");
}
// ---------------------------------------------------------
//  API: MQTT Config
// ---------------------------------------------------------
static void handleApiMqttGet() {
    DynamicJsonDocument doc(256);

    doc["enabled"] = config.mqtt.enabled;
    doc["server"]  = config.mqtt.server;
    doc["port"]    = config.mqtt.port;
    doc["user"]    = config.mqtt.user;
    doc["pass"]    = config.mqtt.pass;
    doc["topic"]   = config.mqtt.prefix;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleApiMqttSet() {
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
    config.mqtt.pass    = req["pass"]    | "";
    config.mqtt.prefix  = req["topic"]   | "Pylontech";

    config.save();
    py_mqtt.begin();


    server.send(200, "text/plain", "MQTT settings saved");
}
// ---------------------------------------------------------
//  API: Dashboard – Battery Info
// ---------------------------------------------------------
static void handleApiBatteryInfo() {
    DynamicJsonDocument doc(256);

    doc["modules"] = lastParsedStack.batteryCount;
    doc["last_update"] = config.lastPwrUpdate;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ---------------------------------------------------------
//  API: Dashboard – System Info
// ---------------------------------------------------------
static void handleApiSystemInfo() {
    DynamicJsonDocument doc(256);

    // --- Current system time ---
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    doc["last_mqtt"] = config.lastMqttContact;
    doc["last_battery"] = config.lastPwrUpdate;


    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    doc["time"]    = String(buf);
    doc["uptime"]  = config.uptimeString();
    doc["version"] = config.firmwareVersion;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ---------------------------------------------------------
//  API: MQTT Status (Dashboard)
// ---------------------------------------------------------
static void handleApiMqttStatus() {
    DynamicJsonDocument doc(256);

    bool connected = py_mqtt.isConnected();

    doc["connected"] = connected;
    doc["server"]    = config.mqtt.server;
    doc["port"]      = config.mqtt.port;
    doc["last_contact"] = config.lastMqttContact;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ---------------------------------------------------------
//  API: PWR Settings + Parser Fields
//  Returns config + last parsed header + raw values
// ---------------------------------------------------------
static void handleApiPwrGet() {
    DynamicJsonDocument doc(8192);

    // Basic config
    doc["fahrenheit"]   = config.battery.useFahrenheit;
    doc["interval_pwr"] = config.battery.intervalPwr / 1000;
    doc["mqtt_stack"]   = config.mqtt.topicStack;
    doc["mqtt_pwr"]     = config.mqtt.topicPwr;

    JsonArray fields = doc.createNestedArray("fields");

    // --------------------------------------------
    // Schritt 1: Parser-Felder sammeln
    // --------------------------------------------
    std::vector<String> allKeys;

    for (auto &h : lastParserHeader) {
        allKeys.push_back(h);
    }

    // --------------------------------------------
    // Schritt 2: Für jedes Parserfeld Config anwenden
    // --------------------------------------------
    for (auto &name : allKeys) {

        // Stack-Felder ausblenden
        if (name.startsWith("Stack") || name == "BatteryCount")
            continue;

        FieldConfig f;

        // Wenn Config existiert → übernehmen
        if (config.battery.fields.count(name)) {
            f = config.battery.fields[name];
        } else {
            // Neue Felder → Defaults
            f.label  = name;
            f.factor = "1";
            f.unit   = "";
            f.mqtt   = false;
            f.send   = false;
        }

        JsonObject o = fields.createNestedObject();
        o["name"]        = name;
        o["display"]     = f.label;
        o["factor"]      = f.factor;
        o["unit"]        = f.unit;
        o["sendMQTT"]    = f.mqtt;
        o["sendPayload"] = f.send;

        // Rohdaten + Wert 1:1 übernehmen
        int idx = -1;
        for (size_t i = 0; i < lastParserHeader.size(); i++) {
            if (lastParserHeader[i] == name) {
                idx = i;
                break;
            }
        }

        if (idx >= 0 && idx < lastParserValues.size()) {
            o["raw"]   = lastParserValues[idx];
            o["value"] = lastParserValues[idx];
        } else {
            o["raw"]   = "";
            o["value"] = "";
        }
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}
// ---------------------------------------------------------
//  API: Save PWR Settings
// ---------------------------------------------------------
extern bool discoverySent;

static void handleApiPwrSet() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(8192);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    // Basic settings
    config.battery.useFahrenheit = req["fahrenheit"] | false;
    config.battery.intervalPwr   = (req["interval_pwr"] | 60) * 1000UL;

    config.mqtt.topicStack = req["mqtt_stack"] | "Stack";
    config.mqtt.topicPwr   = req["mqtt_pwr"]   | "pwr";

    // Parser fields
    JsonArray arr = req["fields"];
    for (JsonObject f : arr) {
        String name = f["name"] | "";
        if (name.length() == 0) continue;

        // Stack-Felder nicht speichern
        if (name.startsWith("Stack") || name == "BatteryCount")
            continue;

        // Feld anlegen falls neu
        if (!config.battery.fields.count(name)) {
            FieldConfig fc;
            fc.label = name;
            fc.factor = "1";
            fc.unit = "";
            fc.mqtt = false;
            fc.send = false;
            config.battery.fields[name] = fc;
        }

        FieldConfig &fc = config.battery.fields[name];

        fc.label = f["display"] | fc.label;
        fc.factor = f["factor"] | fc.factor;
        fc.unit = f["unit"] | fc.unit;
        fc.mqtt = f["sendMQTT"] | false;
        fc.send = f["sendPayload"] | false;
    }

    // Discovery neu senden
    discoverySent = false;

    config.save();
    server.send(200, "text/plain", "PWR settings saved");
}
// ---------------------------------------------------------
//  Route Registration
// ---------------------------------------------------------
void registerRoutes() {

    // Pages
    server.on("/",            HTTP_GET, handleIndex);
    server.on("/battery",     HTTP_GET, handleBatteryPage);
    server.on("/log",         HTTP_GET, handleLogPage);
    server.on("/connect",     HTTP_GET, handleConnectPage);
    server.on("/pwr_setting", HTTP_GET, handlePwrSettingsPage);

    // UART / Scheduler
    server.on("/req",           HTTP_GET, handleReq);
    server.on("/api/lastframe", HTTP_GET, handleApiLastFrame);

    // Log
    server.on("/api/log", HTTP_GET, handleApiLog);
    server.on("/api/log/level", HTTP_GET,  handleApiLogLevelGet);
    server.on("/api/log/level", HTTP_POST, handleApiLogLevelSet);

    // WiFi
    server.on("/api/wifi/status",  HTTP_GET,  handleApiWifiStatus);
    server.on("/api/wifi/scan",    HTTP_GET,  handleApiWifiScan);
    server.on("/api/wifi/connect", HTTP_POST, handleApiWifiConnect);

    // Network
    server.on("/api/net/config", HTTP_GET,  handleApiNetGet);
    server.on("/api/net/config", HTTP_POST, handleApiNetSet);

    // NTP
    server.on("/api/net/ntp", HTTP_GET,  handleApiNtpGet);
    server.on("/api/net/ntp", HTTP_POST, handleApiNtpSet);

    // Time Settings
    server.on("/api/net/time", HTTP_GET,  handleApiTimeGet);
    server.on("/api/net/time", HTTP_POST, handleApiTimeSet);

    // MQTT
    server.on("/api/mqtt/config", HTTP_GET,  handleApiMqttGet);
    server.on("/api/mqtt/config", HTTP_POST, handleApiMqttSet);

    // Dashboard APIs
    server.on("/api/battery/info", HTTP_GET, handleApiBatteryInfo);
    server.on("/api/system/info",  HTTP_GET, handleApiSystemInfo);
    server.on("/api/mqtt/status",  HTTP_GET, handleApiMqttStatus);

    // NEW: PWR settings + parser fields
    server.on("/api/pwr/get", HTTP_GET, handleApiPwrGet);
    server.on("/api/pwr/set", HTTP_POST, handleApiPwrSet);

}