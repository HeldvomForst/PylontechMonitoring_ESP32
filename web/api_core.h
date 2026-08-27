#pragma once
#include <esp_http_server.h>
#include <Arduino.h>

extern httpd_handle_t server;

// ------------------------------------------------------------
// Antwort senden
// ------------------------------------------------------------
inline void apiText(httpd_req_t *req, const String &txt) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, txt.c_str(), txt.length());
}

inline void apiJson(httpd_req_t *req, const String &json) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
}

inline void apiError(httpd_req_t *req, int code, const String &msg) {
    char status[8];
    snprintf(status, sizeof(status), "%d", code);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, msg.c_str(), msg.length());
}

// ------------------------------------------------------------
// Query-Parameter lesen
// ------------------------------------------------------------
inline bool apiHasArg(httpd_req_t *req, const char *name) {
    char buf[256];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK)
        return false;

    char val[128];
    return httpd_query_key_value(buf, name, val, sizeof(val)) == ESP_OK;
}

inline String apiArg(httpd_req_t *req, const char *name) {
    char buf[256];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK)
        return "";

    char val[128];
    if (httpd_query_key_value(buf, name, val, sizeof(val)) == ESP_OK)
        return String(val);

    return "";
}

// ------------------------------------------------------------
// Body lesen (für POST JSON)
// ------------------------------------------------------------
inline String apiGetBody(httpd_req_t *req) {
    int total = req->content_len;
    String body;
    body.reserve(total);

    char buf[512];
    while (total > 0) {
        int toRead = total > (int)sizeof(buf) ? (int)sizeof(buf) : total;
        int len = httpd_req_recv(req, buf, toRead);
        if (len <= 0) break;
        body.concat(String(buf, len));
        total -= len;
    }
    return body;
}
