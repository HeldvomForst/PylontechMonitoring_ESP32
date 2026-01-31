#include "config.h"
#include "py_log.h"

AppConfig config;

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

    battery.enableBat  = false;
    battery.enableStat = false;
    battery.useFahrenheit = false;

    // MQTT defaults
    mqtt.enabled = false;
    mqtt.server  = "";
    mqtt.port    = 1883;
    mqtt.user    = "";
    mqtt.pass    = "";

    mqtt.prefix     = "Pylontech";
    mqtt.topicStack = "Stack";
    mqtt.topicPwr   = "Modul";
    mqtt.topicBat   = "bat";
    mqtt.topicStat  = "stat";
    mqtt.mode       = "active";

    // Hostname / AP
    hostname = generateHostname();
    apSSID   = hostname;

    // NTP
    ntpServer = "pool.ntp.org";

    // Parser fields (8 harte Felder)
    battery.fields.clear();

    auto add = [&](String key, String label, String factor, String unit, bool active){
        FieldConfig f;
        f.label = label;
        f.factor = factor;
        f.unit = unit;
        f.mqtt = active;
        f.send = active;
        battery.fields[key] = f;
    };

    // ---- Batterie-Felder ----
    add("Volt",    "Voltage",     "0.001", "V",  true);
    add("Curr",    "Current",     "0.001", "A",  true);
    add("Tempr",   "Temperature", "0.001", "°C", true);
    add("Coulomb", "SOC",         "1",     "%",  true);

    // ---- Stack-Felder ----
    add("StackVoltAvg",  "Voltage Avg",  "0.001", "V",  true);
    add("StackCurrSum",  "Current Sum",  "0.001", "A",  true);
    add("StackTempMax",  "Temperature",  "0.001", "°C", true);
    add("BatteryCount",  "Battery Count",      "1",     "",   true);

    // Dashboard defaults
    firmwareVersion = "1.0.0";
    currentTime     = "";
    lastPwrUpdate   = "";
    detectedModules = 0;

    lastMqttContact = "";

    Log(LOG_INFO, "factoryDefaults(): 8 fields set");
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
//  Load configuration from NVS
// ----------------------------------------------------
void AppConfig::load() {
    Preferences p;
    p.begin("config", true);

    // Basic
    deviceName = p.getString("devname", deviceName);
    hostname   = p.getString("hostname", hostname);
    wifiSSID   = p.getString("wifi_ssid", wifiSSID);
    wifiPass   = p.getString("wifi_pass", wifiPass);
    apSSID     = p.getString("ap_ssid", apSSID);
    apPass     = p.getString("ap_pass", apPass);
    setupDone  = p.getBool("setup", setupDone);

    // Network
    useStaticIP = p.getBool("net_static", useStaticIP);
    ipAddr      = p.getString("net_ip", ipAddr);
    subnetMask  = p.getString("net_mask", subnetMask);
    gateway     = p.getString("net_gw", gateway);
    dns         = p.getString("net_dns", dns);
    // Time
    ntpServer   = p.getString("ntp_srv", ntpServer);
    timezone = p.getString("tz_name", timezone);
    daylightSaving = p.getBool("tz_dst", daylightSaving);
    ntpResyncInterval = p.getULong("ntp_resync", ntpResyncInterval);

    // MQTT
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

    // Battery
    battery.intervalPwr  = p.getULong("bat_pwr",  battery.intervalPwr);
    battery.intervalBat  = p.getULong("bat_bat",  battery.intervalBat);
    battery.intervalStat = p.getULong("bat_stat", battery.intervalStat);
    battery.enableBat    = p.getBool("bat_en_bat",  battery.enableBat);
    battery.enableStat   = p.getBool("bat_en_stat", battery.enableStat);
    battery.useFahrenheit = p.getBool("bat_fahr", battery.useFahrenheit);

    // Parser fields
    int count = p.getInt("field_count", 0);
    battery.fields.clear();

    for (int i = 0; i < count; i++) {
        String name = p.getString(("f" + String(i) + "_name").c_str(), "");
        if (name.length() == 0) continue;

        FieldConfig f;
        f.label  = p.getString(("f" + String(i) + "_label").c_str(), name);
        f.factor = p.getString(("f" + String(i) + "_factor").c_str(), "1");
        f.unit   = p.getString(("f" + String(i) + "_unit").c_str(), "");
        f.mqtt   = p.getBool(("f" + String(i) + "_mqtt").c_str(), false);
        f.send   = p.getBool(("f" + String(i) + "_send").c_str(), false);

        battery.fields[name] = f;
    }

    // Dashboard
    firmwareVersion = p.getString("fw_ver", firmwareVersion);
    currentTime     = p.getString("cur_time", currentTime);
    lastPwrUpdate   = p.getString("pwr_last", lastPwrUpdate);
    detectedModules = p.getUShort("pwr_mods", detectedModules);

    lastMqttContact = p.getString("mqtt_last", lastMqttContact);
    // Log-Level
    logInfo  = p.getBool("log_info",  true);
    logWarn  = p.getBool("log_warn",  true);
    logError = p.getBool("log_error", true);
    logDebug = p.getBool("log_debug", false);

    p.end();

    Log(LOG_INFO, "Config: battery.fields count = " + String(battery.fields.size()));

    // ----------------------------------------------------
    //  WICHTIG: Wenn keine Felder → NVS löschen + Defaults
    // ----------------------------------------------------
    if (battery.fields.empty()) {
        Log(LOG_WARN, "Config invalid or missing → clearing NVS + applying defaults");
        clearNVS();
        factoryDefaults();
        save();
    }
}

// ----------------------------------------------------
//  Save configuration to NVS
// ----------------------------------------------------
void AppConfig::save() {
    Preferences p;
    p.begin("config", false);

    // Basic
    p.putString("devname", deviceName);
    p.putString("hostname", hostname);
    p.putString("wifi_ssid", wifiSSID);
    p.putString("wifi_pass", wifiPass);
    p.putString("ap_ssid", apSSID);
    p.putString("ap_pass", apPass);
    p.putBool("setup", setupDone);

    // Network
    p.putBool("net_static", useStaticIP);
    p.putString("net_ip", ipAddr);
    p.putString("net_mask", subnetMask);
    p.putString("net_gw", gateway);
    p.putString("net_dns", dns);
    //Time
    p.putString("ntp_srv", ntpServer);
    p.putString("tz_name", timezone);
    p.putBool("tz_dst", daylightSaving);
    p.putULong("ntp_resync", ntpResyncInterval);

    // MQTT
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

    // Battery
    p.putULong("bat_pwr",  battery.intervalPwr);
    p.putULong("bat_bat",  battery.intervalBat);
    p.putULong("bat_stat", battery.intervalStat);
    p.putBool("bat_en_bat",  battery.enableBat);
    p.putBool("bat_en_stat", battery.enableStat);
    p.putBool("bat_fahr", battery.useFahrenheit);

    // Parser fields
    p.putInt("field_count", battery.fields.size());

    int i = 0;
    for (auto &kv : battery.fields) {
        String name = kv.first;
        FieldConfig &f = kv.second;

        p.putString(("f" + String(i) + "_name").c_str(), name);
        p.putString(("f" + String(i) + "_label").c_str(), f.label);
        p.putString(("f" + String(i) + "_factor").c_str(), f.factor);
        p.putString(("f" + String(i) + "_unit").c_str(), f.unit);
        p.putBool(("f" + String(i) + "_mqtt").c_str(), f.mqtt);
        p.putBool(("f" + String(i) + "_send").c_str(), f.send);

        i++;
    }

    // Dashboard
    p.putString("fw_ver", firmwareVersion);
    p.putString("cur_time", currentTime);
    p.putString("pwr_last", lastPwrUpdate);
    p.putUShort("pwr_mods", detectedModules);

    p.putString("mqtt_last", lastMqttContact);

    // Log-Level
    p.putBool("log_info",  logInfo);
    p.putBool("log_warn",  logWarn);
    p.putBool("log_error", logError);
    p.putBool("log_debug", logDebug);

    p.end();
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