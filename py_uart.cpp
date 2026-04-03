#include "py_uart.h"
#include "py_log.h"
#include "py_parser_pwr.h"
#include "py_parser_bat.h"
#include "py_parser_stat.h"

#define BAT_RX_PIN 16
#define BAT_TX_PIN 17

// Global receive buffer
static char g_szRecvBuff[7000];

// Count invalid frames to detect lost communication
static int g_invalidCount = 0;

// ---------------------------------------------------------
// Frame validator: Pylontech console frame
// ---------------------------------------------------------
static bool isValidFrame(const String& f) {
    // Must contain '@' (start of data)
    if (f.indexOf("@") < 0) return false;

    // Must contain '$$' (end of data)
    int posEnd = f.indexOf("$$");
    if (posEnd < 0) return false;

    // Must contain 'pylon>' (console prompt)
    if (f.indexOf("pylon>") < 0) return false;

    // Must contain at least 3 lines
    int lines = 0;
    for (int i = 0; i < f.length(); i++)
        if (f[i] == '\n') lines++;
    if (lines < 3) return false;

    // Must not be a pure wake-up prompt
    if (f.indexOf("Press [Enter]") >= 0 && f.indexOf("Remote command:") < 0)
        return false;

    // Minimum length check
    if (f.length() < 40) return false;

    return true;
}

// ---------------------------------------------------------
// Initialize UART
// ---------------------------------------------------------
void PyUart::begin(int rx, int tx) {
    rxPin = rx;
    txPin = tx;

    Serial2.begin(115200, SERIAL_8N1, rxPin, txPin);
    delay(50);

    Log(LOG_INFO, "UART: begin()");

    commReady     = false;
    busy          = false;
    frameReady    = false;
    frameValid    = false;
    lastCommand   = "";
    lastRawFrame  = "";
    lastPwrFrame  = "";
    lastBatFrame  = "";
    lastStatFrame = "";
    g_invalidCount = 0;

    // Initial wake-up once at startup
    wakeUpConsole();
}

// ---------------------------------------------------------
// Switch baud rate (only used during wake-up)
// ---------------------------------------------------------
void PyUart::switchBaud(int newRate) {
    Serial2.flush();
    delay(20);
    Serial2.end();
    delay(20);
    Serial2.begin(newRate, SERIAL_8N1, rxPin, txPin);
    delay(20);
}

// ---------------------------------------------------------
// Wake up the Pylontech console
// ---------------------------------------------------------
void PyUart::wakeUpConsole() {
    Log(LOG_INFO, "UART: wakeUpConsole()");

    commReady = false;

    // Magic wake-up sequence at 1200 baud
    switchBaud(1200);
    Serial2.write("~20014682C0048520FCC3\r");
    delay(1000);

    byte nl[] = {0x0E, 0x0A};

    // Back to normal console baud
    switchBaud(115200);

    // Send newlines to trigger console prompt
    for (int i = 0; i < 10; i++) {
        Serial2.write(nl, 2);
        delay(1000);

        if (Serial2.available()) {
            while (Serial2.available()) Serial2.read();
            break;
        }
    }

    commReady = true;
    g_invalidCount = 0;
    Log(LOG_INFO, "UART: wakeUpConsole complete → commReady=true");
}

// ---------------------------------------------------------
// Read full frame (blocking, RAW, UNFILTERED)
// Handles pagination: "Press [Enter] to be continued"
// ---------------------------------------------------------
int PyUart::readFromSerial() {
    memset(g_szRecvBuff, 0, sizeof(g_szRecvBuff));
    int recvLen = 0;

    // Wait up to 1.5 seconds for first byte
    for (int i = 0; i < 150 && !Serial2.available(); ++i)
        delay(10);

    // Read until no more data or end marker found
    while (Serial2.available()) {
        char buf[256] = "";
        int r = Serial2.readBytesUntil('>', buf, sizeof(buf) - 1);

        if (r > 0) {
            if (r + recvLen + 1 >= (int)sizeof(g_szRecvBuff)) {
                Log(LOG_WARN, "UART: read overflow");
                break;
            }

            strcat(g_szRecvBuff, buf);
            recvLen += r;

            // End-of-frame marker
            if (strstr(g_szRecvBuff, "$$\r\n\rpylon")) {
                strcat(g_szRecvBuff, ">");
                break;
            }

            // Console prompt continuation
            if (strstr(g_szRecvBuff, "Press [Enter] to be continued,other key to exit"))
                Serial2.write("\r");

            // Small wait for more data
            for (int j = 0; j < 20 && !Serial2.available(); ++j)
                delay(10);
        } else break;
    }

    return recvLen;
}

// ---------------------------------------------------------
// Send command + read response
// ---------------------------------------------------------
bool PyUart::sendCommandAndReadSerialResponse(const char* cmd) {
    // Log TX
    if (cmd && cmd[0]) {
        Log(LOG_DEBUG, "UART TX: '" + String(cmd) + "'");
        Serial2.write(cmd);
    }

    Serial2.write("\n");
    Log(LOG_DEBUG, "UART TX: '\\n'");
    Serial2.flush();

    int len = readFromSerial();
    return (len > 0);
}

// ---------------------------------------------------------
// Public API: send command and parse frame
// ---------------------------------------------------------
bool PyUart::sendCommand(const char* cmd) {

    // If communication was lost → try wake-up once
    if (!commReady) {
        Log(LOG_WARN, "UART: commReady=false → wakeUpConsole()");
        wakeUpConsole();
        if (!commReady) {
            Log(LOG_ERROR, "UART: wakeUpConsole failed");
            return false;
        }
    }

    // Clear RX buffer
    while (Serial2.available()) Serial2.read();
    delay(10);

    lastCommand = String(cmd);

    busy       = true;
    frameReady = false;
    frameValid = false;

    // Send + receive
    if (!sendCommandAndReadSerialResponse(cmd)) {
        busy = false;
        g_invalidCount++;
        Log(LOG_WARN, "UART: no response, invalidCount=" + String(g_invalidCount));

        if (g_invalidCount > 3) {
            commReady = false;
            Log(LOG_ERROR, "UART: too many failures → commReady=false");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return false;
    }

    // Raw data from buffer
    String raw = String(g_szRecvBuff);

    // Always store raw frame
    lastRawFrame = raw;
    frameReady   = true;

    // Validate frame
    frameValid = isValidFrame(raw);

    if (!frameValid) {
        Log(LOG_WARN, "UART: invalid frame received");

        g_invalidCount++;
        if (g_invalidCount > 3) {
            commReady = false;
            Log(LOG_ERROR, "UART: too many invalid frames → commReady=false");
        }

        busy = false;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return false;
    }

    // Valid frame
    g_invalidCount = 0;

    // Store last valid frame for web UI
    if (lastCommand == "pwr") {
        lastPwrFrame = raw;
    } else if (lastCommand.startsWith("bat")) {
        lastBatFrame = raw;
    } else if (lastCommand.startsWith("stat")) {
        lastStatFrame = raw;
    }

    // Parser routing
    if (lastCommand == "pwr") {
        BatteryStack tmpStack;
        std::vector<BatteryModule> tmpModules;
        parsePwrFrame(raw, tmpStack, tmpModules);
    }
    else if (lastCommand.startsWith("bat")) {
        int idx = lastCommand.substring(4).toInt();
        BatData bat;
        parseBatFrame(idx, raw, bat);
    }
    else if (lastCommand.startsWith("stat")) {
        int idx = lastCommand.substring(5).toInt();
        StatData stat;
        parseStatFrame(idx, raw, stat);
    }

    busy = false;

    // Fixed delay after every frame (valid or invalid)
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    return true;
}

// ---------------------------------------------------------
void PyUart::loop() {
    // intentionally empty
}

// ---------------------------------------------------------
String PyUart::getFrame() {
    frameReady = false;
    return lastRawFrame;
}

String PyUart::getLastCommand() const { return lastCommand; }
bool   PyUart::isFrameValid() const   { return frameValid; }
bool   PyUart::hasFrame() const       { return frameReady; }
bool   PyUart::isReady() const        { return commReady; }
bool   PyUart::isBusy() const         { return busy; }