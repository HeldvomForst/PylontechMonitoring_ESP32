#pragma once
#include "api_core.h"
#include <ArduinoJson.h>
#include "../py_parser.h"
#include "../config.h"

extern AppConfig config;
extern PwrTable pwrTable;
extern int      pwrFieldCount;
extern PwrField pwrFields[32];
extern bool     discoveryPwrNeeded;

// ------------------------------------------------------------
// GET /api/pwr/base
// ------------------------------------------------------------
static esp_err_t api_pwr_base(httpd_req_t *req) {

    PwrTable& tbl = pwrTable;

    String out;
    out.reserve(4096);

    out += "{";

    // CONFIG
    out += "\"config\":{";
    out += "\"intervalPwr\":";   out += config.battery.intervalPwr; out += ",";
    out += "\"useFahrenheit\":"; out += (config.battery.useFahrenheit ? "true" : "false");
    out += "},";

    // MQTT
    out += "\"mqtt\":{";
    out += "\"topicStack\":\""; out += config.mqtt.topicStack; out += "\",";
    out += "\"topicPwr\":\"";   out += config.mqtt.topicPwr;   out += "\"";
    out += "},";

    // HEADERS
    out += "\"headers\":[";
    for (int c = 0; c < tbl.cols; c++) {
        if (c > 0) out += ",";
        out += "\"";
        out += (tbl.header[c] ? tbl.header[c] : "");
        out += "\"";
    }
    out += "],";

    // VALUES
    out += "\"values\":[";
    if (tbl.rows > 0) {
        for (int c = 0; c < tbl.cols; c++) {
            if (c > 0) out += ",";
            out += "\"";
            out += (tbl.cell[0][c] ? tbl.cell[0][c] : "");
            out += "\"";
        }
    }
    out += "],";

    // FIELDS
    out += "\"fields\":[";
    bool first = true;

    for (int i = 0; i < pwrFieldCount; i++) {
        const PwrField& f = pwrFields[i];

        const char* raw = nullptr;
        if (tbl.rows > 0) {
            for (int c = 0; c < tbl.cols; c++) {
                if (tbl.header[c] && strcmp(tbl.header[c], f.name.c_str()) == 0) {
                    raw = tbl.cell[0][c];
                    break;
                }
            }
        }

        if (!first) out += ",";
        first = false;

        out += "{";
        out += "\"name\":\"";        out += f.name;    out += "\",";
        out += "\"display\":\"";     out += f.display; out += "\",";
        out += "\"factor\":\"";      out += f.factor;  out += "\",";
        out += "\"unit\":\"";        out += f.unit;    out += "\",";
        out += "\"sendMQTT\":";      out += (f.mqtt ? "true" : "false"); out += ",";
        out += "\"sendPayload\":";   out += (f.send ? "true" : "false"); out += ",";
        out += "\"raw\":\"";         out += (raw ? raw : ""); out += "\",";
        out += "\"value\":\"";       out += (raw ? raw : ""); out += "\"";
        out += "}";
    }

    out += "]";

    out += "}";

    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// POST /api/pwr/set
// ------------------------------------------------------------
static esp_err_t api_pwr_set(httpd_req_t *req) {

    String body = apiGetBody(req);
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
    config.battery.intervalPwr   = doc["config"]["intervalPwr"]   | config.battery.intervalPwr;
    config.battery.useFahrenheit = doc["config"]["useFahrenheit"] | config.battery.useFahrenheit;

    // MQTT
    config.mqtt.topicStack = doc["mqtt"]["topicStack"] | config.mqtt.topicStack;
    config.mqtt.topicPwr   = doc["mqtt"]["topicPwr"]   | config.mqtt.topicPwr;

    // FIELDS
    JsonArray arr = doc["fields"];
    pwrFieldCount = 0;

    for (JsonObject f : arr) {
        if (pwrFieldCount >= 32) break;

        PwrField pf;
        pf.name    = f["name"]        | "";
        pf.display = f["display"]     | pf.name;
        pf.factor  = f["factor"]      | "1";
        pf.unit    = f["unit"]        | "";
        pf.mqtt    = f["sendMQTT"]    | false;
        pf.send    = f["sendPayload"] | false;

        pwrFields[pwrFieldCount++] = pf;
    }

    config.savePwrFields();
    discoveryPwrNeeded = true;
    config.save();

    apiText(req, "PWR settings saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerPwrAPI() {

    httpd_uri_t r1 = { "/api/pwr/base", HTTP_GET,  api_pwr_base, NULL };
    httpd_uri_t r2 = { "/api/pwr/set",  HTTP_POST, api_pwr_set,  NULL };

    httpd_register_uri_handler(server, &r1);
    httpd_register_uri_handler(server, &r2);
}
