#pragma once
#include <Arduino.h>
#include <vector>

#include "py_uart.h"
#include "config.h"
#include "py_parser.h"   // für lastParsedStack + lastParsedModules

class PyScheduler {
public:
    void begin(PyUart* u);
    void loop();

    void enqueue(const String& cmd);

    bool   hasQueuedCommand() const;
    String popNextCommand();

    unsigned long lastCommandFinished = 0;

private:
    PyUart* uart = nullptr;

    unsigned long bootTime = 0;

    unsigned long lastPwr  = 0;
    unsigned long lastBat  = 0;
    unsigned long lastStat = 0;

    bool initialPwrDone        = false;
    bool initialBatDone        = false;
    bool initialStatDone       = false;
    bool initialDiscoveryDone  = false;

    std::vector<String> queue;

    int nextCommandId = 1;   // eindeutige IDs für Befehle
};

extern PyScheduler py_scheduler;
