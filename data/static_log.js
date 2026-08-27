// -------------------------------------------------------------
// Log laden
// -------------------------------------------------------------
function loadLog() {
    fetch('/api/log')
        .then(r => r.text())
        .then(t => {
            const box = document.getElementById('logbox');
            box.textContent = t;
            box.scrollTop = box.scrollHeight;
        })
        .catch(() => {
            document.getElementById('logbox').textContent = 'Error loading log';
        });
}

// -------------------------------------------------------------
// Log-Level laden
// -------------------------------------------------------------
async function loadLogLevel() {
    let r = await fetch('/api/log/level');
    let j = await r.json();

    document.getElementById('log_info_cb').checked = j.info;
    document.getElementById('log_warn_cb').checked = j.warn;
    document.getElementById('log_error_cb').checked = j.error;
    document.getElementById('log_debug_cb').checked = j.debug;
}

// -------------------------------------------------------------
// Log-Level speichern
// -------------------------------------------------------------
async function saveLogLevel() {
    let data = {
        info: document.getElementById('log_info_cb').checked,
        warn: document.getElementById('log_warn_cb').checked,
        error: document.getElementById('log_error_cb').checked,
        debug: document.getElementById('log_debug_cb').checked
    };

    let r = await fetch('/api/log/level', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
    });

    document.getElementById('logmsg').innerText = await r.text();
}

// -------------------------------------------------------------
// Service-Modus (korrekt für dynamisch geladene Seite)
// -------------------------------------------------------------
function initServiceToggle() {
    const cb = document.getElementById("serviceToggle");
    if (!cb) return;

    // Zustand setzen
    cb.checked = localStorage.getItem("service_enabled") === "1";

    // Sidebar aktualisieren
    if (typeof updateServiceLinks === "function") {
        updateServiceLinks();
    }

    // Änderung speichern
    cb.addEventListener("change", () => {
        if (cb.checked) {
            localStorage.setItem("service_enabled", "1");
        } else {
            localStorage.removeItem("service_enabled");
        }

        if (typeof updateServiceLinks === "function") {
            updateServiceLinks();
        }
    });
}

// direkt aufrufen, weil static_log.js NACH dem Einfügen der Seite ausgeführt wird
initServiceToggle();

// -------------------------------------------------------------
// Initial laden
// -------------------------------------------------------------
loadLog();
loadLogLevel();

