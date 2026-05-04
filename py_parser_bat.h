#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

// Globale BAT-Daten für Web-UI (optional)
extern std::vector<BatData> lastParsedBatCells;
extern BatData lastParsedBat;

// BAT parser function
ParseResult parseBatFrame(int moduleIndex,
                          const String& raw,
                          BatData& out);
