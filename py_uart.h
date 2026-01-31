#pragma once
#include <Arduino.h>

class PyUart {
public:
    void begin(int rx, int tx);
    void loop();

    bool sendCommand(const char* cmd);
    String getFrame();

    bool isReady() const;
    bool isBusy() const;
    bool hasFrame() const;

    String getLastRawFrame() const { return lastRawFrame; }
    bool isFrameValid() const { return frameValid; }

    String getLastCommand() const { return lastCommand; }

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
};