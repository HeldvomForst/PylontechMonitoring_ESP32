#pragma once
#include <Arduino.h>

// ---------------------------------------------------------
// PyUart
// Handles all communication with the Pylontech battery.
// This class is intentionally synchronous and blocking,
// because the battery protocol requires strict timing,
// wake‑up sequences, and sequential command/response flow.
//
// IMPORTANT:
// - This class must run ONLY inside the real‑time task (Core 0).
// - No other task may access Serial2.
// - Parser is executed AFTER a complete frame is received.
// - Last PWR/BAT/STAT frames are stored for the website.
// ---------------------------------------------------------
class PyUart {
public:
    void begin(int rx, int tx);
    void loop();   // intentionally empty (legacy compatibility)

    // Send command and receive full frame (blocking)
    bool sendCommand(const char* cmd);

    // Frame access
    bool hasFrame() const;
    bool isFrameValid() const;
    String getFrame();
    String getLastCommand() const;

    // Last known frames for website
    String getLastPwrFrame() const { return lastPwrFrame; }
    String getLastBatFrame() const { return lastBatFrame; }
    String getLastStatFrame() const { return lastStatFrame; }
    String getLastRawFrame() const { return lastRawFrame; }

    // Status
    bool isReady() const;
    bool isBusy() const;

private:
    void switchBaud(int newRate);
    void wakeUpConsole();
    int  readFromSerial();
    bool sendCommandAndReadSerialResponse(const char* cmd);

    bool commReady = false;
    bool busy = false;
    bool frameReady = false;
    bool frameValid = false;

    int rxPin = -1;
    int txPin = -1;

    String lastCommand;
    String lastRawFrame;

    // NEW: store last frames for website
    String lastPwrFrame;
    String lastBatFrame;
    String lastStatFrame;
};