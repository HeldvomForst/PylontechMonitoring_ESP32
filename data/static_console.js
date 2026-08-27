function sendCmd() {
    const cmd = document.getElementById('cmdline').value;
    if (cmd.length === 0) return;

    fetch('/req?code=' + encodeURIComponent(cmd))
        .then(r => r.text())
        .then(ticket => {
            loadFrame(ticket);
        });

    document.getElementById('cmdline').value = '';
}

function quickCmd(c) {
    fetch('/req?code=' + encodeURIComponent(c))
        .then(r => r.text())
        .then(ticket => {
            loadFrame(ticket);
        });
}

function loadFrame(ticket) {
    fetch('/api/lastframe?ticket=' + ticket)
        .then(r => r.text())
        .then(t => {
            const box = document.getElementById('rawout');
            box.value = t;
            box.scrollTop = box.scrollHeight;
        });
}
