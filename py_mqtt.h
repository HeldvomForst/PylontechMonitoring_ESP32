#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "config.h"

// -------------------------------------------------------------
// MQTT Message (Topic + Payload)
// -------------------------------------------------------------
struct MqttMessage {
    char topic[128];
    char payload[512];
    bool retain;
};

// -------------------------------------------------------------
// Discovery phases
// -------------------------------------------------------------
enum DiscoveryPhase {
    DISC_IDLE,
    DISC_STACK,
    DISC_PWR,
    DISC_BAT,
    DISC_STAT,
    DISC_DONE
};

extern DiscoveryPhase discoveryPhase;
extern size_t discPwrIndex;
extern size_t discBatModule;
extern size_t discStatModule;


// -------------------------------------------------------------
// Main MQTT class
// -------------------------------------------------------------
class PyMqtt {
public:
    void begin();
    void loop();
    bool connect();
    void disconnect();
    void reconnectTask();

    // Direktes Publish ohne Queue
    bool publishRaw(const String& topic, const String& payload, bool retain = false);

    // Status
    bool isConnected() { return mqttClient.connected(); }

    // Discovery publishers
    void publishDiscoveryStack();
    void publishDiscoveryPwrModule(int moduleIndex);
    void publishDiscoveryBatModule(int moduleIndex);
    void publishDiscoveryStatModule(int moduleIndex);

    // Discovery state machine
    void handleDiscoveryStep();

    // Data publishers
    void publishStack();
    void publishPwrSingle();
    void publishBatModule(int moduleIndex);
    void publishStat(int moduleIndex);

    void handlePwrBatch();

private:
    // Core MQTT
    WiFiClient   wifiClient;
    PubSubClient mqttClient = PubSubClient(wifiClient);

    bool enabled          = false;
    bool discoveryActive  = false;

    bool mqttOk           = false;
    bool reconnectNeeded  = false;
    unsigned long lastReconnectAttempt = 0;
    unsigned long lastMqttOk           = 0;

    unsigned long wifiConnectedSince   = 0;
    bool mqttStartAllowed              = false;

    // Reconnect timing
    unsigned long reconnectIntervalMs;
    const unsigned long reconnectIntervalMin = 5000;
    const unsigned long reconnectIntervalMax = 60000;

    // Discovery timing
    unsigned long discoveryLastSend = 0;
    unsigned long discoveryStartTime = 0;
    int discoveryMessagesPerSecond = 5;
    int discoveryDelayStartMs      = 2000;

    // Helpers
    String normalizeName(const String& in);
    int decimalsForUnit(const String& unit);
    String deviceClassForUnit(const String& unit);
    bool precisionDiffersFromDefault(const String& unit);
    int precisionForUnit(const String& unit);
    String computeValue(const String& raw, const FieldConfig& fc);
    void addDiscoveryMeta(JsonDocument& doc, const FieldConfig& fc);

    // PWR JSON builder
    String buildPwrJson(int moduleIndex);
    void sendPwrModule(int moduleIndex);

    void logPublishFailure(const String& topic);
};



// Global instance
extern PyMqtt py_mqtt;
