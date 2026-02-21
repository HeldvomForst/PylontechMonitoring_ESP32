#include "py_mqtt.h"
#include "py_log.h"
#include "config.h"
#include "py_parser_pwr.h"
#include <WiFi.h>
#include <map>

#include <set>

extern bool parserHasData;
extern bool newParserData;

extern bool statParserHasData;
extern int  statParserModuleIndex;

extern bool batParserHasData;
extern int  batParserModuleIndex;

std::set<int> discoveredPwr;
std::set<int> discoveredBat;
std::set<int> discoveredStat;
void PyMqtt::resetDiscovery() {
    discoveredPwr.clear();
    discoveredBat.clear();
    discoveredStat.clear();

    discoverySent = false;

    parserHasData     = true;
    newParserData     = true;
    batParserHasData  = true;
    statParserHasData = true;

    Log(LOG_INFO, "MQTT: Discovery reset triggered");
}
std::map<String, String> lastMqttValues;

static unsigned long lastReconnectAttempt = 0;
static unsigned long wifiConnectedSince   = 0;

// Discovery / parser flags
bool discoverySent = false;
bool parserHasData = false;
bool newParserData = false;
bool statParserHasData = false;
int  statParserModuleIndex = 0;
bool batParserHasData = false;
int  batParserModuleIndex = 0;



PyMqtt py_mqtt;

// From parser
extern BatteryStack lastParsedStack;
extern std::vector<BatteryModule> lastParsedModules;

// ----------------------------------------------------
// Helper: make MQTT-safe ID / key
// ----------------------------------------------------


static String make_safe_id(const String& s) {
    String r = s;
    r.toLowerCase();
    for (int i = 0; i < r.length(); i++) {
        char c = r[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            r[i] = '_';
    }
    while (r.startsWith("_")) r.remove(0, 1);
    while (r.endsWith("_"))  r.remove(r.length() - 1);
    if (r.length() == 0) r = "field";
    return r;
}

// ----------------------------------------------------
// Raw values: stack (fixed, computed on ESP)
// ----------------------------------------------------
static String getStackRawValue(const String& name) {
    if (name == "StackVoltAvg") return String(lastParsedStack.avgVoltage_mV);
    if (name == "StackCurrSum") return String(lastParsedStack.totalCurrent_mA);
    if (name == "StackTempMax") return String(lastParsedStack.temperature);
    if (name == "BatteryCount") return String(lastParsedStack.batteryCount);
    return "";
}

// ----------------------------------------------------
// Raw values: module (dynamic, from parser field map)
// ----------------------------------------------------
static String getModuleRawValue(int index, const String& name) {
    const BatteryModule* pm = nullptr;
    for (const auto& m : lastParsedModules) {
        if (m.index == index) {
            pm = &m;
            break;
        }
    }
    if (!pm || !pm->present) return "";

    auto it = pm->fields.find(name);
    if (it == pm->fields.end()) return "";

    return it->second;
}

// ----------------------------------------------------
// Value computation (factor / text / date)
// ----------------------------------------------------
String PyMqtt::computeValue(const String& raw, const FieldConfig& fc) {
    // Text / Date → keine Berechnung
    if (fc.factor == "text" || fc.factor == "date") {
        return raw;
    }

    // Faktor anwenden
    float factor = fc.factor.toFloat();
    float valueC = raw.toFloat() * factor;

    // Temperatur in Fahrenheit umrechnen
    if (fc.unit == "°C" && config.battery.useFahrenheit) {
        float valueF = valueC * 1.8f + 32.0f;
        return String(valueF, 3);
    }

    // Standardwert (Volt, Ampere, SOC, …)
    return String(valueC, 3);
}
// ----------------------------------------------------
// Error logging for publish failures
// ----------------------------------------------------
void PyMqtt::logPublishFailure(const String& topic) {
    Log(LOG_WARN, "MQTT publish failed: " + topic);
    Log(LOG_WARN, "MQTT state = " + String(mqttClient.state()));
    Log(LOG_WARN, "WiFi state = " + String(WiFi.status()));
}

// ----------------------------------------------------
// CENTRAL DISCOVERY FUNCTION
// ----------------------------------------------------
void PyMqtt::buildDiscovery(JsonDocument& doc,
                            const FieldConfig& fc,
                            const String& name,
                            const String& stateTopic,
                            const String& uid,
                            const String& sensorName,
                            const String& deviceId,
                            const String& deviceName)
{
    doc.clear();

    String safe = make_safe_id(name);

    doc["name"]           = sensorName;
    doc["state_topic"]    = stateTopic;
    doc["unique_id"]      = uid;
    doc["value_template"] = "{{ value_json." + safe + " }}";

    // device_class
    if (fc.factor == "date") {
        doc["device_class"] = "timestamp";
    }
    else if (fc.factor != "text") {
        if (fc.unit == "V") {
            doc["device_class"] = "voltage";
        }
        else if (fc.unit == "A") {
            doc["device_class"] = "current";
        }
        else if (fc.unit == "°C") {
            doc["device_class"] = "temperature";
        }
        else if (fc.unit == "%") {
            doc["device_class"] = "battery";
        }
    }

    // unit_of_measurement (nur für echte Messwerte)
    if (fc.factor != "text" && fc.factor != "date" && fc.unit.length()) {
        if (fc.unit == "°C" && config.battery.useFahrenheit) {
            doc["unit_of_measurement"] = "°F";
        } else {
            doc["unit_of_measurement"] = fc.unit;
        }
    }


    // state_class (nur für echte Messwerte)
    if (fc.factor != "text" && fc.factor != "date") {
        doc["state_class"] = "measurement";
    }

    // device block
    JsonObject device = doc.createNestedObject("device");
    JsonArray ids = device.createNestedArray("identifiers");
    ids.add(deviceId);
    device["name"]         = deviceName;
    device["manufacturer"] = "Pylontech ESP32";
    device["model"]        = "Battery Module";
}

// ----------------------------------------------------
// Discovery: stack sensors
// ----------------------------------------------------
void PyMqtt::publishDiscoveryStack() {
    String prefix     = config.mqtt.prefix;
    String subtopic   = config.mqtt.topicStack;   // Stack-Subtopic bleibt wie er ist

    String devId      = prefix + "_" + subtopic;
    String devName    = prefix + " " + subtopic;
    String stateTopic = prefix + "/" + subtopic;

    DynamicJsonDocument doc(512);

    for (auto &kv : config.battery.fields) {
        const String &name = kv.first;
        const FieldConfig &fc = kv.second;

        if (!(name.startsWith("Stack") || name == "BatteryCount"))
            continue;

        if (!fc.send) continue;

        String uid        = devId + "_" + make_safe_id(name);
        String sensorName = subtopic + " " + fc.label;

        buildDiscovery(doc, fc, name, stateTopic, uid, sensorName, devId, devName);

        String out;
        serializeJson(doc, out);
        String topic = "homeassistant/sensor/" + uid + "/config";
        mqttClient.publish(topic.c_str(), out.c_str(), true);
        mqttClient.loop();
        delay(100);
    }
}

void PyMqtt::publishDiscoveryBat() {
    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicPwr;   // Modul-Subtopic (ehemals PWR)

    DynamicJsonDocument doc(512);

    for (const auto& mod : lastParsedModules) {
        if (!mod.present) continue;

        int idx = mod.index;

        String devId      = prefix + "_" + subtopic + String(idx);
        String devName    = prefix + " " + subtopic + " " + String(idx);
        String stateTopic = prefix + "/" + subtopic + "/" + String(idx);

        for (auto &kv : config.battery.fields) {
            const String &name = kv.first;
            const FieldConfig &fc = kv.second;

            if (name.startsWith("Stack") || name == "BatteryCount")
                continue;

            if (!fc.send) continue;

            String uid        = devId + "_" + make_safe_id(name + "_" + String(idx));
            String sensorName = subtopic + " " + String(idx) + " " + fc.label;

            buildDiscovery(doc, fc, name, stateTopic, uid, sensorName, devId, devName);

            String out;
            serializeJson(doc, out);
            String topic = "homeassistant/sensor/" + uid + "/config";
            mqttClient.publish(topic.c_str(), out.c_str(), true);
            mqttClient.loop();
            delay(100);
        }
    }
}
void PyMqtt::publishDiscoveryBatCells(int moduleIndex) {
    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicBat;

    DynamicJsonDocument doc(512);

    // EIN Gerät pro Modul für ALLE Zellen
    String devId   = prefix + "_bat_" + String(moduleIndex);
    String devName = prefix + " BAT " + String(moduleIndex);

    for (const auto& cell : lastParsedBatCells) {

        // State Topic pro Zelle
        String stateTopic = prefix + "/" + subtopic + "/" +
                            String(moduleIndex) + "/cell" + String(cell.cellIndex);

        for (auto &f : cell.fields) {

            if (!config.battery.fields.count(f.name)) continue;
            const FieldConfig &fc = config.battery.fields[f.name];
            if (!fc.send) continue;

            // EIN Device → aber eindeutige Sensor-ID
            String uid = devId + "_cell" + String(cell.cellIndex) + "_" + make_safe_id(f.name);

            String sensorName = "BAT " + String(moduleIndex) +
                                " Cell " + String(cell.cellIndex) +
                                " " + fc.label;

            buildDiscovery(doc, fc, f.name, stateTopic, uid, sensorName, devId, devName);

            String out;
            serializeJson(doc, out);
            String topic = "homeassistant/sensor/" + uid + "/config";
            mqttClient.publish(topic.c_str(), out.c_str(), true);
            mqttClient.loop();
            delay(50);
        }
    }
}
void PyMqtt::publishDiscoveryStat(int moduleIndex) {
    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicStat;

    String devId      = prefix + "_stat_" + String(moduleIndex);
    String devName    = prefix + " STAT " + String(moduleIndex);
    String stateTopic = prefix + "/" + subtopic + "/" + String(moduleIndex);

    DynamicJsonDocument doc(512);

    for (auto &f : lastParsedStat.fields) {

        String name = f.name;

        if (!config.battery.fields.count(name)) continue;
        const FieldConfig &fc = config.battery.fields[name];

        if (!fc.send) continue;

        String uid        = devId + "_" + make_safe_id(name);
        String sensorName = "STAT " + String(moduleIndex) + " " + fc.label;

        buildDiscovery(doc, fc, name, stateTopic, uid, sensorName, devId, devName);

        String out;
        serializeJson(doc, out);
        String topic = "homeassistant/sensor/" + uid + "/config";
        mqttClient.publish(topic.c_str(), out.c_str(), true);
        mqttClient.loop();
        delay(50);
    }
}
// ----------------------------------------------------
// Begin
// ----------------------------------------------------
void PyMqtt::begin() {
    enabled = config.mqtt.enabled;
    if (!enabled) return;

    mqttClient.setServer(config.mqtt.server.c_str(), config.mqtt.port);
    mqttClient.setBufferSize(2048);

    Log(LOG_INFO, "MQTT: BufferSize set to 2048 bytes");
}

// ----------------------------------------------------
// Connect
// ----------------------------------------------------
bool PyMqtt::connect() {
    if (!enabled) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    if (wifiConnectedSince == 0)
        wifiConnectedSince = millis();

    if (millis() - wifiConnectedSince < 10000)
        return false;

    String clientId = "PylontechMonitor-" + String((uint32_t)ESP.getEfuseMac());

    return mqttClient.connect(clientId.c_str(),
                              config.mqtt.user.c_str(),
                              config.mqtt.pass.c_str());
}

// ----------------------------------------------------
// Loop
// ----------------------------------------------------
void PyMqtt::loop() {
    if (!enabled) return;

    if (WiFi.status() != WL_CONNECTED) {
        wifiConnectedSince = 0;
        return;
    }

    if (!mqttClient.connected()) {
        if (millis() - lastReconnectAttempt > 3000) {
            lastReconnectAttempt = millis();
            connect();
        }
        return;
    }

    // Discovery nur einmal
    // PWR Discovery + Publisher
    if (newParserData) {

        publishStack();

        for (const auto& mod : lastParsedModules) {
            if (!mod.present) continue;

            int idx = mod.index;

            // PWR Discovery (unverändert)
            if (!discoveredPwr.count(idx)) {
                publishDiscoveryBat();   // dein alter PWR-Discoverer
                discoveredPwr.insert(idx);
            }

            // PWR Publisher
            publishBat(idx, mod);
        }

        newParserData = false;
    }

    // BAT Discovery + Publisher
    if (batParserHasData) {

        int idx = batParserModuleIndex;

        if (!discoveredBat.count(idx)) {
            publishDiscoveryBatCells(idx);
            discoveredBat.insert(idx);
        }

        publishBatCells(idx);
        batParserHasData = false;
    }

    // STAT Discovery + Publisher
    if (statParserHasData) {

        int idx = statParserModuleIndex;

        if (!discoveredStat.count(idx)) {
            publishDiscoveryStat(idx);
            discoveredStat.insert(idx);
        }

        publishStat(idx);
        statParserHasData = false;
    }

    // PWR → Modul-Daten
    if (newParserData) {
        publishStack();

        for (const auto& mod : lastParsedModules) {
            if (!mod.present) continue;
            publishBat(mod.index, mod);   // ← PWR Publisher
        }

        newParserData = false;
    }

    // BAT → Zell-Daten
    if (batParserHasData) {
        publishBatCells(batParserModuleIndex);
        batParserHasData = false;
    }


    // STAT → Status-Daten
    if (statParserHasData) {
        publishStat(statParserModuleIndex);      // ← STAT Publisher
        statParserHasData = false;
    }

    mqttClient.loop();
}

// ----------------------------------------------------
// Stack publish (one JSON for all stack values)
// ----------------------------------------------------
void PyMqtt::publishStack() {
    if (!enabled || !mqttClient.connected() || !parserHasData) return;

    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicStack;

    String topic = prefix + "/" + subtopic;
    DynamicJsonDocument doc(1024);

    for (auto &kv : config.battery.fields) {
        const String &name = kv.first;
        const FieldConfig &fc = kv.second;

        if (!(name.startsWith("Stack") || name == "BatteryCount"))
            continue;

        if (!fc.mqtt) continue;

        String raw = getStackRawValue(name);
        if (raw.length() == 0) continue;

        String value = computeValue(raw, fc);
        String safe  = make_safe_id(name);

        if (fc.factor == "text" || fc.factor == "date") {
            doc[safe] = value;
        } else {
            doc[safe] = value.toFloat();
        }
    }

    String payload;
    serializeJson(doc, payload);

    Log(LOG_DEBUG, "MQTT: Stack JSON length = " + String(payload.length()));

    if (!mqttClient.publish(topic.c_str(), payload.c_str()))
        logPublishFailure(topic);
}

// ----------------------------------------------------
// Module publish (one JSON per battery, dynamic fields)
// ----------------------------------------------------
void PyMqtt::publishBat(int index, const BatteryModule& mod) {
    if (!enabled || !mqttClient.connected() || !parserHasData || !mod.present) return;

    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicPwr;   // Modul-Subtopic

    String topic = prefix + "/" + subtopic + "/" + String(index);
    DynamicJsonDocument doc(1024);

    for (auto &kv : config.battery.fields) {
        const String& name = kv.first;
        const FieldConfig& fc = kv.second;

        if (name.startsWith("Stack") || name == "BatteryCount")
            continue;

        if (!fc.mqtt) continue;

        String raw = getModuleRawValue(index, name);
        if (raw.length() == 0) continue;

        String value = computeValue(raw, fc);
        String safe  = make_safe_id(name);

        if (fc.factor == "text" || fc.factor == "date") {
            doc[safe] = value;
        } else {
            doc[safe] = value.toFloat();
        }
    }

    config.lastMqttContact = config.getCurrentTimeString();

    String payload;
    serializeJson(doc, payload);

    Log(LOG_DEBUG, "MQTT: Module " + String(index) + " JSON length = " + String(payload.length()));

    if (!mqttClient.publish(topic.c_str(), payload.c_str()))
        logPublishFailure(topic);
}

void PyMqtt::publishBatCells(int moduleIndex) {
    if (!enabled || !mqttClient.connected()) return;
    if (lastParsedBatCells.empty()) return;

    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicBat;

    // Jede Zelle einzeln senden
    for (const auto& cell : lastParsedBatCells) {

        // Topic: prefix/bat/<moduleIndex>/cell<cellIndex>
        String topic = prefix + "/" + subtopic + "/" +
                       String(moduleIndex) + "/cell" + String(cell.cellIndex);

        DynamicJsonDocument doc(1024);

        for (auto &f : cell.fields) {

            if (!config.battery.fields.count(f.name)) continue;
            const FieldConfig &fc = config.battery.fields[f.name];
            if (!fc.mqtt) continue;

            String safe  = make_safe_id(f.name);
            String value = computeValue(f.raw, fc);

            if (fc.factor == "text" || fc.factor == "date")
                doc[safe] = value;
            else
                doc[safe] = value.toFloat();
        }

        String payload;
        serializeJson(doc, payload);

        Log(LOG_INFO,
            "MQTT BAT: sending module " + String(moduleIndex) +
            " cell " + String(cell.cellIndex) +
            " to topic " + topic +
            " (len=" + String(payload.length()) + ")");

        mqttClient.publish(topic.c_str(), payload.c_str());
    }
}

void PyMqtt::publishStat(int moduleIndex) {
    if (!enabled || !mqttClient.connected()) return;
    if (!config.battery.enableStat) return;
    if (lastParsedStat.fields.empty()) return;

    String prefix   = config.mqtt.prefix;
    String subtopic = config.mqtt.topicStat;

    // Topic: prefix/stat/<moduleIndex>
    String topic = prefix + "/" + subtopic + "/" + String(moduleIndex);

    DynamicJsonDocument doc(1024);

    for (auto &f : lastParsedStat.fields) {

        String name = f.name;
        String raw  = f.raw;

        if (!config.battery.fields.count(name)) continue;
        FieldConfig &fc = config.battery.fields[name];

        if (!fc.mqtt) continue;

        String value = computeValue(raw, fc);
        String safe  = make_safe_id(name);

        if (fc.factor == "text" || fc.factor == "date") {
            doc[safe] = value;
        } else {
            doc[safe] = value.toFloat();
        }
    }

    String payload;
    serializeJson(doc, payload);

    Log(LOG_INFO, "MQTT STAT: sending module " + String(moduleIndex) +
                  " to topic " + topic + " (len=" + String(payload.length()) + ")");

    if (!mqttClient.publish(topic.c_str(), payload.c_str()))
        logPublishFailure(topic);
}