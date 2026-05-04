#pragma once

// ---------------------------------------------------------
// TFT_eSPI Projektkonfiguration (ersetzt User_Setup.h)
// ---------------------------------------------------------
#define USER_SETUP_LOADED

#define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// SPI Pins
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS    5
#define TFT_DC    4
#define TFT_RST   2
#define TFT_BL   15   // LED / Backlight

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

// SPI Speed
#define SPI_FREQUENCY  40000000

#include <TFT_eSPI.h>

// ---------------------------------------------------------
// Display-Klasse
// ---------------------------------------------------------
class PyDisplay {
public:
    void begin();
    void loop();
    void showTestbild();

private:
    TFT_eSPI tft = TFT_eSPI();
    bool testbildShown = false;
};
