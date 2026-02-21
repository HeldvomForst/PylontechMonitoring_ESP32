#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "py_parser_pwr.h"
#include "py_parser_bat.h"
#include "py_parser_stat.h"

#define MQTT_MAX_PACKET_SIZE 2048
#include <PubSubClient.h>

// Last MQTT values for web UI
extern std::map<String, String> lastMqttValues;
extern BatData  lastParsedBat;
extern StatData lastParsedStat;

// Discovery / parser state flags
extern bool discoverySent;
extern bool parserHasData;
extern bool newParserData;
extern bool statParserHasData;
extern int  statParserModuleIndex;

class PyMqtt {
public:
    void begin();
    void loop();
    void resetDiscovery();

    // JSON publish
    void publishStack();
    void publishBat(int index, const BatteryModule& mod);
    void publishBatCells(int cellIndex);   // BAT-Parser (Cell-Daten)
    void publishStat(int moduleIndex);      // STAT-Parser

    void logPublishFailure(const String& topic);
    void buildDiscovery(JsonDocument& doc,
                        const FieldConfig& fc,
                        const String& name,
                        const String& stateTopic,
                        const String& uid,
                        const String& sensorName,
                        const String& deviceId,
                        const String& deviceName);
    
    void publishDiscoveryBatCells(int moduleIndex);
    void publishDiscoveryStat(int moduleIndex);


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