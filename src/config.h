#pragma once

// ================================================================
//  ELM327 WiFi Configuration
//  The ELM327 acts as a WiFi access point.
//  Typical defaults are shown – check the label on your device.
// ================================================================
#define ELM327_WIFI_SSID    "WiFi_OBDII"   // AP SSID of your ELM327
#define ELM327_WIFI_PASS    ""             // Leave empty if open
#define ELM327_IP           "192.168.0.10" // ELM327 device IP
#define ELM327_PORT         35000          // TCP port
#define ELM327_TIMEOUT_MS   800

// ================================================================
//  Display Dimensions & Layout
//  Board: Sparkle IoT XH-S3E / JCZN JC4827W543 (480×272 IPS)
// ================================================================
#define DISP_WIDTH    480
#define DISP_HEIGHT   320
#define TAB_BAR_H      36   // LVGL tabview header height

// ================================================================
//  Pin Configuration – JCZN JC4827W543 (NV3041A, QSPI bus)
// ================================================================

// QSPI display bus
#define DISP_SCLK    47
#define DISP_IO0     21
#define DISP_IO1     48
#define DISP_IO2     40
#define DISP_IO3     39
#define DISP_CS      45
#define DISP_RST     -1
#define DISP_BL       1    // Backlight, active HIGH

// Capacitive touch (AXS15231B, I2C 0x3B) – SDA=4, SCL=8, INT=3, no RST
#define TOUCH_SDA     4
#define TOUCH_SCL     8
#define TOUCH_INT     3

// ================================================================
//  Fuel Tank Capacity  ← set to your car's tank size in litres
// ================================================================
#define FUEL_TANK_L  40   // MINI Cooper R56 = 40 L  (BMW F31 = 62 L)

// ================================================================
//  Warning / Critical Thresholds
// ================================================================
#define THRESH_COOLANT_WARN  113.0f   // °C — 1°C above normal max (fan on at 112°C); earliest safe warn
#define THRESH_COOLANT_CRIT  120.0f   //     — pull over now; 10°C margin before damage zone
#define THRESH_OIL_WARN      130.0f
#define THRESH_OIL_CRIT      150.0f
#define THRESH_BATT_WARN_LO   11.8f   // V
#define THRESH_BATT_CRIT_LO   11.0f
