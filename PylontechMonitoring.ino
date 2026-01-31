// =========================
// PylontechMonitoring (ESP32-S)
// =========================

// ---- System Includes ----
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TimeLib.h>
#include "esp_log.h"

// ---- Project Includes ----
#include "config.h"
#include "py_wifimanager.h"
#include "wp_webserver.h"
#include "py_systemmanager.h"
#include "py_uart.h"
#include "py_scheduler.h"
#include "py_parser_pwr.h"
#include "py_log.h"
#include "py_mqtt.h"
#include "web/wp_log.h"

// =========================
//  Logging
// =========================


// =========================
//  Global Objects
// =========================

PyUart py_uart;
extern PyScheduler py_scheduler;
extern PyMqtt py_mqtt;

// =========================
//  Setup
// =========================

void setup() {
    Serial.begin(115200);
    delay(100);

    Log(LOG_INFO, "System booting...");

    // *** WICHTIG: Config laden ***
    config.load();

    // UART + Scheduler
    py_uart.begin(16, 17);
    py_scheduler.begin(&py_uart);

    py_scheduler.enqueue(CMD_PWR);
    Log(LOG_INFO, "Scheduler: initial CMD_PWR enqueued");

    // System Manager
    SystemManager::begin();

    SystemManager::onAPTemporary = [](){
        Log(LOG_WARN, "Starting temporary AP");
        WiFiManagerModule::startTemporaryAP(15 * 60 * 1000);
    };

    SystemManager::onWiFiReset = [](){
        Log(LOG_WARN, "WiFi reset requested");
        WiFiManagerModule::resetWiFi();
    };

    SystemManager::onFactoryReset = [](){
        Log(LOG_ERROR, "Factory reset triggered");
        ESP.restart();
    };

    // WiFi + OTA + NTP
    WiFiManagerModule::begin();

    // MQTT
    py_mqtt.begin();

    // Webserver
    WebServerModule_begin();

    // Webserver Command Callback
    WebServerModule_setCommandCallback([](const String &cmd){
        if (cmd == "pwr") {
            py_scheduler.enqueue(CMD_PWR);
            Log(LOG_INFO, "Web command: pwr");
            return String("OK");
        }
        return String("UNKNOWN");
    });

    Log(LOG_INFO, "Setup complete");
}


// =========================
//  Loop
// =========================

void loop() {
    SystemManager::loop();
    WiFiManagerModule::loop();

    py_uart.loop();
    py_scheduler.loop();
    py_mqtt.loop();

    WebServerModule_handle();
}