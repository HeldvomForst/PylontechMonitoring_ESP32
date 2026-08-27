#include "py_mqtt.h"
#include "py_log.h"
#include "config.h"
#include <ArduinoJson.h>

extern BatTable batTable;
extern StatTable statTable;


// ---------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------
static void logInfo(const String& msg)  { Log(LOG_INFO,  msg); }
static void logWarn(const String& msg)  { Log(LOG_WARN,  msg); }
static void logError(const String& msg) { Log(LOG_ERROR, msg); }
static void logDebug(const String& msg) { Log(LOG_DEBUG, msg); }

// ---------------------------------------------------------
// Helper: sanitize ID for Home Assistant
// ---------------------------------------------------------
static String sanitizeId(const String& in) {
    String out;
    out.reserve(in.length());

    for (char c : in) {
        if (c >= 'A' && c <= 'Z') out += char(c + 32);
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out += c;
        else if (c == '_' || c == '-' || c == ' ' || c == '/' || c == '.') out += '_';
    }
    return out;
}

// ---------------------------------------------------------
// Discovery: Stack
// ---------------------------------------------------------
void PyMqtt::publishDiscoveryStack() {
    if (!enabled || !mqttClient.connected()) return;

    String prefix   = config.mqtt.prefix;
    String prefixId = sanitizeId(prefix);
    String sub      = config.mqtt.topicStack;
    String stateTopic = prefix + "/" + sub;

    const char* keys[]  = { "StackVoltAvg", "StackCurrSum", "StackTempMax", "BatteryCount", "HealthState", "ModuleState" };
    const char* units[] = { "V", "A", "°C", "", "", "" };

    for (int i = 0; i < 6; i++) {
        String key = keys[i];
        String unit = units[i];

        String uniqueId = prefixId + "_stack_" + sanitizeId(key);
        String discTopic = "homeassistant/sensor/" + uniqueId + "/config";

        StaticJsonDocument<512> doc;

        doc["name"]          = key;
        doc["uniq_id"]       = uniqueId;
        doc["obj_id"]        = uniqueId;
        doc["state_topic"]   = stateTopic;
        doc["value_template"] = "{{ value_json." + key + " }}";

        if (unit == String("V")) {
            doc["device_class"] = "voltage";
            doc["unit_of_measurement"] = "V";
        }
        else if (unit == String("A")) {
            doc["device_class"] = "current";
            doc["unit_of_measurement"] = "A";
        }
        else if (unit == String("°C")) {
            doc["device_class"] = "temperature";
            doc["unit_of_measurement"] = "°C";
        }

        JsonObject dev = doc.createNestedObject("dev");
        dev["ids"]  = prefixId;
        dev["name"] = prefix + " Stack";

        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(discTopic.c_str(), payload.c_str(), true);

        vTaskDelay(5);
    }

    // HealthState
    {
        String uniqueId = prefixId + "_stack_healthstate";
        String discTopic = "homeassistant/sensor/" + uniqueId + "/config";

        StaticJsonDocument<512> doc;
        doc["name"]          = "Health State";
        doc["uniq_id"]       = uniqueId;
        doc["obj_id"]        = uniqueId;
        doc["state_topic"]   = stateTopic;
        doc["value_template"] = "{{ value_json.HealthState }}";
        doc["icon"]          = "mdi:heart-pulse";

        JsonObject dev = doc.createNestedObject("dev");
        dev["ids"]  = prefixId;
        dev["name"] = prefix + " Stack";

        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(discTopic.c_str(), payload.c_str(), true);
    }

    // ModuleState
    {
        String uniqueId = prefixId + "_stack_modulestate";
        String discTopic = "homeassistant/sensor/" + uniqueId + "/config";

        StaticJsonDocument<512> doc;
        doc["name"]          = "Module State";
        doc["uniq_id"]       = uniqueId;
        doc["obj_id"]        = uniqueId;
        doc["state_topic"]   = stateTopic;
        doc["value_template"] = "{{ value_json.ModuleState }}";
        doc["icon"]          = "mdi:battery-heart-variant";

        JsonObject dev = doc.createNestedObject("dev");
        dev["ids"]  = prefixId;
        dev["name"] = prefix + " Stack";

        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(discTopic.c_str(), payload.c_str(), true);
    }
}

// ---------------------------------------------------------
// Discovery: PWR Module
// ---------------------------------------------------------
void PyMqtt::publishDiscoveryPwrModule(int moduleIndex) {

    if (!enabled || !mqttClient.connected()) return;

    String prefix   = config.mqtt.prefix;
    String prefixId = sanitizeId(prefix);

    String subtopic   = config.mqtt.topicPwr;
    String subtopicId = sanitizeId(subtopic);

    String stateTopic = prefix + "/" + subtopic + "/" + String(moduleIndex);

    for (int i = 0; i < pwrFieldCount; i++) {

        const PwrField& f = pwrFields[i];
        if (!f.send) continue;

        // JSON-Key für HA = Display-Name (oder Name)
        String jsonKey = f.display.length() ? f.display : f.name;

        // uniq_id darf KEINE Punkte enthalten → sanitizeId korrekt
        String uniqueId = prefixId + "_" + subtopicId + "_" +
                          String(moduleIndex) + "_" + sanitizeId(jsonKey);

        String discTopic = "homeassistant/sensor/" + uniqueId + "/config";

        StaticJsonDocument<512> doc;

        // Name für HA = Display-Name
        doc["name"] = jsonKey;

        doc["uniq_id"]     = uniqueId;
        doc["obj_id"]      = uniqueId;
        doc["state_topic"] = stateTopic;

        // WICHTIG: value_template nutzt denselben JSON-Key wie Publish
        doc["value_template"] = "{{ value_json." + jsonKey + " }}";

        if (!(f.factor == "text" || f.factor == "date" || f.unit == "timestamp")) {

            int dec = decimalsForUnit(f.unit);
            String devClass = deviceClassForUnit(f.unit);

            if (devClass.length() > 0)
                doc["device_class"] = devClass;

            if (f.unit.length() > 0)
                doc["unit_of_measurement"] = f.unit;

            doc["state_class"]               = "measurement";
            doc["suggested_display_precision"] = dec;
        }

        JsonObject dev = doc.createNestedObject("dev");
        dev["ids"]  = prefixId + "_pwr_" + String(moduleIndex);
        dev["name"] = prefix + " PWR " + String(moduleIndex);

        String payload;
        serializeJson(doc, payload);

        mqttClient.publish(discTopic.c_str(), payload.c_str(), true);
        vTaskDelay(5);
    }
}


// ---------------------------------------------------------
// Discovery: BAT Module
// ---------------------------------------------------------
void PyMqtt::publishDiscoveryBatModule(int moduleIndex) {
    if (!enabled || !mqttClient.connected()) return;
    if (!config.battery.enableBat) return;

    String prefix     = config.mqtt.prefix;       // z.B. "Pylontechtest"
    String prefixId   = sanitizeId(prefix);
    String topicBat   = config.mqtt.topicBat;     // "bat"
    String cellPrefix = config.mqtt.cellPrefix;   // "Cell"

    // Modulnummer wie im Publish (1-basiert)
    int moduleTopicIndex = moduleIndex;

    // Anzahl Zellen aus Parser
    if (batTable.rows == 0 || batTable.cols == 0) return;

    for (int row = 0; row < batTable.rows; row++) {

        // CellIndex aus erster Spalte oder Zeilenindex
        int cellIndex = row;
        if (batTable.cell[row][0])
            cellIndex = atoi(batTable.cell[row][0]);

        // state_topic exakt wie Publish
        String stateTopic =
            prefix + "/" +
            topicBat + "/" +
            String(moduleTopicIndex) + "/" +
            cellPrefix + String(cellIndex);

        // Für jede konfigurierte BAT-Spalte ein Sensor
        for (int i = 0; i < batFieldCount; i++) {

            const FieldConfig& fc = batFields[i];
            if (!fc.mqtt) continue;

            // JSON-Key wie Publish
            String jsonKey = fc.display.length() ? fc.display : fc.label;

            // Unique ID
            String uniqueId =
                prefixId + "_bat_" +
                String(moduleTopicIndex) + "_cell" +
                String(cellIndex) + "_" +
                sanitizeId(jsonKey);

            String discTopic =
                "homeassistant/sensor/" + uniqueId + "/config";

            StaticJsonDocument<512> doc;

            // Name wie im Produktivsystem: "Cell 3 TempState"
            String name =
                "Cell " + String(cellIndex) + " " + jsonKey;

            doc["name"]        = name;
            doc["uniq_id"]     = uniqueId;
            doc["obj_id"]      = uniqueId;
            doc["state_topic"] = stateTopic;
            doc["value_template"] =
                "{{ value_json." + jsonKey + " }}";

            // Meta-Daten (device_class, unit, precision)
            if (!(fc.factor == "text" ||
                  fc.factor == "date" ||
                  fc.unit   == "timestamp"))
            {
                addDiscoveryMeta(doc, fc);
            }

            JsonObject dev = doc.createNestedObject("dev");
            dev["ids"]  = prefixId + "_bat_" + String(moduleTopicIndex);
            dev["name"] = prefix + " BAT " + String(moduleTopicIndex);

            String payload;
            serializeJson(doc, payload);

            mqttClient.publish(discTopic.c_str(), payload.c_str(), true);
            vTaskDelay(5);
        }
    }
}


// ---------------------------------------------------------
// Discovery: STAT Module (pointer‑based)
// ---------------------------------------------------------
void PyMqtt::publishDiscoveryStatModule(int moduleIndex) {

    if (!enabled || !mqttClient.connected()) return;
    if (!config.battery.enableStat) return;

    String prefix   = config.mqtt.prefix;
    String prefixId = sanitizeId(prefix);

    String topicStat   = config.mqtt.topicStat;
    String topicStatId = sanitizeId(topicStat);

    int moduleTopicIndex = moduleIndex;

    int total = statFieldCount;
    if (total == 0) return;

    const int CHUNK_SIZE = 20;
    int chunks = (total + CHUNK_SIZE - 1) / CHUNK_SIZE;

    for (int chunk = 0; chunk < chunks; chunk++) {

        String stateTopic =
            prefix + "/" +
            topicStat + "/" +
            String(moduleTopicIndex) + "/" +
            String(chunk);

        int start = chunk * CHUNK_SIZE;
        int end   = start + CHUNK_SIZE;
        if (end > total) end = total;

        for (int i = start; i < end; i++) {

            const FieldConfig& fc = statFields[i];
            if (!fc.send) continue;

            // ORIGINAL Parser-Name
            String parserKey = statTable.name[i];

            // JSON-Key für HA = Display (oder Parser-Name)
            String jsonKey = fc.display.length() ? fc.display : parserKey;

            // uniq_id darf KEINE Punkte enthalten → sanitizeId korrekt
            String uniqueId =
                prefixId + "_stat_" +
                String(moduleTopicIndex) + "_chunk" +
                String(chunk) + "_" +
                sanitizeId(jsonKey);

            String discTopic =
                "homeassistant/sensor/" + uniqueId + "/config";

            StaticJsonDocument<512> doc;

            // Name für HA = Display-Name
            doc["name"] = jsonKey;

            doc["uniq_id"]     = uniqueId;
            doc["obj_id"]      = uniqueId;
            doc["state_topic"] = stateTopic;

            // WICHTIG: value_template nutzt denselben JSON-Key wie Publish
            doc["value_template"] =
                "{{ value_json." + jsonKey + " }}";

            // Meta-Daten
            if (!(fc.factor == "text" ||
                  fc.factor == "date" ||
                  fc.unit   == "timestamp"))
            {
                addDiscoveryMeta(doc, fc);
            }

            JsonObject dev = doc.createNestedObject("dev");
            dev["ids"]  = prefixId + "_stat_" + String(moduleTopicIndex);
            dev["name"] = prefix + " STAT " + String(moduleTopicIndex);

            String payload;
            serializeJson(doc, payload);

            mqttClient.publish(discTopic.c_str(), payload.c_str(), true);
            vTaskDelay(5);
        }
    }
}


// ---------------------------------------------------------
// Discovery State Machine (NEW: no buffers)
// ---------------------------------------------------------
void PyMqtt::handleDiscoveryStep()
{
    if (!enabled) return;
    if (!mqttClient.connected()) return;

    // Discovery Start Delay
    if (millis() < discoveryStartTime + discoveryDelayStartMs)
        return;

    // Throttle: nur X Messages pro Sekunde
    unsigned long intervalMs = 1000 / discoveryMessagesPerSecond;

    if (millis() - discoveryLastSend < intervalMs)
        return;

    discoveryLastSend = millis();

    switch (discoveryPhase)
    {
        case DISC_IDLE:
        case DISC_DONE:
            return;

        case DISC_STACK:
            publishDiscoveryStack();
            discoveryPhase = DISC_PWR;
            discPwrIndex = 0;
            return;

        case DISC_PWR:
        {
            if (discPwrIndex >= config.detectedModules) {
                discoveryPhase = DISC_BAT;
                discBatModule = 0;
                return;
            }

            // Nur senden, wenn mindestens ein Feld send==true ist
            bool hasSendFields = false;
            for (int i = 0; i < pwrFieldCount; i++) {
                if (pwrFields[i].send) {
                    hasSendFields = true;
                    break;
                }
            }

            if (hasSendFields)
                publishDiscoveryPwrModule((int)discPwrIndex + 1);

            discPwrIndex++;
            return;
        }

        case DISC_BAT:
        {
            if (!config.battery.enableBat) {
                discoveryPhase = DISC_STAT;
                discStatModule = 0;
                return;
            }

            if (discBatModule >= config.detectedModules) {
                discoveryPhase = DISC_STAT;
                discStatModule = 0;
                return;
            }

            bool hasSendFields = false;
            for (int i = 0; i < batFieldCount; i++) {
                if (batFields[i].send) {
                    hasSendFields = true;
                    break;
                }
            }

            if (hasSendFields)
                publishDiscoveryBatModule((int)discBatModule + 1);

            discBatModule++;
            return;
        }

        case DISC_STAT:
        {
            if (!config.battery.enableStat) {
                discoveryPhase = DISC_DONE;
                discoveryActive = false;
                return;
            }

            if (discStatModule >= config.detectedModules) {
                discoveryPhase = DISC_DONE;
                discoveryActive = false;
                return;
            }

            bool hasSendFields = false;
            for (int i = 0; i < statFieldCount; i++) {
                if (statFields[i].send) {
                    hasSendFields = true;
                    break;
                }
            }

            if (hasSendFields)
                publishDiscoveryStatModule((int)discStatModule + 1);

            discStatModule++;
            return;
        }
    }
}

