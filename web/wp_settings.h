#pragma once
#include "../wp_webserver.h"
#include "../config.h"
#include "../py_scheduler.h"
#include "wp_ui.h"

// Externe Parser-Daten
extern std::vector<String> lastParserHeader;
extern std::vector<String> lastParserValues;

inline void handleBatterySettingsPage() {

    // Sprache
    String lang = server.hasArg("lang") ? server.arg("lang") : "de";
    bool DE = (lang == "de");

    // Texte
    String T_title      = DE ? "Batterie Einstellungen (PWR)" : "Battery Settings (PWR)";
    String T_intervals  = DE ? "Abfrageintervalle (Sekunden)" : "Polling Intervals (Seconds)";
    String T_cmd        = DE ? "Befehle aktivieren" : "Enable Commands";
    String T_save       = DE ? "Speichern" : "Save";
    String T_back       = DE ? "Zurück" : "Back";
    String T_fahrenheit = DE ? "Temperatur in Fahrenheit senden" : "Send temperature in Fahrenheit";

    String T_mqtt_box   = DE ? "MQTT Themen" : "MQTT Topics";
    String T_mqtt_prefix= DE ? "MQTT Präfix" : "MQTT Prefix";
    String T_mqtt_stack = DE ? "Stack Subtopic" : "Stack Subtopic";
    String T_mqtt_pwr   = DE ? "PWR Subtopic" : "PWR Subtopic";
    String T_mqtt_bat   = DE ? "BAT Subtopic" : "BAT Subtopic";
    String T_mqtt_stat  = DE ? "STAT Subtopic" : "STAT Subtopic";

    String T_col_field  = DE ? "Feldname" : "Field Name";
    String T_col_label  = DE ? "Anzeigename" : "Display Name";
    String T_col_raw    = DE ? "Rohdaten" : "Raw Value";
    String T_col_factor = DE ? "Faktor / Teiler" : "Factor / Divider";
    String T_col_unit   = DE ? "Einheit" : "Unit";
    String T_col_calc   = DE ? "Wert" : "Value";
    String T_col_mqtt   = DE ? "MQTT" : "MQTT";
    String T_col_send   = DE ? "Aktiv senden" : "Active Send";

    // ============================
    // Default-Erkennung für neue Felder (PWR)
    // ============================
    for (size_t i = 0; i < lastParserHeader.size(); i++) {

        String field = lastParserHeader[i];
        String raw   = (i < lastParserValues.size()) ? lastParserValues[i] : "";

        // Wenn Feld schon existiert → überspringen
        if (config.battery.fields.count(field)) continue;

        FieldConfig f;
        f.label = field;

        // 6. SOC / Coulomb (muss VOR Text-Erkennung kommen)
        if (field == "SOC" || field == "Coulomb") {
            f.factor = "1";
            f.unit   = "%";
            f.mqtt   = true;
            f.send   = true;
            config.battery.fields[field] = f;
            continue;
        }

        // 1. TEXT-ERKENNUNG (raw NICHT numerisch)
        bool isNumber = true;
        for (size_t c = 0; c < raw.length(); c++) {
            char ch = raw[c];
            if (!isdigit(ch) && ch != '.' && ch != '-' && ch != '+') {
                isNumber = false;
                break;
            }
        }

        if (!isNumber) {
            f.factor = "text";
            f.unit   = "text";
            f.mqtt   = false;
            f.send   = false;
            config.battery.fields[field] = f;
            continue;
        }

        // 2. FELDNAME ENTHÄLT "Time"
        if (field.indexOf("Time") >= 0) {
            f.factor = "text";
            f.unit   = "text";
            f.mqtt   = false;
            f.send   = false;
            config.battery.fields[field] = f;
            continue;
        }

        // 3. FELD ENDET AUF "Id"
        if (field.endsWith("Id")) {
            f.factor = "1";
            f.unit   = "";
            f.mqtt   = false;
            f.send   = false;
            config.battery.fields[field] = f;
            continue;
        }

        // 4. FELD BEGINNT MIT "V" ODER ENTHÄLT "Volt"
        if (field.startsWith("V") || field.indexOf("Volt") >= 0) {
            f.factor = "0.001";
            f.unit   = "V";
            f.mqtt   = false;
            f.send   = false;
            config.battery.fields[field] = f;
            continue;
        }

        // 5. FELD BEGINNT MIT "T" ODER ENTHÄLT "Temp"
        if (field.startsWith("T") || field.indexOf("Temp") >= 0) {
            f.factor = "0.001";
            f.unit   = "°C";
            f.mqtt   = false;
            f.send   = false;
            config.battery.fields[field] = f;
            continue;
        }

        // 7. Curr / Cxxx (aber nicht Coulomb)
        if (field.startsWith("C")) {
            f.factor = "0.001";
            f.unit   = "A";
            f.mqtt   = false;
            f.send   = false;
            config.battery.fields[field] = f;
            continue;
        }

        // 8. Stack-Felder
        if (field == "StackVoltAvg") {
            f.factor = "0.001";
            f.unit   = "V";
            f.mqtt   = true;
            f.send   = true;
            config.battery.fields[field] = f;
            continue;
        }

        if (field == "StackCurrSum") {
            f.factor = "0.001";
            f.unit   = "A";
            f.mqtt   = true;
            f.send   = true;
            config.battery.fields[field] = f;
            continue;
        }

        if (field == "BatteryCount") {
            f.factor = "1";
            f.unit   = "";
            f.mqtt   = true;
            f.send   = true;
            config.battery.fields[field] = f;
            continue;
        }

        // 9. STANDARD-DEFAULT
        f.factor = "1";
        f.unit   = "";
        f.mqtt   = false;
        f.send   = false;

        config.battery.fields[field] = f;
    }

    // HTML Start
    String page;
    page.reserve(24000);

    page += "<!doctype html><html><head><meta charset='utf-8'><title>";
    page += T_title;
    page += "</title>";

    // Sidebar
    page += webSidebar("battery-settings");

    // CSS
    page += R"rawliteral(
<style>
h1{
  font-size:28px;
  margin-bottom:10px;
  display:flex;
  justify-content:space-between;
  align-items:center;
}
.lang-switch a{
  margin-left:8px;
  text-decoration:none;
  color:#0066cc;
}
.box{
  border:1px solid #ccc;
  padding:15px;
  margin-top:20px;
  border-radius:8px;
  background:#f9f9f9;
}
table{
  width:100%;
  border-collapse:collapse;
  margin-top:20px;
}
table th, table td{
  border:1px solid #ccc;
  padding:8px;
  text-align:left;
}
input[type=number], input[type=text], select{
  width:120px;
  padding:6px;
}
input[type=checkbox]{
  transform:scale(1.3);
}
button{
  padding:12px 24px;
  font-size:16px;
  margin-top:20px;
  border:none;
  border-radius:8px;
  background:#3498db;
  color:white;
  cursor:pointer;
}
</style>

<script>
function syncCheckboxes(row){
    let mqtt = document.getElementById("mqtt_" + row);
    let send = document.getElementById("send_" + row);
    if(send.checked){ mqtt.checked = true; }
    if(!mqtt.checked){ send.checked = false; }
}
</script>
</head><body><div class='content'>
)rawliteral";

    // Titel + Sprachauswahl
    page += "<h1>";
    page += T_title;
    page += "<span class='lang-switch'>";
    page += "<a href='?lang=de'>DE</a>";
    page += "<a href='?lang=en'>EN</a>";
    page += "</span></h1>";

    // FORM START
    page += "<form action='/battery-settings-save' method='post'>";

    // ============================
    // Abfrageintervalle (Sekunden)
    // ============================
    page += "<div class='box'><h3>" + T_intervals + "</h3>";

    page += "<label>PWR</label> <input type='number' name='pwr' value='" + String(config.battery.intervalPwr / 1000) + "'> s ";
    page += "<label>BAT</label> <input type='number' name='bat' value='" + String(config.battery.intervalBat / 1000) + "'> s ";
    page += "<label>STAT</label> <input type='number' name='stat' value='" + String(config.battery.intervalStat / 1000) + "'> s ";

    page += "</div>";

    // ============================
    // Temperatur / Fahrenheit
    // ============================
    page += "<div class='box'><h3>Temperatur</h3>";
    page += "<label><input type='checkbox' name='use_fahrenheit'";
    if (config.battery.useFahrenheit) page += " checked";
    page += "> " + T_fahrenheit + "</label>";
    page += "</div>";

    // ============================
    // MQTT Subtopics
    // ============================
    page += "<div class='box'><h3>" + T_mqtt_box + "</h3>";

    page += "<div><label>" + T_mqtt_prefix + "</label> ";
    page += "<input type='text' name='mqtt_prefix' value='" + config.mqtt.prefix + "'></div>";

    page += "<div><label>" + T_mqtt_stack + "</label> ";
    page += "<input type='text' name='mqtt_t_stack' value='" + config.mqtt.topicStack + "'></div>";

    page += "<div><label>" + T_mqtt_pwr + "</label> ";
    page += "<input type='text' name='mqtt_t_pwr' value='" + config.mqtt.topicPwr + "'></div>";

    page += "<div><label>" + T_mqtt_bat + "</label> ";
    page += "<input type='text' name='mqtt_t_bat' value='" + config.mqtt.topicBat + "'></div>";

    page += "<div><label>" + T_mqtt_stat + "</label> ";
    page += "<input type='text' name='mqtt_t_stat' value='" + config.mqtt.topicStat + "'></div>";

    page += "</div>";

    // ============================
    // Parser Fields (PWR)
    // ============================
    page += "<div class='box'><h3>Parser Fields (PWR)</h3>";

    page += "<table><tr>";
    page += "<th>" + T_col_field  + "</th>";
    page += "<th>" + T_col_label  + "</th>";
    page += "<th>" + T_col_raw    + "</th>";
    page += "<th>" + T_col_factor + "</th>";
    page += "<th>" + T_col_unit   + "</th>";
    page += "<th>" + T_col_calc   + "</th>";
    page += "<th>" + T_col_mqtt   + "</th>";
    page += "<th>" + T_col_send   + "</th>";
    page += "</tr>";

    for (size_t i = 0; i < lastParserHeader.size(); i++) {
        String field = lastParserHeader[i];
        String raw   = (i < lastParserValues.size()) ? lastParserValues[i] : "";

        FieldConfig &fc = config.battery.fields[field];

        page += "<tr>";

        page += "<td>" + field + "</td>";
        page += "<td><input type='text' name='label_" + field + "' value='" + fc.label + "'></td>";
        page += "<td>" + raw + "</td>";

        // Faktor Dropdown
        page += "<td><select name='factor_" + field + "'>";
        const char* factors[] = {"1","0.1","0.01","0.001","0.0001","%","text"};
        for (auto f : factors) {
            page += "<option value='";
            page += f;
            page += "'";
            if (fc.factor == f) page += " selected";
            page += ">";
            page += f;
            page += "</option>";
        }
        page += "</select></td>";

        // Einheit Dropdown
        page += "<td><select name='unit_" + field + "'>";
        const char* units[] = {"","V","A","°C","°F","%","text"};
        const char* unitLabels[] = {"-","V","A","°C","°F","%","Text"};
        for (int u = 0; u < 7; u++) {
            page += "<option value='";
            page += units[u];
            page += "'";
            if (fc.unit == units[u]) page += " selected";
            page += ">";
            page += unitLabels[u];
            page += "</option>";
        }
        page += "</select></td>";

        // Wert-Spalte: aktuell noch Rohwert (später ggf. lastMqttValues)
        page += "<td>" + raw + "</td>";

        page += "<td><input type='checkbox' id='mqtt_" + field + "' name='mqtt_" + field + "'";
        if (fc.mqtt) page += " checked";
        page += " onchange='syncCheckboxes(\"" + field + "\")'></td>";

        page += "<td><input type='checkbox' id='send_" + field + "' name='send_" + field + "'";
        if (fc.send) page += " checked";
        page += " onchange='syncCheckboxes(\"" + field + "\")'></td>";

        page += "</tr>";
    }

    page += "</table></div>";

    // Befehle aktivieren
    page += "<div class='box'><h3>" + T_cmd + "</h3>";
    page += String("<label><input type='checkbox' name='en_bat' ")
         + (config.battery.enableBat ? "checked" : "")
         + "> BAT</label><br>";
    page += String("<label><input type='checkbox' name='en_stat' ")
         + (config.battery.enableStat ? "checked" : "")
         + "> STAT</label>";
    page += "</div>";

    // Save Button
    page += "<button type='submit'>" + T_save + "</button>";

    page += "</form>";

    page += "<a href='/' style='display:block;margin-top:20px;'>" + T_back + "</a>";

    page += "</div></body></html>";

    server.send(200, "text/html", page);
}

static void handleBatterySettingsSave() {

    // 1. Abfrageintervalle (Sekunden → Millisekunden)
    if (server.hasArg("pwr"))
        config.battery.intervalPwr = server.arg("pwr").toInt() * 1000;

    if (server.hasArg("bat"))
        config.battery.intervalBat = server.arg("bat").toInt() * 1000;

    if (server.hasArg("stat"))
        config.battery.intervalStat = server.arg("stat").toInt() * 1000;

    // 2. Fahrenheit-Option
    config.battery.useFahrenheit = server.hasArg("use_fahrenheit");

    // 3. MQTT-Subtopics + Prefix
    if (server.hasArg("mqtt_prefix"))
        config.mqtt.prefix = server.arg("mqtt_prefix");

    if (server.hasArg("mqtt_t_stack"))
        config.mqtt.topicStack = server.arg("mqtt_t_stack");

    if (server.hasArg("mqtt_t_pwr"))
        config.mqtt.topicPwr = server.arg("mqtt_t_pwr");

    if (server.hasArg("mqtt_t_bat"))
        config.mqtt.topicBat = server.arg("mqtt_t_bat");

    if (server.hasArg("mqtt_t_stat"))
        config.mqtt.topicStat = server.arg("mqtt_t_stat");

    // 4. Befehle aktivieren
    config.battery.enableBat  = server.hasArg("en_bat");
    config.battery.enableStat = server.hasArg("en_stat");

    // 5. Parser-Felder in Config schreiben
    for (auto &field : lastParserHeader) {

        FieldConfig &fc = config.battery.fields[field];

        // Label
        String labelKey = "label_" + field;
        if (server.hasArg(labelKey)) {
            fc.label = server.arg(labelKey);
        }

        // Faktor
        String factorKey = "factor_" + field;
        if (server.hasArg(factorKey)) {
            fc.factor = server.arg(factorKey);
        }

        // Einheit
        String unitKey = "unit_" + field;
        if (server.hasArg(unitKey)) {
            fc.unit = server.arg(unitKey);
        }

        // MQTT aktivieren
        String mqttKey = "mqtt_" + field;
        fc.mqtt = server.hasArg(mqttKey);

        // Aktiv senden
        String sendKey = "send_" + field;
        fc.send = server.hasArg(sendKey);
    }

    // 6. Scheduler aktualisieren
    py_scheduler.setIntervals(
        config.battery.intervalPwr,
        config.battery.intervalBat,
        config.battery.intervalStat
    );

    // 7. Config speichern
    config.save();

    // 8. Redirect
    server.sendHeader("Location", "/battery-settings");
    server.send(302, "text/plain", "Saved");
}