#include "py_parser_pwr.h"
#include "py_log.h"
#include "config.h"
#include "py_uart.h"

#include <cctype>
#include <algorithm>

extern PyUart py_uart;

// Global parser data for web interface
std::vector<String> lastParserHeader;
std::vector<String> lastParserValues;

// Discovery / parser state flag (defined in py_mqtt.cpp)
extern bool parserHasData;
extern bool newParserData;

// Global parser results for API & UI
BatteryStack lastParsedStack;
std::vector<BatteryModule> lastParsedModules;

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
    // Only parse if last command was "pwr"
    if (py_uart.getLastCommand() != "pwr") {
        Log(LOG_DEBUG, "PWR parser: ignoring frame (last command was '" + py_uart.getLastCommand() + "')");
        return PARSE_IGNORED;
    }
    
    // skip invalid frames
    if (!py_uart.isFrameValid()) {
        Log(LOG_WARN, "PWR parser: skipping invalid frame");
        return PARSE_FAIL;
    }

    modulesOut.clear();
    stackOut.reset();

    Log(LOG_INFO, "PWR parser: raw frame received, length=" + String(raw.length()));

    // 1) Extract @ ... $$ frame
    String frame;
    if (!extractFrame(raw, frame)) {
        Log(LOG_WARN, "PWR parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    // 2) Split into lines
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

    // 3) Header line
    std::vector<String> header = splitWS(lines[0]);
    if (header.size() < 3) {
        Log(LOG_WARN, "PWR parser: header too small");
        return PARSE_FAIL;
    }

    lastParserHeader = header;
    lastParserValues.clear();

    // Index of Base/Base.St for "Absent" detection
    int baseIndex = -1;
    for (size_t h = 0; h < header.size(); h++) {
        if (header[h] == "Base.St" || header[h] == "Base") {
            baseIndex = h;
            break;
        }
    }

    // 4) Data lines
    for (size_t i = 1; i < lines.size(); i++) {

        std::vector<String> cols = splitWS(lines[i]);

        // Fix "YYYY-MM-DD HH:MM:SS" time split
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

        // Absent detection
        if (baseIndex >= 0 && cols[baseIndex] == "Absent") {
            Log(LOG_INFO, "PWR parser: Absent detected at line " + String(i) + " → stop parsing");
            break;
        }

        // Store first data row for web interface
        if (lastParserValues.empty()) {
            lastParserValues = cols;
        }

        BatteryModule mod;
        mod.present = true;

        // ---------------------------------------------------------
        // NEW: dynamic mapping of ALL fields
        // ---------------------------------------------------------
        for (size_t c = 0; c < header.size(); c++) {
            const String& col = header[c];
            const String& value = cols[c];

            // Store raw value dynamically
            mod.fields[col] = value;

            // Classic fixed fields (unchanged)
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

        Log(LOG_INFO,
            "PWR parser: module " + String(mod.index) +
            " V=" + String(mod.voltage_mV) +
            " I=" + String(mod.current_mA) +
            " T=" + String(mod.temperature) +
            " SOC=" + String(mod.soc)
        );
    }

    // 5) Stack calculation
    if (modulesOut.empty()) {
        Log(LOG_WARN, "PWR parser: no modules parsed");
        return PARSE_FAIL;
    }

    int count = modulesOut.size();
    stackOut.batteryCount = count;

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

    Log(LOG_INFO,
        "PWR parser: stack Vavg=" + String(stackOut.avgVoltage_mV) +
        " I=" + String(stackOut.totalCurrent_mA) +
        " SOC=" + String(stackOut.soc) +
        " T=" + String(stackOut.temperature)
    );

    // Set last battery update timestamp
    config.lastPwrUpdate = config.getCurrentTimeString();

    // Store global parser results
    lastParsedStack = stackOut;
    lastParsedModules = modulesOut;

    // Signal to MQTT that valid data is available
    parserHasData = true;
    newParserData = true;

    return PARSE_OK;
}