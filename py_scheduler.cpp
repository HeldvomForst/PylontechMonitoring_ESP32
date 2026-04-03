#include "py_scheduler.h"
#include "py_log.h"
#include "py_parser_pwr.h"

PyScheduler py_scheduler;

static const unsigned long LOOP_INTERVAL = 100; // 10 Hz

void PyScheduler::begin(PyUart* u) {
    uart = u;
    queue.clear();

    state = SCHED_INIT_WAIT;
    stateStart = millis();

    lastLoop = millis();
    lastCommandFinished = millis();

    lastPwr  = millis();
    lastBat  = millis();
    lastStat = millis();

    Log(LOG_INFO, "Scheduler: started");
}

void PyScheduler::enqueue(const String& cmd) {
    queue.push_back(cmd);
}

void PyScheduler::loop() {
    unsigned long now = millis();

    // 10 Hz Begrenzung
    if (now - lastLoop < LOOP_INTERVAL) return;
    lastLoop = now;

    // Warten bis Task1 den letzten Befehl abgearbeitet hat
    if (uart->isBusy()) return;

    // -----------------------------
    // Start-State-Machine
    // -----------------------------
    switch (state) {

        case SCHED_INIT_WAIT:
            if (now - stateStart >= 15000) {
                Log(LOG_INFO, "Scheduler: initial PWR");
                enqueue("pwr");
                state = SCHED_SEND_PWR;
                stateStart = now;
            }
            return;

        case SCHED_SEND_PWR:
            if (now - stateStart >= 15000) {
                Log(LOG_INFO, "Scheduler: initial BAT");
                int count = lastParsedStack.batteryCount;
                if (count < 1) count = 1;
                for (int i = 1; i <= count; i++)
                    enqueue("bat " + String(i));
                state = SCHED_SEND_BAT;
                stateStart = now;
            }
            return;

        case SCHED_SEND_BAT:
            if (now - stateStart >= 30000) {
                Log(LOG_INFO, "Scheduler: initial STAT");
                int count = lastParsedStack.batteryCount;
                if (count < 1) count = 1;
                for (int i = 1; i <= count; i++)
                    enqueue("stat " + String(i));
                state = SCHED_NORMAL;
                Log(LOG_INFO, "Scheduler: normal mode");
            }
            return;

        case SCHED_NORMAL:
            scheduleAutomatic();
            return;
    }
}

void PyScheduler::scheduleAutomatic() {
    unsigned long now = millis();

    int count = lastParsedStack.batteryCount;
    if (count < 1) count = 1;

    if (now - lastPwr >= config.battery.intervalPwr) {
        enqueue("pwr");
        lastPwr = now;
    }

    if (now - lastBat >= config.battery.intervalBat) {
        for (int i = 1; i <= count; i++)
            enqueue("bat " + String(i));
        lastBat = now;
    }

    if (now - lastStat >= config.battery.intervalStat) {
        for (int i = 1; i <= count; i++)
            enqueue("stat " + String(i));
        lastStat = now;
    }
}

bool PyScheduler::hasQueuedCommand() const {
    return !queue.empty();
}

String PyScheduler::popNextCommand() {
    if (queue.empty()) return "";
    String cmd = queue.front();
    queue.erase(queue.begin());
    return cmd;
}