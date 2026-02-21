#include "py_parser_bat.h"
#include "py_log.h"
#include "py_uart.h"
#include "py_mqtt.h"
#include "py_scheduler.h"

extern PyMqtt py_mqtt;
extern PyUart py_uart;
extern PyScheduler py_scheduler;

// Eine BAT-Antwort enthält ALLE Zellen eines Moduls.
// Wir speichern sie als Liste von Zellen.
BatData lastParsedBat;                    // optional, erste Zelle
std::vector<BatData> lastParsedBatCells;  // eine BatData pro Zelle

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
// Spalten anhand von 2+ Leerzeichen trennen
// 1 Leerzeichen gehört zum Text
// ---------------------------------------------------------
static std::vector<String> splitColumns(const String& line) {
    std::vector<String> out;
    int len = line.length();
    int start = 0;

    while (start < len) {
        // führende Leerzeichen überspringen
        while (start < len && line[start] == ' ') start++;
        if (start >= len) break;

        int end = start;
        int spaceCount = 0;

        while (end < len) {
            char c = line[end];
            if (c == ' ') {
                spaceCount++;
                if (spaceCount >= 2) break;   // 2+ Leerzeichen = Trenner
            } else {
                spaceCount = 0;
            }
            end++;
        }

        String col = line.substring(start, end);
        col.trim();
        if (col.length() > 0) out.push_back(col);

        start = end;
    }

    return out;
}

// ---------------------------------------------------------
// Main BAT parser
// ---------------------------------------------------------
ParseResult parseBatFrame(int /*moduleIndex*/,
                          const String& raw,
                          BatData& out)
{
    out.fields.clear();
    out.cellIndex = -1;
    lastParsedBatCells.clear();

    // 1) Letztes Kommando prüfen
    String last = py_uart.getLastCommand();
    String lastLower = last;
    lastLower.trim();
    lastLower.toLowerCase();

    if (!lastLower.startsWith("bat")) {
        Log(LOG_DEBUG, "BAT parser: ignoring frame (last command was '" + last + "')");
        return PARSE_IGNORED;
    }

    String num = lastLower.substring(3);
    num.trim();
    int moduleIdx = num.toInt();
    if (moduleIdx < 1 || moduleIdx > 16) {
        Log(LOG_WARN, "BAT parser: invalid module index '" + last + "'");
        return PARSE_IGNORED;
    }

    // 2) Frame prüfen
    if (!py_uart.isFrameValid()) {
        Log(LOG_WARN, "BAT parser: skipping invalid frame");
        return PARSE_FAIL;
    }

    Log(LOG_INFO, "BAT parser: raw frame received for module " + String(moduleIdx));

    String frame;
    if (!extractFrame(raw, frame)) {
        Log(LOG_WARN, "BAT parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    // Zeilenenden normalisieren
    frame.replace("\r\n", "\n");
    frame.replace("\r", "\n");

    // 3) Zeilen splitten
    std::vector<String> lines;
    {
        int pos = 0;
        while (true) {
            int nl = frame.indexOf('\n', pos);
            if (nl < 0) {
                String lastLine = trimWS(frame.substring(pos));
                if (lastLine.length() > 0) lines.push_back(lastLine);
                break;
            }
            String line = trimWS(frame.substring(pos, nl));
            if (line.length() > 0) lines.push_back(line);
            pos = nl + 1;
        }
    }

    if (lines.size() < 2) {
        Log(LOG_WARN, "BAT parser: too few lines");
        return PARSE_FAIL;
    }

    // 4) Header: erste Zeile mit 2+ Leerzeichen trennen
    std::vector<String> header = splitColumns(lines[0]);
    if (header.empty()) {
        Log(LOG_WARN, "BAT parser: empty header");
        return PARSE_FAIL;
    }

    // 5) Zellzeilen parsen
    for (int row = 1; row < (int)lines.size(); row++) {
        String line = lines[row];

        // Ende erkannt: z.B. "Command completed successfully"
        String firstToken = line;
        firstToken.trim();
        int spacePos = firstToken.indexOf(' ');
        if (spacePos > 0)
            firstToken = firstToken.substring(0, spacePos);

        if (firstToken.length() == 0)
            continue;

        // Zahl in erster Spalte = gültige Datenzeile
        if (!isDigit(firstToken[0])) {
            // kein Digit → keine Datenzeile mehr → abbrechen
            break;
        }

        std::vector<String> cols = splitColumns(line);
        if (cols.empty()) continue;

        size_t count = min(header.size(), cols.size());

        BatData cell;
        cell.cellIndex = row - 1;   // 0-basiert

        for (size_t c = 0; c < count; c++) {
            BatField f;
            f.name = header[c];
            f.raw  = cols[c];
            cell.fields.push_back(f);
        }

        lastParsedBatCells.push_back(cell);
    }

    if (!lastParsedBatCells.empty()) {
        lastParsedBat = lastParsedBatCells[0];
        out = lastParsedBat;
    }

    extern bool batParserHasData;
    extern int  batParserModuleIndex;

    batParserHasData     = true;
    batParserModuleIndex = moduleIdx;

    Log(LOG_INFO, "BAT parser: parsed " + String(lastParsedBatCells.size()) +
                  " cells for module " + String(moduleIdx));

    py_scheduler.lastCommandFinished = millis();

    return PARSE_OK;
}