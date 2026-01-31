#pragma once
#include <Arduino.h>
#include <vector>
#include "py_uart.h"
#include "py_parser_pwr.h"
#include "config.h"

// Command types handled by the scheduler
enum PyCommandType {
    CMD_PWR,
    CMD_BAT,
    CMD_STAT,
    CMD_RAW
};

// A scheduled command
struct PyScheduledCommand {
    PyCommandType type;
    int   batteryIndex = 0;
    String rawCommand;   // For CMD_RAW
};

class PyScheduler {
public:
    void begin(PyUart* u);
    void setIntervals(unsigned long pwrMs, unsigned long batMs, unsigned long statMs);

    void enqueue(PyCommandType type, int index = 0);
    void enqueueRaw(const String& rawCmd);

    void loop();
    void handleFrame(const String& frame);

    // Latest parsed data from PWR frames
    BatteryStack               stack;
    std::vector<BatteryModule> modules;

private:
    void scheduleAutomatic();
    void processQueue();

    PyUart* uart = nullptr;

    unsigned long intervalPwr = 0;
    unsigned long intervalBat = 0;
    unsigned long intervalStat = 0;

    unsigned long lastPwr = 0;
    unsigned long lastBat = 0;
    unsigned long lastStat = 0;

    std::vector<PyScheduledCommand> queue;
};

// Global instance
extern PyScheduler py_scheduler;