inline void handleLogPage() {

    String html;

    html += F(
"<!DOCTYPE html><html lang='de'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Runtime Log</title>"
"<style>"
"body { font-family: Arial; margin:0; padding:0; background:#f4f4f4; }"
".content { margin-left:260px; padding:20px; }"
".box { background:white; padding:15px; border-radius:8px; "
"       box-shadow:0 0 5px rgba(0,0,0,0.1); margin-bottom:20px; }"
"#logbox { width:100%; height:60vh; border:1px solid #ccc; padding:10px; "
"          overflow-y:auto; white-space:pre-wrap; background:#111; color:#0f0; "
"          border-radius:6px; font-family:Consolas, monospace; font-size:14px; }"
"button { padding:10px 20px; font-size:16px; margin-top:10px; border:none; "
"         border-radius:6px; background:#3498db; color:white; cursor:pointer; }"
"button:hover { background:#2980b9; }"
"label { display:block; margin-top:8px; font-weight:bold; }"
"input[type=checkbox] { transform:scale(1.3); margin-right:8px; accent-color:#3498db; }"
"</style></head><body>"
    );

    html += webSidebar("log");

    html += F("<div class='content'>");
    html += F("<h2>Runtime Log</h2>");

    // ---------------- LOG VIEWER ----------------
    html += F("<div class='box'>");
    html += F("<div id='logbox'>loading...</div>");
    html += F("<button onclick='loadLog()'>Reload</button>");
    html += F("</div>");

    // ---------------- LOG LEVEL FILTER ----------------
    html += F("<div class='box'>");
    html += F("<h3>Log Level Filter</h3>");

    html += F(
        "<label><input type='checkbox' id='log_info'> Info</label>"
        "<label><input type='checkbox' id='log_warn'> Warn</label>"
        "<label><input type='checkbox' id='log_error'> Error</label>"
        "<label><input type='checkbox' id='log_debug'> Debug</label>"
        "<button onclick='saveLogLevel()'>Save</button>"
        "<pre id='logmsg'></pre>"
    );

    html += F("</div>");

    // ---------------- JAVASCRIPT ----------------
    html += F("<script>"

"function loadLog(){"
"  fetch('/api/log')"
"    .then(r => r.text())"
"    .then(t => {"
"      const box = document.getElementById('logbox');"
"      box.textContent = t;"
"      box.scrollTop = box.scrollHeight;"
"    })"
"    .catch(()=>{ document.getElementById('logbox').textContent='Error loading log'; });"
"}"

"async function loadLogLevel(){"
"  let r = await fetch('/api/log/level');"
"  let j = await r.json();"
"  document.getElementById('log_info').checked  = j.info;"
"  document.getElementById('log_warn').checked  = j.warn;"
"  document.getElementById('log_error').checked = j.error;"
"  document.getElementById('log_debug').checked = j.debug;"
"}"

"async function saveLogLevel(){"
"  let data = {"
"    info:  document.getElementById('log_info').checked,"
"    warn:  document.getElementById('log_warn').checked,"
"    error: document.getElementById('log_error').checked,"
"    debug: document.getElementById('log_debug').checked"
"  };"

"  let r = await fetch('/api/log/level',{"
"    method:'POST',"
"    headers:{'Content-Type':'application/json'},"
"    body:JSON.stringify(data)"
"  });"

"  document.getElementById('logmsg').innerText = await r.text();"
"}"

"loadLog();"
"loadLogLevel();"

"</script>");

    html += F("</div></body></html>");

    server.send(200, "text/html", html);
}