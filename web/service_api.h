#pragma once
#include "api_core.h"
#include <esp_http_server.h>
#include <Update.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "service.h"
#include "../config.h"
#include "../py_systemmanager.h"
#include "../py_display.h"

extern PyDisplay display;
extern AppConfig config;

// ------------------------------------------------------------
// /service  → HTML-Seite
// ------------------------------------------------------------
static esp_err_t api_service_page(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, SERVICE_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/restart
// ------------------------------------------------------------
static esp_err_t api_restart(httpd_req_t *req) {
    apiText(req, "Restarting");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    ESP.restart();
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/factoryreset
// ------------------------------------------------------------
static esp_err_t api_factoryreset(httpd_req_t *req) {
    apiText(req, "Factory reset");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    SystemManager::triggerFactoryReset();
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/wifireset
// ------------------------------------------------------------
static esp_err_t api_wifireset(httpd_req_t *req) {
    apiText(req, "WiFi reset");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    SystemManager::triggerWiFiReset();
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/formatspiffs
// ------------------------------------------------------------
static esp_err_t api_formatspiffs(httpd_req_t *req) {

    Log(LOG_WARN, "SPIFFS FORMAT triggered via Service Page");

    apiText(req, "Formatting SPIFFS...");

    vTaskDelay(200 / portTICK_PERIOD_MS);

    bool ok = SPIFFS.format();

    if (!ok) {
        Log(LOG_ERROR, "SPIFFS FORMAT FAILED");
        apiError(req, 500, "SPIFFS format failed");
        return ESP_OK;
    }

    Log(LOG_INFO, "SPIFFS FORMAT completed successfully");

    return ESP_OK;
}


// ------------------------------------------------------------
// /api/version
// ------------------------------------------------------------
static esp_err_t api_version(httpd_req_t *req) {
    apiText(req, config.firmwareVersion);
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/storageinfo
// ------------------------------------------------------------
static esp_err_t api_storageinfo(httpd_req_t *req) {

    size_t spiffsTotal = SPIFFS.totalBytes();
    size_t spiffsUsed  = SPIFFS.usedBytes();
    size_t spiffsFree  = spiffsTotal - spiffsUsed;

    nvs_stats_t st;
    nvs_get_stats(NULL, &st);

    String json = "{";
    json += "\"spiffs_total\":" + String(spiffsTotal) + ",";
    json += "\"spiffs_used\":"  + String(spiffsUsed)  + ",";
    json += "\"spiffs_free\":"  + String(spiffsFree)  + ",";
    json += "\"nvs_total\":"    + String(st.total_entries) + ",";
    json += "\"nvs_used\":"     + String(st.used_entries)  + ",";
    json += "\"nvs_free\":"     + String(st.free_entries);
    json += "}";

    apiJson(req, json);
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/ota  → Firmware-Upload (RAW binary, kein multipart nötig)
// ------------------------------------------------------------
static esp_err_t api_ota(httpd_req_t *req) {

    const size_t MAX_SIZE = 2 * 1024 * 1024;
    const size_t MIN_SIZE = 100 * 1024;

    char buf[1024];
    size_t total = 0;

    // OTA Start
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        apiError(req, 500, "OTA begin failed");
        return ESP_OK;
    }

    while (true) {
        int len = httpd_req_recv(req, buf, sizeof(buf));

        if (len < 0) {
            Update.abort();
            apiError(req, 500, "OTA recv error");
            return ESP_OK;
        }

        if (len == 0) break;  // Upload fertig

        total += len;

        if (total > MAX_SIZE) {
            Update.abort();
            apiError(req, 400, "Firmware too large");
            return ESP_OK;
        }

        if (Update.write((uint8_t*)buf, len) != (size_t)len) {
            Update.abort();
            apiError(req, 500, "OTA write failed");
            return ESP_OK;
        }
    }

    if (total < MIN_SIZE) {
        Update.abort();
        apiError(req, 400, "Firmware too small");
        return ESP_OK;
    }

    if (!Update.end(true)) {
        Update.abort();
        apiError(req, 500, "OTA end failed");
        return ESP_OK;
    }

    apiText(req, "OK");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    ESP.restart();
    return ESP_OK;
}

// ------------------------------------------------------------
// /api/toggle_taskmanager
// ------------------------------------------------------------
static esp_err_t api_toggle_taskmanager(httpd_req_t *req) {

    String arg = apiArg(req, "enabled");
    if (arg.isEmpty()) {
        apiError(req, 400, "Missing enabled");
        return ESP_OK;
    }

    bool en = (arg == "1");
    config.logTaskManager = en;
    config.save();

    Log(LOG_INFO, String("TaskManager logging set to ") + (en ? "ENABLED" : "DISABLED"));

    apiText(req, "OK");
    return ESP_OK;
}



static esp_err_t api_config(httpd_req_t *req) {

    String json = "{";
    json += "\"log_taskmanager\":" + String(config.logTaskManager ? "true" : "false");
    json += "}";

    apiJson(req, json);
    return ESP_OK;
}

static esp_err_t api_set_brightness(httpd_req_t *req) {

    String val = apiArg(req, "value");
    if (val.isEmpty()) {
        apiError(req, 400, "Missing value");
        return ESP_OK;
    }

    int b = val.toInt();
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    config.displayBrightness = b;
    config.saveDisplayBrightness();  

    display.setBrightness(b);

    Log(LOG_INFO, "Display brightness set to " + String(b));

    apiText(req, "OK");
    return ESP_OK;
}


// ------------------------------------------------------------
// Registrierung aller Service-Routen
// ------------------------------------------------------------
inline void registerServiceAPI() {

    httpd_uri_t r1 = { "/service",        HTTP_GET,  api_service_page, NULL };
    httpd_uri_t r2 = { "/api/restart",    HTTP_POST, api_restart,      NULL };
    httpd_uri_t r3 = { "/api/factoryreset", HTTP_POST, api_factoryreset, NULL };
    httpd_uri_t r4 = { "/api/wifireset",  HTTP_POST, api_wifireset,    NULL };
    httpd_uri_t r5 = { "/api/version",    HTTP_GET,  api_version,      NULL };
    httpd_uri_t r6 = { "/api/storageinfo",HTTP_GET,  api_storageinfo,  NULL };
    httpd_uri_t r7 = { "/api/ota",        HTTP_POST, api_ota,          NULL };
	httpd_uri_t r8 = { "/api/formatspiffs", HTTP_POST, api_formatspiffs, NULL };
	httpd_uri_t r9 = { "/api/toggle_taskmanager", HTTP_POST, api_toggle_taskmanager, NULL };
	httpd_uri_t r10 = { "/api/config", HTTP_GET, api_config, NULL };
	httpd_uri_t r11 = { "/api/set_brightness", HTTP_POST, api_set_brightness, NULL };


    httpd_register_uri_handler(server, &r1);
    httpd_register_uri_handler(server, &r2);
    httpd_register_uri_handler(server, &r3);
    httpd_register_uri_handler(server, &r4);
    httpd_register_uri_handler(server, &r5);
    httpd_register_uri_handler(server, &r6);
    httpd_register_uri_handler(server, &r7);
	httpd_register_uri_handler(server, &r8);
	httpd_register_uri_handler(server, &r9);
	httpd_register_uri_handler(server, &r10);
	httpd_register_uri_handler(server, &r11);
}
