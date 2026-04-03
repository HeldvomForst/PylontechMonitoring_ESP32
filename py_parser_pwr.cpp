#include "py_parser_pwr.h"
#include "py_log.h"
#include "config.h"
#include "py_uart.h"
#include "py_mqtt.h"

#include <cctype>
#include <algorithm>

// UART instance
extern PyUart py_uart;

// MQTT queue (from .ino)
extern QueueHandle_t mqttQueue;
extern bool parserHasData;
extern bool newParserData;
extern bool discoveryPwrNeeded;
extern bool discoveryBatNeeded;
extern bool discoveryStatNeeded;

// Global parser data for web interface
std::vector<String> lastParserHeader;
std::vector<String> lastParserValues;

// Global parser results
BatteryStack lastParsedStack;
std::vector<BatteryModule> lastParsedModules;

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
// Helper: split by whitespace
// ---------------------------------------------------------
static std::vector<String> splitWS(const String& line) {
    std::vector<String> out;
    int start = 0;

    while (start < line.length()) {
        while (start < line.length() && isspace((unsigned char)line[start])) start++;
        if (start >= line.length()) break;

        int end = start;
        while (end < line.length() && !isspace((unsigned char)line[end])) end++;

        out.push_back(line.substring(start, end));
        start = end;
    }
    return out;
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
// Main PWR parser
// ---------------------------------------------------------
ParseResult parsePwrFrame(const String& raw,
                          BatteryStack& stackOut,
                          std::vector<BatteryModule>& modulesOut)
{
    if (py_uart.getLastCommand() != "pwr") {
        Log(LOG_DEBUG, "PWR parser: ignoring frame (last command was '" + py_uart.getLastCommand() + "')");
        return PARSE_IGNORED;
    }

    if (!py_uart.isFrameValid()) {
        Log(LOG_WARN, "PWR parser: skipping invalid frame");
        return PARSE_FAIL;
    }

    modulesOut.clear();
    stackOut.reset();

    Log(LOG_INFO, "PWR parser: raw frame received, length=" + String(raw.length()));

    String frame;
    if (!extractFrame(raw, frame)) {
        Log(LOG_WARN, "PWR parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    std::vector<String> lines;
    {
        int pos = 0;
        while (true) {
            int nl = frame.indexOf('\n', pos);
            if (nl < 0) {
                String last = trimWS(frame.substring(pos));
                if (last.length() > 0) lines.push_back(last);
                break;
            }
            String line = trimWS(frame.substring(pos, nl));
            if (line.length() > 0) lines.push_back(line);
            pos = nl + 1;
        }
    }

    if (lines.size() < 2) {
        Log(LOG_WARN, "PWR parser: too few lines");
        return PARSE_FAIL;
    }

    std::vector<String> header = splitWS(lines[0]);
    if (header.size() < 3) {
        Log(LOG_WARN, "PWR parser: header too small");
        return PARSE_FAIL;
    }

    lastParserHeader = header;
    lastParserValues.clear();

    int baseIndex = -1;
    for (size_t h = 0; h < header.size(); h++) {
        if (header[h] == "Base.St" || header[h] == "Base") {
            baseIndex = h;
            break;
        }
    }

    for (size_t i = 1; i < lines.size(); i++) {

        std::vector<String> cols = splitWS(lines[i]);

        int timeIndex = -1;
        for (size_t h = 0; h < header.size(); h++) {
            if (header[h] == "Time") {
                timeIndex = h;
                break;
            }
        }

        if (timeIndex >= 0 && cols.size() > timeIndex + 1) {
            String datePart = cols[timeIndex];
            String timePart = cols[timeIndex + 1];

            bool looksLikeDate = (datePart.indexOf('-') > 0 || datePart.indexOf('/') > 0);
            bool looksLikeTime = (timePart.indexOf(':') > 0);

            if (looksLikeDate && looksLikeTime) {
                cols[timeIndex] = datePart + " " + timePart;
                cols.erase(cols.begin() + timeIndex + 1);
            }
        }

        if (cols.size() < header.size()) continue;

        if (baseIndex >= 0 && cols[baseIndex] == "Absent") {
            Log(LOG_INFO, "PWR parser: Absent detected at line " + String(i));
            break;
        }

        if (lastParserValues.empty()) {
            lastParserValues = cols;
        }

        BatteryModule mod;
        mod.present = true;

        for (size_t c = 0; c < header.size(); c++) {
            const String& col = header[c];
            const String& value = cols[c];

            mod.fields[col] = value;

            if (col == "Power" || col == "Battery") {
                mod.index = value.toInt();
            }
            else if (col == "Volt") {
                mod.voltage_mV = value.toInt();
            }
            else if (col == "Curr") {
                mod.current_mA = value.toInt();
            }
            else if (col == "Tempr") {
                mod.temperature = value.toInt();
            }
            else if (col == "Coulomb" || col == "SOC") {
                String v = value;
                v.replace("%", "");
                mod.soc = v.toInt();
            }
        }

        bool plausible = true;

        plausible &= (mod.index >= 1 && mod.index <= 32);
        plausible &= (mod.voltage_mV > 10000 && mod.voltage_mV < 60000);
        plausible &= (mod.temperature > 1000 && mod.temperature < 60000);
        plausible &= (mod.soc >= 1 && mod.soc <= 100);

        if (!plausible) {
            Log(LOG_WARN, "PWR parser: skipping implausible module line " + String(i));
            continue;
        }

        modulesOut.push_back(mod);
    }

    if (modulesOut.empty()) {
        Log(LOG_WARN, "PWR parser: no modules parsed");
        return PARSE_FAIL;
    }

    int count = modulesOut.size();
    stackOut.batteryCount = count;
    config.detectedModules = count;

    long sumVolt = 0;
    long sumCurr = 0;
    int minSoc = 999;
    int maxTemp = -999;

    for (auto& m : modulesOut) {
        sumVolt += m.voltage_mV;
        sumCurr += m.current_mA;

        if (m.soc < minSoc) minSoc = m.soc;
        if (m.temperature > maxTemp) maxTemp = m.temperature;
    }

    stackOut.avgVoltage_mV   = sumVolt / count;
    stackOut.totalCurrent_mA = sumCurr;
    stackOut.soc             = minSoc;
    stackOut.temperature     = maxTemp;

    config.lastPwrUpdate = config.getCurrentTimeString();

    // --- Double Buffer Write (POINTER VERSION) ---
    //
    // Select the target buffer *after* the frame has been fully validated
    // and parsed. We do NOT toggle before writing, because that would
    // expose MQTT to a half‑written buffer.
    //
    ParsedData* target = useA ? &bufferB : &bufferA;

    // Write the complete PWR frame into the selected buffe
    target->stack   = stackOut;
    target->modules = modulesOut;

    // Update global UI data (not part of the double buffer)
    lastParsedStack = stackOut;
    lastParsedModules = modulesOut;

    // -------------------------------------------------------------
    // Toggle the active buffer ONLY AFTER the frame is fully written.
    // This guarantees that MQTT always reads a complete, consistent
    // snapshot and never a partially written one.
    // -------------------------------------------------------------
    useA = !useA;

    // Mark parser state flags
    parserHasData = true;
    newParserData = true;

    return PARSE_OK;

}