#pragma once
#include "../wp_webserver.h"
#include "wp_ui.h"

/* ---------------------------------------------------------
   Battery Console Page (modern layout, fixed width output)
   --------------------------------------------------------- */
inline void handleBatteryPage() {

    String html;

    html += F(
"<!DOCTYPE html><html lang='de'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Battery Console</title>"
"<style>"
"body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }"
".content { margin-left:260px; padding:20px; }"
".box { background:white; padding:15px; border-radius:8px; "
"       box-shadow:0 0 5px rgba(0,0,0,0.1); margin-bottom:20px; }"
"input, button { font-size:16px; padding:6px; }"
"#cmdline { width:300px; }"
"#rawout { width:100%; max-width:1500px; height:800px; border:1px solid #888; "
"          padding:6px; box-sizing:border-box; resize:none; overflow:auto; "
"          white-space:pre-wrap; font-family:Consolas, monospace; font-size:13px; "
"          background:#111; color:#0f0; border-radius:6px; }"
"a.cmd { margin-right:10px; font-size:16px; text-decoration:none; color:#0066cc; }"
"button { border:none; border-radius:6px; background:#3498db; color:white; cursor:pointer; }"
"button:hover { background:#2980b9; }"
"</style></head><body>"
    );

    html += webSidebar("battery");

    html += F("<div class='content'>");
    html += F("<h2>Battery Console</h2>");

    // Command box
    html += F("<div class='box'>");
    html += F(
"command: <input type='text' id='cmdline' autocomplete='off'>"
"<button onclick='sendCmd()'>send</button>"
"<div style='margin-top:10px;'>"
"  <a href='#' class='cmd' onclick=\"quickCmd('pwr')\">PWR</a>"
"  <a href='#' class='cmd' onclick=\"quickCmd('bat 1')\">BAT 1</a>"
"  <a href='#' class='cmd' onclick=\"quickCmd('bat 2')\">BAT 2</a>"
"  <a href='#' class='cmd' onclick=\"quickCmd('bat 3')\">BAT 3</a>"
"  <a href='#' class='cmd' onclick=\"quickCmd('bat 4')\">BAT 4</a>"
"  <a href='#' class='cmd' onclick=\"quickCmd('help')\">help</a>"
"</div>"
    );
    html += F("</div>");

    // Output box
    html += F("<div class='box'>");
    html += F("<h3>Output</h3>");
    html += F("<textarea id='rawout' readonly>waiting...</textarea>");
    html += F("</div>");

    // JS
    html += F("<script>"
"function sendCmd(){"
"  const cmd = document.getElementById('cmdline').value;"
"  if(cmd.length === 0) return;"
"  fetch('/req?code=' + encodeURIComponent(cmd)).then(()=>loadFrame());"
"  document.getElementById('cmdline').value='';"
"}"

"function quickCmd(c){"
"  fetch('/req?code=' + encodeURIComponent(c)).then(()=>loadFrame());"
"}"

"function loadFrame(){"
"  fetch('/api/lastframe')"
"    .then(r=>r.text())"
"    .then(t=>{"
"      const box = document.getElementById('rawout');"
"      box.value = t;"
"      box.scrollTop = box.scrollHeight;"
"    });"
"}"

"document.getElementById('cmdline').addEventListener('keydown', e=>{"
"  if(e.key === 'Enter'){ e.preventDefault(); sendCmd(); }"
"});"
"</script>");

    html += F("</div></body></html>");

    server.send(200, "text/html", html);
}