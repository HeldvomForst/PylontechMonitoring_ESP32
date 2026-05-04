#include <Arduino.h>
#include "py_display.h"

void PyDisplay::begin() {
    // Backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    tft.drawString("ILI9341 gestartet", 10, 10);
}

void PyDisplay::loop() {
    if (!testbildShown && millis() > 500) {
        showTestbild();
        testbildShown = true;
    }
}

void PyDisplay::showTestbild() {
    tft.fillRect(0,   0, 240, 80, TFT_RED);
    tft.fillRect(0,  80, 240, 80, TFT_GREEN);
    tft.fillRect(0, 160, 240, 80, TFT_BLUE);
    tft.fillRect(0, 240, 240, 80, TFT_YELLOW);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("TESTBILD ILI9341", 30, 140);
}
