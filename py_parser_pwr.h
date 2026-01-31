#pragma once
#include <Arduino.h>
#include <vector>
#include <map>

enum ParseResult {
    PARSE_OK,
    PARSE_FAIL,
    PARSE_IGNORED
};

/**
 * Data model for a single module.
 * Now includes a dynamic field map for ALL parsed values.
 */
struct BatteryModule {
    int index = -1;
    bool present = false;

    // Classic fixed fields (kept for convenience)
    int voltage_mV = 0;
    int current_mA = 0;
    int temperature = 0;
    int soc = 0;

    // NEW: dynamic mapping of ALL raw parser values
    std::map<String, String> fields;
};

/**
 * Data model for the whole stack (unchanged)
 */
struct BatteryStack {
    int batteryCount = 0;

    long avgVoltage_mV = 0;
    long totalCurrent_mA = 0;

    int soc = 0;
    int temperature = 0;

    void reset() {
        batteryCount = 0;
        avgVoltage_mV = 0;
        totalCurrent_mA = 0;
        soc = 0;
        temperature = 0;
    }
};

ParseResult parsePwrFrame(const String& raw,
                   BatteryStack& stackOut,
                   std::vector<BatteryModule>& modulesOut);

// For web settings page
extern BatteryStack lastParsedStack;
extern std::vector<BatteryModule> lastParsedModules;
extern std::vector<String> lastParserHeader;
extern std::vector<String> lastParserValues;