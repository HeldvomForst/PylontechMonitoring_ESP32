#include "py_uart.h"
#include "py_log.h"

#define BAT_RX_PIN 16
#define BAT_TX_PIN 17

// Global receive buffer (same as old .ino)
static char g_szRecvBuff[7000];

// ---------------------------------------------------------
// Frame Validator
// Checks whether a received frame looks complete and valid.
// We do NOT reject frames here — we only mark them as valid/invalid.
// The parser decides whether to process them.
// ---------------------------------------------------------
static bool isValidFrame(const String& f) {
    // 1) Must start with '@'
    if (f.indexOf("@") < 0) return false;

    // 2) Must contain "$$" (end of data section)
    if (f.indexOf("$$") < 0) return false;

    // 3) Must end with "pylon"
    if (f.indexOf("pylon") < 0) return false;

    // 4) Must contain at least 3 lines
    int lines = 0;
    for (int i = 0; i < f.length(); i++)
        if (f[i] == '\n') lines++;
    if (lines < 3) return false;

    // 5) Must not contain console prompts
    if (f.indexOf("Press [Enter]") >= 0) return false;

    // 6) Minimum length check
    if (f.length() < 50) return false;

    return true;
}

// ---------------------------------------------------------
// Initialize UART communication
// ---------------------------------------------------------
void PyUart::begin(int rx, int tx) {
    rxPin = rx;
    txPin = tx;

    Serial2.begin(115200, SERIAL_8N1, rxPin, txPin);
    delay(50);

    Log(LOG_INFO, "UART: begin()");

    commReady = false;
    busy = false;
    frameReady = false;
    frameValid = false;

    wakeUpConsole();
}

// ---------------------------------------------------------
// Switch UART baud rate (legacy logic from old .ino)
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
// Wake up the Pylontech console (legacy logic)
// Sends magic sequence at 1200 baud, then switches back.
// ---------------------------------------------------------
void PyUart::wakeUpConsole() {
    Log(LOG_INFO, "UART: wakeUpConsole()");

    switchBaud(1200);
    Serial2.write("~20014682C0048520FCC3\r");
    delay(1000);

    byte nl[] = {0x0E, 0x0A};

    switchBaud(115200);

    // Try sending newline until console responds
    for (int i = 0; i < 10; i++) {
        Serial2.write(nl, 2);
        delay(1000);

        if (Serial2.available()) {
            while (Serial2.available()) Serial2.read();
            break;
        }
    }

    commReady = true;
    Log(LOG_INFO, "UART: wakeUpConsole complete → commReady=true");
}

// ---------------------------------------------------------
// Read from UART until either:
// - "pylon>" appears (end of frame)
// - timeout occurs
// - buffer is full
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
// Send command and read response (legacy logic)
// ---------------------------------------------------------
bool PyUart::sendCommandAndReadSerialResponse(const char* cmd) {
    switchBaud(115200);

    if (cmd && cmd[0]) Serial2.write(cmd);
    Serial2.write("\n");

    if (readFromSerial() > 0) return true;

    // Retry by waking console
    wakeUpConsole();

    if (cmd && cmd[0]) Serial2.write(cmd);
    Serial2.write("\n");

    return readFromSerial() > 0;
}

// ---------------------------------------------------------
// Public API: send command and capture frame
// Adds frame validation flag
// ---------------------------------------------------------
bool PyUart::sendCommand(const char* cmd) {
    if (!commReady) return false;

    lastCommand = String(cmd); 
    
    busy = true;
    frameReady = false;
    frameValid = false;

    if (!sendCommandAndReadSerialResponse(cmd)) {
        busy = false;
        return false;
    }

    String raw = String(g_szRecvBuff);

    // Validate frame (but do NOT discard it)
    frameValid = isValidFrame(raw);

    if (!frameValid) {
        Log(LOG_WARN, "UART: invalid frame received (parser will skip it)");
    }

    lastRawFrame = raw;
    frameReady = true;
    busy = false;

    return true;
}

// ---------------------------------------------------------
// loop() does nothing because UART logic is blocking
// ---------------------------------------------------------
void PyUart::loop() {
    // intentionally empty
}

// ---------------------------------------------------------
// Retrieve last frame
// ---------------------------------------------------------
String PyUart::getFrame() {
    frameReady = false;
    return lastRawFrame;
}

bool PyUart::isReady() const { return commReady; }
bool PyUart::isBusy() const { return busy; }
bool PyUart::hasFrame() const { return frameReady; }