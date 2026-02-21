#include "py_parser_stat.h"
#include "py_log.h"
#include "py_uart.h"

extern PyUart py_uart;
extern bool statParserHasData;
extern int  statParserModuleIndex;

// Nur EIN Modul im RAM
StatData lastParsedStat;

// ---------------------------------------------------------
// Helper: trim whitespace
// ---------------------------------------------------------
static String trimWS(const String& s) {
    String r = s;
    r.trim();
    return r;
}

// ---------------------------------------------------------
// Extract @ ... $$ frame
// ---------------------------------------------------------
static bool extractFrame(const String& raw, String& frame) {
    int start = raw.indexOf('@');
    int end   = raw.indexOf("$$");

    if (start < 0 || end < 0 || end <= start)
        return false;

    frame = raw.substring(start + 1, end);
    return true;
}

// ---------------------------------------------------------
// ROBUSTER STAT-PARSER
// ---------------------------------------------------------
ParseResult parseStatFrame(int /*moduleIndex*/,
                           const String& raw,
                           StatData& out)
{
    out.fields.clear();
    out.moduleIndex = -1;

    // 1) Nur reagieren, wenn letztes Kommando "stat X" war
    String last = py_uart.getLastCommand();
    String lastLower = last;
    lastLower.trim();
    lastLower.toLowerCase();

    if (!lastLower.startsWith("stat ")) {
        Log(LOG_DEBUG, "STAT parser: ignoring frame (last command was '" + last + "')");
        return PARSE_IGNORED;
    }

    // Modulindex extrahieren
    int idx = lastLower.substring(5).toInt();
    if (idx <= 0 || idx >= 16) {
        Log(LOG_WARN, "STAT parser: invalid module index in command '" + last + "'");
        return PARSE_IGNORED;
    }

    out.moduleIndex = idx;

    // 2) Nur gültige Frames verarbeiten
    if (!py_uart.isFrameValid()) {
        Log(LOG_WARN, "STAT parser: skipping invalid frame");
        return PARSE_FAIL;
    }

    Log(LOG_INFO, "STAT parser: raw frame received for module " + String(idx));

    // 3) @ ... $$ extrahieren
    String frame;
    if (!extractFrame(raw, frame)) {
        Log(LOG_WARN, "STAT parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    // 4) Zeilenweise verarbeiten – ROBUST
    int pos = 0;
    int safetyCounter = 0;   // schützt vor Endlosschleifen

    while (pos < frame.length() && safetyCounter < 200) {

        safetyCounter++;

        int nl = frame.indexOf('\n', pos);
        String line;

        if (nl < 0) {
            line = trimWS(frame.substring(pos));
            pos = frame.length();
        } else {
            line = trimWS(frame.substring(pos, nl));
            pos = nl + 1;
        }

        if (line.length() == 0)
            continue;

        // Ende der Ausgabe
        if (line.startsWith("Command completed"))
            break;

        // Key : Value
        int colon = line.indexOf(':');
        String key, value;

        if (colon >= 0) {
            key   = trimWS(line.substring(0, colon));
            value = trimWS(line.substring(colon + 1));
        }
        else {
            // Fallback: letzter Token ist Value
            int split = line.lastIndexOf(' ');
            if (split > 0) {
                key   = trimWS(line.substring(0, split));
                value = trimWS(line.substring(split));
            }
            else {
                // Zeile ignorieren
                continue;
            }
        }

        // Leere Keys ignorieren
        if (key.length() == 0)
            continue;

        // Feld speichern
        StatField f;
        f.name = key;
        f.raw  = value;
        out.fields.push_back(f);
    }

    if (safetyCounter >= 200) {
        Log(LOG_ERROR, "STAT parser: safety break triggered (malformed frame)");
        return PARSE_FAIL;
    }

    // 5) Nur EIN Modul speichern
    lastParsedStat = out;

    Log(LOG_INFO, "STAT parser: parsed " + String(out.fields.size()) +
                  " ordered fields for module " + String(idx));
    
    extern bool statParserHasData;
    extern int  statParserModuleIndex;

    statParserHasData = true;
    statParserModuleIndex = idx;
    
    py_scheduler.lastCommandFinished = millis();

    return PARSE_OK;
}