#pragma once
#include "../wp_webserver.h"
#include "wp_ui.h"

/* ---------------------------------------------------------
   Connection Settings Page (WiFi, MQTT, NTP, IP Config)
   - Language selector fixed (small, top-right)
   - Timezones lazy-loaded to reduce RAM usage
--------------------------------------------------------- */
inline void handleConnectPage() {

    String html;

    html += F(R"rawliteral(
<!DOCTYPE html>
<html lang='de'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Connection Settings</title>
<style>
body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }
.content { margin-left:260px; padding:20px; }
h2 { margin-top:0; }
.box { background:white; padding:15px; border-radius:8px; box-shadow:0 0 5px rgba(0,0,0,0.1); margin-bottom:20px; }
label { font-weight:bold; display:block; margin-top:10px; }
input, select { padding:8px; font-size:16px; margin-top:4px; width:100%; box-sizing:border-box; }
button { padding:10px 20px; font-size:16px; margin-top:10px; border:none; border-radius:6px; background:#3498db; color:white; cursor:pointer; }
button:hover { background:#2980b9; }
/* Language selector: small, top-right, not full width */
#langsel {
  position:absolute;
  top:10px;
  right:20px;
  font-size:14px;
  padding:4px;
  width:auto;
}
input[type=checkbox] { transform: scale(1.3); margin-right: 8px; accent-color: #3498db; }
</style>
</head>
<body>
)rawliteral");

    // Sidebar
    html += webSidebar("connect");

    html += F(R"rawliteral(
<div class='content'>
  <h2 id='t_title'>Connection Settings</h2>

  <!-- Language selector (fixed position, small) -->
  <select id='langsel' onchange='changeLang()'>
    <option value='de'>Deutsch</option>
    <option value='en'>English</option>
  </select>

  <!-- WiFi Networks -->
  <div class='box'>
    <h3 id='t_wifinets'>WiFi Networks</h3>
    <button id='t_scanbtn' onclick='scan()'>Scan</button>
    <div id='scan'></div>
  </div>

  <!-- MQTT Settings -->
  <div class='box'>
    <h3 id='t_mqtt'>MQTT Settings</h3>

    <label>
      <input type='checkbox' id='mqtt_enabled'>
      <span id='t_mqtt_enabled'>MQTT aktivieren</span>
    </label>

    <label id='t_mqtt_server'>Server:</label>
    <input id='mqtt_server'>

    <label id='t_mqtt_port'>Port:</label>
    <input id='mqtt_port' type='number'>

    <label id='t_mqtt_user'>Username:</label>
    <input id='mqtt_user'>

    <label id='t_mqtt_pass'>Password:</label>
    <input id='mqtt_pass' type='password'>

    <label id='t_mqtt_topic'>Topic Prefix:</label>
    <input id='mqtt_topic'>

    <button id='t_saveMqtt' onclick='saveMqtt()'>Save</button>
    <pre id='mqttmsg'></pre>
  </div>

  <!-- Time Settings (manual time + NTP + timezone) -->
  <div class='box'>
    <h3 id='t_timesettings'>Time Settings</h3>

    <!-- Manual time mode -->
    <label>
      <input type='checkbox' id='manual_mode' onchange='toggleManualMode()'>
      <span id='t_manual_mode'>Manual time input</span>
    </label>

    <!-- Manual time fields -->
    <div id='manual_fields' style='display:none; margin-top:10px;'>
      <label id='t_manual_date'>Date:</label>
      <input id='manual_date' type='date'>

      <label id='t_manual_time'>Time:</label>
      <input id='manual_time' type='time'>

      <label>
        <input type='checkbox' id='manual_dst'>
        <span id='t_manual_dst'>DST +1 hour</span>
      </label>
    </div>

    <!-- NTP mode -->
    <div id='ntp_fields'>
      <label>
        <input type='checkbox' id='use_gateway_ntp' onchange='toggleNtpMode()'>
        <span id='t_use_gateway_ntp'>Use gateway as NTP server</span>
      </label>

      <label>
        <input type='checkbox' id='manual_ntp' onchange='toggleNtpMode()'>
        <span id='t_manual_ntp'>Use manual NTP server</span>
      </label>

      <div id='manual_ntp_field' style='display:none;'>
        <label id='t_ntp_label'>NTP Server:</label>
        <input id='ntp_server' value='pool.ntp.org'>
      </div>

      <!-- Timezone selection (Region -> City), lazy-loaded -->
      <label id='t_tz_region'>Region:</label>
      <select id='tz_region'></select>

      <label id='t_tz_city'>City:</label>
      <select id='tz_city'></select>
    </div>

    <button id='t_saveTime' onclick='saveTime()'>Save</button>
    <pre id='timemsg'></pre>
  </div>

  <!-- IP Configuration -->
  <div class='box'>
    <h3 id='t_ipcfg'>IP Configuration</h3>
    <label>
      <input type='checkbox' id='useStatic' onchange='toggleStatic()'>
      <span id='t_useStatic'>Use static IP</span>
    </label>
    <div id='staticFields' style='display:none;'>
      <label id='t_ip'>IP Address:</label>
      <input id='ipaddr'>
      <label id='t_mask'>Subnet Mask:</label>
      <input id='mask'>
      <label id='t_gw'>Gateway:</label>
      <input id='gw'>
      <label id='t_dns'>DNS:</label>
      <input id='dns'>
    </div>
    <button id='t_saveNet' onclick='saveNet()'>Save</button>
    <pre id='netmsg'></pre>
  </div>

  <button onclick="location.href='/'" id='t_back'>Back</button>
</div>
)rawliteral");

    // JavaScript
    html += F(R"rawliteral(
<script>
// ---------------- LANGUAGE SYSTEM ----------------
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
    timesettings:"Zeiteinstellungen",
    manual_mode:"Zeit manuell eingeben",
    manual_date:"Datum:",
    manual_time:"Uhrzeit:",
    manual_dst:"Sommerzeit +1 Stunde",
    use_gateway_ntp:"Standard-Gateway als NTP-Server verwenden",
    manual_ntp:"Manuellen NTP-Server verwenden",
    ntp_label:"NTP-Server:",
    tz_region:"Region:",
    tz_city:"Stadt:",
    saveTime:"Speichern",
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
    timesettings:"Time Settings",
    manual_mode:"Manual time input",
    manual_date:"Date:",
    manual_time:"Time:",
    manual_dst:"DST +1 hour",
    use_gateway_ntp:"Use gateway as NTP server",
    manual_ntp:"Use manual NTP server",
    ntp_label:"NTP Server:",
    tz_region:"Region:",
    tz_city:"City:",
    saveTime:"Save",
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

// ---------------- WIFI SCAN ----------------
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

// ---------------- MQTT CONFIG ----------------
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

// ---------------- TIMEZONES (lazy-loaded) ----------------
let TZ_DATA = {};
let TZ_READY = false;
let CURRENT_REGION = "";
let CURRENT_CITY_TZ = "";

// Load current time configuration (only current timezone)
async function loadTime(){
  let r = await fetch('/api/net/time');
  let j = await r.json();

  document.getElementById('manual_mode').checked = j.manual_mode;
  document.getElementById('manual_date').value = j.manual_date || '';
  document.getElementById('manual_time').value = j.manual_time || '';
  document.getElementById('manual_dst').checked = j.manual_dst || false;

  document.getElementById('use_gateway_ntp').checked = j.use_gateway_ntp;
  document.getElementById('manual_ntp').checked = j.manual_ntp;
  document.getElementById('ntp_server').value = j.server || 'pool.ntp.org';

  // Extract region + city from timezone string
  let tz = j.timezone || "Europe/Berlin";
  let slash = tz.indexOf('/');
  CURRENT_REGION = (slash > 0) ? tz.substring(0, slash) : "Europe";
  CURRENT_CITY_TZ = tz;

  const regionSel = document.getElementById('tz_region');
  const citySel   = document.getElementById('tz_city');

  // Show only current region
  regionSel.innerHTML = "";
  let optR = document.createElement('option');
  optR.value = CURRENT_REGION;
  optR.textContent = CURRENT_REGION;
  regionSel.appendChild(optR);

  // Show only current city
  citySel.innerHTML = "";
  let cityName = (slash > 0) ? tz.substring(slash + 1) : tz;
  let optC = document.createElement('option');
  optC.value = CURRENT_CITY_TZ;
  optC.textContent = cityName;
  citySel.appendChild(optC);

  toggleManualMode();
  toggleNtpMode();
}

// Load full timezone list only when user interacts
async function ensureTimezonesLoaded(){
  if (TZ_READY) return;

  try{
    let r = await fetch('/api/net/timezones');
    TZ_DATA = await r.json();
    TZ_READY = true;

    const regionSel = document.getElementById('tz_region');
    regionSel.innerHTML = '';

    Object.keys(TZ_DATA).forEach(region=>{
      let opt = document.createElement('option');
      opt.value = region;
      opt.textContent = region;
      regionSel.appendChild(opt);
    });

    if (CURRENT_REGION) regionSel.value = CURRENT_REGION;
    updateCitySelect();
  }catch(e){
    console.error('Failed to load timezones', e);
  }
}

function updateCitySelect(){
  if (!TZ_READY) return;

  const regionSel = document.getElementById('tz_region');
  const citySel   = document.getElementById('tz_city');
  const region    = regionSel.value;

  citySel.innerHTML = '';

  if (!region || !TZ_DATA[region]) return;

  TZ_DATA[region].forEach(entry=>{
    let opt = document.createElement('option');
    opt.value = entry.tz;
    opt.textContent = entry.city;
    citySel.appendChild(opt);
  });

  if (CURRENT_CITY_TZ) {
    citySel.value = CURRENT_CITY_TZ;
  }
}

function toggleManualMode(){
  let manual = document.getElementById('manual_mode').checked;

  document.getElementById('manual_fields').style.display = manual ? 'block' : 'none';
  document.getElementById('ntp_fields').style.opacity = manual ? '0.4' : '1';
  document.getElementById('ntp_fields').style.pointerEvents = manual ? 'none' : 'auto';
}

function toggleNtpMode(){
  let useGateway = document.getElementById('use_gateway_ntp').checked;
  let manualNtp  = document.getElementById('manual_ntp').checked;

  if(useGateway){
    document.getElementById('manual_ntp').checked = false;
    document.getElementById('manual_ntp_field').style.display = 'none';
  }

  if(manualNtp){
    document.getElementById('use_gateway_ntp').checked = false;
    document.getElementById('manual_ntp_field').style.display = 'block';
  }

  if(!useGateway && !manualNtp){
    document.getElementById('manual_ntp_field').style.display = 'none';
  }
}

// Save time configuration
async function saveTime(){
  const regionSel = document.getElementById('tz_region');
  const citySel   = document.getElementById('tz_city');

  let data = {
    manual_mode: document.getElementById('manual_mode').checked,
    manual_date: document.getElementById('manual_date').value,
    manual_time: document.getElementById('manual_time').value,
    manual_dst:  document.getElementById('manual_dst').checked,

    use_gateway_ntp: document.getElementById('use_gateway_ntp').checked,
    manual_ntp:      document.getElementById('manual_ntp').checked,
    server:          document.getElementById('ntp_server').value,
    timezone:        citySel.value || ''
  };

  let r = await fetch('/api/net/time',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)
  });

  document.getElementById('timemsg').innerText = await r.text();
}

// Attach lazy-load handlers for timezone selects
document.getElementById('tz_region').addEventListener('focus', ensureTimezonesLoaded);
document.getElementById('tz_city').addEventListener('focus', ensureTimezonesLoaded);

// ---------------- IP CONFIG ----------------
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
    ip:     document.getElementById('ipaddr').value,
    mask:   document.getElementById('mask').value,
    gw:     document.getElementById('gw').value,
    dns:    document.getElementById('dns').value
  };

  let r = await fetch('/api/net/config',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)
  });

  document.getElementById('netmsg').innerText = await r.text();
}

// Initial load: only current time config (no full timezone list)
loadTime();
</script>
</body>
</html>
)rawliteral");

    server.send(200, "text/html", html);
}