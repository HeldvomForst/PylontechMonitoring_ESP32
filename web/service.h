#pragma once
#include <Arduino.h>

const char SERVICE_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>ESP32 OTA Update</title>

<style>
body { font-family: sans-serif; padding: 20px; }
button { padding: 8px 16px; margin-top: 10px; }
#status { margin-top: 15px; font-weight: bold; }
progress { width: 300px; display:none; }
</style>
</head>

<body>

<h2>ESP32 OTA Update</h2>

<p>Current firmware version: <b id="fwver">...</b></p>

<p>Please select a <b>.bin</b> firmware file:</p>
<input type="file" id="fw"><br><br>

<button onclick="upload()">Flash</button>
<br><br>

<progress id="prog" value="0" max="100"></progress>
<div id="status"></div>

<h3>System Actions</h3>

<button onclick="restartESP()">Restart ESP</button><br><br>
<button onclick="wifiReset()">WiFi Reset</button><br><br>
<button onclick="factoryReset()">Factory Reset</button><br><br>
<button onclick="formatSPIFFS()">Format SPIFFS</button><br><br>

<h3>Storage Information</h3>

<table>
<tr>
<td>SPIFFS Total:</td>
<td><span id="spiffs_total">...</span></td>
</tr>

<tr>
<td>SPIFFS Used:</td>
<td><span id="spiffs_used">...</span></td>
</tr>

<tr>
<td>SPIFFS Free:</td>
<td>
<span id="spiffs_free">...</span><br>
<small id="spiffs_free_pct"></small>
</td>
</tr>

<tr>
<td>NVS Total:</td>
<td><span id="nvs_total">...</span></td>
</tr>

<tr>
<td>NVS Used:</td>
<td><span id="nvs_used">...</span></td>
</tr>

<tr>
<td>NVS Free:</td>
<td>
<span id="nvs_free">...</span><br>
<small id="nvs_free_pct"></small>
</td>
</tr>
</table>

<h3>Logging Options</h3>

<label>
<input type="checkbox" id="taskmgr" onchange="toggleTaskManager()">
Enable Task Manager Log
</label>

<h3>Display Helligkeit</h3>
<input type="range" id="dispBright" min="0" max="255" step="1" oninput="updateBright(this.value)">
<span id="brightVal"></span>

<script>

// ------------------------------------------------------------
// Load firmware version
// ------------------------------------------------------------
fetch("/api/version")
.then(r => r.text())
.then(v => { document.getElementById("fwver").innerText = v; });

// ------------------------------------------------------------
// OTA Upload
// ------------------------------------------------------------
function upload() {
    let file = document.getElementById("fw").files[0];
    if (!file) {
        alert("Please select a .bin file");
        return;
    }

    let prog = document.getElementById("prog");
    let status = document.getElementById("status");

    prog.style.display = "block";
    prog.value = 0;
    status.innerText = "Uploading...";

    let xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/ota");
    xhr.setRequestHeader("Content-Type", "application/octet-stream");

    xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
            let p = Math.round((e.loaded / e.total) * 100);
            prog.value = p;
            status.innerText = "Uploading: " + p + "%";
        }
    };

    xhr.onload = function() {
        if (xhr.status == 200) {
            status.innerText = "Update successful! ESP is restarting...";
            prog.value = 100;
            setTimeout(() => location.reload(), 5000);
        } else {
            status.innerText = "Error: " + xhr.responseText;
        }
    };

    xhr.onerror = function() {
        status.innerText = "Network error during upload";
    };

    xhr.send(file);
}

// ------------------------------------------------------------
// System actions
// ------------------------------------------------------------
function restartESP() {
    if (!confirm("The ESP will restart.")) return;
    fetch("/api/restart", { method: "POST" });
}

function wifiReset() {
    if (!confirm("WiFi settings will be deleted. Continue?")) return;
    fetch("/api/wifireset", { method: "POST" });
}

function factoryReset() {
    if (!confirm("ALL user data will be deleted! Perform factory reset?")) return;
    fetch("/api/factoryreset", { method: "POST" });
}

function formatSPIFFS() {
    if (!confirm("WARNING: This will ERASE the entire website stored on the ESP32.\n\nAfter formatting, you MUST upload all website files again using the File Manager.\n\nDo you want to continue?")) {
        return;
    }

    fetch("/api/formatspiffs", { method: "POST" })
    .then(r => r.text())
    .then(t => alert("SPIFFS formatted.\n\nNow upload the website again via the File Manager."));
}

// ------------------------------------------------------------
// Storage info
// ------------------------------------------------------------
fetch("/api/storageinfo")
.then(r => r.json())
.then(info => {
    document.getElementById("spiffs_total").innerText = info.spiffs_total + " Bytes";
    document.getElementById("spiffs_used").innerText  = info.spiffs_used  + " Bytes";
    document.getElementById("spiffs_free").innerText  = info.spiffs_free  + " Bytes";

    let spiffs_free_pct = Math.round((info.spiffs_free / info.spiffs_total) * 100);
    document.getElementById("spiffs_free_pct").innerText = spiffs_free_pct + "% free";

    document.getElementById("nvs_total").innerText = info.nvs_total + " entries";
    document.getElementById("nvs_used").innerText  = info.nvs_used  + " entries";
    document.getElementById("nvs_free").innerText  = info.nvs_free  + " entries";

    let nvs_free_pct = Math.round((info.nvs_free / info.nvs_total) * 100);
    document.getElementById("nvs_free_pct").innerText = nvs_free_pct + "% free";
});

// ------------------------------------------------------------
// Task Manager Logging
// ------------------------------------------------------------
function loadTaskManagerState() {
    fetch("/api/config")
    .then(r => r.json())
    .then(cfg => {
        document.getElementById("taskmgr").checked = cfg.log_taskmanager;
    });
}

function toggleTaskManager() {
    let en = document.getElementById("taskmgr").checked ? 1 : 0;
    fetch("/api/toggle_taskmanager?enabled=" + en, { method: "POST" });
}

// ------------------------------------------------------------
// Display Brightness
// ------------------------------------------------------------
function loadBrightness() {
    fetch("/api/config")
    .then(r => r.json())
    .then(cfg => {
        document.getElementById("dispBright").value = cfg.display_brightness;
        document.getElementById("brightVal").innerText = cfg.display_brightness;
    });
}

function updateBright(v) {
    document.getElementById("brightVal").innerText = v;
    fetch("/api/set_brightness?value=" + v, { method: "POST" });
}

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
loadBrightness();
loadTaskManagerState();

</script>


</body>
</html>
)rawliteral";
