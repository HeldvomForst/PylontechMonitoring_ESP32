#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"   // Provides BatField, BatData, ParseResult

// ---------------------------------------------------------
// BAT Parser Header
// ---------------------------------------------------------
// All BAT-related data structures (BatField, BatData) are
// defined centrally in config.h. This header only declares
// the parser function and exposes global UI helper variables.
// ---------------------------------------------------------

// Global BAT cell storage (filled by parser, used by UI/MQTT)
extern std::vector<BatData> lastParsedBatCells;
extern BatData lastParsedBat;

// BAT parser function
ParseResult parseBatFrame(int moduleIndex,
                          const String& raw,
                          BatData& out);