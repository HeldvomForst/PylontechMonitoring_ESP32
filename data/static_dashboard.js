// -------------------------------------------------------------
// Dashboard Loader (Promise-basiert, stabil)
// -------------------------------------------------------------
function loadDashboard() {
    return fetch("/api/dashboard")
        .then(r => {
            if (!r.ok) throw new Error("HTTP " + r.status);
            return r.json();
        })
        .then(d => {
            document.getElementById("wifi_mode").textContent = d.wifi.mode;
            document.getElementById("wifi_ssid").textContent = d.wifi.ssid;
            document.getElementById("wifi_ip").textContent = d.wifi.ip;
            document.getElementById("wifi_rssi").textContent = d.wifi.rssi;

            document.getElementById("mqtt_status").textContent =
                d.mqtt.connected ? "Connected" : "Disconnected";

            document.getElementById("mqtt_server").textContent =
                d.mqtt.server + ":" + d.mqtt.port;
			
            document.getElementById("mqtt_last_pwr").textContent  = d.mqtt.last_pwr;
            document.getElementById("mqtt_last_bat").textContent  = d.mqtt.last_bat;
            document.getElementById("mqtt_last_stat").textContent = d.mqtt.last_stat;


            document.getElementById("bat_modules").textContent = d.battery.modules;
            document.getElementById("bat_last").textContent = d.battery.last_update;

            document.getElementById("sys_time").textContent = d.system.time;
            document.getElementById("sys_uptime").textContent = d.system.uptime;
            document.getElementById("sys_fw").textContent = d.system.version;
        })
        .catch(err => {
            console.error("Dashboard load failed:", err);
        });
}

// -------------------------------------------------------------
// Seite initialisieren
// -------------------------------------------------------------
async function initDashboardPage() {
    try {
        await loadDashboard();
    }
    catch (e) {
        console.error("Dashboard init failed:", e);
    }

    if (typeof applyLanguage === "function") {
        applyLanguage();
    }
}

initDashboardPage();
