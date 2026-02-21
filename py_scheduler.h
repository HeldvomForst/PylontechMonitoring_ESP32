#pragma once
#include <Arduino.h>
#include <vector>
#include "py_uart.h"
#include "config.h"



class PyScheduler {
public:
    void begin(PyUart* u);

    // Manuelles Einreihen (Console, Web-UI)
    void enqueue(const String& cmd);

    // Hauptschleife
    void loop();
    unsigned long lastCommandFinished = 0;

private:
    void scheduleAutomatic();
    void processQueue();

    PyUart* uart = nullptr;

    unsigned long lastPwr  = 0;
    unsigned long lastBat  = 0;
    unsigned long lastStat = 0;

    std::vector<String> queue;
};

extern PyScheduler py_scheduler;