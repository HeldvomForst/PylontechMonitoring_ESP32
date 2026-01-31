# Pylontech Battery Monitoring via WiFi
Forked from hidaba/Pylontech-Battery-Monitoring

This project allows you to control and monitor Pylontech US2000B, US2000C, US3000C, US5000, UP2500 batteries via console port over WiFi.
It it's a great starting point to integrate battery with your home automation. (HomeAssitant; MQTT)

**I take no responsibility for any damage that may occur. Use at your own risk.**

# Features:
  * Low cost (around 20$ in total).
  * Adds WiFi capability to your Pylontech US2000B/C , US3000C, US5000 battery.
  * Device exposes web interface that allows to:
    * send console commands and read response over WiFi (no PC needed)
  * MQTT support:
    * device pushes selekted data like SOC, temperature, state, etc to selected MQTT server
  * Easy to configure own WIFI-AP to configuar all setting
  * ArduinoIDE only needed to flash 
  * choose dhcp or static ip

# Parts needed and schematics:
  * [ESP32 microcontroller][https://www.az-delivery.de/en/products/esp32-dev-kitc-v4-ai-thinker-soldered?srsltid=AfmBOopvIYP1HSIimEMsP3LB3BwiXQjazw4n77lb6tjV5jSBTEpTrt2V].
  * [SparkFun MAX3232 Transceiver](https://www.sparkfun.com/products/11189).
  * US2000B: Cable with RJ10 connector (some RJ10 cables have only two wires, make sure to buy one that has all four wires present).
  * US2000C or US5000: Cable with RJ45 connector (see below for more details).
  * Capacitors C1: 10uF, C2: 0.1uF (this is not strictly required, but recommended as Wemos D1 can have large current spikes).

![Schematics](Schemetics.png)

# US2000C/US3000C/US5000 notes:
This battery uses RJ45 cable instead of RJ10. Schematics is the same only plug differs:
  * RJ45 Pin 3 (white-green) = R1IN
  * RJ45 Pin 6 (green)       = T1OUT
  * RJ45 Pin 8 (brown)       = GND
![image](https://user-images.githubusercontent.com/19826327/146428324-29e3f9bf-6cc3-415c-9d60-fa5ee3d65613.png)


# How to get going:
  * Get ESP32
  * Install arduino IDE and ESP32 libraries as 
  * Open [PylontechMonitoring.ino](PylontechMonitoring.ino) in arduino IDE
  * Make sure to copy content of [libraries subdirectory](libraries) to [libraries of your Arduino IDE](https://forum.arduino.cc/index.php?topic=88380.0).
  * Upload project to your device
  * Connect ESP32 to the MAX3232 transreceiver
  * Connect transreceiver to RJ10/RJ45 as descibed in the schematics (all three lines need to be connected)
  * Connect RJ10/RJ45 to the serial port of the Pylontech US2000 battery. If you have multiple batteries - connect to the master one.
  * Connect Wemos D1 to the power via USB
  * Connect to the AP pylon_xxxx
  * Enter IP 192.168.4.1
  * Go to connection page
  * Scan for WIFI
  * Select your WIFI and enter password (after connecting your WIFI you see the IP on the Main-page, 90s after connecting to your WIFI the AP shut down) 
  * Configure your MQTT settings
  * Go to Basiswerte and select your data
    * MQTT the data will be published
    * send you will see sensor on HomeAssitant in the MQTT Intergration
  * default Values are configurated
  * For Stack only 4 Values are supported (Battery counts; current (sum); Temperatur (Max) Volatage (avg)) and hard coded 

# Pylontech Battery Monitor

This project uses an **ESP32** to read data from a Pylontech battery (via Serial) and publish it to an MQTT broker.  
It also supports an internal web interface and (optionally) OTA updates.

---

## Configuration Parameters


## Home Assistant Energy Integration

If you want to display **kWh** (cumulative energy) in Home Assistant’s Energy dashboard, you need sensors of type “energy” instead of “power.” By default, this firmware publishes instantaneous power in watts (`W`), which Home Assistant does not directly include in the Energy panel. To solve this, you can use Home Assistant’s **integration** platform to convert power (W) into energy (kWh).

Below is an example of how to create two custom sensors—one for battery **in** energy (charging) and one for battery **out** energy (discharging)—which you can then select in Home Assistant’s Energy configuration.

### Example YAML Configuration

```yaml
sensor:
  - platform: integration
    source: sensor.pylontechbattery_pylontech_power_in
    name: "Pylontech Battery Energy In"
    unit_prefix: k
    round: 2
    method: left

  - platform: integration
    source: sensor.pylontechbattery_pylontech_power_out
    name: "Pylontech Battery Energy Out"
    unit_prefix: k
    round: 2
    method: left
```

> **Notes**:
> - **`source`** must match the *power sensor* name in watts that the firmware publishes, as seen in Home Assistant’s Developer Tools → States (e.g., `sensor.pylontechbattery_pylontech_power_in`).
> - **`unit_prefix: k`** converts watt-hours (Wh) to kilowatt-hours (kWh).
> - **`round: 2`** sets the number of decimal places.
> - **`method: left`** is one of the integration methods (other options: `trapezoidal`, etc.).

### Using in the Energy Dashboard

1. **Restart** Home Assistant after adding the above configuration.  
2. Go to **Settings → Devices & Services** or directly **Settings → Energy** (depending on your HA version).  
3. In **Settings → Energy**, add a new energy source or battery entry:
   - Select the newly created sensor, e.g., `sensor.pylontech_battery_energy_in` or `sensor.pylontech_battery_energy_out`.
4. Home Assistant will begin accumulating data for these sensors in kilowatt-hours over time. It may take some minutes or hours to populate graphs and statistics in the Energy dashboard.

With this approach, you keep your firmware simple (publishing power in watts) and let Home Assistant handle the accumulation into kWh. Once you have these “Energy” sensors, you can fully utilize Home Assistant’s Energy dashboard to monitor charging and discharging over time.

