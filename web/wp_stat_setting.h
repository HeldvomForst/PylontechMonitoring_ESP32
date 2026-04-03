#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "../config.h"
#include "../py_log.h"
#include "wp_ui.h"

extern WebServer server;

/* ---------------------------------------------------------
   STAT Settings Page
   - Display name is used as MQTT/JSON key
   - Allowed characters: A-Z, a-z, 0-9, underscore
   - All other characters are removed when saving
   - Bilingual hint (DE/EN)
   - English comments only
   --------------------------------------------------------- */
void handleStatSettingsPage() {

    String html;

    html += R"STAT(
<!DOCTYPE html>
<html lang='de'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>STAT Werte</title>
<style>
body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }
.content { margin-left:260px; padding:20px; }
h2 { margin-top:0; }
.lang-box { position:absolute; top:10px; right:20px; }
.lang-box select { padding:5px; font-size:14px; }
.top-grid {
  display:grid;
  grid-template-columns: 1fr 1fr 1fr;
  gap:20px;
  margin-bottom:20px;
}
.box { background:white; padding:15px; border-radius:8px; box-shadow:0 0 5px rgba(0,0,0,0.1); }
.box h3 { margin-top:0; }
.group-title {
  margin-top:25px;
  font-size:20px;
  font-weight:bold;
  color:#333;
}
table { width:100%; border-collapse:collapse; margin-top:10px; }
th, td { padding:8px; border-bottom:1px solid #ddd; text-align:left; }
th { background:#eee; }
.save-btn {
  margin-top:20px; padding:10px 20px; font-size:16px;
  background:#0078d7; color:white; border:none; border-radius:5px; cursor:pointer;
}
.save-btn:hover { background:#005fa3; }
.hint {
  margin-top:10px;
  font-size:13px;
  color:#555;
}
</style>
</head>
<body>
)STAT";

    html += webSidebar("stat_setting");

    html += R"STAT(
<div class='content'>
  <div class='lang-box'>
    <select id='langSel' onchange='changeLang()'>
      <option value='de'>Deutsch</option>
      <option value='en'>English</option>
    </select>
  </div>

  <h2 id='title'>STAT Werte</h2>

<div class='top-grid'>

  <div class='box'>
    <h3>STAT Parser</h3>
    <label>
      <input id='enable_stat' type='checkbox'>
      Parser aktivieren
    </label>
  </div>

  <div class='box'>
    <h3 id='mqttTitle'>MQTT Topic</h3>
    <label id='statLabel'>Stat Subtopic<br>
      <input id='mqtt_stat' type='text' style='width:100%'>
    </label>
  </div>

  <div class='box'>
    <h3 id='intervalTitle'>Abfrageintervall</h3>
    <label id='intervalLabel'>STAT (Sekunden)<br>
      <input id='interval_stat' type='number' min='1' style='width:100%'>
    </label>
  </div>

</div>

  <div class='box'>
    <h3 id='parserTitle'>STAT Parser Fields</h3>

    <div class='hint' id='nameHint'>
      Hinweis: Der Anzeigename wird als MQTT/JSON-Schlüssel verwendet.
      Erlaubt sind nur Buchstaben (A–Z, a–z), Ziffern (0–9) und Unterstrich (_).
      Andere Zeichen werden beim Speichern entfernt.
    </div>

    <div id='groups'></div>

  </div>

  <button class='save-btn' onclick='saveStatSettings()'>Speichern</button>

<script>

/* ---------------------------------------------------------
   Language switch (DE/EN)
--------------------------------------------------------- */
function changeLang(){
  let lang=document.getElementById('langSel').value;
  if(lang==='en'){
    document.getElementById('title').innerText='STAT Values';
    document.getElementById('mqttTitle').innerText='MQTT Topic';
    document.getElementById('statLabel').innerHTML='Stat Subtopic<br>';
    document.getElementById('intervalTitle').innerText='Query Interval';
    document.getElementById('intervalLabel').innerHTML='STAT (seconds)<br>';
    document.getElementById('parserTitle').innerText='STAT Parser Fields';
    document.getElementById('nameHint').innerText =
      'Note: Display name is used as MQTT/JSON key. Allowed: A–Z, a–z, 0–9, underscore (_). Other characters are removed on save.';
  } else {
    location.reload();
  }
}

/* ---------------------------------------------------------
   Sanitize display name (MQTT-safe)
   Allowed: A-Z, a-z, 0-9, underscore
--------------------------------------------------------- */
function sanitizeDisplayName(str){
  return str.replace(/[^A-Za-z0-9_]/g, '');
}

/* ---------------------------------------------------------
   Group detection logic (unchanged)
--------------------------------------------------------- */
function detectGroup(name){
  name = name.toLowerCase();

  if (name.includes("device") || name.includes("data items"))
    return "Meta";

  if (name.includes("cnt") || name.includes("times"))
    return "Zähler";

  if (name.startsWith("bat "))
    return "Batterie Fehler";

  if (name.startsWith("pwr "))
    return "Power Fehler";

  if (name.includes("ht") || name.includes("lt") || name.includes("cot") || name.includes("cut") ||
      name.includes("dot") || name.includes("dut") || name.includes("cht") || name.includes("clt") ||
      name.includes("dht") || name.includes("dlt"))
    return "Temperatur Fehler";

  if (name.includes("reset") || name.includes("shut") || name.includes("rv") ||
      name.includes("bmic") || name.includes("cycle"))
    return "System";

  if (name.includes("percent") || name.includes("coulomb") || name.endsWith(" cap") ||
     (name.includes("soh") && !name.includes("times")))
    return "Kapazität";

  return "Unbekannt";
}

/* ---------------------------------------------------------
   Load STAT parser data
--------------------------------------------------------- */
function loadData(){
  fetch('/api/stat/get').then(r=>r.json()).then(j=>{

    document.getElementById('enable_stat').checked = j.enable_stat;
    document.getElementById('interval_stat').value = j.interval_stat;
    document.getElementById('mqtt_stat').value     = j.mqtt_stat;

    let groups = {};

    // Group fields
    j.fields.forEach(f=>{
      let g = detectGroup(f.name);
      if (!groups[g]) groups[g] = [];
      groups[g].push(f);
    });

    let container = document.getElementById('groups');
    container.innerHTML = '';

    const order = ["Meta","Zähler","Batterie Fehler","Power Fehler","Temperatur Fehler","System","Kapazität","Unbekannt"];

    order.forEach(group=>{
      if (!groups[group]) return;

      container.innerHTML += `<div class='group-title'>${group}</div>
      <table>
      <thead>
        <tr>
          <th>Feldname</th>
          <th>Anzeigename</th>
          <th>Rohdaten</th>
          <th>Faktor</th>
          <th>Einheit</th>
          <th>Wert</th>
          <th>MQTT</th>
          <th>Send</th>
        </tr>
      </thead>
      <tbody id='group_${group}'></tbody>
      </table>`;

      let tbody = document.getElementById('group_'+group);

      groups[group].forEach(f=>{
        let valCell = (f.sendMQTT && f.value) ? f.value : '—';

        let factor = f.factor;
        let unit   = f.unit;
        let mqtt   = f.sendMQTT;
        let send   = f.sendPayload;

        let n   = f.name.toLowerCase();
        let raw = (f.raw || '').toLowerCase();

        let autoDetect = (String(factor) === '1' && unit === '' && !mqtt && !send && f.raw !== '');

        if (autoDetect) {

          if (
            n.includes("percent") ||
            (n.includes("soh") && !n.includes("times")) ||
            raw.includes("%")
          ) {
            factor = '1';
            unit   = '%';
            mqtt   = true;
            send   = true;
          }

          else if (n.includes("coulomb") || n.endsWith(" cap")) {
            factor = '0.001';
            unit   = 'Ah';
          }

          else if (isNaN(parseFloat(raw))) {
            factor = 'text';
            unit   = '';
          }

          else {
            factor = '1';
            unit   = '';
          }
        }

        let row = document.createElement('tr');
        row.innerHTML = `
          <td>${f.name}</td>
          <td><input value='${f.display}'></td>
          <td>${f.raw || '—'}</td>
          <td>
            <select>
              <option value='0.0001' ${factor === '0.0001' ? 'selected' : ''}>0.0001</option>
              <option value='0.001'  ${factor === '0.001'  ? 'selected' : ''}>0.001</option>
              <option value='0.01'   ${factor === '0.01'   ? 'selected' : ''}>0.01</option>
              <option value='0.1'    ${factor === '0.1'    ? 'selected' : ''}>0.1</option>
              <option value='1'      ${factor === '1'      ? 'selected' : ''}>1</option>
              <option value='10'     ${factor === '10'     ? 'selected' : ''}>10</option>
              <option value='text'   ${factor === 'text'   ? 'selected' : ''}>text</option>
              <option value='date'   ${factor === 'date'   ? 'selected' : ''}>date</option>
            </select>
          </td>
          <td>
            <select>
              <option value=''         ${unit === ''         ? 'selected' : ''}></option>
              <option value='V'        ${unit === 'V'        ? 'selected' : ''}>V</option>
              <option value='A'        ${unit === 'A'        ? 'selected' : ''}>A</option>
              <option value='Ah'       ${unit === 'Ah'       ? 'selected' : ''}>Ah</option>
              <option value='°C'       ${unit === '°C'       ? 'selected' : ''}>°C</option>
              <option value='%'        ${unit === '%'        ? 'selected' : ''}>%</option>
              <option value='timestamp'${unit === 'timestamp'? 'selected' : ''}>timestamp</option>
            </select>
          </td>
          <td>${valCell}</td>
          <td><input type='checkbox' class='mqtt' ${mqtt ? 'checked' : ''}></td>
          <td><input type='checkbox' class='send' ${send ? 'checked' : ''}></td>
        `;
        tbody.appendChild(row);
      });
    });

  });
}

/* ---------------------------------------------------------
   Checkbox logic (MQTT ↔ Send)
--------------------------------------------------------- */
document.addEventListener("change", function(e) {
  let target = e.target;

  if (target.classList.contains("mqtt")) {
    let row = target.closest("tr");
    let sendBox = row.querySelector(".send");
    if (!target.checked && sendBox) {
      sendBox.checked = false;
    }
  }

  if (target.classList.contains("send")) {
    let row = target.closest("tr");
    let mqttBox = row.querySelector(".mqtt");
    if (target.checked && mqttBox) {
      mqttBox.checked = true;
    }
  }
});

/* ---------------------------------------------------------
   Save STAT settings
   (Display name is sanitized before saving)
--------------------------------------------------------- */
function saveStatSettings(){
  let data = {
    enable_stat: document.getElementById('enable_stat').checked,
    interval_stat: parseInt(document.getElementById('interval_stat').value),
    mqtt_stat: document.getElementById('mqtt_stat').value,
    fields: []
  };

  document.querySelectorAll('tbody tr').forEach(row => {

    let name = row.cells[0].innerText;

    let dispInput = row.cells[1].querySelector("input");
    let dispRaw   = dispInput.value || name;

    // Sanitize display name
    let dispClean = sanitizeDisplayName(dispRaw);
    dispInput.value = dispClean;

    let factor  = row.cells[3].querySelector("select").value;
    let unit    = row.cells[4].querySelector("select").value;
    let mqtt    = row.cells[6].querySelector("input").checked;
    let send    = row.cells[7].querySelector("input").checked;

    data.fields.push({
      name: name,
      display: dispClean,
      factor: factor,
      unit: unit,
      sendMQTT: mqtt,
      sendPayload: send
    });
  });

  fetch('/api/stat/set', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(data)
  }).then(() => alert('Gespeichert'));
}

loadData();
</script>
</div>
</body>
</html>
)STAT";

    server.send(200, "text/html", html);
}