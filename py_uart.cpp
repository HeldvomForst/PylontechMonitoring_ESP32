#include "py_uart.h"
#include "py_log.h"
#include "config.h"
#include <string.h>

// Pins
#define BAT_RX_PIN 16
#define BAT_TX_PIN 17

// Static RX buffer for lossless reception
static char   rxBuf[8192];
static size_t rxPos = 0;

static int g_invalidCount = 0;
static int enterCount     = 0;

// Auto wakeup counter
static int wakeupCounter = 2;

// Frame queue comes from the .ino
extern QueueHandle_t frameQueue;

volatile int consoleTicket = 0;
volatile int consoleTicketFrameReady = 0;
String consolePendingCommand = "";

// ---------------------------------------------------------
static bool isValidFrame(const String& f) {
    if (f.indexOf("@") < 0) {
        Log(LOG_DEBUG, "UART: invalid frame → missing '@'");
        return false;
    }
    if (f.indexOf("$$") < 0) {
        Log(LOG_DEBUG, "UART: invalid frame → missing '$$'");
        return false;
    }
    if (f.length() < 40) {
        Log(LOG_DEBUG, "UART: invalid frame → too short (" + String(f.length()) + ")");
        return false;
    }

    int lines = 0;
    for (int i = 0; i < f.length(); i++)
        if (f[i] == '\n') lines++;
    if (lines < 3) return false;

    if (f.indexOf("Press [Enter]") >= 0 &&
        f.indexOf("Remote command:") < 0)
        return false;

    return true;
}

// ---------------------------------------------------------
void PyUart::begin(int rx, int tx) {
    rxPin = rx;
    txPin = tx;

    Serial2.setRxBufferSize(4096);
    Serial2.begin(115200, SERIAL_8N1, rxPin, txPin);
    delay(50);

    Log(LOG_INFO, "UART: begin() RX=" + String(rxPin) + " TX=" + String(txPin));

    commReady      = true;
    busy           = false;
    frameValid     = false;
    lastCommand    = "";
    lastRawFrame   = "";
    lastCommandId  = 0;
    g_invalidCount = 0;
    wakeupCounter  = 2;

    state    = UART_IDLE;
    cmdStart = 0;
    rxPos    = 0;
    rxBuf[0] = '\0';
}

// ---------------------------------------------------------
void PyUart::switchBaud(int newRate) {
    Log(LOG_DEBUG, "UART: switchBaud(" + String(newRate) + ")");
    Serial2.flush();
    delay(20);
    Serial2.end();
    delay(20);
    Serial2.begin(newRate, SERIAL_8N1, rxPin, txPin);
    delay(20);
}

// ---------------------------------------------------------
void PyUart::wakeUpConsole() {
    Log(LOG_WARN, "UART: wakeUpConsole() triggered");

    commReady = false;

    switchBaud(1200);
    Serial2.write("~20014682C0048520FCC3\r");
    delay(500);

    byte nl[] = {0x0E, 0x0A};
    switchBaud(115200);

    for (int i = 0; i < 3; i++) {
        Serial2.write(nl, 2);
        delay(200);

        if (Serial2.available()) {
            while (Serial2.available()) Serial2.read();
            break;
        }
    }

    commReady      = true;
    g_invalidCount = 0;

    Log(LOG_INFO, "UART: wakeUpConsole complete → commReady=true");

    Serial2.write("pwr\n");
    Log(LOG_INFO, "UART: auto-PWR after wakeUp");
}

// ---------------------------------------------------------
bool PyUart::sendCommand(const char* cmd, int cmdId) {

    enterCount = 0;

    if (!commReady) {
        Log(LOG_WARN, "UART: commReady=false → skip command");
        return false;
    }

    if (busy || state != UART_IDLE) {
        Log(LOG_WARN, "UART: busy");
        return false;
    }

    while (Serial2.available()) Serial2.read();
    delay(10);

    lastCommandId = cmdId;
    lastCommand   = String(cmd);
    frameValid    = false;

    rxPos    = 0;
    rxBuf[0] = '\0';

    Log(LOG_DEBUG, "UART TX: '" + lastCommand + "' (id=" + String(lastCommandId) + ")");
    Serial2.write(cmd);
    Serial2.write("\n");
    Serial2.flush();

    busy     = true;
    state    = UART_WAITING;
    cmdStart = millis();

    return true;
}

// ---------------------------------------------------------
void PyUart::loop() {

    static unsigned long lastByteTime   = 0;
    static bool          sawEnterPrompt = false;

    // 1. Collect bytes
    while (Serial2.available()) {

        char c = (char)Serial2.read();

        if (rxPos < sizeof(rxBuf) - 1) {
            rxBuf[rxPos++] = c;
            rxBuf[rxPos]   = '\0';
        }

        lastByteTime = millis();

        if (state == UART_WAITING)
            state = UART_COLLECTING;

        if (!sawEnterPrompt &&
            strstr(rxBuf, "Press [Enter] to be continued") != nullptr)
        {
            sawEnterPrompt = true;
        }
    }

    if (state == UART_IDLE)
        return;

    // 2. Timeout
    if (millis() - cmdStart > 1500) {

        Log(LOG_WARN, "UART: timeout");

        busy = false;
        state = UART_IDLE;
        rxPos = 0;
        rxBuf[0] = '\0';
        sawEnterPrompt = false;

        UartFrame f;
        f.type      = FRAME_UNKNOWN;
        f.module    = 0;
        f.commandId = lastCommandId;

        xQueueSend(frameQueue, &f, 0);
        return;
    }

    // 3. Wait for quiet time
    if (millis() - lastByteTime < 150)
        return;

    // 4. ENTER handling for multi-part frames
    if (sawEnterPrompt &&
        millis() - lastByteTime > 200 &&
        strstr(rxBuf, "$$") == nullptr)
    {
        if (enterCount < 2) {
            Serial2.write("\r");
            Log(LOG_DEBUG, "UART: sending ENTER (safe)");
            sawEnterPrompt = false;
            enterCount++;
            return;
        }

        Log(LOG_WARN, "UART: ENTER limit reached → sending 'q'");

        lastRawFrame = String(rxBuf, rxPos);
        frameValid   = false;

        busy     = false;
        state    = UART_IDLE;

        rxPos = 0;
        rxBuf[0] = '\0';
        sawEnterPrompt = false;

        UartFrame f;
        f.type      = FRAME_UNKNOWN;
        f.module    = 0;
        f.commandId = lastCommandId;

        xQueueSend(frameQueue, &f, 0);
        return;
    }

    // 5. Check for frame end
    bool hasEndToken = (strstr(rxBuf, "$$") != nullptr);
    if (!hasEndToken)
        return;

    // 6. Frame complete
    Log(LOG_DEBUG, "UART: frame complete (" + String(rxPos) + " bytes)");

    lastRawFrame = String(rxBuf, rxPos);

    frameValid = isValidFrame(lastRawFrame);

    busy     = false;
    state    = UART_IDLE;

    rxPos = 0;
    rxBuf[0] = '\0';
    sawEnterPrompt = false;

    UartFrame f;
    f.type      = FRAME_UNKNOWN;
    f.module    = 0;
    f.commandId = lastCommandId;

    if (lastCommand == "pwr") {
        f.type = FRAME_PWR;
    }
    else if (lastCommand.startsWith("bat")) {
        f.type   = FRAME_BAT;
        f.module = lastCommand.substring(3).toInt();
    }
    else if (lastCommand.startsWith("stat")) {
        f.type   = FRAME_STAT;
        f.module = lastCommand.substring(4).toInt();
    }

    // Mark console frame ready if this command came from console
    if (lastCommand == consolePendingCommand) {
        consoleTicketFrameReady = consoleTicket;
        consolePendingCommand   = "";
    }

    xQueueSend(frameQueue, &f, 0);
}
