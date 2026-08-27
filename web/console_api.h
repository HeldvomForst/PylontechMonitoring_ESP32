#pragma once

#include "api_core.h"

#include "../py_uart.h"
#include "../py_scheduler.h"
#include "../py_log.h"

extern PyUart py_uart;
extern PyScheduler py_scheduler;

// Console ticket state comes from py_uart.cpp
extern volatile int consoleTicket;
extern volatile int consoleTicketFrameReady;
extern String consolePendingCommand;

// ============================================================
// URL-DECODE helper
// ============================================================
static String urlDecode(const String& s) {
    String out;
    out.reserve(s.length());
    for (int i = 0; i < s.length(); i++) {
        if (s[i] == '%' && i + 2 < s.length()) {
            char hex[3] = { s[i+1], s[i+2], '\0' };
            out += char(strtol(hex, NULL, 16));
            i += 2;
        }
        else if (s[i] == '+') {
            out += ' ';
        }
        else {
            out += s[i];
        }
    }
    return out;
}

// ============================================================
// /req – send console command
// ============================================================
static esp_err_t api_console_req(httpd_req_t *req) {

    Log(LOG_DEBUG, "ConsoleAPI: /req called");

    String code = apiArg(req, "code");
    if (code.isEmpty()) {
        Log(LOG_WARN, "ConsoleAPI: missing code");
        apiError(req, 400, "Missing code");
        return ESP_OK;
    }

    code = urlDecode(code);

    // Create ticket for this console request
    consoleTicket++;

    Log(LOG_DEBUG, "ConsoleAPI: new ticket=" + String(consoleTicket) +
                   " pendingCommand='" + code + "'");

    consolePendingCommand = code;

    // Enqueue via scheduler with console prefix
    py_scheduler.enqueue("console:" + code);

    Log(LOG_DEBUG, "ConsoleAPI: command enqueued → console:" + code);

    // Return ticket to the website
    Log(LOG_DEBUG, "ConsoleAPI: returning ticket=" + String(consoleTicket));
    apiText(req, String(consoleTicket));
    return ESP_OK;
}

// ============================================================
// /api/lastframe – wait for frame belonging to ticket
// ============================================================
static esp_err_t api_console_lastframe(httpd_req_t *req) {

    int ticket = apiArg(req, "ticket").toInt();

    Log(LOG_DEBUG, "ConsoleAPI: /api/lastframe called for ticket=" + String(ticket));

    unsigned long start = millis();

    while (millis() - start < 2000) {

        if (consoleTicketFrameReady == ticket) {

            Log(LOG_DEBUG, "ConsoleAPI: frame ready for ticket=" + String(ticket));
            Log(LOG_DEBUG, "ConsoleAPI: returning lastRawFrame length=" +
                           String(py_uart.getLastRawFrame().length()));

            apiText(req, py_uart.getLastRawFrame());
            return ESP_OK;
        }

        vTaskDelay(10);
    }

    Log(LOG_WARN, "ConsoleAPI: timeout waiting for frame (ticket=" + String(ticket) + ")");
    apiError(req, 408, "Timeout waiting for frame");
    return ESP_OK;
}

// ============================================================
// Registration
// ============================================================
inline void registerConsoleAPI() {

    httpd_uri_t r1 = {
        .uri      = "/req",
        .method   = HTTP_GET,
        .handler  = api_console_req,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &r1);

    httpd_uri_t r2 = {
        .uri      = "/api/lastframe",
        .method   = HTTP_GET,
        .handler  = api_console_lastframe,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &r2);

    Log(LOG_INFO, "ConsoleAPI: registered");
}
