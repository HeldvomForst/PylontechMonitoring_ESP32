#include "py_parser_bat.h"
#include "py_log.h"
#include "py_uart.h"
#include "py_mqtt.h"
#include "py_scheduler.h"
#include "config.h"

extern PyUart py_uart;
extern QueueHandle_t mqttQueue;

extern bool batParserHasData;
extern int  batParserModuleIndex;
extern bool discoveryBatNeeded;

// Global BAT storage (for web UI)
BatData lastParsedBat;
std::vector<BatData> lastParsedBatCells;

// Double buffer extern
extern ParsedData bufferA;
extern ParsedData bufferB;
extern volatile bool useA;


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
// Split columns by 2+ spaces
// ---------------------------------------------------------
static std::vector<String> splitColumns(const String& line) {
    std::vector<String> out;
    int len = line.length();
    int start = 0;

    while (start < len) {
        while (start < len && line[start] == ' ') start++;
        if (start >= len) break;

        int end = start;
        int spaceCount = 0;

        while (end < len) {
            char c = line[end];
            if (c == ' ') {
                spaceCount++;
                if (spaceCount >= 2) break;
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

    // ---------------------------------------------------------
    // 1) Check if this frame belongs to BAT
    // ---------------------------------------------------------
    String last = py_uart.getLastCommand();
    String lastLower = last;
    lastLower.trim();
    lastLower.toLowerCase();

    if (!lastLower.startsWith("bat")) {
        Log(LOG_DEBUG, "BAT parser: ignoring frame (last command was '" + last + "')");
        return PARSE_IGNORED;
    }

    // Extract module index from command (bat1, bat2, ...)
    String num = lastLower.substring(3);
    num.trim();
    int moduleIdx = num.toInt();
    if (moduleIdx < 1 || moduleIdx > 16) {
        Log(LOG_WARN, "BAT parser: invalid module index '" + last + "'");
        return PARSE_IGNORED;
    }

    // Assign module index to the output structure
    out.moduleIndex = moduleIdx;


    if (moduleIdx < 1 || moduleIdx > 16) {
        Log(LOG_WARN, "BAT parser: invalid module index '" + last + "'");
        return PARSE_IGNORED;
    }

    // ---------------------------------------------------------
    // 2) Validate frame
    // ---------------------------------------------------------
    if (!py_uart.isFrameValid()) {
        Log(LOG_WARN, "BAT parser: skipping invalid frame");
        return PARSE_FAIL;
    }

    Log(LOG_INFO, "BAT parser: raw frame received for module " + String(moduleIdx));

    // ---------------------------------------------------------
    // 3) Extract @ ... $$ section
    // ---------------------------------------------------------
    String frame;
    if (!extractFrame(raw, frame)) {
        Log(LOG_WARN, "BAT parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    frame.replace("\r\n", "\n");
    frame.replace("\r", "\n");

    // ---------------------------------------------------------
    // 4) Split into lines
    // ---------------------------------------------------------
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

    // ---------------------------------------------------------
    // 5) Header
    // ---------------------------------------------------------
    std::vector<String> header = splitColumns(lines[0]);
    if (header.empty()) {
        Log(LOG_WARN, "BAT parser: empty header");
        return PARSE_FAIL;
    }

    // ---------------------------------------------------------
    // 6) Parse cell rows
    // ---------------------------------------------------------
    for (int row = 1; row < (int)lines.size(); row++) {
        String line = lines[row];

        // Stop at non‑numeric first token
        String firstToken = line;
        firstToken.trim();
        int spacePos = firstToken.indexOf(' ');
        if (spacePos > 0)
            firstToken = firstToken.substring(0, spacePos);

        if (firstToken.length() == 0)
            continue;

        if (!isDigit(firstToken[0])) {
            break; // end of data
        }

        std::vector<String> cols = splitColumns(line);
        if (cols.empty()) continue;

        size_t count = min(header.size(), cols.size());

        BatData cell;
        cell.cellIndex = row - 1;
        cell.moduleIndex = moduleIdx;

        for (size_t c = 0; c < count; c++) {
            BatField f;
            f.name = header[c];
            f.raw  = cols[c];
            cell.fields.push_back(f);
        }

        lastParsedBatCells.push_back(cell);
    }

    // ---------------------------------------------------------
    // 7) Store first cell for convenience
    // ---------------------------------------------------------
    if (!lastParsedBatCells.empty()) {
        out = lastParsedBatCells[0];
        lastParsedBat = out;

    }

    Log(LOG_INFO, "BAT parser: parsed " + String(lastParsedBatCells.size()) +
                " cells for module " + String(moduleIdx));

    // ---------------------------------------------------------
    // 8) Double Buffer Write (POINTER VERSION)
    // ---------------------------------------------------------
    //
    // Select the target buffer *after* the BAT frame has been fully
    // validated and parsed. We do NOT toggle before writing, because
    // that would expose MQTT to a partially written buffer.
    //
    ParsedData* target = useA ? &bufferB : &bufferA;

    // Write the complete BAT cell list into the selected buffer
    target->batCells = lastParsedBatCells;

    // -------------------------------------------------------------
    // Toggle the active buffer ONLY AFTER the frame is fully written.
    // This guarantees that MQTT always reads a complete, consistent
    // snapshot and never a partially written one.
    // -------------------------------------------------------------
    useA = !useA;

    // Parser flags for MQTT
    batParserHasData = true;
    batParserModuleIndex = moduleIdx;

    // Detect new BAT fields (for Home Assistant discovery)
    static size_t lastBatFieldCount = 0;
    size_t currentFieldCount =
        lastParsedBatCells.empty() ? 0 : lastParsedBatCells[0].fields.size();

    if (currentFieldCount != lastBatFieldCount) {
        discoveryBatNeeded = true;
    }
    lastBatFieldCount = currentFieldCount;

    return PARSE_OK;

}