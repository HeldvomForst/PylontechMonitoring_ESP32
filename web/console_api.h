#pragma once
#include <WebServer.h>
#include "../py_uart.h"
#include "../py_scheduler.h"

extern WebServer server;
extern PyUart py_uart;
extern PyScheduler py_scheduler;

inline void registerConsoleAPI() {

    // /req?code=...
    server.on("/req", HTTP_GET, []() {
        String cmd = server.arg("code");
        py_scheduler.enqueue(cmd);
        server.send(200, "text/plain", "OK");
    });

    // /api/lastframe
    server.on("/api/lastframe", HTTP_GET, []() {

        unsigned long start = millis();
        while (!py_uart.hasFrame()) {
            if (millis() - start > 2000) {
                server.send(200, "text/plain", "TIMEOUT");
                return;
            }
            vTaskDelay(1);
        }

        server.send(200, "text/plain", py_uart.getFrame());
    });
}
