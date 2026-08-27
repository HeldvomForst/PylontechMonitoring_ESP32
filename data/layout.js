// -------------------------------------------------------------
// Sprache laden
// -------------------------------------------------------------
let lang = localStorage.getItem("lang") || "de";
let T = {};

function updateServiceLinks() {
    let enabled = localStorage.getItem("service_enabled") === "1";
    let container = document.getElementById("service-links");

    container.innerHTML = "";

    if (enabled) {
		container.innerHTML = `
		<a href="/framedump">Frame Dump</a>
		<a href="/filemanager">Filemanager</a>
		<a href="/service">Service</a>
		`;
    }
}


// Sprache setzen
function setLanguage(l) {
    localStorage.setItem("lang", l);
    location.reload();
}

// Sprachdatei laden (WICHTIG: applyLanguage NICHT hier ausführen!)
function loadLanguage(l) {
    return fetch("/" + l + ".json")
        .then(r => r.json())
        .then(json => {
            T = json;
        });
}

// Texte ersetzen
function applyLanguage() {
    const map = {
        "title_app": "title_app",
        "nav_dashboard": "dashboard",
        "nav_basevalue": "basevalue",
        "nav_celldata": "celldata",
        "nav_statistic": "statistic",
        "nav_console": "console",
        "nav_connection": "connection",
        "nav_log": "log",
        "nav_health": "health"
    };
	
    const baseMap = {
        "base_title": "base_title",
        "base_temp_header": "base_temp_header",
        "base_temp_label": "base_temp_label",
        "base_mqtt_header": "base_mqtt_header",
        "base_topic_pwr": "base_topic_pwr",
        "base_topic_stack": "base_topic_stack",
        "base_interval_header": "base_interval_header",
        "base_interval_pwr": "base_interval_pwr",
        "base_table_header": "base_table_header",
        "base_col_field": "base_col_field",
        "base_col_display": "base_col_display",
        "base_col_raw": "base_col_raw",
        "base_col_factor": "base_col_factor",
        "base_col_unit": "base_col_unit",
        "base_col_mqtt": "base_col_mqtt",
        "base_col_send": "base_col_send",
        "base_btn_save": "base_btn_save"
    };


	const dashMap = {
		"dash_title": "dash_title",
		"dash_wifi": "dash_wifi",
		"dash_mode": "dash_mode",
		"dash_ssid": "dash_ssid",
		"dash_ip": "dash_ip",
		"dash_rssi": "dash_rssi",
		"dash_mqtt": "dash_mqtt",
		"dash_status": "dash_status",
		"dash_server": "dash_server",
		"dash_last_pwr": "dash_last_pwr",
		"dash_last_bat": "dash_last_bat",
		"dash_last_stat": "dash_last_stat",
		"dash_battery": "dash_battery",
		"dash_detected": "dash_detected",
		"dash_update": "dash_update",
		"dash_system": "dash_system",
		"dash_time": "dash_time",
		"dash_uptime": "dash_uptime",
		"dash_firmware": "dash_firmware"
	};

	
	const cellMap = {
		"cell_title": "cell_title",
		"cell_parser_header": "cell_parser_header",
		"cell_enable_label": "cell_enable_label",
		"cell_mqtt_header": "cell_mqtt_header",
		"cell_topic_label": "cell_topic_label",
		"cell_prefix_label": "cell_prefix_label",
		"cell_interval_header": "cell_interval_header",
		"cell_interval_label": "cell_interval_label",
		"cell_table_header": "cell_table_header",
		"cell_col_field": "cell_col_field",
		"cell_col_display": "cell_col_display",
		"cell_col_raw": "cell_col_raw",
		"cell_col_factor": "cell_col_factor",
		"cell_col_unit": "cell_col_unit",
		"cell_col_mqtt": "cell_col_mqtt",
		"cell_col_send": "cell_col_send",
		"cell_btn_save": "cell_btn_save"
	};

	const statMap = {
		"stat_title": "stat_title",
		"stat_parser_header": "stat_parser_header",
		"stat_enable_label": "stat_enable_label",
		"stat_mqtt_header": "stat_mqtt_header",
		"stat_topic_label": "stat_topic_label",
		"stat_interval_header": "stat_interval_header",
		"stat_interval_label": "stat_interval_label",
		"stat_btn_save": "stat_btn_save"
	};

	const consoleMap = {
		"console_title": "console_title",
		"console_cmd_header": "console_cmd_header",
		"console_btn_send": "console_btn_send",
		"console_quick_pwr": "console_quick_pwr",
		"console_quick_bat1": "console_quick_bat1",
		"console_quick_bat2": "console_quick_bat2",
		"console_quick_bat3": "console_quick_bat3",
		"console_quick_bat4": "console_quick_bat4",
		"console_quick_help": "console_quick_help"
	};

	const connectionMap = {
		"conn_title": "conn_title",

		"conn_wifi_header": "conn_wifi_header",
		"conn_wifi_scan": "conn_wifi_scan",
		"conn_wifi_manual": "conn_wifi_manual",
		"conn_wifi_ssid": "conn_wifi_ssid",
		"conn_wifi_pass": "conn_wifi_pass",
		"conn_wifi_connect": "conn_wifi_connect",

		"conn_mqtt_header": "conn_mqtt_header",
		"conn_mqtt_enabled": "conn_mqtt_enabled",
		"conn_mqtt_server": "conn_mqtt_server",
		"conn_mqtt_port": "conn_mqtt_port",
		"conn_mqtt_user": "conn_mqtt_user",
		"conn_mqtt_pass": "conn_mqtt_pass",
		"conn_mqtt_topic": "conn_mqtt_topic",
		"conn_mqtt_save": "conn_mqtt_save",

		"conn_ntp_header": "conn_ntp_header",
		"conn_ntp_gateway": "conn_ntp_gateway",
		"conn_ntp_manual": "conn_ntp_manual",
		"conn_ntp_time": "conn_ntp_time",
		"conn_ntp_server": "conn_ntp_server",

		"conn_time_date": "conn_time_date",
		"conn_time_time": "conn_time_time",
		"conn_time_dst": "conn_time_dst",
		"conn_time_save": "conn_time_save",

		"conn_tz_header": "conn_tz_header",
		"conn_tz_region": "conn_tz_region",
		"conn_tz_city": "conn_tz_city",

		"conn_ntp_server_header": "conn_ntp_server_header",

		"conn_ip_header": "conn_ip_header",
		"conn_ip_dhcp": "conn_ip_dhcp",
		"conn_ip_addr": "conn_ip_addr",
		"conn_ip_mask": "conn_ip_mask",
		"conn_ip_gw": "conn_ip_gw",
		"conn_ip_dns": "conn_ip_dns",
		"conn_ip_save": "conn_ip_save"
	};

	const logMap = {
		"log_title": "log_title",
		"log_reload": "log_reload",
		"log_filter": "log_filter",
		"log_info": "log_info",
		"log_warn": "log_warn",
		"log_error": "log_error",
		"log_debug": "log_debug",
		"log_save": "log_save",
		"log_service_header": "log_service_header",
		"log_service_enable": "log_service_enable"
	};

	const healthMap = {
		"health_title": "health_title",
		"health_module_header": "health_module_header",
		"health_col_module": "health_col_module",
		"health_col_status": "health_col_status",
		"health_col_tempmax": "health_col_tempmax",
		"health_col_cellmin": "health_col_cellmin",
		"health_col_cellmax": "health_col_cellmax",
		"health_col_delta": "health_col_delta",

		"health_stack_header": "health_stack_header",

		"health_list_header": "health_list_header",
		"health_list_ok_label": "health_list_ok_label",
		"health_list_warn_label": "health_list_warn_label",
		"health_list_err_label": "health_list_err_label",

		"health_hist_header": "health_hist_header",
		"health_hist_warn_label": "health_hist_warn_label",
		"health_hist_err_label": "health_hist_err_label",

		"health_reset": "health_reset",

		"health_threshold_header": "health_threshold_header",
		"health_warn_label": "health_warn_label",
		"health_error_label": "health_error_label",
		"health_save": "health_save"
	};
	
    // Menü
    for (let id in map) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[map[id]];
    }

    // Dashboard
    for (let id in dashMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[dashMap[id]];
    }

    // Basevalue
    for (let id in baseMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[baseMap[id]];
    }

    // Celldata
    for (let id in cellMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[cellMap[id]];
    }

    // Statistic
    for (let id in statMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[statMap[id]];
    }

    // Console
    for (let id in consoleMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[consoleMap[id]];
    }

    // Connection
    for (let id in connectionMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[connectionMap[id]];
    }

    // Log
    for (let id in logMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[logMap[id]];
    }

    // Health
    for (let id in healthMap) {
        let el = document.getElementById(id);
        if (el) el.innerText = T[healthMap[id]];
    }

    // ⭐ WICHTIG: Service‑Links NACH dem Übersetzen wiederherstellen
    if (typeof updateServiceLinks === "function") {
        updateServiceLinks();
    }
}

// -------------------------------------------------------------
// Sidebar
// -------------------------------------------------------------
function toggleSidebar() {
    document.getElementById('sidebar').classList.toggle('open');
}

function loadPage(url) {

    // Alte Skripte entfernen
    document.querySelectorAll(".dynamic-script").forEach(s => s.remove());

    fetch(url)
    .then(r => r.text())
    .then(html => {

        const content = document.getElementById("content");
        content.innerHTML = html;

        const scripts = content.querySelectorAll("script");

        function loadNextScript(i) {
            if (i >= scripts.length) {
                if (typeof applyLanguage === "function") {
                    applyLanguage();
                }
                return;
            }

            const oldScript = scripts[i];
            const newScript = document.createElement("script");
            newScript.classList.add("dynamic-script");

            if (oldScript.src) {
                newScript.src = oldScript.src;
                newScript.onload = () => loadNextScript(i + 1);
            } else {
                newScript.textContent = oldScript.textContent;
                loadNextScript(i + 1);
            }

            document.body.appendChild(newScript);
        }

        loadNextScript(0);
    })
    .catch(err => {
        document.getElementById("content").innerHTML =
            "Error loading page: " + err.message;
    });
}

// -------------------------------------------------------------
// Startseite laden, aber erst NACH Sprachladen
// -------------------------------------------------------------
document.addEventListener("DOMContentLoaded", () => {
    document.getElementById("langsel").value = lang;
    loadLanguage(lang).then(() => {
        updateServiceLinks();

        // Seite aus URL bestimmen
        let path = window.location.pathname;   // z.B. "/", "/dashboard", "/basevalue"
        let pageName = "dashboard";

        if (path !== "/" && path.length > 1) {
            pageName = path.substring(1);     // "/dashboard" -> "dashboard"
        }

        // Fallback: zuletzt besuchte Seite, wenn wir auf "/" sind
        if (path === "/") {
            let last = localStorage.getItem("last_page");
            if (last && last.startsWith("pages_")) {
                pageName = last.replace("pages_", "");
            }
        }

        // Seite laden
        loadPage("pages_" + pageName + ".html");;
    });
});
