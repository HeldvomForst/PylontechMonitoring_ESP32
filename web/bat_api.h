#pragma once
#include "api_core.h"
#include <ArduinoJson.h>
#include "../config.h"
#include "../py_parser.h"

extern AppConfig config;
extern BatTable  batTable;
extern bool      discoveryBatNeeded;

extern FieldConfig batFields[BAT_MAX_COLS];
extern int         batFieldCount;

// ------------------------------------------------------------
// GET /api/bat/cells
// ------------------------------------------------------------
static esp_err_t api_bat_cells(httpd_req_t *req) {

    DynamicJsonDocument doc(4096);

    // CONFIG
    JsonObject cfg = doc.createNestedObject("config");
    cfg["intervalBat"] = config.battery.intervalBat;
    cfg["enableBat"]   = config.battery.enableBat;

    // MQTT
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["topicBat"]   = config.mqtt.topicBat;
    mqtt["cellPrefix"] = config.mqtt.cellPrefix;

    BatTable& tbl = batTable;

    // HEADERS
    JsonArray headers = doc.createNestedArray("headers");
    for (int c = 0; c < tbl.cols; c++) {
        headers.add(tbl.header[c] ? tbl.header[c] : "");
    }

    // VALUES (nur 1 Modul)
    JsonArray values = doc.createNestedArray("values");
    if (tbl.rows > 0) {
        for (int c = 0; c < tbl.cols; c++) {
            const char* v = tbl.cell[0][c];
            values.add(v ? v : "");
        }
    }

    // FIELDS (NVS-Konfiguration)
    JsonArray fields = doc.createNestedArray("fields");

    if (tbl.rows > 0) {
        for (int c = 0; c < tbl.cols; c++) {

            const char* name = tbl.header[c];
            const char* raw  = tbl.cell[0][c];
            if (!name) continue;

            String key(name);

            // passenden FieldConfig suchen
            int idx = -1;
            for (int i = 0; i < batFieldCount; i++) {
                if (batFields[i].label == key) {
                    idx = i;
                    break;
                }
            }
            if (idx < 0) continue;

            const FieldConfig& fc = batFields[idx];

            JsonObject o = fields.createNestedObject();
            o["name"]        = key;
            o["display"]     = fc.display;
            o["factor"]      = fc.factor;
            o["unit"]        = fc.unit;
            o["sendMQTT"]    = fc.mqtt;
            o["sendPayload"] = fc.send;
            o["raw"]         = raw ? raw : "";
            o["value"]       = raw ? raw : "";
        }
    }

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// POST /api/bat/set
// ------------------------------------------------------------
static esp_err_t api_bat_set(httpd_req_t *req) {

    String body = apiGetBody(req);

    Log(LOG_INFO, String("BAT-API: Received JSON length = ") + body.length());
    Log(LOG_INFO, String("BAT-API: Received JSON = ") + body);

    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    // CONFIG
    config.battery.intervalBat = doc["config"]["intervalBat"] | config.battery.intervalBat;
    //config.battery.enableBat   = doc["config"]["enableBat"]   | config.battery.enableBat;

    // MQTT
    config.mqtt.topicBat   = doc["mqtt"]["topicBat"]   | config.mqtt.topicBat;
    config.mqtt.cellPrefix = doc["mqtt"]["cellPrefix"] | config.mqtt.cellPrefix;

    // FIELDS: RAM-Tabelle komplett neu aufbauen
    JsonArray arr = doc["fields"];

    batFieldCount = 0;  // alte RAM-Felder verwerfen

    for (JsonObject f : arr) {

        if (batFieldCount >= BAT_MAX_COLS) break;

        String name = f["name"] | "";
        if (name.length() == 0) continue;

        FieldConfig& fc = batFields[batFieldCount++];

        // Label = technischer Name vom Parser/BMS
        fc.label   = name;

        // Display = Anzeigename (variabel)
        fc.display = f["display"] | name;

        fc.factor  = f["factor"]  | String("1");
        fc.unit    = f["unit"]    | String("");
        fc.mqtt    = f["sendMQTT"]    | false;
        fc.send    = f["sendPayload"] | false;
    }

    Log(LOG_INFO, String("BAT-API: batFieldCount after rebuild = ") + batFieldCount);

    discoveryBatNeeded = true;
    config.save();   // ruft saveBatFields(), das NVS-Chunks löscht und neu schreibt

    apiText(req, "BAT settings saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerBatAPI() {

    httpd_uri_t r1 = { "/api/bat/cells", HTTP_GET,  api_bat_cells, NULL };
    httpd_uri_t r2 = { "/api/bat/set",   HTTP_POST, api_bat_set,   NULL };

    httpd_register_uri_handler(server, &r1);
    httpd_register_uri_handler(server, &r2);
}
