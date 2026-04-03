#pragma once
#include <Arduino.h>
#include <vector>
#include "py_uart.h"
#include "config.h"

enum SchedState {
    SCHED_INIT_WAIT,
    SCHED_SEND_PWR,
    SCHED_SEND_BAT,
    SCHED_SEND_STAT,
    SCHED_NORMAL
};

class PyScheduler {
public:
    void begin(PyUart* u);
    void loop();
    void enqueue(const String& cmd);

    bool hasQueuedCommand() const;
    String popNextCommand();

    unsigned long lastCommandFinished = 0;   // Task1 setzt das

private:
    void scheduleAutomatic();

    PyUart* uart = nullptr;

    unsigned long lastLoop = 0;
    unsigned long lastPwr  = 0;
    unsigned long lastBat  = 0;
    unsigned long lastStat = 0;

    SchedState state = SCHED_INIT_WAIT;
    unsigned long stateStart = 0;

    std::vector<String> queue;
};

extern PyScheduler py_scheduler;