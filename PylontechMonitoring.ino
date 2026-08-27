// =========================
// PylontechMonitoring (ESP32-S)
// Neue stabile Multi-Task-Architektur:
//   - realtimeTask (Core 1): UART-FSM + Scheduler (NICHT Parser!)
//   - parserTask   (Core 1): verarbeitet Frames aus frameQueue
//   - mqttTask     (Core 0): MQTT loop
//   - mqttPublishTask (Core 0): Publish-Queue
//   - webTask      (Core 0): Webserver
//   - wifiTask     (Core 0): WiFiManager + SystemManager
//   - displayTask  (Core 0): Display
//   - monitorTask  (Core 0): RAM-Log
// =========================

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TimeLib.h>
#include <SPIFFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esp_heap_caps.h"

#include "esp_log.h"
#include "config.h"
#include "py_wifimanager.h"
#include "wp_routes.h"
#include "py_systemmanager.h"
#include "py_uart.h"
#include "py_scheduler.h"
#include "py_parser.h"
#include "py_log.h"
#include "py_mqtt.h"
#include "py_display.h"

// Magic Header
const char OTA_MAGIC_HEADER[] = "PYLONTECH_FW_V1";

// =========================
// Queues / Handles
// =========================
QueueHandle_t frameQueue;      // UART → Parser

TaskHandle_t realtimeTaskHandle    = nullptr;
TaskHandle_t parserTaskHandle      = nullptr;
TaskHandle_t mqttTaskHandle        = nullptr;
TaskHandle_t mqttPublishTaskHandle = nullptr;
TaskHandle_t webTaskHandle         = nullptr;
TaskHandle_t wifiTaskHandle        = nullptr;
TaskHandle_t displayTaskHandle     = nullptr;
TaskHandle_t monitorTaskHandle     = nullptr;
TaskHandle_t systemTaskHandle      = nullptr;

// extern aus Modulen
PyUart      py_uart;
extern PyScheduler py_scheduler;
extern PyMqtt      py_mqtt;
extern PyDisplay   display;

// Parser-Flags/Buffer aus deinen Modulen (wie bisher)
extern bool  parserHasData;
extern float stackVoltAvg;
extern float stackCurrSum;
extern float stackTempMax;
extern float stackSocAvg;

extern bool      batParserHasData;
extern int       batParserModuleIndex;
//extern volatile bool batUseA;
//extern BatBuffer batA;
//extern BatBuffer batB;
//extern std::vector<BatData> lastParsedBatCells;

extern bool       statParserHasData;
extern int        statParserModuleIndex;
//extern volatile bool statUseA;
//extern StatBuffer statA;
//extern StatBuffer statB;

// CPU-Load-Monitor
volatile uint32_t idleCounterCore0 = 0;
volatile uint32_t idleCounterCore1 = 0;

volatile uint32_t lastIdleCore0 = 0;
volatile uint32_t lastIdleCore1 = 0;


extern "C" void vApplicationIdleHook(void) {
    if (xPortGetCoreID() == 0) {
        idleCounterCore0++;
    } else {
        idleCounterCore1++;
    }
}



// =========================
//  Task: realtimeTask (Core 1)
//  UART-FSM + Scheduler
//  KEIN PARSER, KEIN MQTT
// =========================
void realtimeTask(void* parameter) {
    for (;;) {
        // UART-FSM (sammelt Frames ein und legt sie intern in lastRawFrame ab,
        // pusht aber bereits in frameQueue – siehe neue py_uart.cpp)
        py_uart.loop();

        // Scheduler
        py_scheduler.loop();

        vTaskDelay(10);
    }
}

// =========================
//  Task: parserTask (Core 1)
//  verarbeitet Frames aus frameQueue
// =========================


// =========================
//  Task: mqttTask (Core 0)
//  nur MQTT loop()
// =========================
void mqttTask(void* parameter) {
    for (;;) {
        py_mqtt.loop();
        vTaskDelay(100);
    }
}


// =========================
//  Task: WiFi (Core 0)
// =========================
void wifiTask(void* parameter) {
    for (;;) {
        WiFiManagerModule::loop();
        vTaskDelay(100);
    }
}

// =========================
//  Task: Display (Core 0)
// =========================
void displayTask(void* parameter) {

    static bool resetDoneToday = false;

    for (;;) {

        // Zeit holen
        String now = config.getCurrentTimeString();  // "YYYY-MM-DD HH:MM:SS"

        // Prüfen ob 03:00:00 erreicht wurde
        if (now.endsWith("03:00:00")) {
            if (!resetDoneToday) {
                Log(LOG_WARN, "Display: nightly reset at 03:00");
                display.reset();
                resetDoneToday = true;
            }
        }

        // Reset-Sperre um Mitternacht zurücksetzen
        if (now.endsWith("00:00:00")) {
            resetDoneToday = false;
        }

        // Normale Display-Aktualisierung
        display.syncFromGlobals();
        display.syncHealth();
        display.loop();

        vTaskDelay(500);
    }
}



// =========================
//  Task: Monitor (Core 0)
// =========================
void monitorTask(void* parameter) {
    for (;;) {

        // =========================
        // RAM-Überwachung
        // =========================
        multi_heap_info_t dram;
        heap_caps_get_info(&dram, MALLOC_CAP_DEFAULT);

        size_t freeBytes    = dram.total_free_bytes;
        size_t largestBlock = dram.largest_free_block;

        if (largestBlock < 12000 || freeBytes < 30000) {
            Log(LOG_WARN,
                String("RAM LOW: free=") + freeBytes +
                " largest=" + largestBlock
            );
        }

        // =========================
        // CPU-Last pro Core
        // =========================
        uint32_t idle0 = idleCounterCore0;
        uint32_t idle1 = idleCounterCore1;

        uint32_t diff0 = idle0 - lastIdleCore0;
        uint32_t diff1 = idle1 - lastIdleCore1;

        lastIdleCore0 = idle0;
        lastIdleCore1 = idle1;

        float idlePercent0 = (diff0 / 50000.0f) * 100.0f;
        float idlePercent1 = (diff1 / 50000.0f) * 100.0f;

        if (idlePercent0 > 100.0f) idlePercent0 = 100.0f;
        if (idlePercent1 > 100.0f) idlePercent1 = 100.0f;

        float cpuLoad0 = 100.0f - idlePercent0;
        float cpuLoad1 = 100.0f - idlePercent1;

        //Log(LOG_INFO,
        //    "CPU Load Core0=" + String(cpuLoad0, 1) +
        //    "% Core1=" + String(cpuLoad1, 1) + "%"
        //);

        // =========================
        // TASK-MANAGER (Windows-Task-Manager für ESP32)
        // =========================
        char *taskBuffer = (char*) malloc(4096);
        if (taskBuffer) {
            memset(taskBuffer, 0, 4096);

            vTaskGetRunTimeStats(taskBuffer);

            if (config.logTaskManager) {
                Log(LOG_INFO, "=== Task Manager ===");
                Log(LOG_INFO, taskBuffer);
            }


            free(taskBuffer);
        } else {
            Log(LOG_ERROR, "TaskManager: malloc failed");
        }

        vTaskDelay(5000);
    }
}


void systemTask(void* parameter) {
    for (;;) {
        SystemManager::loop();
        vTaskDelay(200);
    }
}

void webTask(void* parameter) {
    for (;;) {
        // Webserver arbeitet event-basiert, daher nur kleine Pause
        vTaskDelay(50);
    }
}


// =========================
//  Setup
// =========================
void setup() {
    Serial.begin(115200);
    delay(100);

    Log(LOG_INFO, "System booting...");

    config.load();

    // Queues
    frameQueue = xQueueCreate(10, sizeof(UartFrame));

    if (!frameQueue) {
        Log(LOG_ERROR, "frameQueue creation failed!");
    }

    // UART + Scheduler
    py_uart.begin(16, 17);
    py_scheduler.begin(&py_uart);

    // Initial command
    py_scheduler.enqueue("pwr");

    // System
    SystemManager::begin();

    // WiFi
    WiFiManagerModule::begin();

    // MQTT
    py_mqtt.begin();

    // SPIFFS
    if (!SPIFFS.begin(false)) {
        Log(LOG_WARN, "SPIFFS mount failed, formatting...");
        SPIFFS.begin(true);
    } else {
        Log(LOG_INFO, "SPIFFS mounted");
    }

    // Webserver
    startHttpd();

    // Display
    display.begin();
    display.setBrightness(150);

    // =========================
    // Tasks starten
    // =========================

    xTaskCreatePinnedToCore(realtimeTask, "RT", 7168, NULL, 3, &realtimeTaskHandle, 1);
    xTaskCreatePinnedToCore(parserTask,   "Parser", 7168, NULL, 2, &parserTaskHandle, 1);

    xTaskCreatePinnedToCore(wifiTask,    "WiFi",    4096, NULL, 3, &wifiTaskHandle, 0);
    xTaskCreatePinnedToCore(mqttTask,    "MQTT",   12288, NULL, 1, &mqttTaskHandle, 0);
    xTaskCreatePinnedToCore(displayTask, "Display", 3072, NULL, 1, &displayTaskHandle, 0);
    xTaskCreatePinnedToCore(monitorTask, "Monitor", 3072, NULL, 1, &monitorTaskHandle, 0);
    xTaskCreatePinnedToCore(systemTask, "System", 4096, NULL, 1, &systemTaskHandle, 0);

    Log(LOG_INFO, "Setup complete");
    }

// =========================
//  Arduino Loop (unused)
// =========================
void loop() {
    vTaskDelay(1000);
}
