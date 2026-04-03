#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h" 

// ---------------------------------------------------------
// PWR Parser Header
// ---------------------------------------------------------
// This header only declares the parser function and exposes
// global UI helper variables. All data structures are defined
// centrally in config.h to avoid duplication and include cycles.
// ---------------------------------------------------------

// Main PWR parser function
ParseResult parsePwrFrame(const String& raw,
                          BatteryStack& stackOut,
                          std::vector<BatteryModule>& modulesOut);

// Global parser results for the Web UI (filled by parser)
extern BatteryStack lastParsedStack;
extern std::vector<BatteryModule> lastParsedModules;
extern std::vector<String> lastParserHeader;
extern std::vector<String> lastParserValues;