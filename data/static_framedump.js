let running = true;

function fetchFrame() {
    if (!running) return;

    fetch("/api/framedump")
        .then(r => r.text())
        .then(t => {
            if (t.trim().length > 0) {
                const box = document.getElementById("dump");
                box.textContent += "\n\n--- FRAME ---\n" + t;
                box.scrollTop = box.scrollHeight;
            }
        })
        .catch(e => console.log(e));
}

setInterval(fetchFrame, 1000);

function clearDump() {
    document.getElementById("dump").textContent = "";
}

function toggle() {
    running = !running;
}
