#include "py_scheduler.h"
#include "py_log.h"
#include "py_parser_pwr.h"
#include "py_parser_pwr.h"

// Global instance
PyScheduler py_scheduler;

void PyScheduler::begin(PyUart* u) {
    uart = u;

    // Initialize intervals from config (PWR + BAT + STAT)
    setIntervals(
        config.battery.intervalPwr,
        config.battery.intervalBat,
        config.battery.intervalStat
    );

    unsigned long now = millis();
    lastPwr  = now;
    lastBat  = now;
    lastStat = now;
}

void PyScheduler::setIntervals(unsigned long pwrMs, unsigned long batMs, unsigned long statMs) {
    intervalPwr  = pwrMs;
    intervalBat  = batMs;
    intervalStat = statMs;
}

void PyScheduler::enqueue(PyCommandType type, int index) {
    PyScheduledCommand cmd;
    cmd.type = type;
    cmd.batteryIndex = index;
    queue.push_back(cmd);
}

void PyScheduler::enqueueRaw(const String& rawCmd) {
    PyScheduledCommand cmd;
    cmd.type = CMD_RAW;
    cmd.rawCommand = rawCmd;
    queue.push_back(cmd);
}

/**
 * Automatically schedule standard commands based on intervals.
 * - PWR: stack + module data (parsed by PWR parser)
 * - BAT: per-battery command (later extended)
 * - STAT: diagnostic command (later extended)
 */
void PyScheduler::scheduleAutomatic() {
    unsigned long now = millis();

    if (intervalPwr > 0 && now - lastPwr >= intervalPwr) {
        enqueue(CMD_PWR);
        lastPwr = now;
        Log(LOG_INFO, "Scheduler: auto CMD_PWR enqueued");
    }

    if (intervalBat > 0 && now - lastBat >= intervalBat) {
        enqueue(CMD_BAT, 1);   // TODO: dynamic battery index
        lastBat = now;
        Log(LOG_INFO, "Scheduler: auto CMD_BAT enqueued");
    }

    if (intervalStat > 0 && now - lastStat >= intervalStat) {
        enqueue(CMD_STAT, 1);  // TODO: dynamic battery index
        lastStat = now;
        Log(LOG_INFO, "Scheduler: auto CMD_STAT enqueued");
    }
}

/**
 * Process the queue when UART is ready.
 */
void PyScheduler::processQueue() {
    if (!uart) return;
    if (!uart->isReady()) return;
    if (uart->isBusy()) return;
    if (queue.empty()) return;

    PyScheduledCommand cmd = queue.front();
    queue.erase(queue.begin());

    Log(LOG_INFO, "Scheduler: processing command type=" + String(cmd.type));

    switch (cmd.type) {
        case CMD_PWR:
            uart->sendCommand("pwr");
            break;

        case CMD_BAT: {
            String s = "bat ";
            s += cmd.batteryIndex;
            uart->sendCommand(s.c_str());
            break;
        }

        case CMD_STAT: {
            String s = "stat ";
            s += cmd.batteryIndex;
            uart->sendCommand(s.c_str());
            break;
        }

        case CMD_RAW:
            uart->sendCommand(cmd.rawCommand.c_str());
            break;
    }
}

/**
 * Handle a complete frame received from UART.
 * Currently:
 *  - Only PWR frames are parsed
 *  - BAT and STAT frames will be added later
 */
void PyScheduler::handleFrame(const String& frame) {
    ParseResult r = parsePwrFrame(frame, stack, modules);

    if (r == PARSE_FAIL) {
        Log(LOG_WARN, "Scheduler: PWR parser failed");
        return;
    }

    if (r == PARSE_IGNORED) {
        return;
    }

    // No direct MQTT calls here.
    // MQTT uses global parser state (lastParsedStack / lastParsedModules).
}

/**
 * Main loop:
 *  - Schedule automatic commands
 *  - Process queue
 *  - Handle incoming frames
 */
void PyScheduler::loop() {
    scheduleAutomatic();
    processQueue();

    if (uart && uart->hasFrame()) {
        String frame = uart->getFrame();
        handleFrame(frame);
    }
}