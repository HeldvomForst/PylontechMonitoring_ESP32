#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

#include "py_wifimanager.h"
#include "config.h"
#include "py_log.h"
#include "py_display.h"

using namespace WiFiManagerModule;

extern PyDisplay display;

// ----------------------------------------------------
// Internal state
// ----------------------------------------------------
static Preferences prefs;

static bool   apActive              = false;
static bool   staConnecting         = false;
static unsigned long staStartTime   = 0;

static unsigned long apHoldUntil    = 0;     // AP forced ON until timestamp
static unsigned long staLostTime    = 0;
static unsigned long lastRetry      = 0;

static bool   ntpInitialSynced      = false;
static unsigned long lastNtpResyncMillis = 0;

static bool   wifiDebugMesh         = true;  // detailed mesh diagnostics
static wl_status_t lastWifiStatus   = WL_NO_SHIELD;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------
static bool hasValidIP() {
    return WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

static void applyWifiStability() {
    // maximale Stabilität: kein Sleep, hohe Sendeleistung
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
}

// ----------------------------------------------------
// Mesh-Diagnose: stärksten AP für SSID finden
// ----------------------------------------------------
static bool findStrongestAPForSSID(const String& targetSSID,
                                   int& bestChannel,
                                   uint8_t bestBssid[6]) {
    Log(LOG_INFO, "WiFiMeshDiag: scanning for SSID '" + targetSSID + "' ...");

    int n = WiFi.scanNetworks();
    Log(LOG_INFO, String("WiFiMeshDiag: scanNetworks result = ") + n);

    if (n <= 0) {
        Log(LOG_WARN, "WiFiMeshDiag: no networks found");
        return false;
    }

    int bestIndex = -1;
    int bestRSSI  = -999;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int    rssi = WiFi.RSSI(i);
        int    chan = WiFi.channel(i);
        uint8_t* b  = WiFi.BSSID(i);

        char bssidStr[32];
        sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                b[0], b[1], b[2], b[3], b[4], b[5]);

        if (ssid == targetSSID) {
            Log(LOG_INFO,
                "WiFiMeshDiag: MATCH → RSSI=" + String(rssi) +
                " dBm, CH=" + String(chan) +
                ", BSSID=" + String(bssidStr));
            if (rssi > bestRSSI) {
                bestRSSI  = rssi;
                bestIndex = i;
            }
        } else {
            Log(LOG_DEBUG,
                "WiFiMeshDiag: OTHER → SSID=" + ssid +
                ", RSSI=" + String(rssi) +
                ", CH=" + String(chan));
        }
    }

    if (bestIndex < 0) {
        Log(LOG_WARN, "WiFiMeshDiag: no AP with matching SSID found");
        return false;
    }

    uint8_t* b = WiFi.BSSID(bestIndex);
    for (int i = 0; i < 6; i++) bestBssid[i] = b[i];
    bestChannel = WiFi.channel(bestIndex);

    Log(LOG_INFO,
        "WiFiMeshDiag: strongest AP → index=" + String(bestIndex) +
        ", RSSI=" + String(bestRSSI) +
        ", CH=" + String(bestChannel));

    return true;
}

// ----------------------------------------------------
// AP-Steuerung
// ----------------------------------------------------
static void startAP() {
    if (apActive) {
        Log(LOG_INFO, "WiFiManager: startAP() called but AP already active");
        return;
    }

    Log(LOG_INFO, "WiFiManager: starting AP");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.apSSID.c_str(), config.apPass.c_str());

    apActive = true;

    Log(LOG_INFO, "WiFiManager: AP IP = " + WiFi.softAPIP().toString());
    display.updateWifi(true, "192.168.4.1", 0, true);
}

static void stopAP() {
    if (!apActive) return;

    Log(LOG_INFO, "WiFiManager: stopping AP");
    WiFi.softAPdisconnect(true);
    apActive = false;

    bool sta = (WiFi.status() == WL_CONNECTED && hasValidIP());
    display.updateWifi(sta,
                       sta ? WiFi.localIP().toString() : "---",
                       sta ? WiFi.RSSI() : 0,
                       false);
}

// ----------------------------------------------------
// STA-Start mit Mesh-Auswahl
// ----------------------------------------------------
static void startSTA(const String& ssid, const String& pass) {
    Log(LOG_INFO, "WiFiManager: STA connecting to " + ssid);

    // sauber trennen
    WiFi.disconnect(true);
    vTaskDelay(150 / portTICK_PERIOD_MS);

    applyWifiStability();

    // IP-Konfiguration
    if (!config.useStaticIP) {
        Log(LOG_INFO, "WiFiManager: DHCP enabled → using DHCP");
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    } else {
        IPAddress ip, gw, mask, dns;
        ip.fromString(config.ipAddr);
        gw.fromString(config.gateway);
        mask.fromString(config.subnetMask);
        dns.fromString(config.dns);

        Log(LOG_INFO, "WiFiManager: Static IP → " + ip.toString());
        WiFi.config(ip, gw, mask, dns);
    }

    // AP bleibt ggf. aktiv → AP+STA
    if (apActive) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
    }

    int     bestChannel = 0;
    uint8_t bestBssid[6] = {0};

    bool haveMesh = findStrongestAPForSSID(ssid, bestChannel, bestBssid);
    if (haveMesh) {
        Log(LOG_INFO, "WiFiManager: using strongest AP (BSSID + channel)");
        WiFi.begin(ssid.c_str(), pass.c_str(), bestChannel, bestBssid, true);
    } else {
        Log(LOG_WARN, "WiFiManager: no strongest AP → normal WiFi.begin()");
        WiFi.begin(ssid.c_str(), pass.c_str());
    }

    staConnecting = true;
    staStartTime  = millis();
}

// ----------------------------------------------------
// STA aus Preferences oder Config
// ----------------------------------------------------
static void startSTAFromPrefsOrConfig() {
    prefs.begin("wifi", true);
    String ssid = prefs.getString("ssid", config.wifiSSID);
    String pass = prefs.getString("pass", config.wifiPass);
    prefs.end();

    if (ssid.length() == 0 || pass.length() < 8) {
        Log(LOG_WARN, "WiFiManager: no valid credentials → AP only");
        startAP();
        staConnecting = false;
        return;
    }

    startSTA(ssid, pass);
}

// ----------------------------------------------------
// NTP / Zeitzone
// ----------------------------------------------------
static void applyTimezoneFromConfig() {
    String tzString = findPosixForTimezone(config.timezone);
    setenv("TZ", tzString.c_str(), 1);
    tzset();
    Log(LOG_INFO, "WiFiManager: timezone applied: " + tzString);
}

static void triggerNtpSync() {
    if (config.manual_mode) {
        Log(LOG_INFO, "WiFiManager: manual time mode → skipping NTP sync");
        return;
    }

    IPAddress gw = WiFi.gatewayIP();
    if (gw != IPAddress(0,0,0,0)) {
        config.ntpServer = gw.toString();
        Log(LOG_INFO, "WiFiManager: using gateway as NTP server: " + config.ntpServer);
    } else {
        Log(LOG_WARN, "WiFiManager: gateway not available → skipping NTP");
        return;
    }

    configTime(0, 0, config.ntpServer.c_str());
    lastNtpResyncMillis = millis();

    Log(LOG_INFO, "WiFiManager: NTP sync requested");
}

static void handleNtpLogic() {
    if (!ntpInitialSynced && config.isSystemTimeValid()) {
        ntpInitialSynced = true;

        applyTimezoneFromConfig();

        String t = config.getCurrentTimeString();
        config.currentTime = t;
        config.save();

        Log(LOG_INFO, "WiFiManager: NTP initial sync → " + t);
    }

    if (ntpInitialSynced &&
        (millis() - lastNtpResyncMillis > config.ntpResyncInterval * 1000UL)) {
        Log(LOG_INFO, "WiFiManager: NTP periodic resync");
        triggerNtpSync();
    }
}

// ----------------------------------------------------
// Event-Logging (ESP32 Arduino Core 3.x)
// ----------------------------------------------------
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case WIFI_EVENT_STA_CONNECTED:
            Log(LOG_INFO, "WiFiEvent: STA connected");
            break;
        case IP_EVENT_STA_GOT_IP:
            Log(LOG_INFO, "WiFiEvent: STA got IP " +
                          WiFi.localIP().toString());
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            Log(LOG_WARN, "WiFiEvent: STA disconnected, reason=" +
                          String(info.wifi_sta_disconnected.reason));
            break;
        case IP_EVENT_STA_LOST_IP:
            Log(LOG_WARN, "WiFiEvent: STA lost IP");
            break;
        case WIFI_EVENT_AP_START:
            Log(LOG_INFO, "WiFiEvent: AP start");
            apActive = true;
            break;
        case WIFI_EVENT_AP_STOP:
            Log(LOG_INFO, "WiFiEvent: AP stop");
            apActive = false;
            break;
        default:
            Log(LOG_DEBUG, "WiFiEvent: id=" + String((int)event));
            break;
    }
}

// ----------------------------------------------------
// Public API
// ----------------------------------------------------
WifiStatus WiFiManagerModule::getStatus() {
    WifiStatus s;

    bool sta = (WiFi.status() == WL_CONNECTED && hasValidIP());

    if (sta) {
        s.mode      = "STA";
        s.connected = true;
    } else if (apActive) {
        s.mode      = "AP";
        s.connected = false;
    } else {
        s.mode      = "none";
        s.connected = false;
    }

    s.ssid = WiFi.SSID();
    s.rssi = WiFi.RSSI();
    s.ip   = WiFi.localIP().toString();
    s.mac  = WiFi.macAddress();

    return s;
}

void WiFiManagerModule::begin() {
    Log(LOG_INFO, "WiFiManager: init");

    WiFi.setHostname(config.hostname.c_str());
    applyWifiStability();

    WiFi.onEvent(onWifiEvent);

    // Wenn keine WLAN-Daten vorhanden → nur AP starten (mit apActive)
    if (config.wifiSSID.length() == 0 || config.wifiPass.length() < 8) {
        Log(LOG_INFO, "WiFiManager: no credentials → AP only");
        startAP();
        return;
    }

    // Sonst STA starten
    startSTAFromPrefsOrConfig();
}

void WiFiManagerModule::loop() {
    wl_status_t st  = WiFi.status();
    bool staOK      = (st == WL_CONNECTED && hasValidIP());

    // Statuswechsel loggen
    if (st != lastWifiStatus) {
        Log(LOG_INFO, "WiFiManager: status change → " +
                      String((int)lastWifiStatus) + " → " +
                      String((int)st));
        lastWifiStatus = st;
    }

    // ----------------------------------------------------
    // STA connected
    // ----------------------------------------------------
    if (staConnecting && staOK) {

        Log(LOG_INFO, "WiFiManager: STA connected, IP=" +
                      WiFi.localIP().toString());

        staConnecting = false;
        staLostTime   = 0;

        display.updateWifi(true,
                           WiFi.localIP().toString(),
                           WiFi.RSSI(),
                           apActive);

        triggerNtpSync();
        applyTimezoneFromConfig();

        // ----------------------------------------------------
        // FIX: AP NICHT sofort stoppen!
        // Wenn AP durch Button aktiviert wurde → Timer respektieren
        // Wenn AP nicht aktiv war → 90s aktiv lassen
        // ----------------------------------------------------
        if (apHoldUntil == 0) {
            // AP wurde NICHT durch Button aktiviert → 90s aktiv lassen
            apHoldUntil = millis() + 90000UL;
            Log(LOG_INFO, "WiFiManager: AP will stay active for 90s");
        } else {
            // AP wurde durch Button aktiviert → Timer respektieren
            Log(LOG_INFO, "WiFiManager: AP temporary active until " +
                          String(apHoldUntil));
        }
    }

    // ----------------------------------------------------
    // STA connect timeout
    // ----------------------------------------------------
    if (staConnecting && !staOK) {
        unsigned long elapsed = millis() - staStartTime;
        if (elapsed > 20000UL) { // 20s
            Log(LOG_WARN,
                "WiFiManager: STA connect timeout after 20s, status=" +
                String((int)st) + ", RSSI=" + String(WiFi.RSSI()));
            staConnecting = false;

            if (!apActive) {
                Log(LOG_WARN, "WiFiManager: STA timeout → AP ON");
                startAP();
            }
        }
    }

    // ----------------------------------------------------
    // STA lost → AP + Retry
    // ----------------------------------------------------
    if (!staConnecting && !staOK) {

        display.updateWifi(false, "---", 0, apActive);

        if (staLostTime == 0)
            staLostTime = millis();

        if (!apActive && millis() - staLostTime > 10000UL) {
            Log(LOG_WARN, "WiFiManager: STA lost → AP ON");
            startAP();
        }

        if (millis() - lastRetry > 30000UL) {
            lastRetry = millis();
            Log(LOG_INFO, "WiFiManager: retry STA");
            startSTAFromPrefsOrConfig();
        }
    }

    // ----------------------------------------------------
    // NTP nur bei STA OK
    // ----------------------------------------------------
    if (staOK) {
        handleNtpLogic();
    }

    // ----------------------------------------------------
    // AP auto-stop nach Timer (Button oder 90s)
    // ----------------------------------------------------
    if (apActive && apHoldUntil > 0 && millis() > apHoldUntil && staOK) {
        Log(LOG_INFO, "WiFiManager: AP auto-stop after timeout");
        apHoldUntil = 0;
        stopAP();
    }
}


void WiFiManagerModule::connect(const String& ssid, const String& pass) {
    if (ssid.length() == 0 || pass.length() < 8) {
        Log(LOG_WARN, "WiFiManager: invalid credentials → abort");
        return;
    }

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    Log(LOG_INFO, "WiFiManager: new credentials saved: " + ssid);

    startSTA(ssid, pass);
}

void WiFiManagerModule::resetWiFi() {
    Log(LOG_WARN, "WiFiManager: WiFi reset → clearing credentials");

    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();

    WiFi.disconnect(true);
    delay(200);

    WiFi.mode(WIFI_AP);
    delay(200);

    Log(LOG_INFO, "WiFiManager: starting AP after reset");
    WiFi.softAP(config.apSSID.c_str(), config.apPass.c_str());
    apActive = true;

    Log(LOG_WARN, "WiFiManager: restarting after WiFi reset");
    delay(500);
    ESP.restart();
}

void WiFiManagerModule::startTemporaryAP(unsigned long durationMs) {
    Log(LOG_INFO, "WiFiManager: temporary AP for " +
                  String(durationMs) + " ms");

    startAP();
    apHoldUntil = millis() + durationMs;
}

String WiFiManagerModule::getStatusJson() {
    DynamicJsonDocument doc(256);

    bool sta = (WiFi.status() == WL_CONNECTED && hasValidIP());

    if (sta) {
        doc["mode"] = "STA";
    } else if (apActive) {
        doc["mode"] = "AP";
    } else {
        doc["mode"] = "none";
    }

    doc["connected"] = sta;
    doc["ssid"]      = WiFi.SSID();
    doc["rssi"]      = WiFi.RSSI();
    doc["ip"]        = WiFi.localIP().toString();
    doc["mac"]       = WiFi.macAddress();

    String out;
    serializeJson(doc, out);
    return out;
}

String WiFiManagerModule::scanJson() {
    Log(LOG_INFO, "WiFiManager: scanJson() called");

    // Modus für Scan setzen, ohne den AP dauerhaft zu verlieren
    if (apActive) {
        Log(LOG_INFO, "WiFiManager: scan in AP+STA mode");
        WiFi.mode(WIFI_AP_STA);
    } else {
        Log(LOG_INFO, "WiFiManager: scan in STA-only mode");
        WiFi.mode(WIFI_STA);
    }

    int n = WiFi.scanNetworks();
    Log(LOG_INFO, String("WiFiManager: scanNetworks found ") + n + " networks");

    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.createNestedArray("nets");

    for (int i = 0; i < n; i++) {
        JsonObject o = arr.createNestedObject();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["enc"]  = WiFi.encryptionType(i);
        o["chan"] = WiFi.channel(i);
    }

    String out;
    serializeJson(doc, out);
    Log(LOG_INFO, String("WiFiManager: scanJson payload length = ") + out.length());
    return out;
}

void WiFiManagerModule::setManualTime(int year, int month, int day,
                                      int hour, int minute, bool dst) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour + (dst ? 1 : 0);
    t.tm_min  = minute;
    t.tm_sec  = 0;

    time_t ts = mktime(&t);
    struct timeval now = { .tv_sec = ts, .tv_usec = 0 };
    settimeofday(&now, nullptr);

    ntpInitialSynced = true;
    applyTimezoneFromConfig();

    String cur = config.getCurrentTimeString();
    config.currentTime = cur;

    Log(LOG_WARN, "WiFiManager: manual time set → " + cur +
                  " (DST=" + String(dst ? "on" : "off") + ")");
}

void WiFiManagerModule::applyNetworkConfig() {
    Log(LOG_INFO, "WiFiManager: applying new network config");

    if (!config.useStaticIP) {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    } else {
        IPAddress ip, gw, mask, dns;
        ip.fromString(config.ipAddr);
        gw.fromString(config.gateway);
        mask.fromString(config.subnetMask);
        dns.fromString(config.dns);
        WiFi.config(ip, gw, mask, dns);
    }

    startSTAFromPrefsOrConfig();
}
