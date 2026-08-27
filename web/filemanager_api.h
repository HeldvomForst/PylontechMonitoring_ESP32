#pragma once
#include "api_core.h"
#include <SPIFFS.h>
#include "../py_log.h"      // dein Logging
#include "filemanager.h"    // enthält FILEMANAGER_PAGE

//
// /filemanager → HTML-Seite
//
static esp_err_t api_filemanager_page(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, FILEMANAGER_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

//
// JSON-Escape für Dateinamen
//
static String jsonEscape(const char *s) {
    String out;
    while (*s) {
        char c = *s++;
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((uint8_t)c < 0x20) {
                    char buf[7];
                    sprintf(buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

//
// /fm/list
//
static esp_err_t api_fm_list(httpd_req_t *req) {

    Log(LOG_DEBUG, "FM: LIST");

    File root = SPIFFS.open("/");
    File f = root.openNextFile();

    String out = "[";
    bool first = true;

    while (f) {
        if (!first) out += ",";
        first = false;

        out += "{\"name\":\"";
        out += jsonEscape(f.name());
        out += "\",\"size\":";
        out += f.size();
        out += "}";

        f = root.openNextFile();
    }

    out += "]";

    apiJson(req, out);
    return ESP_OK;
}

//
// /fm/delete
//
static esp_err_t api_fm_delete(httpd_req_t *req) {

    String file = apiArg(req, "file");
    if (file.isEmpty()) {
        apiError(req, 400, "Missing file");
        return ESP_OK;
    }

    String path = "/" + file;
    Log(LOG_INFO, "FM: DELETE " + path);

    if (!SPIFFS.exists(path)) {
        apiError(req, 404, "File not found");
        return ESP_OK;
    }

    SPIFFS.remove(path);
    apiText(req, "OK");
    return ESP_OK;
}

//
// /fm/upload  – wie OTA: kompletten Body lesen, roh schreiben, mit Debug-Log
//
static esp_err_t api_fm_upload(httpd_req_t *req)
{
    String file = apiArg(req, "file");
    if (file.isEmpty()) {
        apiError(req, 400, "Missing file");
        return ESP_OK;
    }

    String path = "/" + file;
    Log(LOG_INFO, "FM: UPLOAD start file=" + path);

    File f = SPIFFS.open(path, "w");
    if (!f) {
        Log(LOG_ERROR, "FM: Cannot open for write: " + path);
        apiError(req, 500, "Cannot open file");
        return ESP_OK;
    }

    char buf[1024];
    size_t total = 0;

    while (true) {
        int len = httpd_req_recv(req, buf, sizeof(buf));

        if (len < 0) {
            Log(LOG_ERROR, "FM: recv error len=" + String(len) + " total=" + String(total));
            f.close();
            apiError(req, 500, "Upload recv error");
            return ESP_OK;
        }

        if (len == 0) {
            Log(LOG_DEBUG, "FM: recv finished total=" + String(total));
            break;
        }

        f.write((uint8_t*)buf, len);
        total += len;

        Log(LOG_DEBUG, "FM: recv len=" + String(len) + " total=" + String(total));
    }

    f.close();

    Log(LOG_INFO, "FM: UPLOAD done file=" + path + " size=" + String(total));

    apiText(req, "OK");
    return ESP_OK;
}

//
// /fm/download
//
static esp_err_t api_fm_download(httpd_req_t *req) {

    String file = apiArg(req, "file");
    if (file.isEmpty()) {
        apiError(req, 400, "Missing file");
        return ESP_OK;
    }

    String path = "/" + file;
    Log(LOG_INFO, "FM: DOWNLOAD " + path);

    if (!SPIFFS.exists(path)) {
        apiError(req, 404, "File not found");
        return ESP_OK;
    }

    File f = SPIFFS.open(path, "r");
    if (!f) {
        Log(LOG_ERROR, "FM: Cannot open for read: " + path);
        apiError(req, 500, "Cannot open file");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/octet-stream");

    uint8_t buf[1024];
    size_t total = 0;
    while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        if (n == 0) break;
        httpd_resp_send_chunk(req, (const char*)buf, n);
        total += n;
    }

    Log(LOG_INFO, "FM: DOWNLOAD done size=" + String(total));

    httpd_resp_send_chunk(req, NULL, 0);
    f.close();
    return ESP_OK;
}

//
// Registrierung
//
inline void registerFilemanagerAPI() {

    httpd_uri_t r1 = { "/filemanager", HTTP_GET,  api_filemanager_page, NULL };
    httpd_uri_t r2 = { "/fm/list",     HTTP_GET,  api_fm_list,          NULL };
    httpd_uri_t r3 = { "/fm/delete",   HTTP_GET,  api_fm_delete,        NULL };
    httpd_uri_t r4 = { "/fm/upload",   HTTP_POST, api_fm_upload,        NULL };
    httpd_uri_t r5 = { "/fm/download", HTTP_GET,  api_fm_download,      NULL };

    httpd_register_uri_handler(server, &r1);
    httpd_register_uri_handler(server, &r2);
    httpd_register_uri_handler(server, &r3);
    httpd_register_uri_handler(server, &r4);
    httpd_register_uri_handler(server, &r5);
}
