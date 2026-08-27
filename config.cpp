#include "config.h"
#include "py_log.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include "py_parser.h"

// -------------------------------------------------------------
// Global mutexes
// -------------------------------------------------------------
portMUX_TYPE batMux  = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE statMux = portMUX_INITIALIZER_UNLOCKED;

// -------------------------------------------------------------
// Global configuration instance
// -------------------------------------------------------------
AppConfig config;

// -------------------------------------------------------------
// Global PWR parser state
// -------------------------------------------------------------
volatile bool pwrFrameReady = false;
int           pwrTotalModules = 0;
int           pwrCurrentModule = 0;

// -------------------------------------------------------------
// Stack aggregate values (used for MQTT + Web)
// -------------------------------------------------------------
float stackVoltAvg  = 0.0f;
float stackCurrSum  = 0.0f;
float stackSocAvg   = 0.0f;
float stackTempMax  = 0.0f;
int   stackBatCount = 0;

// -------------------------------------------------------------
// Global field tables (static, RAM‑optimized)
// -------------------------------------------------------------
PwrField    pwrFields[32];
int         pwrFieldCount = 0;

FieldConfig batFields[BAT_MAX_COLS];
int         batFieldCount = 0;

FieldConfig statFields[STAT_MAX_FIELDS];
int         statFieldCount = 0;

// -------------------------------------------------------------
// Chunk size for NVS JSON storage
// -------------------------------------------------------------
static const size_t CHUNK_SIZE = 1500;
static const size_t STAT_FIELDS_PER_CHUNK = 20;
// -------------------------------------------------------------
// Global health status
// -------------------------------------------------------------
HealthStatus health;

// -------------------------------------------------------------
// Helper: Save JSON in chunks (for large field tables)
// -------------------------------------------------------------
void AppConfig::saveJsonChunked(const char* ns, const char* prefix, const String& json) {

    Log(LOG_INFO, String("NVS-CHUNK: Saving JSON for namespace '") + ns + "', prefix '" + prefix + "'");
    Log(LOG_INFO, String("NVS-CHUNK: JSON length = ") + json.length());

    Preferences p;
    p.begin(ns, false);

    // Remove old chunks
    for (int i = 0; i < 50; i++) {
        String key = String(prefix) + "_" + i;
        if (p.isKey(key.c_str())) {
            p.remove(key.c_str());
        }
    }

    int index = 0;
    int written = 0;
    size_t len = json.length();

    while (index * CHUNK_SIZE < len) {

        size_t start = index * CHUNK_SIZE;
        size_t end   = start + CHUNK_SIZE;
        if (end > len) end = len;

        String part;
        part.reserve(CHUNK_SIZE);
        for (size_t i = start; i < end; i++) {
            part += json[i];
        }

        String key = String(prefix) + "_" + index;

        bool ok = p.putString(key.c_str(), part);
        if (!ok) {
            Log(LOG_ERROR, String("NVS-CHUNK: FAILED writing chunk #") + index);
            break;
        }

        index++;
        written++;
    }

    Log(LOG_INFO, String("NVS-CHUNK: Total chunks written = ") + written);
    p.end();
}

// -------------------------------------------------------------
// Helper: Load JSON from chunks
// -------------------------------------------------------------
String AppConfig::loadJsonChunked(const char* ns, const char* prefix) {
    Preferences p;
    p.begin(ns, true);

    String json = "";
    for (int i = 0; i < 50; i++) {
        String key = String(prefix) + "_" + String(i);
        if (!p.isKey(key.c_str())) break;
        json += p.getString(key.c_str(), "");
    }

    p.end();
    return json;
}

// -------------------------------------------------------------
// Generate hostname from MAC address
// -------------------------------------------------------------
String AppConfig::generateHostname() {
    uint64_t mac = ESP.getEfuseMac();
    uint16_t last = mac & 0xFFFF;

    char buf[32];
    snprintf(buf, sizeof(buf), "pylontech-%04X", last);
    return String(buf);
}

// -------------------------------------------------------------
// Time helpers
// -------------------------------------------------------------
String AppConfig::getCurrentTimeString() {
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return String(buf);
}

bool AppConfig::isSystemTimeValid() {
    time_t now;
    time(&now);
    return (now > 1700000000); // > 2023-01-01
}

// -------------------------------------------------------------
// Clear all NVS namespaces
// -------------------------------------------------------------
void AppConfig::clearNVS() {
    Preferences p;

    p.begin("config", false);
    p.clear();
    p.end();

    p.begin("battery_pwr", false);
    p.clear();
    p.end();

    p.begin("battery_bat", false);
    p.clear();
    p.end();

    p.begin("battery_stat", false);
    p.clear();
    p.end();

    Log(LOG_WARN, "NVS cleared");
}

// -------------------------------------------------------------
// Factory defaults (no reboot, no NVS clear)
// -------------------------------------------------------------
void AppConfig::factoryDefaults() {

    // Battery intervals
    battery.intervalPwr  = 60000;
    battery.intervalBat  = 300000;
    battery.intervalStat = 1800000;

    battery.enableBat  = true;
    battery.enableStat = true;
    battery.useFahrenheit = false;

    // MQTT defaults
    mqtt.enabled = false;
    mqtt.server  = "";
    mqtt.port    = 1883;
    mqtt.user    = "";
    mqtt.pass    = "";

    mqtt.prefix     = "Pylontech";
    mqtt.topicStack = "Stack";
    mqtt.topicPwr   = "pwr";
    mqtt.topicBat   = "bat";
    mqtt.topicStat  = "stat";
    mqtt.mode       = "active";

    // Hostname / AP
    hostname = generateHostname();
    apSSID   = hostname;

    // NTP defaults
    ntpServer = "pool.ntp.org";
    manual_mode = false;
    manual_date = "";
    manual_time = "";
    manual_dst  = false;

    use_gateway_ntp = true;
    manual_ntp      = false;

    // ---------------------------------------------------------
    // PWR field table (static, RAM‑optimized)
    // ---------------------------------------------------------
    pwrFieldCount = 0;

    auto addPwr = [&](const char* name, const char* display, const char* factor, const char* unit) {
        PwrField f;
        f.name    = name;
        f.display = display;
        f.factor  = factor;
        f.unit    = unit;
        f.mqtt    = true;
        f.send    = true;
        pwrFields[pwrFieldCount++] = f;
    };

    addPwr("Volt",  "Voltage",     "0.001", "V");
    addPwr("Curr",  "Current",     "0.001", "A");
    addPwr("Tempr", "Temperature", "0.001", "°C");
    addPwr("SOC",   "SOC",         "1",     "%");

    // ---------------------------------------------------------
    // BAT + STAT field tables start empty
    // ---------------------------------------------------------
    batFieldCount = 0;
    statFieldCount = 0;

    // ---------------------------------------------------------
    // System fields
    // ---------------------------------------------------------
    currentTime     = "";
    detectedModules = 0;
    lastPwrUpdate   = "";


    Log(LOG_INFO, "factoryDefaults(): default fields set");
}

// -------------------------------------------------------------
// Factory reset (clear NVS + reboot)
// -------------------------------------------------------------
void AppConfig::factoryReset() {
    Log(LOG_WARN, "Factory reset triggered");
    clearNVS();
    delay(200);
    ESP.restart();
}

// -------------------------------------------------------------
// Load system configuration (WiFi, MQTT, NTP, etc.)
// -------------------------------------------------------------
void AppConfig::loadSystemConfig() {
    Preferences p;
    p.begin("config", true);

    deviceName = p.getString("devname", deviceName);
    hostname   = p.getString("hostname", hostname);
    wifiSSID   = p.getString("wifi_ssid", wifiSSID);
    wifiPass   = p.getString("wifi_pass", wifiPass);
    apSSID     = p.getString("ap_ssid", apSSID);
    apPass     = p.getString("ap_pass", apPass);
    setupDone  = p.getBool("setup", setupDone);

    useStaticIP = p.getBool("net_static", useStaticIP);
    ipAddr      = p.getString("net_ip", ipAddr);
    subnetMask  = p.getString("net_mask", subnetMask);
    gateway     = p.getString("net_gw", gateway);
    dns         = p.getString("net_dns", dns);

    ntpServer         = p.getString("ntp_srv", ntpServer);
    timezone          = p.getString("tz_name", timezone);
    daylightSaving    = p.getBool("tz_dst", daylightSaving);
    ntpResyncInterval = p.getULong("ntp_resync", ntpResyncInterval);

    manual_mode = p.getBool("manual_mode", false);
    manual_date = p.getString("manual_date", "");
    manual_time = p.getString("manual_time", "");
    manual_dst  = p.getBool("manual_dst", false);

    use_gateway_ntp = p.getBool("use_gateway_ntp", true);
    manual_ntp      = p.getBool("manual_ntp", false);

    mqtt.enabled = p.getBool("mqtt_en", mqtt.enabled);
    mqtt.server  = p.getString("mqtt_srv", mqtt.server);
    mqtt.port    = p.getUShort("mqtt_port", mqtt.port);
    mqtt.user    = p.getString("mqtt_user", mqtt.user);
    mqtt.pass    = p.getString("mqtt_pass", mqtt.pass);

    mqtt.prefix     = p.getString("mqtt_prefix", mqtt.prefix);
    mqtt.topicStack = p.getString("mqtt_t_stack", mqtt.topicStack);
    mqtt.topicPwr   = p.getString("mqtt_t_pwr",   mqtt.topicPwr);
    mqtt.topicBat   = p.getString("mqtt_t_bat",   mqtt.topicBat);
    mqtt.topicStat  = p.getString("mqtt_t_stat",  mqtt.topicStat);
    mqtt.mode       = p.getString("mqtt_mode",    mqtt.mode);
    mqtt.cellPrefix = p.getString("mqtt_cellprefix", mqtt.cellPrefix);

    currentTime     = p.getString("cur_time", currentTime);
    lastPwrUpdate   = p.getString("pwr_last", lastPwrUpdate);
    detectedModules = p.getUShort("pwr_mods", detectedModules);
    lastBatUpdate  = p.getString("bat_last", lastBatUpdate);
    lastStatUpdate = p.getString("stat_last", lastStatUpdate);
    lastUartUpdate = p.getString("uart_last", lastUartUpdate);

    logInfo  = p.getBool("log_info",  true);
    logWarn  = p.getBool("log_warn",  true);
    logError = p.getBool("log_error", true);
    logDebug = p.getBool("log_debug", false);

    logTaskManager = p.getBool("logTaskMgr", false);

    displayBrightness = p.getUChar("disp_bright", 150);

    p.end();

    // Apply defaults if config is empty
    if (hostname.length() == 0 || apSSID.length() == 0) {
        Log(LOG_WARN, "Config empty → applying factory defaults");
        factoryDefaults();
        setupDone = true;
        //saveSystemConfig();
    }
}

// -------------------------------------------------------------
// Save system configuration
// -------------------------------------------------------------
void AppConfig::saveSystemConfig() {
    Preferences p;
    p.begin("config", false);

    p.putString("devname", deviceName);
    p.putString("hostname", hostname);
    p.putString("wifi_ssid", wifiSSID);
    p.putString("wifi_pass", wifiPass);
    p.putString("ap_ssid", apSSID);
    p.putString("ap_pass", apPass);
    p.putBool("setup", setupDone);

    p.putBool("net_static", useStaticIP);
    p.putString("net_ip", ipAddr);
    p.putString("net_mask", subnetMask);
    p.putString("net_gw", gateway);
    p.putString("net_dns", dns);

    p.putString("ntp_srv", ntpServer);
    p.putString("tz_name", timezone);
    p.putBool("tz_dst", daylightSaving);
    p.putULong("ntp_resync", ntpResyncInterval);

    p.putBool("manual_mode", manual_mode);
    p.putString("manual_date", manual_date);
    p.putString("manual_time", manual_time);
    p.putBool("manual_dst", manual_dst);

    p.putBool("use_gateway_ntp", use_gateway_ntp);
    p.putBool("manual_ntp", manual_ntp);

    p.putBool("mqtt_en", mqtt.enabled);
    p.putString("mqtt_srv", mqtt.server);
    p.putUShort("mqtt_port", mqtt.port);
    p.putString("mqtt_user", mqtt.user);
    p.putString("mqtt_pass", mqtt.pass);

    p.putString("mqtt_prefix", mqtt.prefix);
    p.putString("mqtt_t_stack", mqtt.topicStack);
    p.putString("mqtt_t_pwr",   mqtt.topicPwr);
    p.putString("mqtt_t_bat",   mqtt.topicBat);
    p.putString("mqtt_t_stat",  mqtt.topicStat);
    p.putString("mqtt_mode",    mqtt.mode);
    p.putString("mqtt_cellprefix", mqtt.cellPrefix);

    p.putString("cur_time", currentTime);
    p.putString("pwr_last", lastPwrUpdate);
    p.putUShort("pwr_mods", detectedModules);
    p.putString("bat_last", lastBatUpdate);
    p.putString("stat_last", lastStatUpdate);
    p.putString("uart_last", lastUartUpdate);


    p.putBool("log_info",  logInfo);
    p.putBool("log_warn",  logWarn);
    p.putBool("log_error", logError);
    p.putBool("log_debug", logDebug);

    p.putBool("logTaskMgr", logTaskManager);


    p.end();
}

void AppConfig::saveDisplayBrightness() {
    Preferences p;
    p.begin("config", false);
    p.putUChar("disp_bright", displayBrightness);
    p.end();
}

// -------------------------------------------------------------
// Load PWR configuration
// -------------------------------------------------------------
void AppConfig::loadPwrConfig() {
    Preferences p;
    p.begin("battery_pwr", true);

    battery.intervalPwr = p.getULong("interval", battery.intervalPwr);
    battery.enableBat   = p.getBool("enabled", battery.enableBat);

    // Load health thresholds
    battery.cellDiffWarn  = p.getFloat("cellDiffWarn", battery.cellDiffWarn);
    battery.cellDiffError = p.getFloat("cellDiffError", battery.cellDiffError);

    p.end();
}

// -------------------------------------------------------------
// Save PWR configuration
// -------------------------------------------------------------
void AppConfig::savePwrConfig() {
    Preferences p;
    p.begin("battery_pwr", false);

    p.putULong("interval", battery.intervalPwr);
    p.putBool("enabled", battery.enableBat);

    // Save health thresholds
    p.putFloat("cellDiffWarn", battery.cellDiffWarn);
    p.putFloat("cellDiffError", battery.cellDiffError);

    p.end();
}

// -------------------------------------------------------------
// Save PWR field table (static, RAM‑optimized)
// -------------------------------------------------------------
void AppConfig::savePwrFields() {
    Preferences p;
    p.begin("battery_pwr", false);

    // Remove old keys
    p.remove("pwr_count");
    for (int i = 0; i < 256; i++) {
        char key[16];
        snprintf(key, sizeof(key), "pwr_%d", i);
        if (p.isKey(key)) p.remove(key);
    }

    int index = 0;

    for (int i = 0; i < pwrFieldCount; i++) {
        const PwrField& f = pwrFields[i];

        if (!f.mqtt) continue;

        char buf[160];
        snprintf(
            buf, sizeof(buf),
            "%s|%s|%s|%s|%c|%c",
            f.name.c_str(),
            f.display.c_str(),
            f.factor.c_str(),
            f.unit.c_str(),
            f.mqtt ? '1' : '0',
            f.send ? '1' : '0'
        );

        char key[16];
        snprintf(key, sizeof(key), "pwr_%d", index);
        p.putString(key, buf);
        index++;
    }

    p.putInt("pwr_count", index);
    p.end();
}

// -------------------------------------------------------------
// Load PWR field table
// -------------------------------------------------------------
void AppConfig::loadPwrFields() {
    Preferences p;
    p.begin("battery_pwr", true);

    int count = p.getInt("pwr_count", -1);
    if (count < 0) {
        p.end();
        return;
    }

    pwrFieldCount = 0;

    for (int i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "pwr_%d", i);

        if (!p.isKey(key)) continue;

        String packed = p.getString(key, "");
        if (packed.length() == 0) continue;

        const char* pch = packed.c_str();

        const char* s1 = strchr(pch, '|');
        const char* s2 = strchr(s1 + 1, '|');
        const char* s3 = strchr(s2 + 1, '|');
        const char* s4 = strchr(s3 + 1, '|');
        const char* s5 = strchr(s4 + 1, '|');

        PwrField f;
        f.name    = String(pch, s1 - pch);
        f.display = String(s1 + 1, s2 - s1 - 1);
        f.factor  = String(s2 + 1, s3 - s2 - 1);
        f.unit    = String(s3 + 1, s4 - s3 - 1);
        f.mqtt    = (*(s4 + 1) == '1');
        f.send    = (*(s5 + 1) == '1');

        pwrFields[pwrFieldCount++] = f;
    }

    p.end();
}

// -------------------------------------------------------------
// Save BAT configuration
// -------------------------------------------------------------
void AppConfig::saveBatConfig() {
    Preferences p;
    p.begin("battery_bat", false);

    p.putULong("interval", battery.intervalBat);
    p.putBool("enabled", battery.enableBat);

    p.end();
}

// -------------------------------------------------------------
// Save BAT field table (static, RAM‑optimized)
// -------------------------------------------------------------
void AppConfig::saveBatFields() {

    Log(LOG_INFO, String("BAT-SAVE: batFieldCount = ") + batFieldCount);

    DynamicJsonDocument doc(3000);
    JsonArray arr = doc.createNestedArray("fields");

    for (int i = 0; i < batFieldCount; i++) {
        const FieldConfig& fc = batFields[i];

        String packed;
        packed.reserve(80);
        packed += fc.label;   packed += "|";
        packed += fc.display; packed += "|";
        packed += fc.factor;  packed += "|";
        packed += fc.unit;    packed += "|";
        packed += (fc.mqtt ? "1" : "0"); packed += "|";
        packed += (fc.send ? "1" : "0");

        arr.add(packed);
    }

    String json;
    json.reserve(3000);
    serializeJson(doc, json);

    Log(LOG_INFO, String("BAT-SAVE: JSON length = ") + json.length());
    Log(LOG_INFO, String("BAT-SAVE: JSON = ") + json);

    saveJsonChunked("battery_bat", "bat", json);
}


// -------------------------------------------------------------
// Load BAT configuration
// -------------------------------------------------------------
void AppConfig::loadBatConfig() {
    Preferences p;
    p.begin("battery_bat", true);

    battery.intervalBat = p.getULong("interval", battery.intervalBat);
    battery.enableBat   = p.getBool("enabled", battery.enableBat);

    p.end();
}

// -------------------------------------------------------------
// Load BAT field table
// -------------------------------------------------------------
void AppConfig::loadBatFields() {

    String json = loadJsonChunked("battery_bat", "bat");

    Log(LOG_INFO, String("BAT-LOAD: Raw JSON length = ") + json.length());
    Log(LOG_INFO, String("BAT-LOAD: Raw JSON = ") + json);

    if (json.length() == 0) {
        Log(LOG_WARN, "BAT-LOAD: JSON empty");
        return;
    }

    DynamicJsonDocument doc(3000);
    if (deserializeJson(doc, json)) {
        Log(LOG_ERROR, "BAT-LOAD: JSON parse error");
        return;
    }

    Log(LOG_INFO, "BAT-LOAD: JSON parsed");

    JsonArray arr = doc["fields"];
    if (arr.isNull()) {
        Log(LOG_WARN, "BAT-LOAD: fields[] missing");
        return;
    }

    batFieldCount = 0;

    for (JsonVariant v : arr) {
        const char* packed = v.as<const char*>();
        if (!packed) continue;

        const char* p = packed;

        int p1 = strchr(p, '|') - p;
        int p2 = strchr(p + p1 + 1, '|') - p;
        int p3 = strchr(p + p2 + 1, '|') - p;
        int p4 = strchr(p + p3 + 1, '|') - p;
        int p5 = strchr(p + p4 + 1, '|') - p;

        FieldConfig& fc = batFields[batFieldCount++];

        fc.label   = String(p, p1);
        fc.display = String(p + p1 + 1, p2 - p1 - 1);
        fc.factor  = String(p + p2 + 1, p3 - p2 - 1);
        fc.unit    = String(p + p3 + 1, p4 - p3 - 1);
        fc.mqtt    = (*(p + p4 + 1) == '1');
        fc.send    = (*(p + p5 + 1) == '1');
    }

    Log(LOG_INFO, String("BAT-LOAD: batFieldCount = ") + batFieldCount);
}


// -------------------------------------------------------------
// Load STAT configuration
// -------------------------------------------------------------
void AppConfig::loadStatConfig() {
    Preferences p;
    p.begin("battery_stat", true);

    battery.intervalStat = p.getULong("interval", battery.intervalStat);
    battery.enableStat   = p.getBool("enabled", battery.enableStat);

    p.end();
}

// -------------------------------------------------------------
// Save STAT configuration
// -------------------------------------------------------------
void AppConfig::saveStatConfig() {
    Preferences p;
    p.begin("battery_stat", false);

    p.putULong("interval", battery.intervalStat);
    p.putBool("enabled", battery.enableStat);

    p.end();
}

// -------------------------------------------------------------
// Save STAT field table (static, RAM‑optimized)
// -------------------------------------------------------------
void AppConfig::saveStatFields() {

    Preferences p;
    p.begin("battery_stat", false);

    // alte Chunks löschen
    for (int i = 0; i < 20; i++) {
        String key = "chunk_" + String(i);
        if (p.isKey(key.c_str())) p.remove(key.c_str());
    }

    const int FIELDS_PER_CHUNK = 20;

    int chunkIndex = 0;
    int fieldIndex = 0;

    while (fieldIndex < statFieldCount) {

        DynamicJsonDocument doc(2000);
        JsonArray arr = doc.createNestedArray("fields");

        for (int i = 0; i < FIELDS_PER_CHUNK && fieldIndex < statFieldCount; i++, fieldIndex++) {

            const FieldConfig& fc = statFields[fieldIndex];

            String packed;
            packed.reserve(80);
            packed += fc.label;   packed += "|";
            packed += fc.display; packed += "|";
            packed += fc.factor;  packed += "|";
            packed += fc.unit;    packed += "|";
            packed += (fc.mqtt ? "1" : "0"); packed += "|";
            packed += (fc.send ? "1" : "0");

            arr.add(packed);
        }

        String json;
        serializeJson(doc, json);

        String key = "chunk_" + String(chunkIndex);
        p.putString(key.c_str(), json);

        Log(LOG_INFO, String("STAT-SAVE: wrote chunk ") + chunkIndex +
                      " (" + json.length() + " bytes)");

        chunkIndex++;
    }

    p.putInt("chunk_count", chunkIndex);
    p.end();

    Log(LOG_INFO, String("STAT-SAVE: total chunks = ") + chunkIndex);
}


// -------------------------------------------------------------
// Load STAT field table
// -------------------------------------------------------------
void AppConfig::loadStatFields() {

    Preferences p;
    p.begin("battery_stat", true);

    int chunkCount = p.getInt("chunk_count", 0);
    if (chunkCount <= 0) {
        Log(LOG_WARN, "STAT-LOAD: no chunks found");
        statFieldCount = 0;
        p.end();
        return;
    }

    statFieldCount = 0;

    for (int c = 0; c < chunkCount; c++) {

        String key = "chunk_" + String(c);
        if (!p.isKey(key.c_str())) {
            Log(LOG_WARN, String("STAT-LOAD: missing chunk ") + key);
            continue;
        }

        String json = p.getString(key.c_str(), "");
        Log(LOG_INFO, String("STAT-LOAD: chunk ") + c +
                        " length=" + json.length());

        DynamicJsonDocument doc(2000);
        if (deserializeJson(doc, json)) {
            Log(LOG_ERROR, String("STAT-LOAD: JSON parse error in chunk ") + c);
            continue;
        }

        JsonArray arr = doc["fields"];
        if (arr.isNull()) continue;

        for (JsonVariant v : arr) {

            const char* packed = v.as<const char*>();
            if (!packed) continue;

            const char* pch = packed;

            const char* s1 = strchr(pch, '|');
            const char* s2 = strchr(s1 + 1, '|');
            const char* s3 = strchr(s2 + 1, '|');
            const char* s4 = strchr(s3 + 1, '|');
            const char* s5 = strchr(s4 + 1, '|');

            FieldConfig& fc = statFields[statFieldCount++];

            fc.label   = String(pch, s1 - pch);
            fc.display = String(s1 + 1, s2 - s1 - 1);
            fc.factor  = String(s2 + 1, s3 - s2 - 1);
            fc.unit    = String(s3 + 1, s4 - s3 - 1);
            fc.mqtt    = (*(s4 + 1) == '1');
            fc.send    = (*(s5 + 1) == '1');
        }
    }

    p.end();

    Log(LOG_INFO, String("STAT-LOAD: total fields = ") + statFieldCount);
}



// -------------------------------------------------------------
// Main load() and save()
// -------------------------------------------------------------
void AppConfig::load() {
    loadSystemConfig();

    loadPwrConfig();
    loadPwrFields();

    loadBatConfig();
    loadBatFields();

    loadStatConfig();
    loadStatFields();
}

void AppConfig::save() {
    saveSystemConfig();

    savePwrConfig();
    savePwrFields();

    saveBatConfig();
    saveBatFields();

    saveStatConfig();
    saveStatFields();
}

// -------------------------------------------------------------
// Timezone lookup helper
// -------------------------------------------------------------
String findPosixForTimezone(const String& tzName) {
    File f = SPIFFS.open("/timezone.json", "r");
    if (!f) {
        Serial.println("ERROR: timezone.json not found");
        return "UTC0";
    }

    String line;
    String match = "\"" + tzName + "\"";

    while (f.available()) {
        line = f.readStringUntil('\n');

        if (line.indexOf(match) >= 0) {
            String next = f.readStringUntil('\n');
            int p = next.indexOf("\"posix\"");
            if (p >= 0) {
                int q1 = next.indexOf(":", p);
                int q2 = next.indexOf("\"", q1 + 2);
                int q3 = next.indexOf("\"", q2 + 1);
                if (q2 >= 0 && q3 > q2) {
                    return next.substring(q2 + 1, q3);
                }
            }
        }
    }

    return "UTC0";
}

// -------------------------------------------------------------
// Uptime helper
// -------------------------------------------------------------
String AppConfig::uptimeString() {
    unsigned long ms = millis();
    unsigned long s  = ms / 1000;
    unsigned long m  = s / 60;
    unsigned long h  = m / 60;
    unsigned long d  = h / 24;

    char buf[64];
    snprintf(buf, sizeof(buf), "%lu d %02lu:%02lu:%02lu",
             d, h % 24, m % 60, s % 60);

    return String(buf);
}

// -------------------------------------------------------------
// Legacy parser flags (no longer used)
// -------------------------------------------------------------
bool parserHasData = false;
// bool newParserData = false;
bool batParserHasData = false;
int  batParserModuleIndex = 0;
bool statParserHasData = false;
int  statParserModuleIndex = 0;

// -------------------------------------------------------------
// Discovery flags
// -------------------------------------------------------------
bool discoveryPwrNeeded  = false;
bool discoveryBatNeeded  = false;
bool discoveryStatNeeded = false;

// -------------------------------------------------------------
// Global PWR table (used by Web + MQTT)
// -------------------------------------------------------------
PwrTable pwrTable;
