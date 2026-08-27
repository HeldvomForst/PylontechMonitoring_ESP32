#include <Arduino.h>
#include "config.h"
#include "py_parser.h"
#include "py_log.h"

// Work buffer for copied strings
static char work[PWR_WORKBUF_SIZE];
static int  wp = 0;

// Global tables and status
extern PwrTable     pwrTable;
extern HealthStatus health;
extern AppConfig    config;

// MQTT trigger
extern volatile bool pwrFrameReady;
extern int           pwrTotalModules;
extern int           pwrCurrentModule;

// Stack values
extern float stackVoltAvg;
extern float stackCurrSum;
extern float stackSocAvg;
extern float stackTempMax;
extern int   stackBatCount;

// -------------------------------------------------------------
// Copy a string into the work buffer (zero‑terminated)
// -------------------------------------------------------------
static const char* copyToWork(const char* s, int len) {
    if (wp + len + 1 >= PWR_WORKBUF_SIZE) {
        Log(LOG_ERROR, "PWR parser: work buffer overflow");
        return nullptr;
    }
    memcpy(&work[wp], s, len);
    work[wp + len] = '\0';
    const char* ptr = &work[wp];
    wp += len + 1;
    return ptr;
}

// -------------------------------------------------------------
// Copy Arduino String into work buffer
// -------------------------------------------------------------
static const char* copyStringToWork(const String& s) {
    return copyToWork(s.c_str(), s.length());
}

// -------------------------------------------------------------
// Keep history list limited to max entries
// -------------------------------------------------------------
static void pushHistoryLimited(std::vector<int>& list, int value) {
    const size_t MAX_HISTORY = 50;
    list.push_back(value);
    if (list.size() > MAX_HISTORY)
        list.erase(list.begin());
}

// -------------------------------------------------------------
// Main PWR frame parser
// -------------------------------------------------------------
ParseResult parsePwrFrame(const String& raw)
{
    Log(LOG_INFO, "PWR parser: raw length=" + String(raw.length()));

    // Reset work buffer and table
    wp = 0;
    pwrTable.rows = 0;
    pwrTable.cols = 0;

    // Reset health
    health.moduleCount = 0;
    health.okCount     = 0;
    health.warnCount   = 0;
    health.errorCount  = 0;

    health.stackCellMin  = 999999.0f;
    health.stackCellMax  = -999999.0f;
    health.stackCellDiff = 0;

    health.color            = "green";
    health.strongestMessage = "OK";

    // Reset stack values
    stackVoltAvg  = 0;
    stackCurrSum  = 0;
    stackSocAvg   = 0;
    stackTempMax  = -9999.0f;
    stackBatCount = 0;

    // Copy raw frame into local buffer
    static char frameBuf[PWR_WORKBUF_SIZE];
    int len = raw.length();
    if (len >= (int)sizeof(frameBuf)) {
        Log(LOG_ERROR, "PWR parser: frame too large");
        return PARSE_FAIL;
    }
    memcpy(frameBuf, raw.c_str(), len);
    frameBuf[len] = '\0';

    // Locate '@' and '$$'
    char* start = strchr(frameBuf, '@');
    if (!start) {
        Log(LOG_ERROR, "PWR parser: no '@' found");
        return PARSE_FAIL;
    }
    start++;

    char* end = strstr(start, "$$");
    if (!end || end <= start) {
        Log(LOG_ERROR, "PWR parser: no '$$' found");
        return PARSE_FAIL;
    }
    *end = '\0';

    // ---------------------------------------------------------
    // Split into lines
    // ---------------------------------------------------------
    char* linePtr[PWR_MAX_ROWS + 1];
    int   lineCount = 0;

    char* p = start;
    while (*p && lineCount < PWR_MAX_ROWS + 1) {

        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;

        char* lineStart = p;

        while (*p && *p != '\r' && *p != '\n') p++;
        if (*p) {
            *p = '\0';
            p++;
        }

        // Trim left
        char* s = lineStart;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) continue;

        // Trim right
        char* e = s + strlen(s) - 1;
        while (e > s && (*e == ' ' || *e == '\t')) {
            *e = '\0';
            e--;
        }

        if (*s)
            linePtr[lineCount++] = s;
    }

    if (lineCount < 2) {
        Log(LOG_ERROR, "PWR parser: too few lines");
        return PARSE_FAIL;
    }

    // ---------------------------------------------------------
    // Parse header line
    // ---------------------------------------------------------
    char* headerLine = linePtr[0];
    char* colPtr[PWR_MAX_COLS];
    int   colCount = 0;

    {
        char* t = headerLine;
        while (*t && colCount < PWR_MAX_COLS) {
            while (*t == ' ' || *t == '\t') t++;
            if (!*t) break;

            char* tokStart = t;
            while (*t && *t != ' ' && *t != '\t') t++;

            if (*t) {
                *t = '\0';
                t++;
            }
            colPtr[colCount++] = tokStart;
        }
    }

    pwrTable.cols = colCount;
    for (int c = 0; c < colCount; c++)
        pwrTable.header[c] = copyToWork(colPtr[c], strlen(colPtr[c]));

    // Detect important columns by name
    int colVolt = -1, colCurr = -1, colTemp = -1, colCoul = -1;
    int colVlow = -1, colVhigh = -1;
    int colTime = -1;

    for (int c = 0; c < colCount; c++) {
        if (strcmp(colPtr[c], "Volt")    == 0) colVolt   = c;
        if (strcmp(colPtr[c], "Curr")    == 0) colCurr   = c;
        if (strcmp(colPtr[c], "Tempr")   == 0) colTemp   = c;
        if (strcmp(colPtr[c], "Coulomb") == 0) colCoul   = c;
        if (strcmp(colPtr[c], "Vlow")    == 0) colVlow   = c;
        if (strcmp(colPtr[c], "Vhigh")   == 0) colVhigh  = c;
        if (strcmp(colPtr[c], "Time")    == 0) colTime   = c;   // NEW
    }

    // ---------------------------------------------------------
    // Parse data rows
    // ---------------------------------------------------------
    for (int li = 1; li < lineCount; li++) {

        char* line = linePtr[li];

        // Tokenize row
        int   nCols = 0;
        char* t = line;

        while (*t && nCols < PWR_MAX_COLS) {
            while (*t == ' ' || *t == '\t') t++;
            if (!*t) break;

            char* tokStart = t;
            while (*t && *t != ' ' && *t != '\t') t++;

            if (*t) {
                *t = '\0';
                t++;
            }
            colPtr[nCols++] = tokStart;
        }

        if (nCols < 2) continue;
        if (!isdigit((unsigned char)colPtr[0][0])) continue;

        // -----------------------------------------------------
        // Coulomb fix: merge "%" into Coulomb
        // -----------------------------------------------------
        if (colCoul >= 0 && colCoul + 1 < nCols) {
            if (strcmp(colPtr[colCoul + 1], "%") == 0) {

                size_t len = strlen(colPtr[colCoul]);
                if (len + 1 < 16) {
                    colPtr[colCoul][len]   = '%';
                    colPtr[colCoul][len+1] = '\0';
                }

                for (int k = colCoul + 1; k < nCols - 1; k++)
                    colPtr[k] = colPtr[k + 1];

                nCols--;
            }
        }

        // -----------------------------------------------------
        // Time fix: merge date + time into one column
        // -----------------------------------------------------
        if (colTime >= 0 && colTime + 1 < nCols) {

            if (strchr(colPtr[colTime + 1], ':')) {

                size_t len1 = strlen(colPtr[colTime]);
                size_t len2 = strlen(colPtr[colTime + 1]);

                if (len1 + 1 + len2 < 64) {
                    colPtr[colTime][len1] = ' ';
                    memcpy(colPtr[colTime] + len1 + 1,
                           colPtr[colTime + 1],
                           len2 + 1);
                }

                for (int k = colTime + 1; k < nCols - 1; k++)
                    colPtr[k] = colPtr[k + 1];

                nCols--;
            }
        }

        // -----------------------------------------------------
        // Helper to convert column to float
        // -----------------------------------------------------
        auto getFloat = [&](int idx) -> float {
            if (idx < 0 || idx >= nCols) return 0.0f;
            char* s = colPtr[idx];
            if (!s || !*s) return 0.0f;

            char* p = s;
            while (*p) {
                if (*p == '%') { *p = '\0'; break; }
                p++;
            }
            return strtof(s, nullptr);
        };

        // Extract values
        float volt  = getFloat(colVolt)  / 1000.0f;
        float curr  = getFloat(colCurr)  / 1000.0f;
        float temp  = getFloat(colTemp)  / 1000.0f;
        float soc   = getFloat(colCoul);
        float vlow  = getFloat(colVlow)  / 1000.0f;
        float vhigh = getFloat(colVhigh) / 1000.0f;

        if (volt < 10 || volt > 60) continue;

        // Stack aggregation
        stackVoltAvg += volt;
        stackCurrSum += curr;
        stackSocAvg  += soc;

        if (temp > stackTempMax) stackTempMax = temp;
        if (vlow  < health.stackCellMin) health.stackCellMin = vlow;
        if (vhigh > health.stackCellMax) health.stackCellMax = vhigh;

        stackBatCount++;

        // Fill module health
        if (health.moduleCount < 32) {
            ModuleHealth& m = health.modules[health.moduleCount];
            m.index    = health.moduleCount + 1;
            m.tempMax  = temp;
            m.cellMin  = vlow;
            m.cellMax  = vhigh;
            m.cellDiff = vhigh - vlow;
            m.status   = "OK";
            m.strongestState = "OK";
            health.moduleCount++;
        }

        // Fill table row
        int row = pwrTable.rows;
        if (row < PWR_MAX_ROWS) {
            for (int c = 0; c < pwrTable.cols && c < nCols; c++)
                pwrTable.cell[row][c] = copyToWork(colPtr[c], strlen(colPtr[c]));
            pwrTable.rows++;
        }
    }

    if (stackBatCount == 0) {
        Log(LOG_ERROR, "PWR parser: no valid modules");
        return PARSE_FAIL;
    }

    // Finalize stack values
    stackVoltAvg /= stackBatCount;
    stackSocAvg  /= stackBatCount;
    health.stackCellDiff = health.stackCellMax - health.stackCellMin;

    // ---------------------------------------------------------
    // Health evaluation
    // ---------------------------------------------------------
    health.okCount    = 0;
    health.warnCount  = 0;
    health.errorCount = 0;

    float warnTh  = config.battery.cellDiffWarn;
    float errorTh = config.battery.cellDiffError;

    for (int i = 0; i < health.moduleCount; i++) {

        ModuleHealth& m = health.modules[i];
        float d = m.cellDiff;

        if (d >= errorTh) {
            m.status         = "Error";
            m.strongestState = "Error";
            health.errorModules[health.errorCount++] = m.index;
            pushHistoryLimited(health.errorHistory, m.index);
        }
        else if (d >= warnTh) {
            m.status         = "Warn";
            m.strongestState = "Warn";
            health.warnModules[health.warnCount++] = m.index;
            pushHistoryLimited(health.warnHistory, m.index);
        }
        else {
            m.status         = "OK";
            m.strongestState = "OK";
            health.okModules[health.okCount++] = m.index;
        }
    }

    // Stack color
    if (health.errorCount > 0) {
        health.color            = "red";
        health.strongestMessage = "Fehler";
    }
    else if (health.warnCount > 0) {
        health.color            = "yellow";
        health.strongestMessage = "Warnung";
    }
    else {
        health.color            = "green";
        health.strongestMessage = "OK";
    }

    // MQTT trigger
    pwrTotalModules  = pwrTable.rows;
    pwrCurrentModule = 0;
    pwrFrameReady    = true;

    // Save detected module count
    int modules = pwrTable.rows;
    if (modules > config.battery.maxModules)
        modules = config.battery.maxModules;

    config.detectedModules = modules;

    Log(LOG_INFO,
        "PWR parser OK: header=" + String(pwrTable.cols) +
        " cols, rows=" + String(pwrTable.rows) +
        ", detectedModules=" + String(config.detectedModules) +
        ", stackDelta=" + String(health.stackCellDiff, 3) +
        ", ok=" + String(health.okCount) +
        ", warn=" + String(health.warnCount) +
        ", err=" + String(health.errorCount));

    return PARSE_OK;
}
