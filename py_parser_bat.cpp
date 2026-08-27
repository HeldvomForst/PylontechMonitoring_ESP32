#include <Arduino.h>
#include "config.h"
#include "py_parser.h"
#include "py_log.h"
#include "py_uart.h"

extern PyUart py_uart;

// Workbuffer für alle Strings
static char batWork[BAT_WORKBUF_SIZE];
static int batWp = 0;

// Globale BAT‑Tabelle
BatTable batTable;

// MQTT‑Trigger
extern bool batParserHasData;
extern int  batParserModuleIndex;

// ---------------------------------------------------------
// Hilfsfunktion: prüfen, ob Zeile Header ist
// ---------------------------------------------------------
static bool isHeaderLine(const char* buf, int s, int e) {

    // 1) Erste Spalte darf NICHT mit einer Zahl beginnen
    if (isdigit((unsigned char)buf[s]))
        return false;

    int colCount = 0;
    int textCols = 0;

    int cs = s;
    while (cs < e) {
        while (cs < e && buf[cs] == ' ') cs++;
        if (cs >= e) break;

        int ce = cs;
        int spaceCount = 0;
        while (ce < e) {
            char c = buf[ce];
            if (c == ' ') {
                spaceCount++;
                if (spaceCount >= 2) break;
            } else {
                spaceCount = 0;
            }
            ce++;
        }

        int ts = cs;
        int te = ce;
        while (ts < te && buf[ts] == ' ') ts++;
        while (te > ts && buf[te - 1] == ' ') te--;

        int len = te - ts;
        if (len > 0) {
            colCount++;

            int digits = 0;
            for (int i = ts; i < te; i++) {
                if (isdigit((unsigned char)buf[i]))
                    digits++;
            }

            // weniger als 20% Ziffern → Textspalte
            if (digits < len * 0.2f)
                textCols++;
        }

        cs = ce;
    }

    if (colCount < 5)
        return false;

    // mindestens 80% Textspalten
    if (textCols < (int)(colCount * 0.8f))
        return false;

    return true;
}

// ---------------------------------------------------------
// RAM‑optimierter BAT‑Parser
// ---------------------------------------------------------
ParseResult parseBatFrame(int moduleIndex, const String& raw)
{
    batTable.rows = 0;
    batTable.cols = 0;
    batWp = 0;

    int start = raw.indexOf('@');
    int end   = raw.indexOf("$$");
    if (start < 0 || end < 0 || end <= start)
        return PARSE_FAIL;

    const char* buf = raw.c_str();
    int pos = start + 1;
    int len = end;

    const int MAX_LINES = 40;
    int lineStart[MAX_LINES];
    int lineEnd[MAX_LINES];
    int lineCount = 0;

    int curStart = pos;
    while (curStart < len && lineCount < MAX_LINES) {
        int curEnd = curStart;
        while (curEnd < len && buf[curEnd] != '\r' && buf[curEnd] != '\n')
            curEnd++;

        int s = curStart;
        int e = curEnd;
        while (s < e && buf[s] == ' ') s++;
        while (e > s && buf[e - 1] == ' ') e--;

        if (e > s) {
            lineStart[lineCount] = s;
            lineEnd[lineCount]   = e;
            lineCount++;
        }

        curStart = curEnd + 1;
    }

    if (lineCount < 3)
        return PARSE_FAIL;

    // Header suchen
    int headerIndex = -1;
    for (int i = 0; i < lineCount; i++) {
        if (isHeaderLine(buf, lineStart[i], lineEnd[i])) {
            headerIndex = i;
            break;
        }
    }
    if (headerIndex < 0)
        return PARSE_FAIL;

    // Header kopieren → batTable.header[], batTable.cols
    {
        int s = lineStart[headerIndex];
        int e = lineEnd[headerIndex];

        int cs = s;
        int col = 0;

        while (cs < e && col < BAT_MAX_COLS) {
            while (cs < e && buf[cs] == ' ') cs++;
            if (cs >= e) break;

            int ce = cs;
            int spaceCount = 0;
            while (ce < e) {
                char c = buf[ce];
                if (c == ' ') {
                    spaceCount++;
                    if (spaceCount >= 2) break;
                } else {
                    spaceCount = 0;
                }
                ce++;
            }

            int ts = cs;
            int te = ce;
            while (ts < te && buf[ts] == ' ') ts++;
            while (te > ts && buf[te - 1] == ' ') te--;

            int lenCol = te - ts;
            if (lenCol > 0) {
                if (batWp + lenCol + 1 >= BAT_WORKBUF_SIZE)
                    return PARSE_FAIL;

                memcpy(&batWork[batWp], buf + ts, lenCol);
                batWork[batWp + lenCol] = '\0';
                batTable.header[col] = &batWork[batWp];
                batWp += lenCol + 1;
                col++;
            }

            cs = ce;
        }

        batTable.cols = col;
    }

    // Datenzeilen: ALLE Zellen (0..14) in batTable.cell[row][col]
    batTable.rows = 0;

    for (int i = headerIndex + 1; i < lineCount; i++) {

        int s = lineStart[i];
        int e = lineEnd[i];

        // Datenzeile beginnt mit Ziffer (Cell‑Index)
        if (!isdigit((unsigned char)buf[s]))
            continue;

        if (batTable.rows >= BAT_MAX_ROWS)
            break;

        int row = batTable.rows;
        int cs  = s;
        int col = 0;

        while (cs < e && col < batTable.cols) {
            while (cs < e && buf[cs] == ' ') cs++;
            if (cs >= e) break;

            int ce = cs;
            int spaceCount = 0;
            while (ce < e) {
                char c = buf[ce];
                if (c == ' ') {
                    spaceCount++;
                    if (spaceCount >= 2) break;
                } else {
                    spaceCount = 0;
                }
                ce++;
            }

            int ts = cs;
            int te = ce;
            while (ts < te && buf[ts] == ' ') ts++;
            while (te > ts && buf[te - 1] == ' ') te--;

            int lenCol = te - ts;
            if (lenCol > 0) {
                if (batWp + lenCol + 1 >= BAT_WORKBUF_SIZE)
                    return PARSE_FAIL;

                memcpy(&batWork[batWp], buf + ts, lenCol);
                batWork[batWp + lenCol] = '\0';
                batTable.cell[row][col] = &batWork[batWp];
                batWp += lenCol + 1;
                col++;
            }

            cs = ce;
        }

        batTable.rows++;
    }

    batParserHasData     = true;
    batParserModuleIndex = moduleIndex;

    return PARSE_OK;
}


