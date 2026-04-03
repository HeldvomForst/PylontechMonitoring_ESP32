// =========================
// PylontechMonitoring (ESP32-S)
// Clean architecture with 2 tasks:
//   - Task 1 (Core 0): Real‑time pipeline (UART + Parser)
//   - Task 2 (Core 1): Non‑critical pipeline (Scheduler + MQTT + Webserver + WiFi)
// =========================

// ---- System Includes ----
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TimeLib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ---- Project Includes ----
#include "esp_log.h"
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
//  Global Data Structures
// =========================

// Global queue handle
QueueHandle_t mqttQueue;

// Global objects
PyUart py_uart;
extern PyScheduler py_scheduler;
extern PyMqtt py_mqtt;


// =========================
//  Task 1: Real‑Time Pipeline (Core 0)
//  UART → Parser → Queue
// =========================
void realtimeTask(void* parameter) {

    for (;;) {

        // 1) UART must be ready
        if (!py_uart.isReady()) {
            py_uart.begin(16, 17);
            vTaskDelay(100);
            continue;
        }

        // 2) If UART is busy, wait
        if (py_uart.isBusy()) {
            vTaskDelay(1);
            continue;
        }

        // 3) Check if scheduler has a command
        if (!py_scheduler.hasQueuedCommand()) {
            vTaskDelay(1);
            continue;
        }

        // 4) Pop next command
        String cmd = py_scheduler.popNextCommand();
        if (cmd.length() == 0) {
            vTaskDelay(1);
            continue;
        }

        Log(LOG_INFO, "Task1: executing command: " + cmd);

        // 5) Execute UART command (blocking allowed)
        bool ok = py_uart.sendCommand(cmd.c_str());

        if (!ok || !py_uart.hasFrame()) {
            Log(LOG_WARN, "Task1: UART failed for command: " + cmd);
            py_scheduler.lastCommandFinished = millis();
            vTaskDelay(1);
            continue;
        }

        // 6) Get raw frame
        String raw = py_uart.getFrame();

        // 7) Mark command finished
        py_scheduler.lastCommandFinished = millis();

        // 8) Small delay
        vTaskDelay(1);
    }
}

void noncriticalTask(void* parameter) {
    MqttMessage msg;

    for (;;) {

        // Scheduler (non‑critical)
        py_scheduler.loop();

        // Process MQTT queue (Task 1 → Task 2)
        while (xQueueReceive(mqttQueue, &msg, 0) == pdTRUE) {
            py_mqtt.publishRaw(msg.topic, msg.payload);
        }

        // MQTT internal loop
        py_mqtt.loop();

        // Webserver + WiFi + SystemManager
        WebServerModule_handle();
        WiFiManagerModule::loop();
        SystemManager::loop();

        // RAM Debug (nur wenn Debug aktiviert)
        if (config.logDebug) {
            static unsigned long lastRamLog = 0;
            unsigned long now = millis();

            // Nur alle 5 Sekunden loggen
            if (now - lastRamLog > 5000) {
                lastRamLog = now;

                multi_heap_info_t info;
                heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);

                Log(LOG_DEBUG,
                    String("RAM: free=") + info.total_free_bytes +
                    " largest=" + info.largest_free_block +
                    " min_free=" + info.minimum_free_bytes +
                    " alloc=" + info.total_allocated_bytes
                );
            }
        }

        vTaskDelay(1);
    }
}

// =========================
//  Setup
// =========================
void setup() {
    Serial.begin(115200);
    delay(100);

    Log(LOG_INFO, "System booting...");

    // Load configuration
    config.load();

    // Create MQTT queue
    mqttQueue = xQueueCreate(
        50,                      // number of buffered messages
        sizeof(MqttMessage)      // size of each element
    );

    if (mqttQueue == NULL) {
        Log(LOG_ERROR, "MQTT Queue could not be created!");
    }

    // UART + Scheduler
    py_uart.begin(16, 17);
    py_scheduler.begin(&py_uart);

    // Initial command
    py_scheduler.enqueue("pwr");
    Log(LOG_INFO, "Scheduler: initial CMD_PWR enqueued");

    // System Manager
    SystemManager::begin();

    // WiFi + OTA + NTP
    WiFiManagerModule::begin();

    // MQTT
    py_mqtt.begin();

    // Webserver
    WebServerModule_begin();

    // Webserver command callback
    WebServerModule_setCommandCallback([](const String &cmd){
        if (cmd == "pwr") {
            py_scheduler.enqueue("pwr");
            Log(LOG_INFO, "Web command: pwr");
            return String("OK");
        }
        return String("UNKNOWN");
    });

    // Start Task 1 (Real‑Time) on Core 0
    xTaskCreatePinnedToCore(
        realtimeTask,
        "RealTime Task",
        8192,
        NULL,
        2,          // higher priority
        NULL,
        0           // Core 0
    );

    // Start Task 2 (Non‑Critical) on Core 1
    xTaskCreatePinnedToCore(
        noncriticalTask,
        "NonCritical Task",
        8192,
        NULL,
        1,          // normal priority
        NULL,
        1           // Core 1
    );

    Log(LOG_INFO, "Setup complete");
}


// =========================
//  Arduino Loop (unused)
// =========================
void loop() {
    // Empty – all logic moved to FreeRTOS tasks
    vTaskDelay(1000);
}