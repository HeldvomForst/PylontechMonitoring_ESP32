#pragma once
#include <Arduino.h>
#include <vector>
#include "py_parser_pwr.h"
#include "py_scheduler.h"
extern PyScheduler py_scheduler;


struct StatField {
    String name;   // Original Feldname
    String raw;    // Rohwert
};

struct StatData {
    int moduleIndex = -1;
    std::vector<StatField> fields;   // Reihenfolge bleibt erhalten
};

extern StatData lastParsedStat;

ParseResult parseStatFrame(int moduleIndex,
                           const String& raw,
                           StatData& out);
