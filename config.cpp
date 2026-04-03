#include "config.h"
#include "py_log.h"
#include <ArduinoJson.h>



AppConfig config;

// ---------------------------------------------------------
// Central timezone list (compact, grouped by region)
// ---------------------------------------------------------
const TimezoneEntry TIMEZONES[] = {
    // Europe
    {"Europe", "London",   "Europe/London",   "GMT0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00"},
    {"Europe", "Berlin",   "Europe/Berlin",   "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"},
    {"Europe", "Paris",    "Europe/Paris",    "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"},
    {"Europe", "Rome",     "Europe/Rome",     "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"},
    {"Europe", "Madrid",   "Europe/Madrid",   "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"},
    {"Europe", "Vienna",   "Europe/Vienna",   "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"},
    {"Europe", "Athens",   "Europe/Athens",   "EET-2EEST-3,M3.5.0/03:00:00,M10.5.0/04:00:00"},
    {"Europe", "Moscow",   "Europe/Moscow",   "MSK-3"},

    // America
    {"America", "New York","America/New_York","EST+5EDT+4,M3.2.0/02:00:00,M11.1.0/02:00:00"},
    {"America", "Chicago", "America/Chicago", "CST+6CDT+5,M3.2.0/02:00:00,M11.1.0/02:00:00"},
    {"America", "Denver",  "America/Denver",  "MST+7MDT+6,M3.2.0/02:00:00,M11.1.0/02:00:00"},
    {"America", "Los Angeles","America/Los_Angeles","PST+8PDT+7,M3.2.0/02:00:00,M11.1.0/02:00:00"},
    {"America", "Anchorage","America/Anchorage","AKST+9AKDT+8,M3.2.0/02:00:00,M11.1.0/02:00:00"},
    {"Pacific", "Honolulu","Pacific/Honolulu","HST+10"},

    // South America
    {"America", "Sao Paulo","America/Sao_Paulo","BRT+3"},
    {"America", "Buenos Aires","America/Argentina/Buenos_Aires","ART+3"},
    {"America", "Santiago","America/Santiago","CLT+4CLST+3,M9.1.6/24:00:00,M4.1.6/24:00:00"},

    // Australia / NZ
    {"Australia","Sydney", "Australia/Sydney","AEST-10AEDT-11,M10.1.0/02:00:00,M4.1.0/03:00:00"},
    {"Australia","Melbourne","Australia/Melbourne","AEST-10AEDT-11,M10.1.0/02:00:00,M4.1.0/03:00:00"},
    {"Pacific", "Auckland","Pacific/Auckland","NZST-12NZDT-13,M9.5.0/02:00:00,M4.1.0/03:00:00"},

    // Asia
    {"Asia", "Dubai",      "Asia/Dubai",      "GST-4"},
    {"Asia", "Karachi",    "Asia/Karachi",    "PKT-5"},
    {"Asia", "Dhaka",      "Asia/Dhaka",      "BDT-6"},
    {"Asia", "Bangkok",    "Asia/Bangkok",    "ICT-7"},
    {"Asia", "Singapore",  "Asia/Singapore",  "SGT-8"},
    {"Asia", "Shanghai",   "Asia/Shanghai",   "CST-8"},
    {"Asia", "Tokyo",      "Asia/Tokyo",      "JST-9"},
    {"Asia", "Seoul",      "Asia/Seoul",      "KST-9"},

    // Africa
    {"Africa", "Johannesburg","Africa/Johannesburg","SAST-2"},
    {"Africa", "Cairo",       "Africa/Cairo",      "EET-2"},
    {"Africa", "Nairobi",     "Africa/Nairobi",    "EAT-3"},

    // GMT Offsets
    {"GMT", "GMT-12", "Etc/GMT+12", "GMT+12"},
    {"GMT", "GMT-11", "Etc/GMT+11", "GMT+11"},
    {"GMT", "GMT-10", "Etc/GMT+10", "GMT+10"},
    {"GMT", "GMT-9",  "Etc/GMT+9",  "GMT+9"},
    {"GMT", "GMT-8",  "Etc/GMT+8",  "GMT+8"},
    {"GMT", "GMT-7",  "Etc/GMT+7",  "GMT+7"},
    {"GMT", "GMT-6",  "Etc/GMT+6",  "GMT+6"},
    {"GMT", "GMT-5",  "Etc/GMT+5",  "GMT+5"},
    {"GMT", "GMT-4",  "Etc/GMT+4",  "GMT+4"},
    {"GMT", "GMT-3",  "Etc/GMT+3",  "GMT+3"},
    {"GMT", "GMT-2",  "Etc/GMT+2",  "GMT+2"},
    {"GMT", "GMT-1",  "Etc/GMT+1",  "GMT+1"},
    {"GMT", "GMT+0",  "Etc/GMT",    "GMT0"},
    {"GMT", "GMT+1",  "Etc/GMT-1",  "GMT-1"},
    {"GMT", "GMT+2",  "Etc/GMT-2",  "GMT-2"},
    {"GMT", "GMT+3",  "Etc/GMT-3",  "GMT-3"},
    {"GMT", "GMT+4",  "Etc/GMT-4",  "GMT-4"},
    {"GMT", "GMT+5",  "Etc/GMT-5",  "GMT-5"},
    {"GMT", "GMT+6",  "Etc/GMT-6",  "GMT-6"},
    {"GMT", "GMT+7",  "Etc/GMT-7",  "GMT-7"},
    {"GMT", "GMT+8",  "Etc/GMT-8",  "GMT-8"},
    {"GMT", "GMT+9",  "Etc/GMT-9",  "GMT-9"},
    {"GMT", "GMT+10", "Etc/GMT-10", "GMT-10"},
    {"GMT", "GMT+11", "Etc/GMT-11", "GMT-11"},
    {"GMT", "GMT+12", "Etc/GMT-12", "GMT-12"}
};

const size_t TIMEZONE_COUNT = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

static const size_t CHUNK_SIZE = 1500;


// ----------------------------------------------------
//  Helper: Save JSON in chunks
// ----------------------------------------------------
void AppConfig::saveJsonChunked(const char* ns, const char* prefix, const String& json) {

    Log(LOG_INFO, String("NVS-CHUNK: Saving JSON for namespace '") + ns + "', prefix '" + prefix + "'");
    Log(LOG_INFO, String("NVS-CHUNK: JSON length = ") + json.length());

    Preferences p;
    p.begin(ns, false);

    // Alte Chunks entfernen
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
// ----------------------------------------------------
//  Helper: Load JSON from chunks
// ----------------------------------------------------
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

// ----------------------------------------------------
//  Generate hostname from MAC address
// ----------------------------------------------------
String AppConfig::generateHostname() {
    uint64_t mac = ESP.getEfuseMac();
    uint16_t last = mac & 0xFFFF;

    char buf[32];
    snprintf(buf, sizeof(buf), "pylontech-%04X", last);
    return String(buf);
}

// ----------------------------------------------------
//  Generate Time
// ----------------------------------------------------
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

// ----------------------------------------------------
//  NVS komplett löschen
// ----------------------------------------------------
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

// ----------------------------------------------------
//  Default-Werte setzen (ohne löschen, ohne reboot)
// ----------------------------------------------------
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

    // NTP
    ntpServer = "pool.ntp.org";
    // Manual time mode
    manual_mode = false;
    manual_date = "";
    manual_time = "";
    manual_dst  = false;

    use_gateway_ntp = true;   // factory default
    manual_ntp      = false;

    // Default PWR fields
    battery.fieldsPwr.clear();

    auto add = [&](std::map<String, FieldConfig>& map, String key, String label, String factor, String unit, bool active){
        FieldConfig f;
            f.label = label;
            f.display = label;   // NEW: default display name = label
            f.factor = factor;
            f.unit = unit;
            f.mqtt = active;
            f.send = active;

            map[key] = f;

    };

    add(battery.fieldsPwr, "Volt",    "Voltage",     "0.001", "V",  true);
    add(battery.fieldsPwr, "Curr",    "Current",     "0.001", "A",  true);
    add(battery.fieldsPwr, "Tempr",   "Temperature", "0.001", "°C", true);
    add(battery.fieldsPwr, "Coulomb", "SOC",         "1",     "%",  true);

    // BAT + STAT empty by default
    battery.fieldsBat.clear();
    battery.fieldsStat.clear();

    firmwareVersion = "0.RC.1";
    currentTime     = "";
    lastPwrUpdate   = "";
    detectedModules = 0;
    lastMqttContact = "";

    Log(LOG_INFO, "factoryDefaults(): default fields set");
}

// ----------------------------------------------------
//  Factory Reset (löscht ALLES + reboot)
// ----------------------------------------------------
void AppConfig::factoryReset() {
    Log(LOG_WARN, "Factory reset triggered");
    clearNVS();
    delay(200);
    ESP.restart();
}

// ----------------------------------------------------
//  Load ONLY system configuration
// ----------------------------------------------------
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
    // Manual time mode
    manual_mode = p.getBool("manual_mode", false);
    manual_date = p.getString("manual_date", "");
    manual_time = p.getString("manual_time", "");
    manual_dst  = p.getBool("manual_dst", false);

    // NTP modes
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

    firmwareVersion = p.getString("fw_ver", firmwareVersion);
    currentTime     = p.getString("cur_time", currentTime);
    lastPwrUpdate   = p.getString("pwr_last", lastPwrUpdate);
    detectedModules = p.getUShort("pwr_mods", detectedModules);
    lastMqttContact = p.getString("mqtt_last", lastMqttContact);

    logInfo  = p.getBool("log_info",  true);
    logWarn  = p.getBool("log_warn",  true);
    logError = p.getBool("log_error", true);
    logDebug = p.getBool("log_debug", false);

    p.end();

    // ----------------------------------------------------
    // Defaults laden, wenn Config leer ist
    // ----------------------------------------------------
    if (hostname.length() == 0 || apSSID.length() == 0) {
        Log(LOG_WARN, "Config empty → applying factory defaults");
        factoryDefaults();
        setupDone = true;
        saveSystemConfig();
    }
}


// ----------------------------------------------------
//  Save ONLY system configuration
// ----------------------------------------------------
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
    // Manual time mode
    p.putBool("manual_mode", manual_mode);
    p.putString("manual_date", manual_date);
    p.putString("manual_time", manual_time);
    p.putBool("manual_dst", manual_dst);

    // NTP modes
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

    p.putString("fw_ver", firmwareVersion);
    p.putString("cur_time", currentTime);
    p.putString("pwr_last", lastPwrUpdate);
    p.putUShort("pwr_mods", detectedModules);
    p.putString("mqtt_last", lastMqttContact);

    p.putBool("log_info",  logInfo);
    p.putBool("log_warn",  logWarn);
    p.putBool("log_error", logError);
    p.putBool("log_debug", logDebug);

    p.end();
}

// ----------------------------------------------------
//  PWR CONFIG
// ----------------------------------------------------
void AppConfig::loadPwrConfig() {
    Preferences p;
    p.begin("battery_pwr", true);

    battery.intervalPwr = p.getULong("interval", battery.intervalPwr);
    battery.enableBat   = p.getBool("enabled", battery.enableBat);

    p.end();
}

void AppConfig::savePwrConfig() {
    Preferences p;
    p.begin("battery_pwr", false);

    p.putULong("interval", battery.intervalPwr);
    p.putBool("enabled", battery.enableBat);

    p.end();
}

// ----------------------------------------------------
//  PWR FIELDS (JSON + chunks)
// ----------------------------------------------------
void AppConfig::savePwrFields() {
    DynamicJsonDocument doc(3000);
    JsonArray arr = doc.createNestedArray("fields");

    for (auto &kv : battery.fieldsPwr) {
        const String& name = kv.first;
        const FieldConfig& fc = kv.second;

        String packed;
        packed.reserve(80);
        packed += name; packed += "|";
        packed += fc.display; packed += "|";
        packed += fc.factor; packed += "|";
        packed += fc.unit; packed += "|";
        packed += (fc.mqtt ? "1" : "0"); packed += "|";
        packed += (fc.send ? "1" : "0");

        arr.add(packed);
    }

    String json;
    json.reserve(3000);
    serializeJson(doc, json);

    saveJsonChunked("battery_pwr", "pwr", json);
}

void AppConfig::loadPwrFields() {
    String json = loadJsonChunked("battery_pwr", "pwr");
    if (json.length() == 0) return;

    DynamicJsonDocument doc(3000);
    if (deserializeJson(doc, json)) return;

    JsonArray arr = doc["fields"];
    if (arr.isNull()) return;

    battery.fieldsPwr.clear();

    for (JsonVariant v : arr) {
        const char* packed = v.as<const char*>();
        if (!packed) continue;

        const char* p = packed;

        int p1 = strchr(p, '|') - p;
        int p2 = strchr(p + p1 + 1, '|') - p;
        int p3 = strchr(p + p2 + 1, '|') - p;
        int p4 = strchr(p + p3 + 1, '|') - p;
        int p5 = strchr(p + p4 + 1, '|') - p;

        String name(p, p1);
        String display(p + p1 + 1, p2 - p1 - 1);
        String factor(p + p2 + 1, p3 - p2 - 1);
        String unit(p + p3 + 1, p4 - p3 - 1);
        bool mqtt = (*(p + p4 + 1) == '1');
        bool send = (*(p + p5 + 1) == '1');

        FieldConfig fc;
        fc.label = display;
        fc.display = display;
        fc.factor = factor;
        fc.unit = unit;
        fc.mqtt = mqtt;
        fc.send = send;

        battery.fieldsPwr[name] = fc;
    }
}

void AppConfig::saveBatConfig() {
    Preferences p;
    p.begin("battery_bat", false);

    p.putULong("interval", battery.intervalBat);
    p.putBool("enabled", battery.enableBat);

    p.end();
}

// ----------------------------------------------------
//  BAT FIELDS (JSON + chunks)
// ----------------------------------------------------
void AppConfig::saveBatFields() {
    DynamicJsonDocument doc(3000);
    JsonArray arr = doc.createNestedArray("fields");

    for (auto &kv : battery.fieldsBat) {
        const String& name = kv.first;
        const FieldConfig& fc = kv.second;

        String packed;
        packed.reserve(80);
        packed += name; packed += "|";
        packed += fc.display; packed += "|";
        packed += fc.factor; packed += "|";
        packed += fc.unit; packed += "|";
        packed += (fc.mqtt ? "1" : "0"); packed += "|";
        packed += (fc.send ? "1" : "0");

        arr.add(packed);
    }

    String json;
    json.reserve(3000);
    serializeJson(doc, json);

    saveJsonChunked("battery_bat", "bat", json);
}

// ----------------------------------------------------
//  BAT CONFIG
// ----------------------------------------------------
void AppConfig::loadBatConfig() {
    Preferences p;
    p.begin("battery_bat", true);

    battery.intervalBat = p.getULong("interval", battery.intervalBat);
    battery.enableBat   = p.getBool("enabled", battery.enableBat);

    p.end();
}

void AppConfig::loadBatFields() {
    String json = loadJsonChunked("battery_bat", "bat");
    if (json.length() == 0) return;

    DynamicJsonDocument doc(3000);
    if (deserializeJson(doc, json)) return;

    JsonArray arr = doc["fields"];
    if (arr.isNull()) return;

    battery.fieldsBat.clear();

    for (JsonVariant v : arr) {
        const char* packed = v.as<const char*>();
        if (!packed) continue;

        const char* p = packed;

        int p1 = strchr(p, '|') - p;
        int p2 = strchr(p + p1 + 1, '|') - p;
        int p3 = strchr(p + p2 + 1, '|') - p;
        int p4 = strchr(p + p3 + 1, '|') - p;
        int p5 = strchr(p + p4 + 1, '|') - p;

        String name(p, p1);
        String display(p + p1 + 1, p2 - p1 - 1);
        String factor(p + p2 + 1, p3 - p2 - 1);
        String unit(p + p3 + 1, p4 - p3 - 1);
        bool mqtt = (*(p + p4 + 1) == '1');
        bool send = (*(p + p5 + 1) == '1');

        FieldConfig fc;
        fc.label = display;
        fc.display = display;
        fc.factor = factor;
        fc.unit = unit;
        fc.mqtt = mqtt;
        fc.send = send;

        battery.fieldsBat[name] = fc;
    }
}

// ----------------------------------------------------
//  STAT CONFIG
// ----------------------------------------------------
void AppConfig::loadStatConfig() {
    Preferences p;
    p.begin("battery_stat", true);

    battery.intervalStat = p.getULong("interval", battery.intervalStat);
    battery.enableStat   = p.getBool("enabled", battery.enableStat);

    p.end();
}

void AppConfig::saveStatConfig() {
    Preferences p;
    p.begin("battery_stat", false);

    p.putULong("interval", battery.intervalStat);
    p.putBool("enabled", battery.enableStat);

    p.end();
}

// ----------------------------------------------------
//  STAT FIELDS (JSON + chunks)
// ----------------------------------------------------
void AppConfig::saveStatFields() {
    DynamicJsonDocument doc(3000);
    JsonArray arr = doc.createNestedArray("fields");

    for (auto &kv : battery.fieldsStat) {
        const String& name = kv.first;
        const FieldConfig& fc = kv.second;

        String packed;
        packed.reserve(80);
        packed += name; packed += "|";
        packed += fc.display; packed += "|";
        packed += fc.factor; packed += "|";
        packed += fc.unit; packed += "|";
        packed += (fc.mqtt ? "1" : "0"); packed += "|";
        packed += (fc.send ? "1" : "0");

        arr.add(packed);
    }

    String json;
    json.reserve(3000);
    serializeJson(doc, json);

    saveJsonChunked("battery_stat", "stat", json);
}

void AppConfig::loadStatFields() {
    String json = loadJsonChunked("battery_stat", "stat");
    if (json.length() == 0) return;

    DynamicJsonDocument doc(3000);
    if (deserializeJson(doc, json)) return;

    JsonArray arr = doc["fields"];
    if (arr.isNull()) return;

    battery.fieldsStat.clear();

    for (JsonVariant v : arr) {
        const char* packed = v.as<const char*>();
        if (!packed) continue;

        const char* p = packed;

        int p1 = strchr(p, '|') - p;
        int p2 = strchr(p + p1 + 1, '|') - p;
        int p3 = strchr(p + p2 + 1, '|') - p;
        int p4 = strchr(p + p3 + 1, '|') - p;
        int p5 = strchr(p + p4 + 1, '|') - p;

        String name(p, p1);
        String display(p + p1 + 1, p2 - p1 - 1);
        String factor(p + p2 + 1, p3 - p2 - 1);
        String unit(p + p3 + 1, p4 - p3 - 1);
        bool mqtt = (*(p + p4 + 1) == '1');
        bool send = (*(p + p5 + 1) == '1');

        FieldConfig fc;
        fc.label = display;
        fc.display = display;
        fc.factor = factor;
        fc.unit = unit;
        fc.mqtt = mqtt;
        fc.send = send;

        battery.fieldsStat[name] = fc;
    }
}

// ----------------------------------------------------
//  Main load() and save()
// ----------------------------------------------------
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

String getTimezoneJson() {
    DynamicJsonDocument doc(4096);

    // Wir erzeugen Arrays nur einmal pro Region
    std::map<String, JsonArray> regionArrays;

    for (size_t i = 0; i < TIMEZONE_COUNT; i++) {
        const TimezoneEntry& tz = TIMEZONES[i];

        // Falls Region noch nicht existiert → Array anlegen
        if (!regionArrays.count(tz.region)) {
            regionArrays[tz.region] = doc.createNestedArray(tz.region);
        }

        JsonArray arr = regionArrays[tz.region];

        JsonObject o = arr.createNestedObject();
        o["city"] = tz.city;
        o["tz"]   = tz.tzName;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

String findPosixForTimezone(const String& tzName) {
    const char* target = tzName.c_str();

    for (size_t i = 0; i < TIMEZONE_COUNT; i++) {
        // Direkter Vergleich ohne String-Objekte
        if (strcmp(target, TIMEZONES[i].tzName) == 0) {
            return TIMEZONES[i].posix;
        }
    }

    // Fallback
    return "UTC0";
}




// ----------------------------------------------------
//  Uptime helper
// ----------------------------------------------------
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