#pragma once
#include "api_core.h"
#include <ArduinoJson.h>
#include "../py_parser.h"
#include "../config.h"

extern AppConfig config;
extern StatTable statTable;
extern bool      discoveryStatNeeded;

extern FieldConfig statFields[STAT_MAX_FIELDS];
extern int         statFieldCount;

// ------------------------------------------------------------
// GET /api/stat/values
// ------------------------------------------------------------
static esp_err_t api_stat_values(httpd_req_t *req) {

    StatTable& tbl = statTable;

    httpd_resp_set_type(req, "application/json");

    // JSON Start
    httpd_resp_send_chunk(req, "{", 1);

    // CONFIG
    {
        String chunk;
        chunk.reserve(128);
        chunk += "\"config\":{";
        chunk += "\"enableStat\":";   chunk += (config.battery.enableStat ? "true" : "false"); chunk += ",";
        chunk += "\"intervalStat\":"; chunk += config.battery.intervalStat;
        chunk += "},";
        httpd_resp_send_chunk(req, chunk.c_str(), chunk.length());
    }

    // MQTT
    {
        String chunk;
        chunk.reserve(128);
        chunk += "\"mqtt\":{";
        chunk += "\"topicStat\":\""; chunk += config.mqtt.topicStat; chunk += "\"";
        chunk += "},";
        httpd_resp_send_chunk(req, chunk.c_str(), chunk.length());
    }

    // HEADERS
    httpd_resp_send_chunk(req, "\"headers\":[", 11);
    for (int i = 0; i < tbl.count; i++) {
        String chunk;
        chunk.reserve(128);
        if (i > 0) chunk += ",";
        chunk += "\"";
        chunk += (tbl.name[i] ? tbl.name[i] : "");
        chunk += "\"";
        httpd_resp_send_chunk(req, chunk.c_str(), chunk.length());
    }
    httpd_resp_send_chunk(req, "],", 2);

    // VALUES
    httpd_resp_send_chunk(req, "\"values\":[", 10);
    for (int i = 0; i < tbl.count; i++) {
        String chunk;
        chunk.reserve(128);
        if (i > 0) chunk += ",";
        chunk += "\"";
        chunk += (tbl.value[i] ? tbl.value[i] : "");
        chunk += "\"";
        httpd_resp_send_chunk(req, chunk.c_str(), chunk.length());
    }
    httpd_resp_send_chunk(req, "],", 2);

    // FIELDS
    httpd_resp_send_chunk(req, "\"fields\":[", 10);

    bool first = true;

    for (int i = 0; i < tbl.count; i++) {

        const char* key = tbl.name[i];
        const char* raw = tbl.value[i];
        if (!key) continue;

        int idx = -1;
        for (int j = 0; j < statFieldCount; j++) {
            if (statFields[j].label == key) {
                idx = j;
                break;
            }
        }
        if (idx < 0) continue;

        const FieldConfig& fc = statFields[idx];

        String chunk;
        chunk.reserve(256);

        if (!first) chunk += ",";
        first = false;

        chunk += "{";
        chunk += "\"name\":\"";        chunk += key;        chunk += "\",";
        chunk += "\"display\":\"";     chunk += fc.display; chunk += "\",";
        chunk += "\"factor\":\"";      chunk += fc.factor;  chunk += "\",";
        chunk += "\"unit\":\"";        chunk += fc.unit;    chunk += "\",";
        chunk += "\"sendMQTT\":";      chunk += (fc.mqtt ? "true" : "false"); chunk += ",";
        chunk += "\"sendPayload\":";   chunk += (fc.send ? "true" : "false"); chunk += ",";
        chunk += "\"raw\":\"";         chunk += (raw ? raw : ""); chunk += "\",";
        chunk += "\"value\":\"";       chunk += (raw ? raw : ""); chunk += "\"";
        chunk += "}";

        httpd_resp_send_chunk(req, chunk.c_str(), chunk.length());
    }

    // JSON Ende
    httpd_resp_send_chunk(req, "]}", 2);

    // Abschluss
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

// ------------------------------------------------------------
// POST /api/stat/set
// ------------------------------------------------------------
static esp_err_t api_stat_set(httpd_req_t *req) {

    String body = apiGetBody(req);
    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    Log(LOG_INFO, String("STAT-SET: RAW JSON length = ") + body.length());

    // Pointertabelle leeren
    statFieldCount = 0;

    // Felder im JSON suchen
    int pos = body.indexOf("\"fields\"");
    pos = body.indexOf("[", pos);
    if (pos < 0) {
        apiError(req, 400, "fields[] missing");
        return ESP_OK;
    }

    while (true) {

        int objStart = body.indexOf("{", pos);
        if (objStart < 0) break;

        int objEnd = body.indexOf("}", objStart);
        if (objEnd < 0) break;

        String fieldJson = body.substring(objStart, objEnd + 1);

        StaticJsonDocument<512> doc;
        if (!deserializeJson(doc, fieldJson)) {

            bool mqttActive = doc["sendMQTT"] | false;

            // *** NEU: nur MQTT-aktive Felder speichern ***
            if (mqttActive) {
                FieldConfig& fc = statFields[statFieldCount++];

                fc.label   = doc["name"]        | "";
                fc.display = doc["display"]     | fc.label;
                fc.factor  = doc["factor"]      | "1";
                fc.unit    = doc["unit"]        | "";
                fc.mqtt    = true;              // nur aktive
                fc.send    = doc["sendPayload"] | false;
            }
        }

        pos = objEnd + 1;
    }

    Log(LOG_INFO, String("STAT-SET: MQTT-active fields = ") + statFieldCount);

    // Jetzt Pointertabelle als JSON-Chunks speichern
    config.saveStatFields();

    apiText(req, "STAT saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerStatAPI() {

    httpd_uri_t r1 = { "/api/stat/values", HTTP_GET,  api_stat_values, NULL };
    httpd_uri_t r2 = { "/api/stat/set",    HTTP_POST, api_stat_set,    NULL };

    httpd_register_uri_handler(server, &r1);
    httpd_register_uri_handler(server, &r2);
}
