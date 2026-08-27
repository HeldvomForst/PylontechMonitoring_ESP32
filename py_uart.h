#pragma once
#include <Arduino.h>

extern volatile int consoleTicket;
extern volatile int consoleTicketFrameReady;
extern String consolePendingCommand;

enum FrameType {
    FRAME_UNKNOWN = 0,
    FRAME_PWR,
    FRAME_BAT,
    FRAME_STAT
};

// Queue-Struct: KEINE Strings mehr!
struct UartFrame {
    FrameType type;
    int       module;
    int       commandId;   // Zuordnung Kommando → Frame
};

class PyUart {
public:
    void begin(int rx, int tx);
    bool sendCommand(const char* cmd, int cmdId);
    void loop();

    bool isReady() const { return commReady; }
    bool isBusy()  const { return busy; }

    String getLastCommand() const { return lastCommand; }
    String getLastRawFrame() const { return lastRawFrame; }

    bool isFrameValid() const { return frameValid; }

private:
    void switchBaud(int newRate);
    void wakeUpConsole();

    bool commReady   = false;
    bool busy        = false;
    bool frameValid  = false;

    int rxPin = -1;
    int txPin = -1;

    String lastCommand;
    String lastRawFrame;

    int lastCommandId = 0;   // ID des zuletzt gesendeten Befehls

    enum UartState {
        UART_IDLE,
        UART_WAITING,
        UART_COLLECTING
    };

    UartState      state      = UART_IDLE;
    unsigned long  cmdStart   = 0;
};
