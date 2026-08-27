#include "py_scheduler.h"
#include "py_log.h"
#include "config.h"
#include "py_parser.h"

PyScheduler py_scheduler;

void PyScheduler::begin(PyUart* u) {
    uart = u;
    queue.clear();

    bootTime = millis();

    lastPwr  = millis();
    lastBat  = millis();
    lastStat = millis();

    initialPwrDone        = false;
    initialBatDone        = false;
    initialStatDone       = false;
    initialDiscoveryDone  = false;

    Log(LOG_INFO, "Scheduler: started");
}

void PyScheduler::enqueue(const String& cmd) {
    Log(LOG_DEBUG, "Scheduler: enqueue → " + cmd);
    queue.push_back(cmd);
}

String PyScheduler::popNextCommand() {
    if (queue.empty()) return "";
    String cmd = queue.front();
    queue.erase(queue.begin());
    Log(LOG_DEBUG, "Scheduler: pop → " + cmd);
    return cmd;
}

void PyScheduler::loop() {
    unsigned long now = millis();

    // UART busy? Then wait
    if (!uart || uart->isBusy()) return;

    // Initial sequence (only once)
    unsigned long sinceBoot = now - bootTime;

    if (!initialPwrDone && sinceBoot >= 20000) {
        enqueue("pwr");
        Log(LOG_INFO, "Scheduler: INITIAL PWR");
        initialPwrDone = true;
        return;
    }

    if (initialPwrDone && !initialBatDone && sinceBoot >= 25000) {
        enqueue("bat 1");
        Log(LOG_INFO, "Scheduler: INITIAL BAT");
        initialBatDone = true;
        return;
    }

    if (initialBatDone && !initialStatDone && sinceBoot >= 45000) {
        enqueue("stat 1");
        Log(LOG_INFO, "Scheduler: INITIAL STAT");
        initialStatDone = true;
        return;
    }

    if (initialStatDone && !initialDiscoveryDone && sinceBoot >= 50000) {
        Log(LOG_INFO, "Scheduler: INITIAL DISCOVERY triggered");

        discoveryPwrNeeded  = true;
        discoveryBatNeeded  = true;
        discoveryStatNeeded = true;

        initialDiscoveryDone = true;
        return;
    }

    // Cyclic scheduling
    if (now - lastPwr >= config.battery.intervalPwr) {
        enqueue("pwr");
        lastPwr = now;
        Log(LOG_INFO, "Scheduler: PWR scheduled");
    }

    if (now - lastBat >= config.battery.intervalBat) {
        if (config.battery.enableBat) {
            for (int i = 1; i <= config.detectedModules; i++) {
                enqueue("bat " + String(i));
            }
            Log(LOG_INFO, "Scheduler: BAT scheduled (" + String(config.detectedModules) + " modules)");
        }
        lastBat = now;
    }

    if (now - lastStat >= config.battery.intervalStat) {
        if (config.battery.enableStat) {
            for (int i = 1; i <= config.detectedModules; i++) {
                enqueue("stat " + String(i));
            }
            Log(LOG_INFO, "Scheduler: STAT scheduled (" + String(config.detectedModules) + " modules)");
        }
        lastStat = now;
    }

    static unsigned long lastSend = 0;

    if (!queue.empty()) {

        // 1 second pause between commands
        if (millis() - lastSend < 1000)
            return;

        String cmd = popNextCommand();

        int id = nextCommandId++;   // unique ID for this command

        if (cmd.startsWith("console:")) {

            String realCmd = cmd.substring(8);  // strip "console:"

            Log(LOG_INFO, "Scheduler: DIRECT console command → " + realCmd);

            if (uart->sendCommand(realCmd.c_str(), id)) {
                lastSend = millis();
                lastCommandFinished = lastSend;
            } else {
                Log(LOG_WARN, "Scheduler: sendCommand failed for console '" + realCmd + "'");
            }

            return;
        }

        if (cmd.length() > 0) {

            Log(LOG_INFO, "Scheduler: send → " + cmd + " (id=" + String(id) + ")");

            if (uart->sendCommand(cmd.c_str(), id)) {
                lastSend = millis();
                lastCommandFinished = lastSend;
            } else {
                Log(LOG_WARN, "Scheduler: sendCommand failed for '" + cmd + "'");
            }
        }
    }
}
