#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"   // Provides StatField, StatData, ParseResult

// ---------------------------------------------------------
// STAT Parser Header
// ---------------------------------------------------------
// All STAT-related data structures (StatField, StatData) are
// defined centrally in config.h. This header only declares
// the parser function and exposes global UI helper variables.
// ---------------------------------------------------------

// Global STAT storage (filled by parser, used by UI/MQTT)
extern StatData lastParsedStat;

// STAT parser function
ParseResult parseStatFrame(int moduleIndex,
                           const String& raw,
                           StatData& out);