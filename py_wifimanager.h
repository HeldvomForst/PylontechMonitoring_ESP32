// py_wifimanager.h
#pragma once
#include <Arduino.h>

struct WifiStatus {
    String mode;
    bool   connected;
    String ssid;
    int    rssi;
    String ip;
    String mac;
};

namespace WiFiManagerModule {

    // Core lifecycle
    void begin();
    void loop();

    // Change credentials (STA)
    void connect(const String& ssid, const String& pass);

    // Clear credentials and force AP + restart
    void resetWiFi();

    // Force AP on for a limited time (e.g. button)
    void startTemporaryAP(unsigned long durationMs);

    // Status / diagnostics
    WifiStatus getStatus();
    String    getStatusJson();
    String    scanJson();

    // Time / NTP
    void setManualTime(int year, int month, int day,
                       int hour, int minute, bool dst);

    // Apply new IP config (static/DHCP) and reconnect STA
    void applyNetworkConfig();
}
