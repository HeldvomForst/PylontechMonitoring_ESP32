#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"   // Provides StatField, StatData, ParseResult

// Global STAT storage (for Web UI)
extern StatData lastParsedStat;

// STAT parser function
ParseResult parseStatFrame(int moduleIndex,
                           const String& raw,
                           StatData& out);
