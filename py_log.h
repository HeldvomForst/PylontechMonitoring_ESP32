#pragma once
#include <Arduino.h>
#include "web/wp_ui.h"

enum LogLevel {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
};

void Log(LogLevel lvl, const String& msg);

void WebLog(const String& msg);
String WebLogGet();
void WebLogClear();