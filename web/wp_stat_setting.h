#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "../config.h"
#include "../py_log.h"
#include "wp_ui.h"

extern WebServer server;

void handleStatSettingsPage() {

    String html;

    html += R"(
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
</style>
</head>
<body>
)";

    html += webSidebar("stat_setting");

    html += R"(
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

    <div id='groups'></div>

  </div>

  <button class='save-btn' onclick='saveStatSettings()'>Speichern</button>

<script>
function changeLang(){
  let lang=document.getElementById('langSel').value;
  if(lang==='en'){
    document.getElementById('title').innerText='STAT Values';
    document.getElementById('mqttTitle').innerText='MQTT Topic';
    document.getElementById('statLabel').innerHTML='Stat Subtopic<br>';
    document.getElementById('intervalTitle').innerText='Query Interval';
    document.getElementById('intervalLabel').innerHTML='STAT (seconds)<br>';
    document.getElementById('parserTitle').innerText='STAT Parser Fields';
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

// Gruppierungslogik
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

  if (name.includes("soh") || name.includes("percent") || name.includes("coulomb") || name.includes("cap"))
    return "Kapazität";

  return "Unbekannt";
}

function loadData(){
  fetch('/api/stat/get').then(r=>r.json()).then(j=>{

    document.getElementById('enable_stat').checked = j.enable_stat;
    document.getElementById('interval_stat').value = j.interval_stat;
    document.getElementById('mqtt_stat').value     = j.mqtt_stat;

    let groups = {};

    // Felder gruppieren
    j.fields.forEach(f=>{
      let g = detectGroup(f.name);
      if (!groups[g]) groups[g] = [];
      groups[g].push(f);
    });

    let container = document.getElementById('groups');
    container.innerHTML = '';

    // Gruppen in Reihenfolge anzeigen
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

        let row = document.createElement('tr');
        row.innerHTML = `
          <td>${f.name}</td>
          <td><input id='disp_${f.name}' value='${f.display}'></td>
          <td>${f.raw || '—'}</td>
          <td>
            <select id='fac_${f.name}'>
              <option value='1'>1</option>
              <option value='text'>text</option>
            </select>
          </td>
          <td>
            <select id='unit_${f.name}'>
              <option value=''></option>
              <option value='%'>%</option>
            </select>
          </td>
          <td>${valCell}</td>
          <td><input type='checkbox' id='mqtt_${f.name}' ${f.sendMQTT ? 'checked' : ''}></td>
          <td><input type='checkbox' id='send_${f.name}' ${f.sendPayload ? 'checked' : ''}></td>
        `;
        tbody.appendChild(row);

        // Auto-Detect
        let factor = f.factor;
        let unit   = f.unit;
        let mqtt   = f.sendMQTT;
        let send   = f.sendPayload;

        let n = f.name.toLowerCase();
        let raw = (f.raw || '').toLowerCase();

        let autoDetect = (factor === '1' && unit === '' && !mqtt && !send && f.raw !== '');

        if (autoDetect) {
          if (n.includes("soh") || raw.includes("%")) {
            factor = '1';
            unit = '%';
            mqtt = true;
            send = true;
          }
          else if (isNaN(parseFloat(raw))) {
            factor = 'text';
            unit = '';
            mqtt = false;
            send = false;
          }
        }

        document.getElementById('fac_' + f.name).value = factor;
        document.getElementById('unit_' + f.name).value = unit;
        document.getElementById('mqtt_' + f.name).checked = mqtt;
        document.getElementById('send_' + f.name).checked = send;
      });
    });
  });
}

function saveStatSettings(){
  let data={
    enable_stat: document.getElementById('enable_stat').checked,
    interval_stat:parseInt(document.getElementById('interval_stat').value),
    mqtt_stat:document.getElementById('mqtt_stat').value,
    fields:[]
  };

  document.querySelectorAll('tbody tr').forEach(row=>{
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

  fetch('/api/stat/set',{
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