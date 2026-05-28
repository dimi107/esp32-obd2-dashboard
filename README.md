# ESP32 OBD-II Dashboard

Embedded vehicle diagnostics display for ESP32-S3 — reads 12 live OBD-II PIDs via ELM327 WiFi and renders them on a 480×272 IPS touchscreen using LVGL.

## Stack

| Layer | Technology |
|---|---|
| MCU | ESP32-S3 (dual-core, 16 MB flash, OPI PSRAM) |
| UI framework | LVGL 8.3 |
| OBD-II | ELMduino 3.4 over TCP/WiFi |
| Build system | PlatformIO / Arduino framework |
| Language | C++17 |

## Features

- Polls 12 PIDs in a fast/slow interleaved loop (RPM + speed every cycle, others rotated)
- Reads and clears stored DTCs (Mode 03/04) on demand via touch button
- Reads odometer via UDS Mode 22 (DID 0xF452) with automatic fallback DID
- Multilingual UI: English / Bulgarian, persisted to ESP32 NVS across reboots
- Per-PID sanity bounds reject garbled ELM327 responses; failed PIDs auto-skip after 10 errors

## Flash and run

**Hardware:** ESP32-S3 board (JCZN JC4827W543 or compatible), ELM327 WiFi OBD-II adapter

```bash
git clone https://github.com/dimi107/esp32-obd2-dashboard
cd esp32-obd2-dashboard

# 1. Edit src/config.h — set your ELM327 SSID/IP and fuel tank size
# 2. Connect the board via USB, then:
pio run --target upload
```

Open `dashboard_preview.html` in a browser to preview the UI without hardware.

## Project structure

```
src/
  main.cpp          — setup/loop: display init, OBD task spawn, LVGL tick
  obd_task.cpp      — FreeRTOS task: WiFi connect, PID polling, DTC/UDS state machine
  config.h          — ELM327 credentials, pin map, thresholds  ← edit before flash
  vehicle_data.h    — mutex-protected shared struct read by both cores
  dtc_table.h       — DTC code-to-description lookup table
  ui/
    display.cpp/h   — QSPI display + I2C touch driver init (NV3041A, AXS15231B)
    dashboard.cpp/h — LVGL screen layout and live value update
    language.cpp/h  — EN/BG string tables and language toggle logic
  fonts/            — custom Montserrat cyrillic bitmap fonts for LVGL
include/            — board-specific headers (lv_conf.h, ESP32 HAL overrides)
platformio.ini      — build config: esp32s3 release + debug env
dashboard_preview.html — static UI mockup, open in browser
```

## What this demonstrates

- Embedded C++17 on ESP32-S3 with FreeRTOS dual-core task separation
- OBD-II Mode 01 PID polling and UDS Mode 22 diagnostics over WiFi TCP
- Non-blocking state machine for resilient ELM327 communication
- LVGL 8.3 UI with custom cyrillic fonts and capacitive touch input
- Mutex-protected shared data structure between display and OBD cores
