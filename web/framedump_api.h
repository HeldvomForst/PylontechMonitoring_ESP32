#pragma once
#include "api_core.h"
#include "../py_uart.h"

extern PyUart py_uart;

// ------------------------------------------------------------
// Handler
// ------------------------------------------------------------
static esp_err_t api_framedump(httpd_req_t *req) {
    apiText(req, py_uart.getLastRawFrame());
    return ESP_OK;
}

// ------------------------------------------------------------
// Registrierung
// ------------------------------------------------------------
inline void registerFramedumpApi() {

    httpd_uri_t r = {
        .uri      = "/api/framedump",
        .method   = HTTP_GET,
        .handler  = api_framedump,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &r);
}
