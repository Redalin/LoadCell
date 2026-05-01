# Drone Scale

A weighing system for drone racing start lines, used to verify drones meet the
minimum weight requirements for Spec racing. Built around an ESP32 with HX711
load cell amplifiers.

## Overview

The system runs as a mesh of up to five ESP32 nodes:

- **1 parent** (device ID `0`, hostname `LaunchScale`) hosts the WiFi
  connection, mDNS, async web UI (with WebSocket + ElegantOTA), and lane-pit
  buttons. The parent does not have a load cell attached.
- **Up to 4 children** (device IDs `1`–`4`, named `Yellow`, `Grey`, `Purple`,
  `Black`) - each reads its own HX711 load cell and reports weight, battery
  voltage, and firmware version to the parent over ESP-NOW.

Nodes communicate via ESP-NOW on channel `6`. This channel is configurable in [include/config.h](include/config.h)
Children send a buffered weight sample to the parent every 2s. 
The parent broadcasts data to connected web clients in real time.

## Hardware

- ESP32 dev board (`esp32dev`)
- HX711 load cell amplifier + load cell (children only)
- SSD1306 128×32 I²C OLED (all nodes)
- Tare / config push button (all nodes, button to ground)
- Battery voltage divider on each node (see below)
- Parent only: 4 lane-switch inputs to ground

## Pin assignments

These are defined in [include/config.h](include/config.h) and
[src/nodeConfig.cpp](src/nodeConfig.cpp).

| Function                | Pin    | Notes                                  |
| ----------------------- | ------ | -------------------------------------- |
| HX711 `DOUT` (child)    | GPIO16 | `LOADCELL_DOUT_PIN`                    |
| HX711 `SCK` (child)     | GPIO17 | `LOADCELL_SCK_PIN`                     |
| Tare button (parent)    | GPIO14 | `INPUT_PULLUP`, switch to GND          |
| Tare button (child)     | GPIO15 | `INPUT_PULLUP`, switch to GND          |
| Battery sense (VBAT)    | GPIO36 | ADC1_CH0 / `VP`, via voltage divider   |
| OLED SDA / SCL          | default I²C | SSD1306 at address `0x3C`         |
| Lane switches (parent)  | GPIO16, 17, 18, 19 | `INPUT_PULLUP`, switch to GND |

Pins `16` and `17` are reused: on a child they drive the HX711, on the parent
they read the first two lane switches. The same firmware behaves correctly for
both because the role is selected by device ID at runtime.

## Battery voltage divider

The ADC on `GPIO36` cannot read a full LiPo voltage directly, so each node
needs a resistor divider between the battery `+` rail and `VBAT_PIN`, with the
bottom of the divider tied to GND.

```
   VBAT (+) ──[ R1 ]──┬──[ R2 ]── GND
                      │
                      └── GPIO36 (VBAT_PIN)
```

Defaults assume **R1 = 20 kΩ** (top) and **R2 = 4.7 kΩ** (bottom), giving a
divide ratio of `(R1 + R2) / R2 ≈ 5.255`, which keeps a 3S LiPo (~12.6 V max)
comfortably under the ADC's input limit. If you use different resistors,
update `VBAT_DIVIDER_R1` and `VBAT_DIVIDER_R2` in
[include/config.h](include/config.h) — the firmware uses the calibrated
`analogReadMilliVolts()` and the divider ratio to recover the actual battery
voltage.

The reading is averaged (16 samples) and refreshed every 10 s.

## Secrets

WiFi credentials and the parent's MAC address live in `include/secrets.h`,
which is **gitignored**. Use the committed template to create your own:

```bash
cp include/secrets.h.example include/secrets.h
```

Then edit [include/secrets.h](include/secrets.h.example) to set:

- `KNOWN_SSID[]` / `KNOWN_PASSWORD[]` — networks the parent will try to join.
- `APNAME` / `APPASS` — fallback access point credentials if no known
  network is found.
- `PARENT_MAC_ADDR` — the parent ESP32's MAC address, used by children to
  target ESP-NOW transmissions. Print the parent's MAC by flashing it once
  and reading the serial monitor (`[DEFAULT] ESP32 Board MAC Address: ...`),
  then convert it to the `{0xa0, 0xb1, 0xc2, 0xd3, 0xe4, 0xf5}` form.

## Assigning a node ID

Device ID is stored in NVS preferences and selected at boot:

1. Hold the tare button while powering the node on.
2. The OLED shows `Config: ID 1`.
3. Each subsequent press cycles the ID through `1` → `2` → `3` → `4` → `1`.
4. After 10 s the ID is saved and the node reboots.

ID `0` (parent) cannot be assigned this way — it is the default for an
unconfigured node and can also be set via the `/nodeid?id=0` endpoint on the
parent's web UI. Per-ID names and HX711 calibration factors are tabled in
[src/nodeConfig.cpp](src/nodeConfig.cpp).

## Build & upload

This is a PlatformIO project. Two environments are defined in
[platformio.ini](platformio.ini):

- `esp32dev` — real-hardware build with WiFi, ESP-NOW, HX711, LittleFS,
  ElegantOTA. This is the default.
- `wokwi` — stripped-down build (only `src/main_wokwi.cpp`) for the online
  Wokwi simulator. Uploads to physical hardware are blocked by
  [scripts/block_wokwi_upload.py](scripts/block_wokwi_upload.py).

```bash
pio run                       # build the default env
pio run -t upload             # flash the ESP32
pio run -t uploadfs           # flash the LittleFS web assets
pio run -e wokwi              # build the Wokwi preview
```

OTA updates after the first flash are available on the parent at
`http://<hostname>.local/update`.
