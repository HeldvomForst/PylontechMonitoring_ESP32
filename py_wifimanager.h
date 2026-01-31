#pragma once
#include <Arduino.h>

namespace WiFiManagerModule {

    void begin();
    void loop();

    void connect(const String& ssid, const String& pass);
    void resetWiFi();

    void startTemporaryAP(unsigned long durationMs);

    String getStatusJson();
    String scanJson();
}