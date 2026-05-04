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

async function loadLogLevel() {
    let r = await fetch('/api/log/level');
    let j = await r.json();

    document.getElementById('log_info').checked = j.info;
    document.getElementById('log_warn').checked = j.warn;
    document.getElementById('log_error').checked = j.error;
    document.getElementById('log_debug').checked = j.debug;
}

async function saveLogLevel() {
    let data = {
        info: document.getElementById('log_info').checked,
        warn: document.getElementById('log_warn').checked,
        error: document.getElementById('log_error').checked,
        debug: document.getElementById('log_debug').checked
    };

    let r = await fetch('/api/log/level', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
    });

    document.getElementById('logmsg').innerText = await r.text();
}

loadLog();
loadLogLevel();
