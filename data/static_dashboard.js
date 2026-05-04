function loadDashboard() {
    fetch("/api/dashboard")
        .then(r => r.json())
        .then(d => {
            wifi_mode.textContent = d.wifi.mode;
            wifi_ssid.textContent = d.wifi.ssid;
            wifi_ip.textContent = d.wifi.ip;
            wifi_rssi.textContent = d.wifi.rssi;
            mqtt_status.textContent = d.mqtt.connected ? "Connected" : "Disconnected";
            mqtt_server.textContent = d.mqtt.server + ":" + d.mqtt.port;
            mqtt_last.textContent = d.mqtt.last_contact;
            bat_modules.textContent = d.battery.modules;
            bat_last.textContent = d.battery.last_update;
            sys_time.textContent = d.system.time;
            sys_uptime.textContent = d.system.uptime;
            sys_fw.textContent = d.system.version;
        });
}
loadDashboard();