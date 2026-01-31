#include "wp_webserver.h"
#include "wp_routes.h"
#include "web/wp_settings.h"


WebServer server(80);

#include "py_log.h"
extern String webLog;

static void handleApiLog() {
  server.send(200, "text/plain", webLog);
}

// interne Callback‑Pointer
CmdCallback g_cmdCb = nullptr;
StatusCallback g_statusCb = nullptr;

void WebServerModule_begin() {
    registerRoutes();
    server.begin();
}

void WebServerModule_handle() {
    server.handleClient();
}

void WebServerModule_setCommandCallback(CmdCallback cb) {
    g_cmdCb = cb;
}

void WebServerModule_setStatusCallback(StatusCallback cb) {
    g_statusCb = cb;
}