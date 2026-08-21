# Overhead Water Tank Monitoring — Project Context

## Project Goal

Build a home-use overhead water tank monitoring system using:

- ESP32-WROOM development board
- ESP-IDF
- Cursor as the development environment
- Home Wi-Fi
- Blynk Cloud / Blynk mobile app
- JSN-SR04T waterproof ultrasonic sensor to be added later

The current development strategy is to complete the cloud/mobile frontend first using simulated tank-level values, and only then integrate the ultrasonic sensor.

---

## Current System Architecture

### Current Development Phase

```text
ESP32-WROOM
    |
    | Wi-Fi
    v
Home Router
    |
    | Internet / HTTPS
    v
Blynk Cloud
    |
    v
Blynk Mobile App
```

### Final Intended Architecture

```text
JSN-SR04T Ultrasonic Sensor
            |
            v
       ESP32-WROOM
            |
            | Wi-Fi
            v
       Home Router
            |
            v
       Blynk Cloud
            |
            v
       Mobile App
```

The ESP32 will eventually calculate:

- Distance from sensor to water surface
- Water level percentage
- Approximate litres available
- Low-level and full-tank status
- Optional pump state/control

---

# Development Environment

## Host PC

- Ubuntu Linux
- Cursor IDE
- ESP-IDF extension/environment
- ESP32 connected over `/dev/ttyUSB0`

## ESP32 Target

```text
esp32
```

The board is an ESP32-WROOM development board.

## Serial Port

```text
/dev/ttyUSB0
```

The Linux user has already been added to the `dialout` group.

Typical flashing command:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Do not use `sudo` with `idf.py`.

---

# Completed Milestones

## Milestone 1 — ESP-IDF Project / Flashing

COMPLETED.

The ESP-IDF project builds and flashes successfully from Cursor.

The basic application prints:

```text
Water Tank Monitor Starting!
```

Project target:

```bash
idf.py set-target esp32
```

Typical commands:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Milestone 2 — Wi-Fi Connectivity

COMPLETED.

The ESP32 successfully connects to the home Wi-Fi network using ESP-IDF native Wi-Fi APIs.

Observed connection example:

```text
Connected. IP Address: 192.168.0.103
Connected to SSID: Aziz
Wi-Fi Connected Successfully!
```

Observed RSSI was approximately:

```text
-48 dBm
```

Wi-Fi is therefore working correctly.

The project already has or should preserve a Wi-Fi module organized as:

```text
main/
├── wifi.c
└── wifi.h
```

The Wi-Fi implementation uses:

- `esp_wifi`
- `esp_event`
- `esp_netif`
- `nvs_flash`
- FreeRTOS Event Groups

`wifi_init_sta()` should block until either:

- Wi-Fi connects successfully, or
- maximum retry attempts are reached.

---

## Milestone 3 — Blynk Cloud

PARTIALLY COMPLETED.

The Blynk cloud configuration is already working from the Ubuntu PC.

### Blynk Setup

Template:

```text
Overhead Water Tank
```

Hardware:

```text
ESP32
```

Connection:

```text
WiFi
```

### Existing Blynk Datastream

```text
Name: Tank Level
Virtual Pin: V0
Data Type: Integer
Minimum: 0
Maximum: 100
Unit: %
```

### Cloud API Test

The following request has already been tested successfully:

```bash
curl "https://blynk.cloud/external/api/update?token=YOUR_AUTH_TOKEN&v0=50"
```

Changing values through curl updates the Blynk datastream correctly.

Therefore this path is already proven:

```text
Ubuntu PC
    |
    | HTTPS
    v
Blynk Cloud
    |
    v
V0 Tank Level
```

The Blynk Auth Token is private and MUST NOT be committed to Git.

---

# Current Development Task

The next task is to make the ESP32 perform the same Blynk HTTPS update that was successfully tested using `curl`.

The ESP32 should initially use SIMULATED tank-level values.

Expected behavior:

```text
10%
20%
30%
40%
50%
60%
70%
80%
90%
100%
10%
...
```

Update interval:

```text
5 seconds
```

No ultrasonic sensor should be integrated yet.

---

# Required Source Structure

Create or maintain the following structure:

```text
Water_Tank_Monitoring/
├── CMakeLists.txt
├── sdkconfig
└── main/
    ├── CMakeLists.txt
    ├── main.c
    ├── wifi.c
    ├── wifi.h
    ├── blynk.c
    └── blynk.h
```

Keep modules separated. Do not place the whole application in `main.c`.

---

# Required Firmware Behavior

## `main.c`

Responsibilities:

1. Print startup message.
2. Initialize NVS.
3. Connect to Wi-Fi using `wifi_init_sta()`.
4. Start simulated tank-level updates.
5. Send tank-level values to Blynk every 5 seconds.

Suggested logic:

```c
int tank_level = 10;

while (1)
{
    blynk_send_tank_level(tank_level);

    tank_level += 10;

    if (tank_level > 100)
    {
        tank_level = 10;
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
}
```

---

# Blynk Module Requirements

Create:

```text
blynk.c
blynk.h
```

Expose:

```c
esp_err_t blynk_send_tank_level(int level);
```

Use native ESP-IDF APIs.

Do NOT use the Arduino Blynk library.

Use:

```text
esp_http_client
```

The HTTP request should be equivalent to:

```text
GET https://blynk.cloud/external/api/update?token=<TOKEN>&v0=<LEVEL>
```

Example:

```text
https://blynk.cloud/external/api/update?token=TOKEN&v0=50
```

Log:

- tank level being sent
- HTTP status code
- HTTP/HTTPS errors

Expected successful log example:

```text
BLYNK: Tank level 50% sent, HTTP status = 200
```

---

# Credentials

Do not hard-code real credentials in committed source files.

Credentials that must eventually be handled properly:

- Wi-Fi SSID
- Wi-Fi password
- Blynk Auth Token

For initial local development, placeholders may be used:

```c
#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define BLYNK_TOKEN     "YOUR_BLYNK_AUTH_TOKEN"
```

Prefer moving these later to either:

- `menuconfig`
- a non-committed secrets header
- NVS provisioning

Never place real credentials in this README.

---

# `main/CMakeLists.txt`

The component should include at least:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "wifi.c"
        "blynk.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_client
)
```

Adjust dependencies if required by the installed ESP-IDF version.

---

# HTTPS / TLS Requirement

Blynk uses HTTPS.

The implementation must use proper certificate verification.

Do not intentionally disable TLS certificate verification for the final implementation.

Depending on ESP-IDF version, use an appropriate ESP-IDF certificate bundle mechanism such as:

```c
.crt_bundle_attach = esp_crt_bundle_attach
```

and include the required component dependency if necessary.

If the current ESP-IDF version requires menuconfig configuration for certificate bundles, configure it accordingly.

---

# Error Handling Requirements

The application should clearly log failures for:

- Wi-Fi connection failure
- DNS failure
- TLS/SSL failure
- HTTP connection failure
- non-200 HTTP status
- Blynk request failure

Do not silently ignore errors.

---

# Coding Style

Keep the firmware simple and modular.

Preferred organization:

```text
main.c       -> application flow
wifi.c       -> Wi-Fi connectivity
blynk.c      -> Blynk/cloud communication
```

Later:

```text
tank.c       -> tank calculations
ultrasonic.c -> JSN-SR04T interface
```

Avoid unnecessary frameworks or Arduino compatibility layers.

Use native ESP-IDF APIs.

---

# Future Milestones

## Milestone 4 — Ultrasonic Sensor

After Blynk communication is proven from the ESP32, integrate the JSN-SR04T.

The JSN-SR04T probe will be mounted above the water surface.

The sensor provides distance to the water surface.

Basic calculation:

```text
water_height = usable_tank_height - measured_distance

tank_percentage =
    water_height / usable_tank_height * 100
```

For a normal vertical cylindrical overhead tank, water-height percentage is approximately volume percentage.

Filtering should later be added to reduce unstable ultrasonic readings.

---

## Future Blynk Datastreams

Planned:

| Virtual Pin | Parameter | Unit |
|---|---|---|
| V0 | Tank Level | % |
| V1 | Water Available | L |
| V2 | Sensor Distance | mm |
| V3 | Pump Status | ON/OFF |
| V4 | Wi-Fi RSSI | dBm |

Only V0 is required right now.

---

# Important Design Context

This is a HOME overhead water tank project, not an industrial tank-monitoring system.

Priorities are:

- low cost
- simplicity
- good reliability
- easy mobile monitoring
- native ESP-IDF firmware
- Wi-Fi connectivity
- no industrial 4–20 mA interfaces
- no cellular modem at this stage

The ESP32 will be located inside the house where Wi-Fi coverage is reliable.

The tank is approximately two floors above the Wi-Fi router, which is why the ESP32 is not intended to be mounted beside the tank in the final architecture.

The sensor-side electrical architecture will be decided later.

---

# Instructions for Cursor Agent

Using the above context:

1. Inspect the existing project before modifying files.
2. Preserve the currently working Wi-Fi implementation unless a change is required.
3. Create `blynk.c` and `blynk.h`.
4. Modify `main.c` to send simulated tank levels from 10–100%.
5. Update `main/CMakeLists.txt`.
6. Use native ESP-IDF `esp_http_client`.
7. Use HTTPS with proper certificate verification.
8. Add useful ESP-IDF logging.
9. Do not integrate the JSN-SR04T yet.
10. Do not add Arduino dependencies.
11. Do not expose or commit real Wi-Fi/Blynk credentials.
12. Build the project and fix all compilation errors.
13. The final target is:

```text
ESP32 -> Wi-Fi -> HTTPS -> Blynk V0 -> Mobile Gauge
```

14. Expected test sequence on the Blynk gauge:

```text
10 -> 20 -> 30 -> 40 -> 50 -> 60 -> 70 -> 80 -> 90 -> 100 -> repeat
```

with approximately 5 seconds between updates.
