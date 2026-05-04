#pragma once
#include <WebServer.h>
#include "../py_log.h"
#include "../config.h"

extern WebServer server;

inline void registerRuntimeAPI() {

    // /api/log
    server.on("/api/log", HTTP_GET, []() {
        server.send(200, "text/plain", WebLogGet());
    });

    // /api/log/level (GET)
    server.on("/api/log/level", HTTP_GET, []() {
        DynamicJsonDocument doc(128);
        doc["info"]  = config.logInfo;
        doc["warn"]  = config.logWarn;
        doc["error"] = config.logError;
        doc["debug"] = config.logDebug;

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    // /api/log/level (POST)
    server.on("/api/log/level", HTTP_POST, []() {

        if (!server.hasArg("plain")) {
            server.send(400, "text/plain", "Missing body");
            return;
        }

        DynamicJsonDocument req(128);
        if (deserializeJson(req, server.arg("plain"))) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }

        config.logInfo  = req["info"]  | true;
        config.logWarn  = req["warn"]  | true;
        config.logError = req["error"] | true;
        config.logDebug = req["debug"] | false;

        config.save();
        server.send(200, "text/plain", "Log level updated");
    });
}
