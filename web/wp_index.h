#pragma once
#include "../wp_webserver.h"
#include "wp_ui.h"
#include "../config.h"

/* ---------------------------------------------------------
   Dashboard (Index Page)
   --------------------------------------------------------- */
inline void handleIndex() {

    String html;

    html += F(
"<!DOCTYPE html><html lang='de'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Pylontech Dashboard</title>"
"<style>"
"body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }"
".content { margin-left:260px; padding:20px; }"
"h2 { margin-top:0; }"
".box { background:white; padding:15px; border-radius:8px; "
"       box-shadow:0 0 5px rgba(0,0,0,0.1); margin-bottom:20px; }"
".row { display:grid; grid-template-columns:1fr 1fr; gap:20px; }"
".wifi-bars { display:inline-flex; align-items:flex-end; gap:3px; height:20px; margin-left:10px; }"
".bar { width:6px; background:#ccc; border-radius:2px; }"
".b1{height:20%;}.b2{height:40%;}.b3{height:60%;}.b4{height:80%;}.b5{height:100%;}"
".active { background:#2ecc71 !important; }"
"</style></head><body>"
    );

    // Sidebar
    html += webSidebar("index");

    html += F("<div class='content'>");
    html += F("<h2>System Dashboard</h2>");

    // ---------------------------------------------------------
    // WLAN + MQTT (Row 1)
    // ---------------------------------------------------------
    html += F("<div class='row'>");

    // WLAN Box
    html += F(
"<div class='box'>"
"  <h3>WiFi</h3>"
"  <div id='wifi_info'>loading...</div>"
"  <div class='wifi-bars' id='wifiBars'>"
"    <div class='bar b1'></div>"
"    <div class='bar b2'></div>"
"    <div class='bar b3'></div>"
"    <div class='bar b4'></div>"
"    <div class='bar b5'></div>"
"  </div>"
"</div>"
    );

    // MQTT Box
    html += F(
"<div class='box'>"
"  <h3>MQTT</h3>"
"  <div id='mqtt_info'>loading...</div>"
"</div>"
    );

    html += F("</div>"); // end row 1

    // ---------------------------------------------------------
    // Battery + System (Row 2)
    // ---------------------------------------------------------
    html += F("<div class='row'>");

    // Battery Box
    html += F(
"<div class='box'>"
"  <h3>Batterie</h3>"
"  <div id='bat_info'>loading...</div>"
"</div>"
    );

    // System Box
    html += F(
"<div class='box'>"
"  <h3>System</h3>"
"  <div id='sys_info'>loading...</div>"
"</div>"
    );

    html += F("</div>"); // end row 2

    // ---------------------------------------------------------
    // JavaScript
    // ---------------------------------------------------------
    html += F("<script>");

    // ---------------- WIFI ----------------
    html += F(R"rawliteral(
function rssiToBars(rssi){
  if(rssi >= -50) return 5;
  if(rssi >= -60) return 4;
  if(rssi >= -70) return 3;
  if(rssi >= -80) return 2;
  if(rssi >= -90) return 1;
  return 0;
}

function updateBars(rssi){
  let bars = rssiToBars(rssi);
  let elements = document.querySelectorAll('#wifiBars .bar');
  elements.forEach((el, index) => {
    if(index < bars) el.classList.add('active');
    else el.classList.remove('active');
  });
}

async function loadWiFi(){
  let r = await fetch('/api/wifi/status');
  let j = await r.json();

  document.getElementById('wifi_info').innerHTML =
    "Mode: " + j.mode + "<br>" +
    "SSID: " + j.ssid + "<br>" +
    "IP: " + j.ip + "<br>" +
    "RSSI: " + j.rssi + " dBm";

  updateBars(j.rssi);
}
loadWiFi();
)rawliteral");

    // ---------------- MQTT ----------------
    html += F(R"rawliteral(
async function loadMQTT(){
  let r = await fetch('/api/mqtt/status');
  let j = await r.json();

  document.getElementById('mqtt_info').innerHTML =
    "Status: " + (j.connected ? "Connected" : "Disconnected") + "<br>" +
    "Server: " + j.server + ":" + j.port + "<br>" +
    "Last contact: " + j.last_contact;
}
loadMQTT();
)rawliteral");

    // ---------------- BATTERY ----------------
    html += F(R"rawliteral(
async function loadBattery(){
  let r = await fetch('/api/battery/info');
  let j = await r.json();

  document.getElementById('bat_info').innerHTML =
    "Detected modules: " + j.modules + "<br>" +
    "Last update: " + j.last_update;
}
loadBattery();
)rawliteral");

    // ---------------- SYSTEM ----------------
    html += F(R"rawliteral(
async function loadSystem(){
  let r = await fetch('/api/system/info');
  let j = await r.json();

  document.getElementById('sys_info').innerHTML =
    "Time: " + j.time + "<br>" +
    "Uptime: " + j.uptime + "<br>" +
    "Firmware: " + j.version;
}
loadSystem();
)rawliteral");

    html += F("</script></body></html>");

    server.send(200, "text/html", html);
}