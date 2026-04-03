#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "../config.h"
#include "../py_log.h"
#include "wp_ui.h"

extern WebServer server;

/* ---------------------------------------------------------
   PWR Settings Page (Basic Values)
   - Display name is used as MQTT/JSON key
   - Allowed characters: A-Z, a-z, 0-9, underscore (_)
   - All other characters are removed when saving
   - Fully bilingual (DE/EN)
   - Same sanitizing logic as BAT settings page
   --------------------------------------------------------- */
void handlePwrSettingsPage() {

    String html;

    html += R"PWR(
<!DOCTYPE html>
<html lang='de'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Basiswerte</title>
<style>
body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }
.content { margin-left:260px; padding:20px; }
h2 { margin-top:0; }
.lang-box { position:absolute; top:10px; right:20px; }
.lang-box select { padding:5px; font-size:14px; }
.box { background:white; padding:15px; border-radius:8px; box-shadow:0 0 5px rgba(0,0,0,0.1); }
table { width:100%; border-collapse:collapse; margin-top:20px; }
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
)PWR";

    html += webSidebar("pwr_setting");

    html += R"PWR(
<div class='content'>
  <div class='lang-box'>
    <select id='langSel' onchange='changeLang()'>
      <option value='de'>Deutsch</option>
      <option value='en'>English</option>
    </select>
  </div>

  <h2 id='title'>Basiswerte</h2>

  <div class='box'>
    <h3 id='parserTitle'>PWR Parser Fields</h3>

    <div class='hint' id='nameHint'>
      Hinweis: Der Anzeigename wird als MQTT/JSON-Schlüssel verwendet.
      Erlaubt sind nur Buchstaben (A–Z, a–z), Ziffern (0–9) und Unterstrich (_).
      Andere Zeichen werden beim Speichern entfernt.
    </div>

    <table>
      <thead>
        <tr>
          <th id='colField'>Feldname</th>
          <th id='colDisp'>Anzeigename</th>
          <th id='colRaw'>Rohdaten</th>
          <th id='colFactor'>Faktor</th>
          <th id='colUnit'>Einheit</th>
          <th id='colValue'>Wert</th>
          <th id='colMqtt'>MQTT</th>
          <th id='colSend'>Send</th>
        </tr>
      </thead>
      <tbody id='parserTable'></tbody>
    </table>
  </div>

  <button class='save-btn' onclick='savePwrSettings()'>Speichern</button>

<script>

/* ---------------------------------------------------------
   Language switch (DE/EN)
--------------------------------------------------------- */
function changeLang(){
  let lang=document.getElementById('langSel').value;
  if(lang==='en'){
    document.getElementById('title').innerText='Basic Values';
    document.getElementById('parserTitle').innerText='PWR Parser Fields';
    document.getElementById('colField').innerText='Field';
    document.getElementById('colDisp').innerText='Display Name';
    document.getElementById('colRaw').innerText='Raw Data';
    document.getElementById('colFactor').innerText='Factor';
    document.getElementById('colUnit').innerText='Unit';
    document.getElementById('colValue').innerText='Value';
    document.getElementById('colMqtt').innerText='MQTT';
    document.getElementById('colSend').innerText='Send';
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
   Load PWR parser data
--------------------------------------------------------- */
function loadData(){
  fetch('/api/pwr/get').then(r=>r.json()).then(j=>{

    let table = document.getElementById('parserTable');
    table.innerHTML = '';

    j.fields.forEach(f=>{

      let valCell = (f.sendMQTT && f.value) ? f.value : '—';
      let disp = f.display || f.name;

      let row = document.createElement('tr');
      row.innerHTML = `
        <td>${f.name}</td>
        <td><input id='disp_${f.name}' value='${disp}'></td>
        <td>${f.raw || '—'}</td>
        <td>
          <select id='fac_${f.name}'>
            <option value='0.0001'>0.0001</option>
            <option value='0.001'>0.001</option>
            <option value='0.01'>0.01</option>
            <option value='0.1'>0.1</option>
            <option value='1'>1</option>
            <option value='10'>10</option>
            <option value='text'>text</option>
            <option value='date'>date</option>
          </select>
        </td>
        <td>
          <select id='unit_${f.name}'>
            <option value=''></option>
            <option value='V'>V</option>
            <option value='A'>A</option>
            <option value='°C'>°C</option>
            <option value='%'>%</option>
            <option value='Ah'>Ah</option>
            <option value='timestamp'>timestamp</option>
          </select>
        </td>
        <td>${valCell}</td>
        <td><input type='checkbox' id='mqtt_${f.name}' ${f.sendMQTT?'checked':''}></td>
        <td><input type='checkbox' id='send_${f.name}' ${f.sendPayload?'checked':''}></td>
      `;
      table.appendChild(row);

      document.getElementById('fac_'+f.name).value = f.factor;
      document.getElementById('unit_'+f.name).value = f.unit;
    });
  });
}

/* ---------------------------------------------------------
   Save PWR settings
--------------------------------------------------------- */
function savePwrSettings(){
  let data={ fields:[] };

  document.querySelectorAll('#parserTable tr').forEach(row=>{
    let name=row.cells[0].innerText;

    let dispInput = document.getElementById('disp_'+name);
    let dispRaw   = dispInput.value || name;

    // Clean display name
    let dispClean = sanitizeDisplayName(dispRaw);
    dispInput.value = dispClean;

    data.fields.push({
      name:name,
      display:dispClean,
      factor:document.getElementById('fac_'+name).value,
      unit:document.getElementById('unit_'+name).value,
      sendMQTT:document.getElementById('mqtt_'+name).checked,
      sendPayload:document.getElementById('send_'+name).checked
    });
  });

  fetch('/api/pwr/set',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)
  }).then(()=>alert('Gespeichert'));
}

loadData();
</script>
</div>
</body>
</html>
)PWR";

    server.send(200, "text/html", html);
}