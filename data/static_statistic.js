function sanitize(str){
    return str.replace(/[^A-Za-z0-9_]/g, "");
}

function autodetect(name, raw) {

    let n = name.toLowerCase();
    let r = (raw || "").toLowerCase();

    if (n.includes("percent") || r.includes("%"))
        return { factor: "1", unit: "%", mqtt: true, send: true };

    if (n.includes("coulomb"))
        return { factor: "0.001", unit: "Ah", mqtt: true, send: true };

    if (isNaN(parseFloat(r)))
        return { factor: "text", unit: "", mqtt: false, send: false };

    return { factor: "1", unit: "", mqtt: false, send: false };
}

function statLoad() {
    fetch("/api/stat/values")
        .then(r => r.json())
        .then(j => {

            // CONFIG oben
            document.getElementById("enable_stat").checked = j.config.enableStat;
            document.getElementById("topic_stat").value = j.mqtt.topicStat;
            document.getElementById("interval_stat").value = j.config.intervalStat / 1000;

            // Tabelle unten
            let container = document.getElementById("stat_groups");
            container.innerHTML = "";

            // Eine einzige Tabelle erzeugen
            let table = document.createElement("table");
            table.className = "conn-table";

            table.innerHTML = `
                <thead>
                    <tr>
                        <th>Feldname</th>
                        <th>Anzeigename</th>
                        <th>Rohdaten</th>
                        <th>Faktor</th>
                        <th>Einheit</th>
                        <th>MQTT</th>
                        <th>Send</th>
                    </tr>
                </thead>
                <tbody></tbody>
            `;

            let tbody = table.querySelector("tbody");

            // gespeicherte Felder
            let saved = {};
            j.fields.forEach(f => saved[f.name] = f);

            // ALLE FELDER UNTEREINANDER
            for (let i = 0; i < j.headers.length; i++) {

                let name = j.headers[i];
                let raw  = j.values[i];

                let display = name;
                let factor  = "1";
                let unit    = "";
                let mqtt    = false;
                let send    = false;

                if (saved[name]) {
                    display = saved[name].display;
                    factor  = saved[name].factor;
                    unit    = saved[name].unit;
                    mqtt    = saved[name].sendMQTT;
                    send    = saved[name].sendPayload;
                }
                else {
                    let auto = autodetect(name, raw);
                    factor = auto.factor;
                    unit   = auto.unit;
                    mqtt   = auto.mqtt;
                    send   = auto.send;
                }

                let row = document.createElement("tr");
                row.innerHTML = `
                    <td>${name}</td>
                    <td><input value="${display}"></td>
                    <td>${raw}</td>
                    <td>
                        <select>
                            <option value="0.0001">0.0001</option>
                            <option value="0.001">0.001</option>
                            <option value="0.01">0.01</option>
                            <option value="0.1">0.1</option>
                            <option value="1">1</option>
                            <option value="10">10</option>
                            <option value="text">text</option>
                            <option value="date">date</option>
                        </select>
                    </td>
                    <td>
                        <select>
                            <option value=""></option>
                            <option value="V">V</option>
                            <option value="A">A</option>
                            <option value="Ah">Ah</option>
                            <option value="°C">°C</option>
                            <option value="%">%</option>
                            <option value="timestamp">timestamp</option>
                        </select>
                    </td>
                    <td><input type="checkbox" class="mqtt"></td>
                    <td><input type="checkbox" class="send"></td>
                `;

                // Werte setzen
                row.cells[3].querySelector("select").value = factor;
                row.cells[4].querySelector("select").value = unit;
                row.cells[5].querySelector("input").checked = mqtt;
                row.cells[6].querySelector("input").checked = send;

                tbody.appendChild(row);
            }

            container.appendChild(table);
        });
}

document.addEventListener("change", function(e) {

    let target = e.target;

    // MQTT deaktiviert → SEND deaktivieren
    if (target.classList.contains("mqtt")) {
        let row = target.closest("tr");
        if (!row) return;

        let sendBox = row.querySelector(".send");
        if (!sendBox) return;

        if (!target.checked) {
            sendBox.checked = false;
        }
    }

    // SEND aktiviert → MQTT aktivieren
    if (target.classList.contains("send")) {
        let row = target.closest("tr");
        if (!row) return;

        let mqttBox = row.querySelector(".mqtt");
        if (!mqttBox) return;

        if (target.checked) {
            mqttBox.checked = true;
        }
    }
});

statLoad();

function saveStatSettings() {

    let data = {
        config: {
            enableStat: document.getElementById("enable_stat").checked,
            intervalStat: parseInt(document.getElementById("interval_stat").value) * 1000
        },
        mqtt: {
            topicStat: document.getElementById("topic_stat").value
        },
        fields: []
    };

    document.querySelectorAll('#stat_groups table tbody tr').forEach(row => {

        let name = row.cells[0].innerText;

        // Display-Name
        let dispInput = row.cells[1].querySelector("input");
        let dispRaw   = dispInput.value || name;

        // Sanitizen
        let dispClean = sanitize(dispRaw);
        dispInput.value = dispClean;

        // Faktor
        let factor = row.cells[3].querySelector("select").value;

        // Einheit
        let unit = row.cells[4].querySelector("select").value;

        // MQTT
        let mqtt = row.cells[5].querySelector("input").checked;

        // Send
        let send = row.cells[6].querySelector("input").checked;

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
