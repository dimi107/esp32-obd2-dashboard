#include "language.h"

Language g_lang = LANG_EN;

// NOTE: Bulgarian strings are Cyrillic UTF-8.
// LVGL will display them correctly only after a custom font is generated with
// lv_font_conv covering range U+0400–U+04FF and declared in lv_conf.h.
// Until then, BG strings will render as boxes on hardware (preview HTML works fine).

const char* const METRIC_NAMES[2][12] = {
    // EN
    { "RPM", "Speed", "Coolant", "Battery",
      "Fuel", "MAF", "Intake Pres", "Eng. Load",
      "Intake Air", "Fuel Trim", "Ign. Timing", "LT Fuel Trim" },
    // BG
    { "Обороти", "Скорост", "Охлад.", "Батерия",
      "Гориво",  "МАФ", "Вх. налягане", "Натоварване",
      "Вх. въздух", "Кратк. кор. гор.", "Момент запалване", "Дълг. кор. гор." },
};

const char* const TAB_NAMES[2][5] = {
    { "MAIN",   "INFO 1", "INFO 2", "DTC",    "SET"       },  // EN
    { "ГЛАВНА", "ИНФО 1", "ИНФО 2", "ГРЕШКИ", "НАСТР." }, // BG
};

const char* const UI_STRINGS[2][UI_STR_COUNT] = {
    // EN
    {
        "SCAN CODES",
        "CLEAR CODES",
        "Tap  SCAN CODES  to read fault codes",
        "Scanning for fault codes...",
        "Clearing fault codes...",
        "No fault codes stored - system OK",
        "%d fault code found",
        "%d fault codes found",
        "Language",
        "READ",
        "Mileage (DME)",
    },
    // BG
    {
        "СКАНИРАЙ",
        "ИЗЧИСТИ",
        "Натиснете  СКАНИРАЙ  за грешки",
        "Търсене на грешки...",
        "Изчистване на грешки...",
        "Няма грешки - системата е OK",
        "%d грешка намерена",
        "%d грешки намерени",
        "Език",
        "ЧЕТЕНЕ",
        "Пробег (DME)",
    },
};
