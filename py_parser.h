#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
// Parser-Result kommt aus config.h → NICHT erneut definieren!
// ============================================================

// ============================================================
// Externe Tabellen (RAM-optimiert)
// ============================================================

// PWR
extern PwrTable pwrTable;
extern volatile bool pwrFrameReady;
extern int pwrTotalModules;
extern int pwrCurrentModule;

extern float stackVoltAvg;
extern float stackCurrSum;
extern float stackSocAvg;
extern float stackTempMax;
extern int   stackBatCount;

// BAT
extern BatTable batTable;

// STAT
extern StatTable statTable;

// ============================================================
// Parser-Flags
// ============================================================
extern bool parserHasData;
extern bool batParserHasData;
extern int  batParserModuleIndex;
extern bool statParserHasData;
extern int  statParserModuleIndex;

// ============================================================
// Parser-Funktionen
// ============================================================

// PWR
ParseResult parsePwrFrame(const String& raw);

// BAT
ParseResult parseBatFrame(int moduleIndex, const String& raw);

// STAT
ParseResult parseStatFrame(int moduleIndex, const String& raw);

// Parser-Task
void parserTask(void* parameter);
