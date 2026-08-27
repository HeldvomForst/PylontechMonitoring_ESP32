#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <map>          // ⚠ dynamic RAM (PWR + STAT config)
#include <vector>       // ⚠ dynamic RAM (legacy parser structs)
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

// -------------------------------------------------------------
// Mutexes
// -------------------------------------------------------------
extern portMUX_TYPE batMux;
extern portMUX_TYPE statMux;

// -------------------------------------------------------------
// Global constants
// -------------------------------------------------------------
#define PWR_MAX_ROWS        32
#define PWR_MAX_COLS        32
#define BAT_MAX_COLS        20
#define BAT_MAX_ROWS        20
#define STAT_MAX_FIELDS     256

#define PWR_WORKBUF_SIZE    8192
#define BAT_WORKBUF_SIZE    4096
#define STAT_WORKBUF_SIZE   4096

// -------------------------------------------------------------
// PWR table (RAM‑optimized)
// -------------------------------------------------------------
struct PwrTable {
    int rows;
    int cols;
    const char* header[PWR_MAX_COLS];
    const char* cell[PWR_MAX_ROWS][PWR_MAX_COLS];
};

// -------------------------------------------------------------
// STAT table (RAM‑optimized)
// -------------------------------------------------------------
struct StatTable {
    int count = 0;
    const char* name[STAT_MAX_FIELDS];
    const char* value[STAT_MAX_FIELDS];
};

// -------------------------------------------------------------
// BAT table (RAM‑optimized)
// -------------------------------------------------------------
struct BatTable {
    int cols = 0;
    int rows = 0;

    const char* header[BAT_MAX_COLS];
    const char* cell[BAT_MAX_ROWS][BAT_MAX_COLS];
};

// -------------------------------------------------------------
// Timezone (optional)
// -------------------------------------------------------------
struct TimezoneEntry {
    const char* region;
    const char* city;
    const char* tzName;
    const char* posix;
};

String getTimezoneJson();                 // ⚠ optional
String findPosixForTimezone(const String& tzName);
extern const TimezoneEntry TIMEZONES[];   // ⚠ optional
extern const size_t TIMEZONE_COUNT;       // ⚠ optional

extern const char OTA_MAGIC_HEADER[];     // ⚠ optional

// -------------------------------------------------------------
// PWR frame status
// -------------------------------------------------------------
extern volatile bool pwrFrameReady;
extern int pwrTotalModules;
extern int pwrCurrentModule;

// -------------------------------------------------------------
// FieldConfig (shared by PWR, BAT, STAT)
// -------------------------------------------------------------
struct FieldConfig {
    String label;
    String display;
    String factor;
    String unit;
    bool mqtt;
    bool send;
};

// -------------------------------------------------------------
// Global BAT field configuration (RAM‑optimized)
// -------------------------------------------------------------
extern FieldConfig batFields[BAT_MAX_COLS];
extern int batFieldCount;

// -------------------------------------------------------------
// Global STAT field configuration (RAM‑optimized target)
// -------------------------------------------------------------
extern FieldConfig statFields[STAT_MAX_FIELDS];   
extern int statFieldCount;                        

// -------------------------------------------------------------
// Battery configuration (intervals + flags)
// -------------------------------------------------------------
struct BatteryConfig {
    unsigned long intervalPwr  = 60000;
    unsigned long intervalBat  = 300000;
    unsigned long intervalStat = 1800000;

    bool enableBat  = false;
    bool enableStat = false;

    bool useFahrenheit = false;

    uint8_t maxModules = 16;

    std::map<String, FieldConfig> fieldsPwr;   // ⚠ dynamic RAM (kept)
    //std::map<String, FieldConfig> fieldsStat;  // ⚠ WILL BE REMOVED (STAT → static)
    
    float cellDiffWarn  = 0.020f;
    float cellDiffError = 0.040f;
};

// -------------------------------------------------------------
// MQTT configuration
// -------------------------------------------------------------
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
    String cellPrefix = "Cell";

    String mode = "active";
};

// -------------------------------------------------------------
// Battery module + stack (PWR)
// -------------------------------------------------------------
struct BatteryModule {
    bool present = false;
    int index = 0;
    int voltage_mV = 0;
    int current_mA = 0;
    int temperature = 0;
    int soc = 0;

    //std::map<String, String> fields;   // ⚠ dynamic RAM (legacy)
};

struct BatteryStack {
    int batteryCount = 0;
    int avgVoltage_mV = 0;
    int totalCurrent_mA = 0;
    int temperature = 0;
    int soc = 0;

    void reset() {
        batteryCount = 0;
        avgVoltage_mV = 0;
        totalCurrent_mA = 0;
        temperature = 0;
        soc = 0;
    }
};

// -------------------------------------------------------------
// PWR field table (RAM‑optimized)
// -------------------------------------------------------------
struct PwrField {
    String name;
    String display;
    String factor;
    String unit;
    bool mqtt;
    bool send;
};

extern PwrField pwrFields[32];
extern int pwrFieldCount;

// -------------------------------------------------------------
// ❌ Legacy BAT raw field (parser only)
// -------------------------------------------------------------
// struct BatField {
//     String name;
//     String raw;
// };

// -------------------------------------------------------------
// ❌ Legacy BAT parsed data (parser only)
// -------------------------------------------------------------
// struct BatData {
//     int moduleIndex;
//     int cellIndex = -1;
//     std::vector<BatField> fields;
// };

// -------------------------------------------------------------
// ❌ Legacy STAT raw field (parser only)
// -------------------------------------------------------------
// struct StatField {
//     String name;
//     String raw;
// };

// -------------------------------------------------------------
// ❌ Legacy STAT parsed data (parser only)
// -------------------------------------------------------------
// struct StatData {
//     int moduleIndex = -1;
//     std::vector<StatField> fields;
// };

// -------------------------------------------------------------
// ❌ Legacy ParsedData (not used anymore)
// -------------------------------------------------------------
// struct ParsedData {
//     BatteryStack stack;
//     std::vector<BatteryModule> modules;
//     std::vector<BatData> batCells;
//     StatData stat;
// };

// -------------------------------------------------------------
// ❌ Legacy double buffers (not used anymore)
// -------------------------------------------------------------
// struct PwrBuffer {
//     BatteryStack stack;
//     std::vector<BatteryModule> modules;
// };

// struct BatBuffer {
//     BatTable table;
// };

// struct StatBuffer {
//     StatData stat;
// };

// -------------------------------------------------------------
// Parser result
// -------------------------------------------------------------
enum ParseResult {
    PARSE_OK,
    PARSE_FAIL,
    PARSE_IGNORED
};

#define MAX_MODULES 16

// -------------------------------------------------------------
// ❌ Legacy parser flags (not used anymore)
// -------------------------------------------------------------
extern bool parserHasData;
// extern bool newParserData;
extern bool batParserHasData;
extern int  batParserModuleIndex;
extern bool statParserHasData;
extern int  statParserModuleIndex;

extern bool discoveryPwrNeeded;
extern bool discoveryBatNeeded;
extern bool discoveryStatNeeded;

// -------------------------------------------------------------
// AppConfig
// -------------------------------------------------------------
class AppConfig {
public:
    String deviceName = "PylontechMonitor";
    String hostname   = "";
    String wifiSSID   = "";
    String wifiPass   = "";
    String apSSID     = "";
    String apPass     = "";
    bool   setupDone  = false;

    bool   useStaticIP = false;
    String ipAddr      = "";
    String subnetMask  = "";
    String gateway     = "";
    String dns         = "";

    // NTP (kept)
    String ntpServer = "pool.ntp.org";
    bool   manual_mode = false;
    bool   manual_dst  = false;
    bool   use_gateway_ntp = true;
    bool   manual_ntp      = false;
    String manual_date = "";
    String manual_time = "";

    String timezone = "Europe/Berlin";
    bool   daylightSaving = true;
    uint32_t ntpResyncInterval = 86400;

    MqttConfig    mqtt;
    BatteryConfig battery;

    String   firmwareVersion  = "1.4.0";
    String   currentTime      = "";
    uint16_t detectedModules  = 0;
    String   lastPwrUpdate    = "";
    String   lastBatUpdate    = "";
    String   lastStatUpdate    = "";
    String   lastUartUpdate    = "";

    void load();
    void save();

    void loadSystemConfig();
    void saveSystemConfig();

    void loadPwrConfig();
    void savePwrConfig();
    void loadPwrFields();
    void savePwrFields();

    void loadBatConfig();
    void saveBatConfig();
    void loadBatFields();
    void saveBatFields();

    void loadStatConfig();
    void saveStatConfig();
    void loadStatFields();
    void saveStatFields();

    void clearNVS();           // kept
    void factoryDefaults();
    void factoryReset();       // kept

    String uptimeString();     // kept
    String getCurrentTimeString(); // kept
    bool   isSystemTimeValid();    // kept

    bool logInfo  = true;
    bool logWarn  = true;
    bool logError = true;
    bool logDebug = false;

    bool logTaskManager = false;

    void saveDisplayBrightness();
    uint8_t displayBrightness = 150;   // Default-Helligkeit



private:
    String generateHostname();
    void   saveJsonChunked(const char* ns, const char* prefix, const String& json);
    String loadJsonChunked(const char* ns, const char* prefix);

        // Cache für Health-Anzeige
    String lastHealthColor;
    String lastHealthOK;
    String lastHealthWarn;
    String lastHealthErr;
    String lastHealthMsg;
};

// -------------------------------------------------------------
// Health status (kept)
// -------------------------------------------------------------
struct ModuleHealth {
    int         index;
    float       tempMax;
    float       cellMin;
    float       cellMax;
    float       cellDiff;
    const char* strongestState;
    const char* status;
};

struct HealthStatus {
    ModuleHealth modules[PWR_MAX_ROWS];
    int          moduleCount = 0;

    int okModules[PWR_MAX_ROWS];
    int warnModules[PWR_MAX_ROWS];
    int errorModules[PWR_MAX_ROWS];

    int okCount    = 0;
    int warnCount  = 0;
    int errorCount = 0;

    float stackCellMin  = 0;
    float stackCellMax  = 0;
    float stackCellDiff = 0;

    const char* color            = "green";
    const char* strongestMessage = "OK";

    std::vector<int> warnHistory;   // kept
    std::vector<int> errorHistory;  // kept
};

extern HealthStatus health;
extern AppConfig    config;
