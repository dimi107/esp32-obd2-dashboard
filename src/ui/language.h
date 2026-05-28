#pragma once

enum Language { LANG_EN = 0, LANG_BG = 1 };

extern Language g_lang;

// Display names for the 12 metrics (order matches VehicleValues / METRICS[])
extern const char* const METRIC_NAMES[2][12];

enum UIStr {
    UI_SCAN = 0,       // scan button label
    UI_CLEAR,          // clear button label
    UI_DTC_IDLE,       // idle prompt on FAULTS tab
    UI_DTC_SCANNING,   // feedback after tapping SCAN
    UI_DTC_CLEARING,   // feedback after tapping CLEAR
    UI_DTC_OK,         // no faults message
    UI_DTC_N_SINGLE,   // "%d fault code found"  – printf format
    UI_DTC_N_MULTI,    // "%d fault codes found" – printf format
    UI_LANG_LABEL,     // "Language" row label in settings
    UI_ODO_READ,       // "READ" button label on odometer row
    UI_ODO_LABEL,      // "Mileage (DME)" row label
    UI_STR_COUNT
};

extern const char* const UI_STRINGS[2][UI_STR_COUNT];

// Tab bar labels (5 tabs: MAIN, PERF, INFO, DTC, SET)
extern const char* const TAB_NAMES[2][5];
