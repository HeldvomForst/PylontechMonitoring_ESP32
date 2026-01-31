#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "py_systemmanager.h"
#include "py_log.h"          // Log()
#include "py_wifimanager.h"  // WiFi reset
#include "config.h"          // zentrale Konfig

// ----------------------------------------------------
// Konfiguration
// ----------------------------------------------------
static const uint8_t BUTTON_PIN = 0;   // BOOT-Button
static const unsigned long LONG_PRESS_TIME = 15000; // 15s
static const unsigned long SHORT_PRESS_MAX = 1000;  // <1s
static const unsigned long MULTI_PRESS_TIMEOUT = 1000; // 1s Pause

// ----------------------------------------------------
// Interne Variablen
// ----------------------------------------------------
static unsigned long buttonPressStart = 0;
static unsigned long lastButtonRelease = 0;
static int shortPressCount = 0;
static bool buttonWasPressed = false;

// Boot-Counter
static Preferences prefs;
static int bootCounter = 0;

// ----------------------------------------------------
// Initialisierung
// ----------------------------------------------------
void SystemManager::begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    prefs.begin("system", false);
    bootCounter = prefs.getInt("bootcount", 0);
    bootCounter++;
    prefs.putInt("bootcount", bootCounter);
    prefs.end();

    Log(LOG_INFO, "SystemManager: BootCounter = " + String(bootCounter));
}

// ----------------------------------------------------
// Button-Handling
// ----------------------------------------------------
void SystemManager::loop() {
    handleButton();
}

// ----------------------------------------------------
// Button-Auswertung
// ----------------------------------------------------
void SystemManager::handleButton() {
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    unsigned long now = millis();

    // --- Button gedrückt ---
    if (pressed && !buttonWasPressed) {
        buttonWasPressed = true;
        buttonPressStart = now;
    }

    // --- Button losgelassen ---
    if (!pressed && buttonWasPressed) {
        buttonWasPressed = false;
        unsigned long pressDuration = now - buttonPressStart;

        // Langer Druck → Factory Reset
        if (pressDuration > LONG_PRESS_TIME) {
            Log(LOG_WARN, "SystemManager: LONG PRESS → FACTORY RESET");
            triggerFactoryReset();
            return;
        }

        // Kurzer Druck → zählen
        if (pressDuration < SHORT_PRESS_MAX) {
            shortPressCount++;
            lastButtonRelease = now;
        }
    }

    // --- Auswertung der kurzen Drücke ---
    if (shortPressCount > 0 && (now - lastButtonRelease > MULTI_PRESS_TIMEOUT)) {

        if (shortPressCount == 1) {
            Log(LOG_INFO, "SystemManager: 1x short press → AP TEMPORARY");
            triggerAPTemporary();
        }

        if (shortPressCount >= 5) {
            Log(LOG_WARN, "SystemManager: 5x short press → WIFI RESET");
            triggerWiFiReset();
        }

        shortPressCount = 0;
    }
}

// ----------------------------------------------------
// Event-Funktionen
// ----------------------------------------------------
void SystemManager::triggerAPTemporary() {
    if (onAPTemporary) onAPTemporary();
}

void SystemManager::triggerWiFiReset() {
    if (onWiFiReset) onWiFiReset();
}

void SystemManager::triggerFactoryReset() {

    Log(LOG_ERROR, "SystemManager: FACTORY RESET initiated");
    delay(200);

    // ----------------------------------------------------
    // ALLE gespeicherten Daten löschen
    // ----------------------------------------------------
    Preferences p;

    const char* namespaces[] = {
        "wifi",
        "config",
        "system",
        "battery",
        "scheduler",
        "uart"
    };

    for (auto ns : namespaces) {
        p.begin(ns, false);
        p.clear();
        p.end();
        Log(LOG_INFO, String("SystemManager: cleared namespace '") + ns + "'");
    }

    // ----------------------------------------------------
    // Optional: WiFi trennen
    // ----------------------------------------------------
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);

    Log(LOG_WARN, "SystemManager: all data cleared → restarting...");
    delay(500);

    ESP.restart();
}

// ----------------------------------------------------
// BootCounter Getter
// ----------------------------------------------------
int SystemManager::getBootCounter() {
    return bootCounter;
}

// ----------------------------------------------------
// Callback-Variablen
// ----------------------------------------------------
std::function<void()> SystemManager::onAPTemporary = nullptr;
std::function<void()> SystemManager::onWiFiReset = nullptr;
std::function<void()> SystemManager::onFactoryReset = nullptr;