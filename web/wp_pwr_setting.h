#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "../config.h"
#include "../py_log.h"
#include "wp_ui.h"

extern WebServer server;


/* ---------------------------------------------------------
   Render the PWR settings page ("Basiswerte")
   - Dynamic parser fields
   - Raw data from lastParserValues
   - MQTT + Send logic
   - Dropdowns for factor & unit
   - No value calculation
   - Language switch (DE/EN)
   --------------------------------------------------------- */
void handlePwrSettingsPage() {

    String html;

    html += R"(
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
.top-grid {
  display:grid;
  grid-template-columns: 1fr 1fr 1fr;
  gap:20px;
  margin-bottom:20px;
}
.box { background:white; padding:15px; border-radius:8px; box-shadow:0 0 5px rgba(0,0,0,0.1); }
.box h3 { margin-top:0; }
table { width:100%; border-collapse:collapse; margin-top:20px; }
th, td { padding:8px; border-bottom:1px solid #ddd; text-align:left; }
th { background:#eee; }
.save-btn {
  margin-top:20px; padding:10px 20px; font-size:16px;
  background:#0078d7; color:white; border:none; border-radius:5px; cursor:pointer;
}
.save-btn:hover { background:#005fa3; }
</style>
</head>
<body>
)";

    html += webSidebar("pwr_setting");

    html += R"(
<div class='content'>
  <div class='lang-box'>
    <select id='langSel' onchange='changeLang()'>
      <option value='de'>Deutsch</option>
      <option value='en'>English</option>
    </select>
  </div>

  <h2 id='title'>Basiswerte</h2>

  <div class='top-grid'>
    <div class='box'>
      <h3 id='tempTitle'>Temperatur</h3>
      <label><input type='checkbox' id='fahrenheit'>
      <span id='tempLabel'>Temperatur in Fahrenheit senden</span></label>
    </div>

    <div class='box'>
      <h3 id='mqttTitle'>MQTT Topics</h3>
      <label id='stackLabel'>Stack Subtopic<br>
        <input id='mqtt_stack' type='text' style='width:100%'>
      </label><br><br>
      <label id='pwrLabel'>Modul Subtopic<br>
        <input id='mqtt_pwr' type='text' style='width:100%'>
      </label>
    </div>

    <div class='box'>
      <h3 id='intervalTitle'>Abfrageintervall</h3>
      <label id='intervalLabel'>PWR (Sekunden)<br>
        <input id='interval_pwr' type='number' min='1' style='width:100%'>
      </label>
    </div>
  </div>

  <div class='box'>
    <h3 id='parserTitle'>PWR Parser Fields</h3>
    <table>
      <thead>
        <tr>
          <th id='colField'>Feldname</th>
          <th id='colDisp'>Anzeigename</th>
          <th id='colRaw'>Rohdaten</th>
          <th id='colFactor'>Faktor / Teiler</th>
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
function changeLang(){
  let lang=document.getElementById('langSel').value;
  if(lang==='en'){
    document.getElementById('title').innerText='Basic Values';
    document.getElementById('tempTitle').innerText='Temperature';
    document.getElementById('tempLabel').innerText='Send temperature in Fahrenheit';
    document.getElementById('mqttTitle').innerText='MQTT Topics';
    document.getElementById('stackLabel').innerHTML='Stack Subtopic<br>';
    document.getElementById('pwrLabel').innerHTML='Modul Subtopic<br>';
    document.getElementById('intervalTitle').innerText='Query Interval';
    document.getElementById('intervalLabel').innerHTML='PWR (seconds)<br>';
    document.getElementById('parserTitle').innerText='PWR Parser Fields';
    document.getElementById('colField').innerText='Field';
    document.getElementById('colDisp').innerText='Display Name';
    document.getElementById('colRaw').innerText='Raw Data';
    document.getElementById('colFactor').innerText='Factor';
    document.getElementById('colUnit').innerText='Unit';
    document.getElementById('colValue').innerText='Value';
    document.getElementById('colMqtt').innerText='MQTT';
    document.getElementById('colSend').innerText='Send';
  } else {
    location.reload();
  }
}

function mqttChanged(name){
  let m=document.getElementById('mqtt_'+name);
  let s=document.getElementById('send_'+name);
  if(!m.checked){ s.checked=false; }
}

function sendChanged(name){
  let m=document.getElementById('mqtt_'+name);
  let s=document.getElementById('send_'+name);
  if(s.checked){ m.checked=true; }
}

function loadData(){
  fetch('/api/pwr/get').then(r=>r.json()).then(j=>{
    document.getElementById('fahrenheit').checked = j.fahrenheit;
    document.getElementById('interval_pwr').value = j.interval_pwr;
    document.getElementById('mqtt_stack').value = j.mqtt_stack;
    document.getElementById('mqtt_pwr').value = j.mqtt_pwr;

    let table = document.getElementById('parserTable');
    table.innerHTML = '';

    j.fields.forEach(f=>{
      let row = document.createElement('tr');
      let valCell = (f.sendMQTT && f.value) ? f.value : '—';
      row.innerHTML = `
        <td>${f.name}</td>
        <td><input id='disp_${f.name}' value='${f.display}'></td>
        <td id='raw_${f.name}'>${f.raw || '—'}</td>
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
            <option value='°F'>°F</option>
            <option value='%'>%</option>
            <option value='timestamp'>timestamp</option>
          </select>
        </td>
        <td id='val_${f.name}'>${valCell}</td>
        <td><input type='checkbox' id='mqtt_${f.name}' ${f.sendMQTT ? 'checked' : ''} onchange='mqttChanged("${f.name}")'></td>
        <td><input type='checkbox' id='send_${f.name}' ${f.sendPayload ? 'checked' : ''} onchange='sendChanged("${f.name}")'></td>
      `;
      table.appendChild(row);

      let factor = f.factor;
      let unit   = f.unit;
      let mqtt   = f.sendMQTT;
      let send   = f.sendPayload;

      let autoDetect = (factor === '1' && unit === '' && !mqtt && !send && f.raw !== '');
      let n = f.name.toLowerCase();
      let raw = (f.raw || '').toLowerCase();

      if (autoDetect) {
        if (raw.endsWith('%') || n.endsWith('%')) {
          factor = '1'; unit = '%'; mqtt = true; send = true;
        }
        else if (n.includes('time')) {
          factor = 'date'; unit = 'timestamp'; mqtt = false; send = false;
        }
		else if (isNaN(parseFloat(raw))) {
          factor = 'text'; unit = ''; mqtt = false; send = false;
        }
        else if (n.includes('volt')) {
          factor = '0.001'; unit = 'V'; mqtt = true; send = true;
        }
        else if (n.includes('curr')) {
          factor = '0.001'; unit = 'A'; mqtt = true; send = true;
        }
        else if (n.endsWith('id')) {
          factor = '1'; unit = ''; mqtt = false; send = false;
        }
        else if (n.includes('t')) {
          factor = '0.001'; unit = '°C'; mqtt = false; send = false;
        }
		else if (n.includes('volt')) {
          factor = '0.001'; unit = 'V'; mqtt = true; send = true;
        }
		else if (n.includes('v')) {
          factor = '0.001'; unit = 'V'; mqtt = false; send = false;
        }
        else {
          factor = 'text'; unit = ''; mqtt = false; send = false;
        }
      }

      document.getElementById('fac_' + f.name).value = factor;
      document.getElementById('unit_' + f.name).value = unit;
      document.getElementById('mqtt_' + f.name).checked = mqtt;
      document.getElementById('send_' + f.name).checked = send;
    });
  });
}

function savePwrSettings(){
  let data={
    fahrenheit:document.getElementById('fahrenheit').checked,
    interval_pwr:parseInt(document.getElementById('interval_pwr').value),
    mqtt_stack:document.getElementById('mqtt_stack').value,
    mqtt_pwr:document.getElementById('mqtt_pwr').value,
    fields:[]
  };

  document.querySelectorAll('#parserTable tr').forEach(row=>{
    let name=row.cells[0].innerText;
    data.fields.push({
      name:name,
      display:document.getElementById('disp_'+name).value,
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
)";

    server.send(200, "text/html", html);
}