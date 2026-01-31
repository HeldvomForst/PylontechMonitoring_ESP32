#pragma once
#include "../wp_webserver.h"
#include "wp_ui.h"

/* ---------------------------------------------------------
   Connection Settings Page (WiFi, MQTT, NTP, IP Config)
   --------------------------------------------------------- */
inline void handleConnectPage() {

    String html;

    html += F(
"<!DOCTYPE html><html lang='de'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Connection Settings</title>"
"<style>"
"body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }"
".content { margin-left:260px; padding:20px; }"
"h2 { margin-top:0; }"
".box { background:white; padding:15px; border-radius:8px; box-shadow:0 0 5px rgba(0,0,0,0.1); margin-bottom:20px; }"
"label { font-weight:bold; display:block; margin-top:10px; }"
"input { padding:8px; font-size:16px; margin-top:4px; width:100%; box-sizing:border-box; }"
"button { padding:10px 20px; font-size:16px; margin-top:10px; border:none; border-radius:6px; background:#3498db; color:white; cursor:pointer; }"
"button:hover { background:#2980b9; }"
"#langsel { position:absolute; top:10px; right:20px; font-size:16px; }"
"input[type=checkbox] { transform: scale(1.3); margin-right: 8px; accent-color: #3498db; }"
"</style></head><body>"
    );

    // Sidebar
    html += webSidebar("connect");

    html += F("<div class='content'>");
    html += F("<h2 id='t_title'>Connection Settings</h2>");

    // Language selector
    html += F(
"<select id='langsel' onchange='changeLang()'>"
"  <option value='de'>Deutsch</option>"
"  <option value='en'>English</option>"
"</select>"
    );

    // ---------------------------------------------------------
    // WiFi Networks
    // ---------------------------------------------------------
    html += F(
"<div class='box'>"
"  <h3 id='t_wifinets'>WiFi Networks</h3>"
"  <button id='t_scanbtn' onclick='scan()'>Scan</button>"
"  <div id='scan'></div>"
"</div>"
    );

// ---------------------------------------------------------
// MQTT Settings
// ---------------------------------------------------------
html += F(
"<div class='box'>"
"  <h3 id='t_mqtt'>MQTT Settings</h3>"

"  <label><input type='checkbox' id='mqtt_enabled'> <span id='t_mqtt_enabled'>MQTT aktivieren</span></label>"

"  <label id='t_mqtt_server'>Server:</label>"
"  <input id='mqtt_server'>"

"  <label id='t_mqtt_port'>Port:</label>"
"  <input id='mqtt_port' type='number'>"

"  <label id='t_mqtt_user'>Username:</label>"
"  <input id='mqtt_user'>"

"  <label id='t_mqtt_pass'>Password:</label>"
"  <input id='mqtt_pass' type='password'>"

"  <label id='t_mqtt_topic'>Topic Prefix:</label>"
"  <input id='mqtt_topic'>"

"  <button id='t_saveMqtt' onclick='saveMqtt()'>Save</button>"
"  <pre id='mqttmsg'></pre>"
"</div>"
);

// ---------------------------------------------------------
// Time Settings (NTP + Timezone + DST)
// ---------------------------------------------------------
// ---------------------------------------------------------
// Time Settings (NTP + Timezone + DST)
// ---------------------------------------------------------
html += F(
"<div class='box'>"
"  <h3 id='t_timesettings'>Time Settings</h3>"

"  <label id='t_ntp_label'>NTP Server:</label>"
"  <input id='ntp_server'>"

"  <label id='t_timezone'>Timezone:</label>"
"  <select id='timezone'>"

"    <option value='Etc/GMT+12'>(GMT‑12) Baker Island</option>"
"    <option value='Etc/GMT+11'>(GMT‑11) Samoa</option>"
"    <option value='Pacific/Honolulu'>(GMT‑10) Honolulu</option>"
"    <option value='America/Anchorage'>(GMT‑9) Anchorage</option>"
"    <option value='America/Los_Angeles'>(GMT‑8) Los Angeles</option>"
"    <option value='America/Denver'>(GMT‑7) Denver</option>"
"    <option value='America/Chicago'>(GMT‑6) Chicago</option>"
"    <option value='America/New_York'>(GMT‑5) New York</option>"
"    <option value='America/Halifax'>(GMT‑4) Halifax</option>"
"    <option value='America/Sao_Paulo'>(GMT‑3) São Paulo</option>"
"    <option value='America/Noronha'>(GMT‑2) Fernando de Noronha</option>"
"    <option value='Atlantic/Azores'>(GMT‑1) Azores</option>"

"    <option value='Europe/London'>(GMT±0) London</option>"

"    <option value='Europe/Berlin'>(GMT+1) Berlin</option>"
"    <option value='Europe/Athens'>(GMT+2) Athens</option>"
"    <option value='Europe/Moscow'>(GMT+3) Moscow</option>"
"    <option value='Asia/Dubai'>(GMT+4) Dubai</option>"
"    <option value='Asia/Karachi'>(GMT+5) Karachi</option>"
"    <option value='Asia/Dhaka'>(GMT+6) Dhaka</option>"
"    <option value='Asia/Bangkok'>(GMT+7) Bangkok</option>"
"    <option value='Asia/Singapore'>(GMT+8) Singapore</option>"
"    <option value='Asia/Tokyo'>(GMT+9) Tokyo</option>"
"    <option value='Australia/Sydney'>(GMT+10) Sydney</option>"
"    <option value='Pacific/Noumea'>(GMT+11) New Caledonia</option>"
"    <option value='Pacific/Auckland'>(GMT+12) Auckland</option>"

"  </select>"

"  <label><input type='checkbox' id='dst'> <span id='t_dst'>Daylight Saving Time</span></label>"

"  <button id='t_saveTime' onclick='saveTime()'>Save</button>"
"  <pre id='timemsg'></pre>"
"</div>"
);

    // ---------------------------------------------------------
    // IP Configuration
    // ---------------------------------------------------------
    html += F(
"<div class='box'>"
"  <h3 id='t_ipcfg'>IP Configuration</h3>"
"  <label><input type='checkbox' id='useStatic' onchange='toggleStatic()'> <span id='t_useStatic'>Use static IP</span></label>"
"  <div id='staticFields' style='display:none;'>"
"    <label id='t_ip'>IP Address:</label><input id='ipaddr'>"
"    <label id='t_mask'>Subnet Mask:</label><input id='mask'>"
"    <label id='t_gw'>Gateway:</label><input id='gw'>"
"    <label id='t_dns'>DNS:</label><input id='dns'>"
"  </div>"
"  <button id='t_saveNet' onclick='saveNet()'>Save</button>"
"  <pre id='netmsg'></pre>"
"</div>"
    );

    html += F("<button onclick=\"location.href='/'\" id='t_back'>Back</button>");

    // ---------------------------------------------------------
    // JavaScript
    // ---------------------------------------------------------
    html += F("<script>");

    // ---------------- LANGUAGE SYSTEM ----------------
    html += F(R"rawliteral(
const LANG = {
  de:{
    title:"Verbindungseinstellungen",
    wifinets:"WLAN Netzwerke",
    scanbtn:"Scan starten",
    mqtt:"MQTT Einstellungen",
    mqtt_enabled:"MQTT aktivieren",
    mqtt_server:"Server:",
    mqtt_port:"Port:",
    mqtt_user:"Benutzername:",
    mqtt_pass:"Passwort:",
    mqtt_topic:"Topic-Prefix:",
    saveMqtt:"Speichern",
    ntp:"Zeitserver",
    ntp_label:"Zeitserver:",
	timesettings:"Zeiteinstellungen",
	timezone:"Zeitzone:",
	dst:"Sommerzeit",
	saveTime:"Speichern",
    saveNtp:"Speichern",
    ipcfg:"IP-Konfiguration",
    useStatic:"Feste IP verwenden",
    ip:"IP-Adresse:",
    mask:"Subnetzmaske:",
    gw:"Gateway:",
    dns:"DNS:",
    saveNet:"Speichern",
    back:"Zurück"
  },
  en:{
    title:"Connection Settings",
    wifinets:"WiFi Networks",
    scanbtn:"Start Scan",
    mqtt:"MQTT Settings",
    mqtt_enabled:"Enable MQTT",
    mqtt_server:"Server:",
    mqtt_port:"Port:",
    mqtt_user:"Username:",
    mqtt_pass:"Password:",
    mqtt_topic:"Topic Prefix:",
    saveMqtt:"Save",
    ntp:"NTP Server",
    ntp_label:"NTP Server:",
	timesettings:"Time Settings",
	timezone:"Timezone:",
	dst:"Daylight Saving Time",
	saveTime:"Save",
    saveNtp:"Save",
    ipcfg:"IP Configuration",
    useStatic:"Use static IP",
    ip:"IP Address:",
    mask:"Subnet Mask:",
    gw:"Gateway:",
    dns:"DNS:",
    saveNet:"Save",
    back:"Back"
  }
};

function safeSet(id,val){
  const e=document.getElementById(id);
  if(e) e.innerText=val;
}

function applyLanguage(l){
  const T = LANG[l];
  for(let k in T) safeSet("t_"+k,T[k]);
}

function changeLang(){
  let l = document.getElementById("langsel").value;
  localStorage.setItem("lang",l);
  applyLanguage(l);
}

let lang = localStorage.getItem("lang") || "de";
document.getElementById("langsel").value = lang;
applyLanguage(lang);
)rawliteral");

    // ---------------- WIFI SCAN ----------------
    html += F(R"rawliteral(
async function scan(){
  document.getElementById('scan').innerText = 'Scanning...';
  let r = await fetch('/api/wifi/scan');
  let j = await r.json();

  let unique = [];
  let seen = new Set();
  j.nets.forEach(n=>{
    if(!seen.has(n.ssid)){
      seen.add(n.ssid);
      unique.push(n);
    }
  });

  unique.sort((a,b)=>b.rssi - a.rssi);

  let out = '';
  unique.forEach(n=>{
    out += "<div style='margin-top:10px;'><b>"+n.ssid+"</b> ("+n.rssi+" dBm)<br>";
    out += "<input id='pw_"+n.ssid+"' type='password' placeholder='Password'>";
    out += "<button onclick=\"connectTo('"+n.ssid+"')\">Connect</button></div>";
  });

  document.getElementById('scan').innerHTML = out;
}

async function connectTo(ssid){
  let pw = document.getElementById('pw_'+ssid).value;
  let r = await fetch('/api/wifi/connect',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:ssid, pass:pw})
  });
  alert(await r.text());
}
)rawliteral");

    // ---------------- MQTT CONFIG ----------------
    html += F(R"rawliteral(
async function loadMqtt(){
  let r = await fetch('/api/mqtt/config');
  let j = await r.json();

  document.getElementById('mqtt_enabled').checked = j.enabled;
  document.getElementById('mqtt_server').value = j.server;
  document.getElementById('mqtt_port').value = j.port;
  document.getElementById('mqtt_user').value = j.user;
  document.getElementById('mqtt_pass').value = j.pass;
  document.getElementById('mqtt_topic').value = j.topic;
}
loadMqtt();

async function saveMqtt(){
  let data = {
    enabled: document.getElementById('mqtt_enabled').checked,
    server: document.getElementById('mqtt_server').value,
    port: parseInt(document.getElementById('mqtt_port').value),
    user: document.getElementById('mqtt_user').value,
    pass: document.getElementById('mqtt_pass').value,
    topic: document.getElementById('mqtt_topic').value
  };

  let r = await fetch('/api/mqtt/config',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)
  });

  document.getElementById('mqttmsg').innerText = await r.text();
}
)rawliteral");

// ---------------- TIME SETTINGS ----------------
html += F(R"rawliteral(
async function loadTime(){
  let r = await fetch('/api/net/time');
  let j = await r.json();

  document.getElementById('ntp_server').value = j.server;
  document.getElementById('timezone').value = j.timezone;
  document.getElementById('dst').checked = j.dst;
}
loadTime();

async function saveTime(){
  let data = {
    server: document.getElementById('ntp_server').value,
    timezone: document.getElementById('timezone').value,
    dst: document.getElementById('dst').checked
  };

  let r = await fetch('/api/net/time',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)
  });

  document.getElementById('timemsg').innerText = await r.text();
}
)rawliteral");

    // ---------------- IP CONFIG ----------------
    html += F(R"rawliteral(
async function loadNet(){
  let r = await fetch('/api/net/config');
  let j = await r.json();

  document.getElementById('useStatic').checked = j.static;
  document.getElementById('ipaddr').value = j.ip;
  document.getElementById('mask').value = j.mask;
  document.getElementById('gw').value = j.gw;
  document.getElementById('dns').value = j.dns;

  toggleStatic();
}
loadNet();

function toggleStatic(){
  document.getElementById('staticFields').style.display =
    document.getElementById('useStatic').checked ? 'block' : 'none';
}

async function saveNet(){
  let data = {
    static: document.getElementById('useStatic').checked,
    ip: document.getElementById('ipaddr').value,
    mask: document.getElementById('mask').value,
    gw: document.getElementById('gw').value,
    dns: document.getElementById('dns').value
  };

  let r = await fetch('/api/net/config',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)
  });

  document.getElementById('netmsg').innerText = await r.text();
}
)rawliteral");

    html += F("</script></body></html>");

    server.send(200, "text/html", html);
}