#include "py_mqtt.h"
#include "py_log.h"
#include "config.h"
#include "py_parser.h"

extern BatTable batTable;
extern PwrTable pwrTable;
extern StatTable statTable;
extern float stackVoltAvg;
extern float stackCurrSum;
extern float stackTempMax;
extern int   stackBatCount;
extern HealthStatus health;
extern FieldConfig batFields[BAT_MAX_COLS];
extern int batFieldCount;


// Logging helpers
static void logDebug(const String& msg) { Log(LOG_DEBUG, msg); }

static String getStackHealthState() {
    if (health.errorCount > 0) return "ERROR";
    if (health.warnCount  > 0) return "WARN";
    return "OK";
}

static String getModuleStatusString() {
    String out;
    for (int i = 0; i < health.moduleCount; i++) {
        const ModuleHealth& m = health.modules[i];
        if (m.status == nullptr) out += "0";
        else if (strcmp(m.status, "Fehler") == 0)   out += "2";
        else if (strcmp(m.status, "Warnung") == 0)  out += "1";
        else                                        out += "0";
    }
    return out;
}

// ---------------------------------------------------------
// NAME NORMALIZATION (wird für PWR/STAT nicht mehr benutzt,
// bleibt aber für evtl. andere Zwecke erhalten)
// ---------------------------------------------------------
String PyMqtt::normalizeName(const String& in) {
    String out;
    bool upperNext = true;

    for (char c : in) {
        if (c == ' ' || c == '_' || c == '-' || c == '.') {
            upperNext = true;
            continue;
        }
        if (!isalnum(c)) continue;

        if (upperNext) {
            out += (char)toupper(c);
            upperNext = false;
        } else {
            out += (char)c;
        }
    }
    return out;
}

// ---------------------------------------------------------
// DECIMAL PRECISION
// ---------------------------------------------------------
int PyMqtt::decimalsForUnit(const String& unit) {
    if (unit == "V")  return 3;
    if (unit == "A")  return 3;
    if (unit == "°C") return 1;
    if (unit == "%")  return 0;
    if (unit == "Ah") return 3;
    return 0;
}

// ---------------------------------------------------------
// DEVICE CLASS
// ---------------------------------------------------------
String PyMqtt::deviceClassForUnit(const String& unit) {
    if (unit == "V")  return "voltage";
    if (unit == "A")  return "current";
    if (unit == "°C") return "temperature";
    if (unit == "%")  return "battery";
    return "";
}

// ---------------------------------------------------------
// precisionDiffersFromDefault / precisionForUnit
// ---------------------------------------------------------
bool PyMqtt::precisionDiffersFromDefault(const String& unit) {
    return decimalsForUnit(unit) != 0;
}

int PyMqtt::precisionForUnit(const String& unit) {
    return decimalsForUnit(unit);
}

// ---------------------------------------------------------
// COMPUTE VALUE
// ---------------------------------------------------------
String PyMqtt::computeValue(const String& raw, const FieldConfig& fc) {
    if (fc.factor == "date" || fc.unit == "timestamp")
        return raw;

    if (fc.factor == "text")
        return raw;

    float factor = fc.factor.toFloat();
    float valueC = raw.toFloat() * factor;

    if (fc.unit == "°C" && config.battery.useFahrenheit) {
        float valueF = valueC * 1.8f + 32.0f;
        return String(valueF, decimalsForUnit("°F"));
    }

    return String(valueC, decimalsForUnit(fc.unit));
}

void PyMqtt::addDiscoveryMeta(JsonDocument& doc, const FieldConfig& fc) {
    if (fc.factor == "text" || fc.factor == "date" || fc.unit == "timestamp")
        return;

    int    dec      = decimalsForUnit(fc.unit);
    String devClass = deviceClassForUnit(fc.unit);

    if (devClass.length() > 0)
        doc["device_class"] = devClass;

    if (fc.unit.length() > 0)
        doc["unit_of_measurement"] = fc.unit;

    doc["state_class"]               = "measurement";
    doc["suggested_display_precision"] = dec;
}

// ---------------------------------------------------------
// BUILD PWR JSON – Keys
// ---------------------------------------------------------
String PyMqtt::buildPwrJson(int moduleIndex) {
    if (moduleIndex < 0 || moduleIndex >= pwrTable.rows)
        return "{}";

    StaticJsonDocument<512> doc;

    for (int i = 0; i < pwrFieldCount; i++) {
        const PwrField& f = pwrFields[i];
        if (!f.mqtt) continue;

        const char* raw = nullptr;

        // Lookup IMMER über f.name (Batterie-Wahrheit)
        for (int c = 0; c < pwrTable.cols; c++) {
            if (strcmp(pwrTable.header[c], f.name.c_str()) == 0) {
                raw = pwrTable.cell[moduleIndex][c];
                break;
            }
        }
        if (!raw) continue;

        // JSON-Key für HA = Display-Name (oder Name)
        String jsonKey = f.display.length() ? f.display : f.name;

        if (f.factor == "text" || f.factor == "date" || f.unit == "timestamp") {
            doc[jsonKey] = raw;
        } else {
            float factor = f.factor.toFloat();
            float value  = atof(raw) * factor;
            doc[jsonKey] = value;
        }
    }

    String json;
    serializeJson(doc, json);
    return json;
}


void PyMqtt::sendPwrModule(int moduleIndex) {
    if (!enabled) return;

    String topic =
        config.mqtt.prefix + "/" +
        config.mqtt.topicPwr + "/" +
        String(moduleIndex + 1);

    String payload = buildPwrJson(moduleIndex);
    publishRaw(topic, payload, false);
}



// ---------------------------------------------------------
// PUBLISH STACK JSON
// ---------------------------------------------------------
void PyMqtt::publishStack() {
    if (!enabled) {
        logDebug("MQTT: publishStack skipped (disabled)");
        return;
    }

    String topic = config.mqtt.prefix + "/" + config.mqtt.topicStack;

    StaticJsonDocument<256> doc;
    doc["StackVoltAvg"] = stackVoltAvg;
    doc["StackCurrSum"] = stackCurrSum;
    doc["StackTempMax"] = stackTempMax;
    doc["BatteryCount"] = stackBatCount;
    doc["HealthState"]  = getStackHealthState();
    doc["ModuleState"]  = getModuleStatusString();

    String payload;
    serializeJson(doc, payload);

    bool ok = publishRaw(topic, payload, false);

    if (ok) {
        config.lastPwrUpdate = config.getCurrentTimeString();
        Log(LOG_INFO, "MQTT: PWR publish OK (6 fields)");
    } else {
        Log(LOG_WARN, "MQTT: PWR publish FAILED");
    }
}


// ---------------------------------------------------------
// PUBLISH BAT CELLS – pointer‑basiert, pro Zelle ein Topic
// ---------------------------------------------------------
void PyMqtt::publishBatModule(int moduleIndex) {
    if (!config.mqtt.enabled) return;
    if (!config.battery.enableBat) return;

    if (batTable.rows == 0 || batTable.cols == 0) return;

    String prefix     = config.mqtt.prefix;
    String topicBat   = config.mqtt.topicBat;
    String cellPrefix = config.mqtt.cellPrefix;

    int moduleTopicIndex = moduleIndex;

    int totalSent = 0;

    for (int row = 0; row < batTable.rows; row++) {

        int cellIndex = row;
        if (batTable.cols > 0 && batTable.cell[row][0]) {
            cellIndex = atoi(batTable.cell[row][0]);
        }

        String topic =
            prefix + "/" +
            topicBat + "/" +
            String(moduleTopicIndex) + "/" +
            cellPrefix + String(cellIndex);

        StaticJsonDocument<512> doc;

        int fieldCount = 0;

        for (int c = 0; c < batTable.cols; c++) {

            const char* rawName  = batTable.header[c];
            const char* rawValue = batTable.cell[row][c];

            if (!rawName || !rawValue) continue;

            String key(rawName);

            int idx = -1;
            for (int i = 0; i < batFieldCount; i++) {
                if (batFields[i].label == key) {
                    idx = i;
                    break;
                }
            }
            if (idx < 0) continue;

            const FieldConfig& fc = batFields[idx];

            if (!fc.mqtt)
                continue;

            String jsonKey = fc.display.length() ? fc.display : key;
            String value   = computeValue(rawValue, fc);

            doc[jsonKey] = value;
            fieldCount++;
        }

        if (fieldCount == 0) continue;

        String payload;
        serializeJson(doc, payload);

        bool ok = publishRaw(topic, payload, false);

        if (ok) {
            totalSent += fieldCount;
        }
    }

    if (totalSent > 0) {
        config.lastBatUpdate = config.getCurrentTimeString();
        Log(LOG_INFO, "MQTT: BAT publish OK (" + String(totalSent) + " fields total)");
    } else {
        Log(LOG_WARN, "MQTT: BAT publish FAILED (no fields sent)");
    }
}

// ---------------------------------------------------------
// PUBLISH STAT JSON – pointer‑based, no std::map, no heap
// ---------------------------------------------------------
void PyMqtt::publishStat(int moduleIndex) {
    if (!enabled) return;
    if (!config.battery.enableStat) return;
    if (statTable.count == 0) return;

    const int CHUNK_SIZE = 20;
    int total  = statTable.count;
    int chunks = (total + CHUNK_SIZE - 1) / CHUNK_SIZE;

    int totalSent = 0;

    for (int chunk = 0; chunk < chunks; chunk++) {

        StaticJsonDocument<1024> doc;

        int start = chunk * CHUNK_SIZE;
        int end   = start + CHUNK_SIZE;
        if (end > total) end = total;

        int fieldCount = 0;

        for (int i = start; i < end; i++) {

            const char* rawName  = statTable.name[i];
            const char* rawValue = statTable.value[i];
            if (!rawName || !rawValue) continue;

            String key(rawName);

            int idx = -1;
            for (int j = 0; j < statFieldCount; j++) {
                if (statFields[j].label == key) {
                    idx = j;
                    break;
                }
            }
            if (idx < 0) continue;

            const FieldConfig& fc = statFields[idx];

            if (!fc.mqtt)
                continue;

            String jsonKey = fc.display.length() ? fc.display : key;

            doc[jsonKey] = rawValue;
            fieldCount++;
        }

        if (fieldCount == 0) continue;

        String payload;
        serializeJson(doc, payload);

        String topic =
            config.mqtt.prefix + "/" +
            config.mqtt.topicStat + "/" +
            String(moduleIndex) + "/" +
            String(chunk);

        bool ok = publishRaw(topic, payload, false);

        if (ok) {
            totalSent += fieldCount;
        }
    }

    if (totalSent > 0) {
        config.lastStatUpdate = config.getCurrentTimeString();
        Log(LOG_INFO, "MQTT: STAT publish OK (" + String(totalSent) + " fields total)");
    } else {
        Log(LOG_WARN, "MQTT: STAT publish FAILED (no fields sent)");
    }
}
