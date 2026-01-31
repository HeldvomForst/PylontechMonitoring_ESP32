
#include "py_log.h"
#include <Arduino.h>
#include "esp_log.h"
#include "config.h"

extern AppConfig config;

// ---------------------------------------------------------
// Timestamp: YYYY.MM.DD hh:mm:ss,ms
// ---------------------------------------------------------
String getTimestampWithMs() {
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    unsigned long ms = millis() % 1000;

    char buf[40];
    snprintf(
        buf, sizeof(buf),
        "%04d.%02d.%02d %02d:%02d:%02d,%03lu",
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec,
        ms
    );

    return String(buf);
}

// ---------------------------------------------------------
// Web-Log Buffer
// ---------------------------------------------------------
static String webLog;

void WebLog(const String& msg) {
    webLog += msg + "\n";

    if (webLog.length() > 8000) {
        webLog.remove(0, 2000);
    }
}

String WebLogGet() {
    return webLog;
}

void WebLogClear() {
    webLog = "";
}

// ---------------------------------------------------------
// Main-Logfunction
// ---------------------------------------------------------
void Log(LogLevel lvl, const String& msg) {

    // --- Log-Level-Filter ---
    if ((lvl == LOG_INFO  && !config.logInfo)  ||
        (lvl == LOG_WARN  && !config.logWarn)  ||
        (lvl == LOG_ERROR && !config.logError) ||
        (lvl == LOG_DEBUG && !config.logDebug)) {
        return;
    }

    // --- Zeitstempel ---
    String ts = getTimestampWithMs() + " ";

    // --- Prefix ---
    String prefix;
    switch (lvl) {
        case LOG_INFO:  prefix = "[INFO] ";  break;
        case LOG_WARN:  prefix = "[WARN] ";  break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
        case LOG_DEBUG: prefix = "[DEBUG] "; break;
    }

    String line = ts + prefix + msg;

    // --- ESP_LOG ---
    switch (lvl) {
        case LOG_INFO:  ESP_LOGI("Pylontech", "%s", line.c_str()); break;
        case LOG_WARN:  ESP_LOGW("Pylontech", "%s", line.c_str()); break;
        case LOG_ERROR: ESP_LOGE("Pylontech", "%s", line.c_str()); break;
        case LOG_DEBUG: ESP_LOGD("Pylontech", "%s", line.c_str()); break;
    }

    // --- Web-Log ---
    WebLog(line);

    // --- Serial ---
    Serial.println(line);
}