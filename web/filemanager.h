#pragma once
#include <Arduino.h>

const char FILEMANAGER_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>File Manager</title>
<style>
body { font-family: sans-serif; padding: 20px; }
table { border-collapse: collapse; width: 100%; margin-top: 20px; }
td, th { border: 1px solid #ccc; padding: 6px; }
button { padding: 6px 12px; margin-left: 10px; }
</style>
</head>
<body>

<h2>ESP32 File Manager</h2>

<input type="file" id="file">
<button onclick="upload()">Upload</button>

<table id="tbl"></table>

<script>
function load() {
    fetch('/fm/list')
    .then(r => r.json())
    .then(list => {
        let html = "<tr><th>Name</th><th>Size</th><th>Action</th></tr>";
        list.forEach(f => {
            html += `<tr>
                <td>${f.name}</td>
                <td>${f.size}</td>
                <td>
                    <a href="/fm/download?file=${f.name}">Download</a>
                    <a href="#" onclick="del('${f.name}')">Delete</a>
                </td>
            </tr>`;
        });
        document.getElementById("tbl").innerHTML = html;
    });
}

function upload() {
    let f = document.getElementById("file").files[0];
    let fd = new FormData();
    fd.append("file", f, f.name);
    fetch("/fm/upload", { method:"POST", body:fd })
        .then(() => load());
}

function del(name) {
    fetch("/fm/delete?file=" + name)
        .then(() => load());
}

load();
</script>

</body>
</html>
)rawliteral";
