#pragma once
#include <ArduinoJson.h>
#include "../wp_webserver.h"
#include "../py_parser_bat.h"
#include "../config.h"

static void handleApiBatCells();

static void registerBatAPI() {
    server.on("/api/bat/cells", HTTP_GET, handleApiBatCells);
}

static void handleApiBatCells() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");

    server.sendContent("{");

    // CONFIG
    server.sendContent("\"config\":{");
    server.sendContent("\"intervalBat\":");
    server.sendContent(String(config.battery.intervalBat));
    server.sendContent(",");
    server.sendContent("\"enableBat\":");
    server.sendContent(config.battery.enableBat ? "true" : "false");
    server.sendContent("},");

    // MQTT
    server.sendContent("\"mqtt\":{");
    server.sendContent("\"topicBat\":\"");
    server.sendContent(config.mqtt.topicBat);
    server.sendContent("\",");
    server.sendContent("\"cellPrefix\":\"");
    server.sendContent(config.mqtt.cellPrefix);
    server.sendContent("\"");
    server.sendContent("},");

    // HEADERS
    server.sendContent("\"headers\":[");
    for (size_t i = 0; i < lastParsedBat.fields.size(); i++) {
        if (i > 0) server.sendContent(",");
        server.sendContent("\"");
        server.sendContent(lastParsedBat.fields[i].name);
        server.sendContent("\"");
    }
    server.sendContent("],");

    // VALUES
    server.sendContent("\"values\":[");
    for (size_t i = 0; i < lastParsedBat.fields.size(); i++) {
        if (i > 0) server.sendContent(",");
        server.sendContent("\"");
        server.sendContent(lastParsedBat.fields[i].raw);
        server.sendContent("\"");
    }
    server.sendContent("],");

    // FIELDS (nur NVS-Felder!)
    server.sendContent("\"fields\":[");

    bool first = true;
    for (auto &pf : lastParsedBat.fields) {

        // Nur Felder aus NVS zurückgeben
        if (!config.battery.fieldsBat.count(pf.name)) {
            continue;
        }

        const FieldConfig &f = config.battery.fieldsBat.at(pf.name);

        DynamicJsonDocument doc(256);
        JsonObject o = doc.to<JsonObject>();

        o["name"]        = pf.name;
        o["display"]     = f.display;
        o["factor"]      = f.factor;
        o["unit"]        = f.unit;
        o["sendMQTT"]    = f.mqtt;
        o["sendPayload"] = f.send;
        o["raw"]         = pf.raw;
        o["value"]       = pf.raw;

        String tmp;
        serializeJson(o, tmp);

        if (!first) server.sendContent(",");
        first = false;

        server.sendContent(tmp);
    }

    server.sendContent("]");

    server.sendContent("}");
}

static void handleApiBatSet() {

    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    DynamicJsonDocument req(4096);
    if (deserializeJson(req, server.arg("plain"))) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    // CONFIG
    config.battery.intervalBat = req["config"]["intervalBat"] | config.battery.intervalBat;
    config.battery.enableBat   = req["config"]["enableBat"]   | config.battery.enableBat;

    // MQTT
    config.mqtt.topicBat   = req["mqtt"]["topicBat"]   | config.mqtt.topicBat;
    config.mqtt.cellPrefix = req["mqtt"]["cellPrefix"] | config.mqtt.cellPrefix;

    // FIELDS
    JsonArray arr = req["fields"];
    for (JsonObject f : arr) {

        String name = f["name"] | "";
        if (name.length() == 0) continue;

        if (!config.battery.fieldsBat.count(name)) {
            FieldConfig fc;
            fc.label   = name;
            fc.display = name;
            fc.factor  = "1";
            fc.unit    = "";
            fc.mqtt    = false;
            fc.send    = false;
            config.battery.fieldsBat[name] = fc;
        }

        FieldConfig &fc = config.battery.fieldsBat[name];

        fc.display = f["display"]     | fc.display;
        fc.label   = f["display"]     | fc.label;
        fc.factor  = f["factor"]      | fc.factor;
        fc.unit    = f["unit"]        | fc.unit;
        fc.mqtt    = f["sendMQTT"]    | false;
        fc.send    = f["sendPayload"] | false;
    }

	discoveryBatNeeded  = true;

    config.save();

    server.send(200, "text/plain", "BAT settings saved");
}
