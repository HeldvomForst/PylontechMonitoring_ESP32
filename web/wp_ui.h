#pragma once
#include <Arduino.h>

inline String webSidebar(const String& active) {
    String html = R"rawliteral(
<style>
.sidebar {
    position: fixed;
    top: 0;
    left: 0;
    width: 180px;
    height: 100%;
    background-color: #2c3e50;
    padding-top: 20px;
    z-index: 10;
}
.sidebar a {
    display: block;
    color: #ecf0f1;
    padding: 12px 20px;
    text-decoration: none;
    font-size: 16px;
}
.sidebar a:hover {
    background-color: #34495e;
}
.sidebar a.active {
    background-color: #1abc9c;
    color: #ffffff;
    font-weight: bold;
}
.content {
    margin-left: 200px;
    padding: 20px;
}
</style>

<div class="sidebar">
)rawliteral";

    auto link = [&](const String& url, const String& label, const String& key) {
        html += "<a href='" + url + "' class='";
        if (active == key) html += "active";
        html += "'>" + label + "</a>";
    };

    link("/",            "Startseite",      "home");
    link("/pwr_setting", "Basiswerte",      "pwr_setting");
    link("/battery",     "Battery Console", "battery");
    link("/connect",     "Connection",      "connect");
    link("/log",         "Runtime Log",     "log");

    html += "</div>";
    return html;
}