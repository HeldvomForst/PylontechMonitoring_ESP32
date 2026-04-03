#include "py_parser_stat.h"
#include "py_log.h"
#include "py_uart.h"
#include "py_mqtt.h"
#include "config.h"

// UART instance
extern PyUart py_uart;

// MQTT queue (from .ino)
extern QueueHandle_t mqttQueue;
extern bool statParserHasData;
extern int  statParserModuleIndex;
extern bool discoveryStatNeeded;

// Global STAT storage (for web UI)
StatData lastParsedStat;

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
// STAT parser
// ---------------------------------------------------------
ParseResult parseStatFrame(int /*moduleIndex*/,
                           const String& raw,
                           StatData& out)
{
    out.fields.clear();
    out.moduleIndex = -1;

    // ---------------------------------------------------------
    // 1) Check if this frame belongs to STAT
    // ---------------------------------------------------------
    String last = py_uart.getLastCommand();
    String lastLower = last;
    lastLower.trim();
    lastLower.toLowerCase();

    if (!lastLower.startsWith("stat ")) {
        Log(LOG_DEBUG, "STAT parser: ignoring frame (last command was '" + last + "')");
        return PARSE_IGNORED;
    }

    // Extract module index
    int idx = lastLower.substring(5).toInt();
    if (idx <= 0 || idx >= 16) {
        Log(LOG_WARN, "STAT parser: invalid module index in command '" + last + "'");
        return PARSE_IGNORED;
    }

    out.moduleIndex = idx;

    // ---------------------------------------------------------
    // 2) Validate frame
    // ---------------------------------------------------------
    if (!py_uart.isFrameValid()) {
        Log(LOG_WARN, "STAT parser: skipping invalid frame");
        return PARSE_FAIL;
    }

    Log(LOG_INFO, "STAT parser: raw frame received for module " + String(idx));

    // ---------------------------------------------------------
    // 3) Extract @ ... $$ section
    // ---------------------------------------------------------
    String frame;
    if (!extractFrame(raw, frame)) {
        Log(LOG_WARN, "STAT parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    // ---------------------------------------------------------
    // 4) Parse lines robustly
    // ---------------------------------------------------------
    int pos = 0;
    int safetyCounter = 0;

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

        // End of output
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
            // Fallback: last token is value
            int split = line.lastIndexOf(' ');
            if (split > 0) {
                key   = trimWS(line.substring(0, split));
                value = trimWS(line.substring(split));
            }
            else {
                continue;
            }
        }

        if (key.length() == 0)
            continue;

        StatField f;
        f.name = key;
        f.raw  = value;
        out.fields.push_back(f);
    }

    if (safetyCounter >= 200) {
        Log(LOG_ERROR, "STAT parser: safety break triggered (malformed frame)");
        return PARSE_FAIL;
    }

    // ---------------------------------------------------------
    // 5) Store global result (for Web UI)
    // ---------------------------------------------------------
    lastParsedStat = out;

    Log(LOG_INFO, "STAT parser: parsed " + String(out.fields.size()) +
                " ordered fields for module " + String(idx));

    // ---------------------------------------------------------
    // 6) Double Buffer Write (POINTER VERSION)
    // ---------------------------------------------------------
    //
    // Select the target buffer *after* the STAT frame has been fully
    // validated and parsed. We do NOT toggle before writing, because
    // that would expose MQTT to a partially written buffer.
    //
    ParsedData* target = useA ? &bufferB : &bufferA;

    // Write the complete STAT structure into the selected buffer
    target->stat = out;

    // -------------------------------------------------------------
    // Toggle the active buffer ONLY AFTER the frame is fully written.
    // This guarantees that MQTT always reads a complete, consistent
    // snapshot and never a partially written one.
    // -------------------------------------------------------------
    useA = !useA;

    // ---------------------------------------------------------
    // 7) Send MQTT message via queue (Task 2)
    // ---------------------------------------------------------
    MqttMessage msg;
    snprintf(msg.topic, sizeof(msg.topic),
            "pylontech/stat/module%d", idx);
    snprintf(msg.payload, sizeof(msg.payload),
            "%u", (unsigned)out.fields.size());
    xQueueSend(mqttQueue, &msg, 0);

    // Detect new STAT field names for Home Assistant discovery
    static std::vector<String> lastStatFieldNames;

    std::vector<String> currentNames;
    for (auto &f : out.fields) currentNames.push_back(f.name);

    if (idx == 1 && !currentNames.empty() && currentNames != lastStatFieldNames) {
        discoveryStatNeeded = true;
        lastStatFieldNames = currentNames;
    }

    // Parser flags for MQTT
    statParserHasData     = true;
    statParserModuleIndex = idx;

    return PARSE_OK;
}