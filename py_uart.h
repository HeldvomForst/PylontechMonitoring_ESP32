#pragma once
#include <Arduino.h>

class PyUart {
public:
    void begin(int rx, int tx);
    void loop();   // intentionally empty

    // Blocking command → fills lastRawFrame
    bool sendCommand(const char* cmd);

    // Frame access for realtimeTask()
    bool hasFrame() const { return frameReady; }
    bool isFrameValid() const { return frameValid; }
    String getFrame();   // returns lastRawFrame and clears frameReady

    // Status
    bool isReady() const { return commReady; }
    bool isBusy() const { return busy; }
    String getLastCommand() const { return lastCommand; }

    // Last frames for web UI
    String getLastPwrFrame() const { return lastPwrFrame; }
    String getLastBatFrame() const { return lastBatFrame; }
    String getLastStatFrame() const { return lastStatFrame; }
    String getLastRawFrame() const { return lastRawFrame; }

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

    String lastPwrFrame;
    String lastBatFrame;
    String lastStatFrame;
};
