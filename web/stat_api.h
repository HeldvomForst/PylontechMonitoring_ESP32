#pragma once
#include <ArduinoJson.h>
#include "../wp_webserver.h"
#include "../py_parser_stat.h"
#include "../config.h"

static void handleApiStatValues();

static void registerStatAPI() {
    server.on("/api/stat/values", HTTP_GET, handleApiStatValues);
}

static void handleApiStatValues() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");

    server.sendContent("{");

    // CONFIG
    server.sendContent("\"config\":{");
    server.sendContent("\"intervalStat\":");
    server.sendContent(String(config.battery.intervalStat));
    server.sendContent(",");
    server.sendContent("\"enableStat\":");
    server.sendContent(config.battery.enableStat ? "true" : "false");
    server.sendContent("},");

    // MQTT
    server.sendContent("\"mqtt\":{");
    server.sendContent("\"topicStat\":\"");
    server.sendContent(config.mqtt.topicStat);
    server.sendContent("\"");
    server.sendContent("},");

    // HEADERS
    server.sendContent("\"headers\":[");
    for (size_t i = 0; i < lastParsedStat.fields.size(); i++) {
        if (i > 0) server.sendContent(",");
        server.sendContent("\"");
        server.sendContent(lastParsedStat.fields[i].name);
        server.sendContent("\"");
    }
    server.sendContent("],");

    // VALUES
    server.sendContent("\"values\":[");
    for (size_t i = 0; i < lastParsedStat.fields.size(); i++) {
        if (i > 0) server.sendContent(",");
        server.sendContent("\"");
        server.sendContent(lastParsedStat.fields[i].raw);
        server.sendContent("\"");
    }
    server.sendContent("],");

    // FIELDS (nur NVS-Felder!)
    server.sendContent("\"fields\":[");

    bool first = true;
    for (auto &pf : lastParsedStat.fields) {

        // Nur Felder aus NVS zurückgeben
        if (!config.battery.fieldsStat.count(pf.name)) {
            continue;
        }

        const FieldConfig &f = config.battery.fieldsStat.at(pf.name);

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

static void handleApiStatSet() {

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
    config.battery.intervalStat = req["config"]["intervalStat"] | config.battery.intervalStat;
    config.battery.enableStat   = req["config"]["enableStat"]   | config.battery.enableStat;

    // MQTT
    config.mqtt.topicStat = req["mqtt"]["topicStat"] | config.mqtt.topicStat;

    // FIELDS
    JsonArray arr = req["fields"];
    for (JsonObject f : arr) {

        String name = f["name"] | "";
        if (name.length() == 0) continue;

        if (!config.battery.fieldsStat.count(name)) {
            FieldConfig fc;
            fc.label   = name;
            fc.display = name;
            fc.factor  = "1";
            fc.unit    = "";
            fc.mqtt    = false;
            fc.send    = false;
            config.battery.fieldsStat[name] = fc;
        }

        FieldConfig &fc = config.battery.fieldsStat[name];

        fc.display = f["display"]     | fc.display;
        fc.label   = f["display"]     | fc.label;
        fc.factor  = f["factor"]      | fc.factor;
        fc.unit    = f["unit"]        | fc.unit;
        fc.mqtt    = f["sendMQTT"]    | false;
        fc.send    = f["sendPayload"] | false;
    }


	discoveryStatNeeded = true;

    config.save();

    server.send(200, "text/plain", "STAT settings saved");
}

