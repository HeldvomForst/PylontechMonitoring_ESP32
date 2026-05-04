function sanitize(str){
    return str.replace(/[^A-Za-z0-9_]/g, "");
}

function batLoad() {
    fetch("/api/bat/cells")
        .then(r => r.json())
        .then(j => {

            // Config
            document.getElementById("enable_bat").checked = j.config.enableBat;
            document.getElementById("topic_bat").value   = j.mqtt.topicBat;
            document.getElementById("cell_prefix").value = j.mqtt.cellPrefix;
            document.getElementById("interval_bat").value = j.config.intervalBat / 1000;

            // Tabelle
            let table = document.getElementById("bat_table");
            table.innerHTML = "";

            // gespeicherte Felder
            let saved = {};
            j.fields.forEach(f => saved[f.name] = f);

            for (let i = 0; i < j.headers.length; i++) {

                let name = j.headers[i];
                let raw  = j.values[i] || "";

                // Default
                let display = name;
                let factor  = "1";
                let unit    = "";
                let mqtt    = false;
                let send    = false;

                // gespeicherte Werte überschreiben
                if (saved[name]) {
                    display = saved[name].display;
                    factor  = saved[name].factor;
                    unit    = saved[name].unit;
                    mqtt    = saved[name].sendMQTT;
                    send    = saved[name].sendPayload;
                }
                else {
                    // Autodetect
                    let n = name.toLowerCase();
                    let r = raw.toLowerCase();

                    if (r.includes("%")) {
                        factor = "1"; unit = "%";
                    }
                    else if (r.includes("mah")) {
                        factor = "0.001"; unit = "Ah";
                    }
                    else if (isNaN(parseFloat(r))) {
                        factor = "text"; unit = "";
                    }
                    else if (n.includes("volt")) {
                        factor = "0.001"; unit = "V";
                    }
                    else if (n.includes("curr")) {
                        factor = "0.001"; unit = "A";
                    }
                    else if (n.includes("temp") || n.includes("tempr")) {
                        factor = "0.001"; unit = "°C";
                    }
                }

                // Tabelle rendern
                let row = document.createElement("tr");
                row.innerHTML = `
                    <td>${name}</td>
                    <td><input id="disp_${name}" value="${display}"></td>
                    <td>${raw}</td>
                    <td>
                        <select id="fac_${name}">
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
                        <select id="unit_${name}">
                            <option value=""></option>
                            <option value="V">V</option>
                            <option value="A">A</option>
                            <option value="°C">°C</option>
                            <option value="%">%</option>
                            <option value="Ah">Ah</option>
                            <option value="timestamp">timestamp</option>
                        </select>
                    </td>
                    <td><input type="checkbox" id="mqtt_${name}"></td>
                    <td><input type="checkbox" id="send_${name}"></td>
                `;

                table.appendChild(row);

                // Werte setzen
                document.getElementById("fac_" + name).value = factor;
                document.getElementById("unit_" + name).value = unit;
                document.getElementById("mqtt_" + name).checked = mqtt;
                document.getElementById("send_" + name).checked = send;
            }
        });
}

document.addEventListener("change", function(e) {
    let target = e.target;

    if (target.id.startsWith("mqtt_")) {
        let row = target.closest("tr");
        if (!row) return;
        let sendBox = row.querySelector("[id^='send_']");
        if (!sendBox) return;
        if (!target.checked) sendBox.checked = false;
    }

    if (target.id.startsWith("send_")) {
        let row = target.closest("tr");
        if (!row) return;
        let mqttBox = row.querySelector("[id^='mqtt_']");
        if (!mqttBox) return;
        if (target.checked) mqttBox.checked = true;
    }
});


function batSave() {

    let data = {
        config: {
            intervalBat: parseInt(document.getElementById("interval_bat").value) * 1000,
            enableBat: document.getElementById("enable_bat").checked
        },
        mqtt: {
            topicBat:   document.getElementById("topic_bat").value,
            cellPrefix: document.getElementById("cell_prefix").value
        },
        fields: []
    };

    document.querySelectorAll("#bat_table tr").forEach(row => {
        let name = row.cells[0].innerText;
        let disp = sanitize(document.getElementById("disp_" + name).value);

        data.fields.push({
            name: name,
            display: disp,
            factor: document.getElementById("fac_" + name).value,
            unit:   document.getElementById("unit_" + name).value,
            sendMQTT: document.getElementById("mqtt_" + name).checked,
            sendPayload: document.getElementById("send_" + name).checked
        });
    });

    fetch("/api/bat/set", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(data)
    }).then(() => alert("Gespeichert"));
}

batLoad();
