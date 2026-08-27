// -------------------------------------------------------------
// Global Error Handler
// -------------------------------------------------------------
window.onerror = function(msg, url, line, col, error) {
    console.log("JS ERROR:", msg, "Line:", line, "Col:", col);
};

// -------------------------------------------------------------
// Hilfsfunktion: fetch als Promise
// -------------------------------------------------------------
function fetchJson(url) {
    return fetch(url).then(r => {
        if (!r.ok) throw new Error("HTTP " + r.status);
        return r.json();
    });
}

function fetchText(url, options = {}) {
    return fetch(url, options).then(r => r.text());
}

// -------------------------------------------------------------
// Init – SEQUENTIELL
// -------------------------------------------------------------
async function initConnectionPage() {

    try {
        await wifiLoadStatus();
        await mqttLoad();
        await tzLoad();
        await timeLoad();
        await ipLoad();
    }
    catch (e) {
        console.error("Connection init failed:", e);
    }

    // Event Listener erst NACH dem Laden setzen
    let region = document.getElementById("tz_region");
    let city   = document.getElementById("tz_city");

    if (region) region.addEventListener("change", tzRegionChanged);
    if (city)   city.addEventListener("change", tzCityChanged);

    // Sprache erst anwenden, wenn ALLES geladen ist
    if (typeof applyLanguage === "function") {
        applyLanguage();
    }
}

// -------------------------------------------------------------
// WiFi
// -------------------------------------------------------------
function wifiLoadStatus() {
    return fetchJson("/api/wifi")
        .then(j => {
            document.getElementById("wifi_status").textContent =
                `Connected: ${j.connected}, SSID: ${j.ssid}, RSSI: ${j.rssi}, IP: ${j.ip}`;
        });
}

function wifiScan() {
    return fetchJson("/api/wifi_scan")   // statt "/api/wifi/scan"
        .then(data => {

            let nets = data.nets || [];

            // Duplikate → stärkstes Signal behalten
            let best = {};
            nets.forEach(n => {
                if (!best[n.ssid] || n.rssi > best[n.ssid].rssi) {
                    best[n.ssid] = n;
                }
            });

            let list = Object.values(best).sort((a, b) => b.rssi - a.rssi);

            let html = "";
            list.forEach(n => {
                html += `
                    <div class="wifi-entry">

                        <div class="wifi-row">
                            <span class="wifi-ssid">${n.ssid}</span>
                            <span class="wifi-signal">${n.rssi} dBm</span>
                        </div>

                        <div class="wifi-row">
                            <input type="password" id="pass_${n.ssid}" placeholder="Passwort eingeben">
                            <button onclick="wifiConnectScan('${n.ssid}')">Connect</button>
                        </div>

                    </div>
                `;
            });

            document.getElementById("wifi_scanlist").innerHTML = html;
        });
}

function wifiSelectAndConnect(ssid) {
    document.getElementById("wifi_ssid").value = ssid;
    document.getElementById("wifi_pass").value = "";
    document.getElementById("wifi_manual").checked = true;
    wifiToggleManual();
}

function wifiConnectScan(ssid) {
    let pass = document.getElementById("pass_" + ssid).value;

    let data = { ssid: ssid, pass: pass };

    return fetchText("/api/wifi", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(data)
    })
    .then(t => {
        document.getElementById("wifi_msg").textContent = t;
        return wifiLoadStatus();
    });
}

function wifiToggleManual() {
    const box = document.getElementById("wifi_manual_box");
    box.style.display = document.getElementById("wifi_manual").checked ? "block" : "none";
}

// -------------------------------------------------------------
// MQTT
// -------------------------------------------------------------
function mqttLoad() {
    return fetchJson("/api/mqtt")
        .then(j => {
            document.getElementById("mqtt_enabled").checked = j.enabled;
            document.getElementById("mqtt_server").value = j.server;
            document.getElementById("mqtt_port").value = j.port;
            document.getElementById("mqtt_user").value = j.user;
            document.getElementById("mqtt_topic").value = j.topic;
        });
}

function mqttSave() {
    let data = {
        enabled: document.getElementById("mqtt_enabled").checked,
        server: document.getElementById("mqtt_server").value,
        port: Number(document.getElementById("mqtt_port").value),
        user: document.getElementById("mqtt_user").value,
        pass: document.getElementById("mqtt_pass").value,
        topic: document.getElementById("mqtt_topic").value
    };

    return fetchText("/api/mqtt", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(data)
    })
    .then(t => document.getElementById("mqtt_msg").textContent = t);
}

// -------------------------------------------------------------
// NTP / Time
// -------------------------------------------------------------
if (typeof TZDATA === "undefined") var TZDATA = {};
if (typeof CURRENT_NTP_SERVER === "undefined") var CURRENT_NTP_SERVER = "";

function ntpModeSelect(mode) {

    const g = document.getElementById("ntp_gateway");
    const m = document.getElementById("ntp_manual");
    const t = document.getElementById("time_manual");

    if (mode === "gateway") { g.checked = true;  m.checked = false; t.checked = false; }
    if (mode === "manual")  { g.checked = false; m.checked = true;  t.checked = false; }
    if (mode === "time")    { g.checked = false; m.checked = false; t.checked = true; }

    document.getElementById("ntp_manual_box").style.display = m.checked ? "block" : "none";
    document.getElementById("time_manual_box").style.display = t.checked ? "block" : "none";

    updateCurrentNtpServer();
}

function updateCurrentNtpServer() {
    const t = document.getElementById("time_manual").checked;

    if (t) {
        document.getElementById("ntp_current").textContent = "Manual time (kein NTP)";
    } else {
        document.getElementById("ntp_current").textContent = CURRENT_NTP_SERVER;
    }
}

// -------------------------------------------------------------
// Timezones
// -------------------------------------------------------------
function tzLoad() {
    return fetchJson("/timezone.json")
        .then(j => {
            TZDATA = j;

            let regionSel = document.getElementById("tz_region");
            regionSel.innerHTML = "";

            Object.keys(j).forEach(region => {
                let o = document.createElement("option");
                o.value = region;
                o.textContent = region;
                regionSel.appendChild(o);
            });

            tzRegionChanged();
        });
}

function tzRegionChanged() {
    let region = document.getElementById("tz_region").value;
    let citySel = document.getElementById("tz_city");
    citySel.innerHTML = "";

    TZDATA[region].forEach(e => {
        let o = document.createElement("option");
        o.value = e.tz;
        o.textContent = e.city;
        citySel.appendChild(o);
    });

    tzCityChanged();
}

function tzCityChanged() {
    let tz = document.getElementById("tz_city").value;
    document.getElementById("tz_full").value = tz;
}

// -------------------------------------------------------------
// Time Load
// -------------------------------------------------------------
function timeLoad() {
    return fetchJson("/api/time")
        .then(j => {

            document.getElementById("ntp_gateway").checked = j.use_gateway_ntp;
            document.getElementById("ntp_manual").checked  = j.manual_ntp;
            document.getElementById("time_manual").checked = j.manual_mode;

            document.getElementById("ntp_server").value = "pool.ntp.org";

            document.getElementById("time_date").value = j.manual_date;
            document.getElementById("time_time").value = j.manual_time;
            document.getElementById("time_dst").checked = j.manual_dst;

            let [region, city] = j.timezone.split("/");
            document.getElementById("tz_region").value = region;
            tzRegionChanged();
            document.getElementById("tz_city").value = j.timezone;

            CURRENT_NTP_SERVER = j.server;
            document.getElementById("ntp_current").textContent = CURRENT_NTP_SERVER;

            ntpModeSelect(
                j.use_gateway_ntp ? "gateway" :
                j.manual_ntp      ? "manual"  :
                                    "time"
            );
        });
}

// -------------------------------------------------------------
// Time Save
// -------------------------------------------------------------
function timeSave() {
    let data = {
        manual_mode: document.getElementById("time_manual").checked,
        manual_date: document.getElementById("time_date").value,
        manual_time: document.getElementById("time_time").value,
        manual_dst: document.getElementById("time_dst").checked,
        use_gateway_ntp: document.getElementById("ntp_gateway").checked,
        manual_ntp: document.getElementById("ntp_manual").checked,
        server: document.getElementById("ntp_server").value,
        timezone: document.getElementById("tz_full").value
    };

    return fetchText("/api/time", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(data)
    })
    .then(t => document.getElementById("time_msg").textContent = t);
}

// -------------------------------------------------------------
// IP Config
// -------------------------------------------------------------
function ipLoad() {
    return fetchJson("/api/network")
        .then(j => {

            document.getElementById("ip_dhcp").checked = j.dhcp;

            document.getElementById("ip_addr").value = j.ip;
            document.getElementById("ip_mask").value = j.mask;
            document.getElementById("ip_gw").value = j.gw;
            document.getElementById("ip_dns").value = j.dns;

            ipToggleDhcp();
        });
}

function ipToggleDhcp() {
    const dhcp = document.getElementById("ip_dhcp").checked;
    document.getElementById("ip_static_box").style.display = dhcp ? "none" : "block";
}

function ipSave() {
    let data = {
        dhcp: document.getElementById("ip_dhcp").checked,
        ip:   document.getElementById("ip_addr").value,
        mask: document.getElementById("ip_mask").value,
        gw:   document.getElementById("ip_gw").value,
        dns:  document.getElementById("ip_dns").value
    };

    return fetchText("/api/network", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(data)
    })
    .then(t => document.getElementById("ip_msg").textContent = t);
}

// -------------------------------------------------------------
// Seite initialisieren
// -------------------------------------------------------------
initConnectionPage();
