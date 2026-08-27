#include "py_mqtt.h"
#include "py_log.h"
#include "config.h"
#include "py_display.h"
#include <WiFi.h>

// ---------------------------------------------------------
// External parser flags and global data
// ---------------------------------------------------------
extern QueueHandle_t mqttQueue;

// PWR parser flags
extern bool parserHasData;
extern volatile bool pwrFrameReady;
extern int pwrCurrentModule;
extern int pwrTotalModules;

// Stack aggregate values
extern float stackVoltAvg;
extern float stackCurrSum;
extern float stackTempMax;
extern int   stackBatCount;

// BAT parser flags
extern bool batParserHasData;
extern int  batParserModuleIndex;

// STAT parser flags
extern bool statParserHasData;
extern int  statParserModuleIndex;

// Discovery flags
extern bool discoveryPwrNeeded;
extern bool discoveryBatNeeded;
extern bool discoveryStatNeeded;

// Display
extern PyDisplay display;

// ---------------------------------------------------------
// Discovery State (global, not static)
// ---------------------------------------------------------
DiscoveryPhase discoveryPhase  = DISC_IDLE;
size_t         discPwrIndex    = 0;
size_t         discBatModule   = 0;
size_t         discStatModule  = 0;

// ---------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------
static void logInfo(const String& msg)  { Log(LOG_INFO,  msg); }
static void logWarn(const String& msg)  { Log(LOG_WARN,  msg); }
static void logError(const String& msg) { Log(LOG_ERROR, msg); }
static void logDebug(const String& msg) { Log(LOG_DEBUG, msg); }

// ---------------------------------------------------------
// Global instance
// ---------------------------------------------------------
PyMqtt py_mqtt;

// ---------------------------------------------------------
// Handle PWR batch publishing (module-by-module)
// ---------------------------------------------------------
void PyMqtt::handlePwrBatch() {
    if (!pwrFrameReady) return;

    static unsigned long lastModuleSent = 0;
    const unsigned long MODULE_INTERVAL_MS = 10;

    unsigned long now = millis();

    if (now - lastModuleSent < MODULE_INTERVAL_MS)
        return;

    if (pwrCurrentModule >= pwrTotalModules) {
        pwrFrameReady = false;
        return;
    }

    sendPwrModule(pwrCurrentModule);
    pwrCurrentModule++;

    lastModuleSent = now;
}

// ---------------------------------------------------------
// BEGIN
// ---------------------------------------------------------
void PyMqtt::begin() {
    enabled = config.mqtt.enabled;

    if (!enabled) {
        logInfo("MQTT disabled in configuration");
        return;
    }

    mqttClient.setServer(config.mqtt.server.c_str(), config.mqtt.port);
    mqttClient.setBufferSize(1024);
    mqttClient.setKeepAlive(60);   // NEU: mehr Luft für große Publishes

    mqttOk             = false;
    reconnectNeeded    = true;
    wifiConnectedSince = 0;
    mqttStartAllowed   = false;

    reconnectIntervalMs = reconnectIntervalMin;

    discoveryPwrNeeded = true;

    logInfo("MQTT: BufferSize set to 1024 bytes, KeepAlive=60s");
}


// ---------------------------------------------------------
// CONNECT
// ---------------------------------------------------------
bool PyMqtt::connect() {
    if (!enabled) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    if (wifiConnectedSince == 0)
        wifiConnectedSince = millis();

    if (millis() - wifiConnectedSince < 10000)
        return false;

    String clientId = "PylontechMonitor-" + String((uint32_t)ESP.getEfuseMac());

    bool ok = mqttClient.connect(
        clientId.c_str(),
        config.mqtt.user.c_str(),
        config.mqtt.pass.c_str()
    );

    if (ok) {
        logInfo("MQTT connected as " + clientId);
        display.updateMqtt(true, config.mqtt.server);

        mqttOk          = true;
        reconnectNeeded = false;
        lastMqttOk      = millis();

        reconnectIntervalMs = reconnectIntervalMin;
    } else {
        logWarn("MQTT connection failed");
        display.updateMqtt(false, "---");

        mqttOk = false;
        reconnectIntervalMs = std::min(reconnectIntervalMs * 2, reconnectIntervalMax);
    }

    return ok;
}

// ---------------------------------------------------------
// DISCONNECT
// ---------------------------------------------------------
void PyMqtt::disconnect() {
    if (!enabled) return;

    mqttClient.disconnect();
    mqttOk          = false;
    reconnectNeeded = true;
}

// ---------------------------------------------------------
// RECONNECT TASK
// ---------------------------------------------------------
void PyMqtt::reconnectTask() {
    if (!enabled) return;

    unsigned long now = millis();

    if (mqttOk && mqttClient.connected())
        return;

    if (WiFi.status() != WL_CONNECTED)
        return;

    if (now - lastReconnectAttempt < reconnectIntervalMs)
        return;

    lastReconnectAttempt = now;
    logInfo("MQTT: reconnect attempt...");
    connect();
}


// ---------------------------------------------------------
// RAW PUBLISH
// ---------------------------------------------------------
bool PyMqtt::publishRaw(const String& topic, const String& payload, bool retain) {
    if (!enabled) return false;

    if (!mqttClient.connected()) {
        logWarn("MQTT: publishRaw() but not connected, topic=" + topic);
        mqttOk          = false;
        reconnectNeeded = true;
        return false;
    }

    bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), retain);
    if (!ok) {
        logPublishFailure(topic);
        // NEU: Verbindung nicht sofort als tot markieren
        // mqttOk          = false;
        // reconnectNeeded = true;
        return false;
    }

    lastMqttOk = millis();
    return true;
}



// ---------------------------------------------------------
// MAIN MQTT LOOP
// ---------------------------------------------------------
void PyMqtt::loop() {
    if (!enabled) return;

    unsigned long now = millis();

    // WiFi not connected → stop MQTT
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnectedSince = 0;
        mqttStartAllowed   = false;
        mqttOk             = false;
        mqttClient.disconnect();
        return;
    }

    // WiFi just connected
    if (wifiConnectedSince == 0) {
        wifiConnectedSince = now;
        return;
    }

    // WiFi must be stable for 15 seconds
    if (!mqttStartAllowed && (now - wifiConnectedSince < 15000)) {
        return;
    }

    mqttStartAllowed = true;

    // Reconnect logic
    if (!mqttOk || !mqttClient.connected())
        reconnectNeeded = true;

    if (reconnectNeeded)
        reconnectTask();

    if (!mqttOk || !mqttClient.connected())
        return;

    mqttClient.loop();

    // ---------------------------------------------------------
    // DISCOVERY TRIGGER
    // ---------------------------------------------------------
    if (!discoveryActive) {
        if (discoveryPwrNeeded) {
            discoveryActive    = true;
            discoveryPhase     = DISC_STACK;
            discPwrIndex       = 0;
            discoveryPwrNeeded = false;
            discoveryStartTime = millis();
        }
        else if (discoveryBatNeeded) {
            discoveryActive     = true;
            discoveryPhase      = DISC_BAT;
            discBatModule       = 0;
            discoveryBatNeeded  = false;
            discoveryStartTime = millis();
        }
        else if (discoveryStatNeeded) {
            discoveryActive      = true;
            discoveryPhase       = DISC_STAT;
            discStatModule       = 0;
            discoveryStatNeeded  = false;
            discoveryStartTime = millis();
        }
    }

    // ---------------------------------------------------------
    // DISCOVERY EXECUTION (NEW: no buffers)
    // ---------------------------------------------------------
    if (discoveryActive) {
        handleDiscoveryStep();
    }

    // ---------------------------------------------------------
    // STACK PUBLISH (PWR parser finished)
    // ---------------------------------------------------------
    if (parserHasData) {
        publishStack();
        parserHasData = false;
    }

    // ---------------------------------------------------------
    // PWR MODULE BATCH PUBLISH
    // ---------------------------------------------------------
    handlePwrBatch();

    // ---------------------------------------------------------
    // BAT MODULE
    // ---------------------------------------------------------
    if (batParserHasData) {
        publishBatModule(batParserModuleIndex);
        batParserHasData = false;
    }

    // ---------------------------------------------------------
    // STAT MODULE
    // ---------------------------------------------------------
    if (statParserHasData) {
        publishStat(statParserModuleIndex);
        statParserHasData = false;
    }
}

// ---------------------------------------------------------
// LOG PUBLISH FAILURE
// ---------------------------------------------------------
void PyMqtt::logPublishFailure(const String& topic) {
    logWarn("MQTT: publish failed for topic: " + topic);
}
