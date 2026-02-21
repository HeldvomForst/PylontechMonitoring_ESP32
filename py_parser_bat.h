#pragma once
#include <Arduino.h>
#include <vector>
#include "py_parser_pwr.h"
#include "py_scheduler.h"

extern PyScheduler py_scheduler;

// ---------------------------------------------------------
// BAT field + BAT cell structure
// ---------------------------------------------------------
struct BatField {
    String name;
    String raw;
};

struct BatData {
    int cellIndex = -1;                 // 0–14
    std::vector<BatField> fields;       // alle Spalten der BAT-Zeile
};

// ---------------------------------------------------------
// GLOBAL BAT CELL STORAGE
// Wird vom Parser gefüllt, vom MQTT-Publisher gelesen
// ---------------------------------------------------------
extern std::vector<BatData> lastParsedBatCells;

// Optional: falls du es noch brauchst
extern BatData lastParsedBatModules[16];

// ---------------------------------------------------------
// Parser-Signatur
// ---------------------------------------------------------
ParseResult parseBatFrame(int moduleIndex,
                          const String& raw,
                          BatData& out);