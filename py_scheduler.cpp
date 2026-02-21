#include "py_scheduler.h"
#include "py_log.h"
#include "py_parser_pwr.h" 

PyScheduler py_scheduler;

// ---------------------------------------------------------
// Begin
// ---------------------------------------------------------
void PyScheduler::begin(PyUart* u) {
    uart = u;

    unsigned long now = millis();
    lastPwr  = now;
    lastBat  = now;
    lastStat = now;

    queue.clear();
}

// ---------------------------------------------------------
// Manuelles Einreihen (Console, Web-UI)
// ---------------------------------------------------------
void PyScheduler::enqueue(const String& cmd) {
    queue.push_back(cmd);
}

// ---------------------------------------------------------
// Automatische Befehle erzeugen
// ---------------------------------------------------------
void PyScheduler::scheduleAutomatic() {
    unsigned long now = millis();

    // --- PWR ---
    if (config.battery.intervalPwr > 0 &&
        now - lastPwr >= config.battery.intervalPwr) {

        enqueue("pwr");
        lastPwr = now;
    }

    // Anzahl Module aus PWR-Parser
    int moduleCount = lastParsedStack.batteryCount;
    if (moduleCount < 1) moduleCount = 1; // Fallback

    // --- BAT ---
    if (config.battery.intervalBat > 0 &&
        now - lastBat >= config.battery.intervalBat) {

        for (int i = 1; i <= moduleCount; i++) {
            enqueue("bat " + String(i));
        }
        lastBat = now;
    }

    // --- STAT ---
    if (config.battery.intervalStat > 0 &&
        now - lastStat >= config.battery.intervalStat) {

        for (int i = 1; i <= moduleCount; i++) {
            enqueue("stat " + String(i));
        }
        lastStat = now;
    }
}

// ---------------------------------------------------------
// FIFO Queue → UART senden
// ---------------------------------------------------------
void PyScheduler::processQueue() {
    if (!uart) return;
    if (millis() - lastCommandFinished < 100) {
        return;
    }

    if (!uart->isReady()) return;
    if (uart->isBusy()) return;
    if (queue.empty()) return;

    String cmd = queue.front();
    queue.erase(queue.begin());

    Log(LOG_INFO, "Scheduler: sending command: " + cmd);
    uart->sendCommand(cmd.c_str());
}

// ---------------------------------------------------------
// Hauptschleife
// ---------------------------------------------------------
void PyScheduler::loop() {
    scheduleAutomatic();
    processQueue();
}