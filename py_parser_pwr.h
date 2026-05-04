#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

// ---------------------------------------------------------
// PWR Parser Header
// ---------------------------------------------------------
// Liefert nur BatteryStack + BatteryModule-Liste zurück.
// Alle Strukturen kommen aus config.h.
// ---------------------------------------------------------

// Main PWR parser function
ParseResult parsePwrFrame(const String& raw,
                          BatteryStack& stackOut,
                          std::vector<BatteryModule>& modulesOut);

// Globale Parser-Ergebnisse für Web UI (optional)
extern BatteryStack lastParsedStack;
extern std::vector<BatteryModule> lastParsedModules;
extern std::vector<String> lastParserHeader;
extern std::vector<String> lastParserValues;
