#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "py_parser_pwr.h"

#define MQTT_MAX_PACKET_SIZE 2048
#include <PubSubClient.h>

// Last MQTT values for web UI
extern std::map<String, String> lastMqttValues;

// Discovery / parser state flags
extern bool discoverySent;
extern bool parserHasData;
extern bool newParserData;


class PyMqtt {
public:
    void begin();
    void loop();

    // JSON publish
    void publishStack();
    void publishBat(int index, const BatteryModule& mod);

    void logPublishFailure(const String& topic);
    void buildDiscovery(JsonDocument& doc,
                        const FieldConfig& fc,
                        const String& name,
                        const String& stateTopic,
                        const String& uid,
                        const String& sensorName,
                        const String& deviceId,
                        const String& deviceName);

    bool isConnected() { return mqttClient.connected(); }

private:
    WiFiClient   wifiClient;
    PubSubClient mqttClient = PubSubClient(wifiClient);

    bool enabled = false;

    bool connect();

    String computeValue(const String& raw, const FieldConfig& fc);

    // Discovery
    void publishDiscoveryStack();
    void publishDiscoveryBat();
};

extern PyMqtt py_mqtt;