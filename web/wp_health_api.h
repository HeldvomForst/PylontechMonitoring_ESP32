#pragma once
#include "api_core.h"
#include "../config.h"

extern HealthStatus health;
extern AppConfig config;

// ------------------------------------------------------------
// Handler: GET /api/health
// ------------------------------------------------------------
static esp_err_t api_health_get(httpd_req_t *req) {

    StaticJsonDocument<4096> doc;

    // MODULES
    JsonArray mods = doc.createNestedArray("modules");
    for (int i = 0; i < health.moduleCount; i++) {
        const ModuleHealth& m = health.modules[i];
        JsonObject o = mods.createNestedObject();
        o["index"]    = m.index;
        o["status"]   = m.status;
        o["tempMax"]  = m.tempMax;
        o["cellMin"]  = m.cellMin;
        o["cellMax"]  = m.cellMax;
        o["cellDiff"] = m.cellDiff;
    }

    // STACK
    JsonObject st = doc.createNestedObject("stack");
    st["cellMin"]  = health.stackCellMin;
    st["cellMax"]  = health.stackCellMax;
    st["cellDiff"] = health.stackCellDiff;

    // OK / WARN / ERROR
    auto addListArr = [&](JsonArray arr, const int* v, int count) {
        for (int i = 0; i < count; i++) arr.add(v[i]);
    };

    addListArr(doc.createNestedArray("ok"),    health.okModules,    health.okCount);
    addListArr(doc.createNestedArray("warn"),  health.warnModules,  health.warnCount);
    addListArr(doc.createNestedArray("error"), health.errorModules, health.errorCount);

    // History
    auto addListVec = [&](JsonArray arr, const std::vector<int>& v) {
        for (int x : v) arr.add(x);
    };

    addListVec(doc.createNestedArray("warnHistory"),  health.warnHistory);
    addListVec(doc.createNestedArray("errorHistory"), health.errorHistory);

    // STRONGEST + COLOR
    doc["strongest"] = health.strongestMessage;
    doc["color"]     = health.color;

    // CONFIG
    JsonObject cfg = doc.createNestedObject("config");
    cfg["cellDiffWarn"]  = config.battery.cellDiffWarn;
    cfg["cellDiffError"] = config.battery.cellDiffError;

    String out;
    serializeJson(doc, out);
    apiJson(req, out);
    return ESP_OK;
}

// ------------------------------------------------------------
// Handler: POST /api/health
// ------------------------------------------------------------
static esp_err_t api_health_post(httpd_req_t *req) {

    String body = apiGetBody(req);
    if (body.isEmpty()) {
        apiError(req, 400, "Missing body");
        return ESP_OK;
    }

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, body)) {
        apiError(req, 400, "Invalid JSON");
        return ESP_OK;
    }

    config.battery.cellDiffWarn  = doc["cellDiffWarn"]  | config.battery.cellDiffWarn;
    config.battery.cellDiffError = doc["cellDiffError"] | config.battery.cellDiffError;

    config.save();
    apiText(req, "Health config saved");
    return ESP_OK;
}

// ------------------------------------------------------------
// Handler: GET /api/health/reset
// ------------------------------------------------------------
static esp_err_t api_health_reset(httpd_req_t *req) {
    health.warnHistory.clear();
    health.errorHistory.clear();
    apiText(req, "OK");
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerHealthAPI() {

    httpd_uri_t r1 = {
        .uri      = "/api/health",
        .method   = HTTP_GET,
        .handler  = api_health_get,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &r1);

    httpd_uri_t r2 = {
        .uri      = "/api/health",
        .method   = HTTP_POST,
        .handler  = api_health_post,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &r2);

    httpd_uri_t r3 = {
        .uri      = "/api/health/reset",
        .method   = HTTP_GET,
        .handler  = api_health_reset,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &r3);
}
