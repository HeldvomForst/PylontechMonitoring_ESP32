#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <map>

// ----------------------------------------------------
//  Single parser field configuration
// ----------------------------------------------------
struct FieldConfig {
    String label;     // Display name
    String factor;    // Conversion factor ("1", "0.001", "%", "text")
    String unit;      // Unit ("V", "A", "°C", "%", "")
    bool mqtt;        // MQTT enabled
    bool send;        // Send in MQTT payload
};

// ----------------------------------------------------
//  Battery configuration
// ----------------------------------------------------
struct BatteryConfig {
    unsigned long intervalPwr  = 60000;
    unsigned long intervalBat  = 300000;
    unsigned long intervalStat = 1800000;

    bool enableBat  = true;
    bool enableStat = true;

    bool useFahrenheit = false;

    std::map<String, FieldConfig> fields;
};

// ----------------------------------------------------
//  MQTT configuration
// ----------------------------------------------------
struct MqttConfig {
    bool enabled = false;

    String server = "";
    uint16_t port = 1883;
    String user = "";
    String pass = "";

    String prefix     = "Pylontech";
    String topicStack = "Stack";
    String topicPwr   = "pwr";
    String topicBat   = "bat";
    String topicStat  = "stat";

    String mode = "active";
};

// ----------------------------------------------------
//  AppConfig
// ----------------------------------------------------
class AppConfig {
public:
    // ===== 1. Basic settings =====
    String deviceName = "PylontechMonitor";
    String hostname   = "";
    String wifiSSID   = "";
    String wifiPass   = "";
    String apSSID     = "";
    String apPass     = "";
    bool setupDone    = false;

    // ===== 2. Network =====
    bool useStaticIP = false;
    String ipAddr     = "";
    String subnetMask = "";
    String gateway    = "";
    String dns        = "";

    // ===== 2.1 NTP =====
    String ntpServer = "pool.ntp.org";
    // ===== 2.2 Timezone & DST =====
    // IANA timezone name (e.g. "Europe/Berlin", "America/New_York")
    String timezone = "Europe/Berlin";

    // Enable or disable daylight saving time (DST)
    bool daylightSaving = true;

    // NTP resync interval in seconds (default: 24h)
    uint32_t ntpResyncInterval = 86400; 


    // ===== 3. MQTT =====
    MqttConfig mqtt;

    // ===== 4. Battery =====
    BatteryConfig battery;

    // ===== 5. Dashboard fields =====
    String firmwareVersion = "0.Beta.3";
    String currentTime     = "";
    String lastPwrUpdate   = "";
    uint16_t detectedModules = 0;

    // ===== 6. MQTT status for dashboard =====
    String lastMqttContact = "";

    // ===== Methods =====
    void load();
    void save();
    void clearNVS();
    void factoryDefaults();
    void factoryReset();
    void generateDefaultsIfNeeded();

    // Uptime helper
    String uptimeString();
    
    // Time helpers
    String getCurrentTimeString();
    bool isSystemTimeValid();

    //log level 
    bool logInfo  = true;
    bool logWarn  = true;
    bool logError = true;
    bool logDebug = false;




private:
    String generateHostname();
};

// Global instance
extern AppConfig config;