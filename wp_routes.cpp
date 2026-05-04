#include "wp_routes.h"
#include "wp_webserver.h"

// Neue API-Module
#include "web/dashboard_api.h"
#include "web/filemanager_api.h"
//#include "web/wp_runtime.h"
//#include "web/wp_console.h"
#include "web/runtime_api.h"
#include "web/console_api.h"
#include "web/wp_connect_api.h"
#include "web/pwr_api.h"
#include "web/bat_api.h"
#include "web/stat_api.h"

// System-Module
//#include "py_wifimanager.h"
//#include "py_uart.h"
//#include "py_scheduler.h"
//#include "py_mqtt.h"
//#include "py_log.h"

//#include <ArduinoJson.h>

//extern PyScheduler py_scheduler;
//extern PyUart py_uart;
//extern PyMqtt py_mqtt;
//extern bool discoveryPwrNeeded;
//extern bool discoveryBatNeeded;
//extern bool discoveryStatNeeded;

void registerRoutes() {

    registerDashboardAPI(server);
    registerFileManagerAPI(server);
    registerRuntimeAPI();
    registerConsoleAPI();
    registerPwrAPI();
    registerBatAPI();
    registerStatAPI();

    // Connect API
    server.on("/api/wifi",     HTTP_GET,  apiWifiGet);
    server.on("/api/wifi",     HTTP_POST, apiWifiPost);
    server.on("/api/wifi/scan",HTTP_GET,  apiWifiScan);

    server.on("/api/mqtt",     HTTP_GET,  apiMqttGet);
    server.on("/api/mqtt",     HTTP_POST, apiMqttPost);

    server.on("/api/time",     HTTP_GET,  apiTimeGet);
    server.on("/api/time",     HTTP_POST, apiTimePost);

    server.on("/api/network",  HTTP_GET,  apiNetworkGet);
    server.on("/api/network",  HTTP_POST, apiNetworkPost);

    // Root
    server.serveStatic("/", SPIFFS, "/index.html");
    server.serveStatic("/", SPIFFS, "/");

    //server.on("/runtime", HTTP_GET, handleRuntimePage);
    //server.on("/console", HTTP_GET, handleConsolePage);

    // Basevalue
    server.on("/api/pwr/base", HTTP_GET, handleApiPwrBase);
    server.on("/api/pwr/set",  HTTP_POST, handleApiPwrSet);

    // Celldata
    server.on("/api/bat/cells", HTTP_GET, handleApiBatCells);
    server.on("/api/bat/set",  HTTP_POST, handleApiBatSet);

    // Statistic
    server.on("/api/stat/values", HTTP_GET, handleApiStatValues);
    server.on("/api/stat/set",  HTTP_POST, handleApiStatSet);

}