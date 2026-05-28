#include "dashboard.h"
#include "display.h"
#include "../config.h"
#include "../dtc_table.h"
#include "language.h"
#include <lvgl.h>
#include <Preferences.h>
#include <cmath>
#include <cstdio>
#include <cstring>

// ── Colour palette ─────────────────────────────────────────────────────────
#define C_BG    lv_color_hex(0x0f172a)
#define C_TILE  lv_color_hex(0x1e293b)
#define C_EDGE  lv_color_hex(0x334155)
#define C_TEXT  lv_color_white()
#define C_DIM   lv_color_hex(0x94a3b8)
#define C_OK    lv_color_hex(0x22c55e)
#define C_WARN  lv_color_hex(0xf59e0b)
#define C_CRIT  lv_color_hex(0xef4444)
#define C_TRACK lv_color_hex(0x2d3f55)
#define C_BLUE  lv_color_hex(0x3b82f6)

#define NT (-999.0f)

// ── Metric descriptor (no name field – use METRIC_NAMES[g_lang][idx]) ──────
struct MetricDef {
    const char* unit;
    const char* fmt;
    float minVal, maxVal;
    float warnHigh, critHigh;
    float warnLow,  critLow;
    float displayScale;
    bool  useArc;
};

// 12 metrics, 4 per tab.  Order must match VehicleValues fields.
static const MetricDef METRICS[12] = {
    // ── MAIN ──────────────────────────────────────────────────────────────
    { "",                "%.0f",    0,   8000, 6500, 7500, NT,    NT,    1.0f,  true  }, // RPM
    { "km/h",            "%.0f",    0,    280, NT,   NT,   NT,    NT,    1.0f,  false }, // Speed
    { "\xc2\xb0""C",    "%.0f",   40,    115, 113,  120,  NT,    NT,    1.0f,  true  }, // Coolant (N12 normal 103-112°C; fan on at 112; warn 113, crit 120)
    { "V",               "%.1f",   10,     16, NT,   NT,   11.8f, 11.0f, 1.0f,  false }, // Battery
    // ── PERF ──────────────────────────────────────────────────────────────
    { "L",               "%.0f",    0,    100, NT,   NT,   20,    10,    FUEL_TANK_L/100.0f, true  }, // Fuel
    { "g/s",             "%.1f",    0,    100, NT,   NT,   NT,    NT,    1.0f,  true  }, // MAF
    { "bar",             "%.2f",    0,    300, 230,  270,  NT,    NT,    0.01f, true  }, // Intake Pres (arc kPa, label bar)
    { "%",               "%.0f",    0,    100, NT,   NT,   NT,    NT,    1.0f,  false }, // Engine Load
    // ── DETAIL ────────────────────────────────────────────────────────────
    { "\xc2\xb0""C",    "%.0f",  -20,     70, 50,   NT,   NT,    NT,    1.0f,  true  }, // Intake Air
    { "%",              "%+.1f", -25,      25, 15,   20,  -15,   -20,   1.0f,  false }, // Fuel Trim
    { "\xc2\xb0",       "%.1f", -10,      50, NT,   NT,   5.0f,  0.0f,  1.0f,  false }, // Ign. Timing
    { "%",              "%+.1f", -25,      25, 15,   20,  -15,   -20,   1.0f,  false }, // LT Fuel Trim
};

// ── Widget references ──────────────────────────────────────────────────────
struct TileW { lv_obj_t* arc; lv_obj_t* val; lv_obj_t* name_lbl; };
static TileW     g_tiles[12];
static lv_obj_t* g_status_lbl    = nullptr;
static lv_obj_t* g_dtc_list      = nullptr;
static lv_obj_t* g_dtc_status    = nullptr;
static lv_obj_t* g_scan_lbl      = nullptr;
static lv_obj_t* g_clear_lbl     = nullptr;
static lv_obj_t* g_lang_btns[2]  = { nullptr, nullptr };
static lv_obj_t* g_odom_lbl      = nullptr;
static lv_obj_t* g_odo_read_lbl  = nullptr;
static lv_obj_t* g_odo_key_lbl   = nullptr;
static lv_obj_t* g_lang_label    = nullptr;  // "Language" row label in SET tab
static lv_obj_t* g_tabview       = nullptr;
static lv_obj_t* g_tab_bar       = nullptr;
static const char* g_tab_map[6]  = {};   // 5 tab names + "" terminator
static int       g_last_dtccount = -1;
static float     g_tile_vals[12] = { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN };

// ── Helpers ────────────────────────────────────────────────────────────────
static lv_color_t severity_color(const MetricDef& m, float v) {
    if (isnan(v)) return C_DIM;
    if ((m.critHigh > NT && v >= m.critHigh) || (m.critLow > NT && v <= m.critLow))
        return C_CRIT;
    if ((m.warnHigh > NT && v >= m.warnHigh) || (m.warnLow > NT && v <= m.warnLow))
        return C_WARN;
    return C_OK;
}

static void update_tile(int idx, float val) {
    bool same = (isnan(val) && isnan(g_tile_vals[idx])) || (val == g_tile_vals[idx]);
    if (same) return;
    g_tile_vals[idx] = val;

    const MetricDef& m = METRICS[idx];
    TileW& w = g_tiles[idx];
    lv_color_t col = severity_color(m, val);

    if (w.arc && !isnan(val)) {
        float c = val < m.minVal ? m.minVal : (val > m.maxVal ? m.maxVal : val);
        lv_arc_set_value(w.arc, static_cast<int>(c + 0.5f));
        lv_obj_set_style_arc_color(w.arc, col, LV_PART_INDICATOR);
    }
    if (w.val) {
        if (isnan(val)) {
            lv_label_set_text(w.val, "--");
        } else {
            char buf[20];
            snprintf(buf, sizeof(buf), m.fmt, val * m.displayScale);
            lv_label_set_text(w.val, buf);
        }
        lv_obj_set_style_text_color(w.val, col, 0);
    }
}

// ── Build one metric tile ──────────────────────────────────────────────────
static void build_tile(lv_obj_t* parent, int x, int y, int w, int h, int idx, bool main_style) {
    const MetricDef& m   = METRICS[idx];
    const char*      name = METRIC_NAMES[g_lang][idx];

    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, w, h);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tile, C_TILE, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, 6, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_border_color(tile, C_EDGE, 0);
    lv_obj_set_style_pad_all(tile, 4, 0);

    lv_obj_t* name_lbl_out = nullptr;

    if (!main_style) {
        lv_obj_t* title = lv_label_create(tile);
        lv_obj_set_style_text_font(title, &montserrat_16_cyr, 0);
        lv_obj_set_style_text_color(title, C_DIM, 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_label_set_text(title, name);
        name_lbl_out = title;
    }

    if (m.useArc) {
        int as = main_style ? (h - 8) : (h - 28);
        if (as > w - 10) as = w - 10;
        if (as > (main_style ? 120 : 100)) as = main_style ? 120 : 100;

        lv_obj_t* arc = lv_arc_create(tile);
        lv_obj_set_size(arc, as, as);
        lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
        lv_arc_set_bg_angles(arc, 225, 135);
        lv_arc_set_range(arc, static_cast<int>(m.minVal), static_cast<int>(m.maxVal));
        lv_arc_set_value(arc, static_cast<int>(m.minVal));
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_set_style_arc_color(arc, C_TRACK, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, C_OK,    LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arc, as / 8,  LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, as / 8,  LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
        lv_obj_align(arc, LV_ALIGN_BOTTOM_MID, 0, -2);

        lv_obj_t* val = lv_label_create(arc);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(val, C_OK, 0);
        lv_obj_align(val, LV_ALIGN_CENTER, 0, -8);
        lv_label_set_text(val, "--");

        lv_obj_t* sublbl = lv_label_create(arc);
        lv_obj_set_style_text_font(sublbl, &montserrat_14_cyr, 0);
        lv_obj_set_style_text_color(sublbl, C_DIM, 0);
        lv_obj_align_to(sublbl, val, LV_ALIGN_OUT_BOTTOM_MID, (idx == 2) ? -14 : 0, 2);

        if (main_style) {
            // Show "Name Unit" (e.g. "Coolant °C") or just "Name" if no unit
            char sub[32];
            if (m.unit[0]) snprintf(sub, sizeof(sub), "%s %s", name, m.unit);
            else            snprintf(sub, sizeof(sub), "%s", name);
            lv_label_set_text(sublbl, sub);
            name_lbl_out = sublbl;
        } else {
            lv_label_set_text(sublbl, m.unit);
            // name_lbl_out stays as the title header
        }

        g_tiles[idx] = { arc, val, name_lbl_out };
    } else {
        lv_obj_t* val = lv_label_create(tile);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(val, C_OK, 0);
        lv_obj_align(val, LV_ALIGN_CENTER, 0, main_style ? -4 : 8);
        lv_label_set_text(val, "--");

        lv_obj_t* ulbl = lv_label_create(tile);
        lv_obj_set_style_text_font(ulbl, &montserrat_16_cyr, 0);
        lv_obj_set_style_text_color(ulbl, C_DIM, 0);
        lv_obj_align_to(ulbl, val, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
        lv_label_set_text(ulbl, m.unit);

        if (main_style) {
            lv_obj_t* nlbl = lv_label_create(tile);
            lv_obj_set_style_text_font(nlbl, &montserrat_16_cyr, 0);
            lv_obj_set_style_text_color(nlbl, C_DIM, 0);
            lv_obj_align(nlbl, LV_ALIGN_BOTTOM_MID, 0, -2);
            lv_label_set_text(nlbl, name);
            name_lbl_out = nlbl;
        }

        g_tiles[idx] = { nullptr, val, name_lbl_out };
    }
}

// ── Build a metric tab (2×2 grid of 4 tiles) ──────────────────────────────
static void build_metric_tab(lv_obj_t* tab, int first) {
    const bool main_style = (first == 0);

    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);

    static const int G  = 3;
    const int W  = DISP_WIDTH;
    const int H  = DISP_HEIGHT - TAB_BAR_H;
    const int TW = (W - 2 * G - G) / 2;
    const int TH = (H - 2 * G - G) / 2;

    build_tile(tab, G,         G,         TW, TH, first + 0, main_style);
    build_tile(tab, G+TW+G,    G,         TW, TH, first + 1, main_style);
    build_tile(tab, G,         G+TH+G,    TW, TH, first + 2, main_style);
    build_tile(tab, G+TW+G,    G+TH+G,    TW, TH, first + 3, main_style);
}

// ── DTC tab callbacks ──────────────────────────────────────────────────────
static void scan_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    VehicleData* vd = static_cast<VehicleData*>(lv_event_get_user_data(e));
    if (vd->lock()) {
        vd->dtcScanReq  = true;
        vd->dtcScanDone = false;  // force dashboard_update to re-trigger when scan completes
        vd->unlock();
    }
    g_last_dtccount = -1;  // force list repopulation even if DTC count is unchanged
    if (g_dtc_status) {
        lv_label_set_text(g_dtc_status, UI_STRINGS[g_lang][UI_DTC_SCANNING]);
        lv_obj_set_style_text_color(g_dtc_status, C_WARN, 0);
    }
    if (g_dtc_list) lv_obj_clean(g_dtc_list);  // clear stale results while scanning
}

static void clear_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    VehicleData* vd = static_cast<VehicleData*>(lv_event_get_user_data(e));
    if (vd->lock()) { vd->dtcClearReq = true; vd->unlock(); }
    if (g_dtc_status) {
        lv_label_set_text(g_dtc_status, UI_STRINGS[g_lang][UI_DTC_CLEARING]);
        lv_obj_set_style_text_color(g_dtc_status, C_WARN, 0);
    }
    if (g_dtc_list) lv_obj_clean(g_dtc_list);
    g_last_dtccount = -1;
}

// ── Rebuild DTC list from snapshot ────────────────────────────────────────
static void update_dtc_list(const VehicleValues& v) {
    if (!g_dtc_list) return;
    lv_obj_clean(g_dtc_list);

    if (v.dtcCount == 0) {
        // No codes: message goes in the list, status cleared
        lv_obj_t* item = lv_list_add_text(g_dtc_list, UI_STRINGS[g_lang][UI_DTC_OK]);
        if (item) lv_obj_set_style_text_color(item, C_OK, 0);
        if (g_dtc_status) lv_label_set_text(g_dtc_status, "");
    } else {
        // Codes found: count in compact status (red), codes in list
        char status_buf[64];
        const char* fmt = UI_STRINGS[g_lang][v.dtcCount == 1 ? UI_DTC_N_SINGLE : UI_DTC_N_MULTI];
        snprintf(status_buf, sizeof(status_buf), fmt, v.dtcCount);
        if (g_dtc_status) {
            lv_label_set_text(g_dtc_status, status_buf);
            lv_obj_set_style_text_color(g_dtc_status, C_CRIT, 0);
        }
        for (int i = 0; i < v.dtcCount; i++) {
            char line[80];
            snprintf(line, sizeof(line), "%-6s  %s",
                     v.dtcCodes[i], dtc_description(v.dtcCodes[i]));
            lv_obj_t* item = lv_list_add_text(g_dtc_list, line);
            if (item) {
                lv_obj_set_style_text_font(item, &montserrat_16_cyr, 0);
                lv_obj_set_style_text_color(item, C_WARN, 0);
            }
        }
    }
}

// ── Build FAULTS tab ───────────────────────────────────────────────────────
static void build_faults_tab(lv_obj_t* tab, VehicleData* vd) {
    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);

    // Row 0: buttons (h=36)
    lv_obj_t* scan_btn = lv_btn_create(tab);
    lv_obj_set_size(scan_btn, 145, 36);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(scan_btn, C_BLUE, 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x1d4ed8), LV_STATE_PRESSED);
    lv_obj_add_event_cb(scan_btn, scan_cb, LV_EVENT_CLICKED, vd);
    g_scan_lbl = lv_label_create(scan_btn);
    lv_obj_center(g_scan_lbl);
    lv_obj_set_style_text_font(g_scan_lbl, &montserrat_16_cyr, 0);
    lv_label_set_text(g_scan_lbl, UI_STRINGS[g_lang][UI_SCAN]);

    lv_obj_t* clr_btn = lv_btn_create(tab);
    lv_obj_set_size(clr_btn, 145, 36);
    lv_obj_align(clr_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(clr_btn, lv_color_hex(0x7f1d1d), 0);
    lv_obj_set_style_bg_color(clr_btn, lv_color_hex(0x450a0a), LV_STATE_PRESSED);
    lv_obj_add_event_cb(clr_btn, clear_cb, LV_EVENT_CLICKED, vd);
    g_clear_lbl = lv_label_create(clr_btn);
    lv_obj_center(g_clear_lbl);
    lv_obj_set_style_text_font(g_clear_lbl, &montserrat_16_cyr, 0);
    lv_label_set_text(g_clear_lbl, UI_STRINGS[g_lang][UI_CLEAR]);

    // Row 1: compact status (h=20) — shows count or scan/clear progress, hidden when idle
    g_dtc_status = lv_label_create(tab);
    lv_obj_set_style_text_font(g_dtc_status, &montserrat_14_cyr, 0);
    lv_obj_set_style_text_color(g_dtc_status, C_DIM, 0);
    lv_obj_align(g_dtc_status, LV_ALIGN_TOP_LEFT, 0, 42);
    lv_label_set_text(g_dtc_status, "");

    // Row 2: DTC list — fills remaining space (y=66 to bottom)
    const int dtc_list_y = 66;
    const int dtc_list_h = (DISP_HEIGHT - TAB_BAR_H) - 12 - dtc_list_y;  // 284-12-66=206
    g_dtc_list = lv_list_create(tab);
    lv_obj_set_size(g_dtc_list, DISP_WIDTH - 12, dtc_list_h);
    lv_obj_align(g_dtc_list, LV_ALIGN_TOP_LEFT, 0, dtc_list_y);
    lv_obj_set_style_bg_color(g_dtc_list, C_BG, 0);
    lv_obj_set_style_bg_opa(g_dtc_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_dtc_list, C_EDGE, 0);
    lv_obj_set_style_border_width(g_dtc_list, 1, 0);
    lv_obj_set_style_radius(g_dtc_list, 6, 0);
    lv_obj_set_style_pad_row(g_dtc_list, 2, 0);

    // Initial idle prompt inside the list
    lv_obj_t* idle = lv_list_add_text(g_dtc_list, UI_STRINGS[g_lang][UI_DTC_IDLE]);
    if (idle) lv_obj_set_style_text_color(idle, C_DIM, 0);
}

// ── ODO read button callback ───────────────────────────────────────────────
static void odo_read_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    VehicleData* vd = static_cast<VehicleData*>(lv_event_get_user_data(e));
    if (vd->lock()) { vd->odoReadReq = true; vd->odomValid = false; vd->odomErr = false; vd->unlock(); }
    if (g_odom_lbl) {
        lv_label_set_text(g_odom_lbl, "...");
        lv_obj_set_style_text_color(g_odom_lbl, C_WARN, 0);
    }
}

// ── Language button callback ───────────────────────────────────────────────
static void lang_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Language lang = (Language)(uintptr_t)lv_event_get_user_data(e);
    set_language(lang);
}

// ── Build SETTINGS tab ─────────────────────────────────────────────────────
static void build_settings_tab(lv_obj_t* tab, VehicleData* vd) {
    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tab, 10, 0);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);

    // Single row: [Language label] ─── [EN] [BG]
    lv_obj_t* row = lv_obj_create(tab);
    lv_obj_set_size(row, DISP_WIDTH - 20, 50);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(row, C_TILE, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, C_EDGE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    g_lang_label = lv_label_create(row);
    lv_obj_set_style_text_font(g_lang_label, &montserrat_16_cyr, 0);
    lv_obj_set_style_text_color(g_lang_label, C_TEXT, 0);
    lv_obj_align(g_lang_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(g_lang_label, UI_STRINGS[g_lang][UI_LANG_LABEL]);

    static const char* const BTN_LABELS[2] = { "EN", "BG" };
    for (int i = 0; i < 2; i++) {
        lv_obj_t* btn = lv_btn_create(row);
        lv_obj_set_size(btn, 52, 34);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, (i == 0) ? -60 : -2, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_add_event_cb(btn, lang_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* bl = lv_label_create(btn);
        lv_obj_set_style_text_font(bl, &montserrat_16_cyr, 0);
        lv_obj_center(bl);
        lv_label_set_text(bl, BTN_LABELS[i]);
        g_lang_btns[i] = btn;
    }

    // Apply initial highlight via set_language (called with current g_lang)
    for (int i = 0; i < 2; i++) {
        bool active = (i == (int)g_lang);
        lv_obj_set_style_bg_color(g_lang_btns[i], active ? C_BLUE : C_TILE, 0);
        lv_obj_set_style_border_color(g_lang_btns[i], active ? C_BLUE : C_EDGE, 0);
    }

    // Odometer row
    lv_obj_t* odo_row = lv_obj_create(tab);
    lv_obj_set_size(odo_row, DISP_WIDTH - 20, 50);
    lv_obj_align(odo_row, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(odo_row, C_TILE, 0);
    lv_obj_set_style_bg_opa(odo_row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(odo_row, C_EDGE, 0);
    lv_obj_set_style_border_width(odo_row, 1, 0);
    lv_obj_set_style_radius(odo_row, 6, 0);
    lv_obj_set_style_pad_hor(odo_row, 12, 0);
    lv_obj_set_style_pad_ver(odo_row, 0, 0);
    lv_obj_set_scrollbar_mode(odo_row, LV_SCROLLBAR_MODE_OFF);

    g_odo_key_lbl = lv_label_create(odo_row);
    lv_obj_set_style_text_font(g_odo_key_lbl, &montserrat_16_cyr, 0);
    lv_obj_set_style_text_color(g_odo_key_lbl, C_TEXT, 0);
    lv_obj_align(g_odo_key_lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(g_odo_key_lbl, UI_STRINGS[g_lang][UI_ODO_LABEL]);

    // READ button on the right
    lv_obj_t* odo_btn = lv_btn_create(odo_row);
    lv_obj_set_size(odo_btn, 72, 34);
    lv_obj_align(odo_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(odo_btn, C_BLUE, 0);
    lv_obj_set_style_bg_color(odo_btn, lv_color_hex(0x1d4ed8), LV_STATE_PRESSED);
    lv_obj_set_style_radius(odo_btn, 4, 0);
    lv_obj_add_event_cb(odo_btn, odo_read_cb, LV_EVENT_CLICKED, vd);
    g_odo_read_lbl = lv_label_create(odo_btn);
    lv_obj_center(g_odo_read_lbl);
    lv_obj_set_style_text_font(g_odo_read_lbl, &montserrat_16_cyr, 0);
    lv_label_set_text(g_odo_read_lbl, UI_STRINGS[g_lang][UI_ODO_READ]);

    // Odometer value to the left of the button
    g_odom_lbl = lv_label_create(odo_row);
    lv_obj_set_style_text_font(g_odom_lbl, &montserrat_16_cyr, 0);
    lv_obj_set_style_text_color(g_odom_lbl, C_DIM, 0);
    lv_obj_align(g_odom_lbl, LV_ALIGN_RIGHT_MID, -80, 0);
    lv_label_set_text(g_odom_lbl, "-- km");
}

// ── Public: switch language, update all labels, persist to NVS ────────────
void set_language(Language lang) {
    g_lang = lang;

    // Update metric name labels on all tiles
    for (int i = 0; i < 12; i++) {
        if (!g_tiles[i].name_lbl) continue;
        const MetricDef& m = METRICS[i];
        const char* name = METRIC_NAMES[lang][i];
        char buf[32];
        // Main-style arc tiles combine "Name Unit" in the sub-label
        if (i < 4 && m.useArc && m.unit[0])
            snprintf(buf, sizeof(buf), "%s %s", name, m.unit);
        else
            snprintf(buf, sizeof(buf), "%s", name);
        lv_label_set_text(g_tiles[i].name_lbl, buf);
    }

    // Tab bar names — rename tabs in place
    if (g_tabview) {
        for (int i = 0; i < 5; i++)
            lv_tabview_rename_tab(g_tabview, i, TAB_NAMES[lang][i]);
    }

    // FAULTS tab — buttons
    if (g_scan_lbl)  lv_label_set_text(g_scan_lbl,  UI_STRINGS[lang][UI_SCAN]);
    if (g_clear_lbl) lv_label_set_text(g_clear_lbl, UI_STRINGS[lang][UI_CLEAR]);

    // FAULTS tab — list content (translate idle/result text)
    if (g_dtc_list) {
        if (g_last_dtccount < 0) {
            lv_obj_clean(g_dtc_list);
            lv_obj_t* idle = lv_list_add_text(g_dtc_list, UI_STRINGS[lang][UI_DTC_IDLE]);
            if (idle) lv_obj_set_style_text_color(idle, C_DIM, 0);
        } else if (g_last_dtccount == 0) {
            lv_obj_clean(g_dtc_list);
            lv_obj_t* item = lv_list_add_text(g_dtc_list, UI_STRINGS[lang][UI_DTC_OK]);
            if (item) lv_obj_set_style_text_color(item, C_OK, 0);
            if (g_dtc_status) lv_label_set_text(g_dtc_status, "");
        } else {
            char buf[64];
            const char* fmt = UI_STRINGS[lang][g_last_dtccount == 1 ? UI_DTC_N_SINGLE : UI_DTC_N_MULTI];
            snprintf(buf, sizeof(buf), fmt, g_last_dtccount);
            if (g_dtc_status) {
                lv_label_set_text(g_dtc_status, buf);
                lv_obj_set_style_text_color(g_dtc_status, C_CRIT, 0);
            }
        }
    }

    // SET tab
    if (g_lang_label)   lv_label_set_text(g_lang_label,   UI_STRINGS[lang][UI_LANG_LABEL]);
    if (g_odo_read_lbl) lv_label_set_text(g_odo_read_lbl, UI_STRINGS[lang][UI_ODO_READ]);
    if (g_odo_key_lbl)  lv_label_set_text(g_odo_key_lbl,  UI_STRINGS[lang][UI_ODO_LABEL]);

    // Settings button highlight
    for (int i = 0; i < 2; i++) {
        if (!g_lang_btns[i]) continue;
        bool active = (i == (int)lang);
        lv_obj_set_style_bg_color(g_lang_btns[i], active ? C_BLUE : C_TILE, 0);
        lv_obj_set_style_border_color(g_lang_btns[i], active ? C_BLUE : C_EDGE, 0);
    }

    // Persist to NVS
    Preferences prefs;
    prefs.begin("obd", false);
    prefs.putUChar("lang", (uint8_t)lang);
    prefs.end();
}

// ── Public: create all widgets ─────────────────────────────────────────────
void dashboard_init(VehicleData* vd) {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t* tv = lv_tabview_create(scr, LV_DIR_TOP, TAB_BAR_H);
    lv_obj_set_size(tv, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_pos(tv, 0, 0);
    lv_obj_set_style_bg_color(tv, C_BG, 0);
    lv_obj_set_style_pad_all(tv, 0, 0);

    lv_obj_t* tabs = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(tabs, lv_color_hex(0x0a1020), 0);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(tabs, &montserrat_14_cyr, 0);  // slightly smaller for 5 tabs
    lv_obj_set_style_text_color(tabs, C_DIM, 0);
    lv_obj_set_style_text_color(tabs, C_TEXT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tabs, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tabs, C_BLUE, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tabs, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_pad_right(tabs, 56, 0);        // reserve gap on right for status label
    lv_obj_set_style_pad_hor(tabs, 2, LV_PART_ITEMS); // tighter tab button margins

    lv_obj_t* t0 = lv_tabview_add_tab(tv, TAB_NAMES[g_lang][0]);
    lv_obj_t* t1 = lv_tabview_add_tab(tv, TAB_NAMES[g_lang][1]);
    lv_obj_t* t2 = lv_tabview_add_tab(tv, TAB_NAMES[g_lang][2]);
    lv_obj_t* t3 = lv_tabview_add_tab(tv, TAB_NAMES[g_lang][3]);
    lv_obj_t* t4 = lv_tabview_add_tab(tv, TAB_NAMES[g_lang][4]);

    // Store refs for language switching
    g_tabview = tv;
    g_tab_bar = tabs;
    for (int i = 0; i < 5; i++) g_tab_map[i] = TAB_NAMES[g_lang][i];
    g_tab_map[5] = "";

    build_metric_tab(t0, 0);
    build_metric_tab(t1, 4);
    build_metric_tab(t2, 8);
    build_faults_tab(t3, vd);
    build_settings_tab(t4, vd);

    // Create on screen (not tabview) so it sits on top of the tab bar at TOP_RIGHT
    g_status_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(g_status_lbl, &montserrat_14_cyr, 0);
    lv_obj_set_style_text_color(g_status_lbl, C_CRIT, 0);
    lv_obj_align(g_status_lbl, LV_ALIGN_TOP_RIGHT, -6, (TAB_BAR_H - 14) / 2);
    lv_label_set_text(g_status_lbl, "NO OBD");
}

// ── Public: refresh all values (call from main loop) ──────────────────────
void dashboard_update(const VehicleValues& v) {
    const float vals[12] = {
        v.rpm, v.speedKmh, v.coolantTempC, v.batteryV,
        v.fuelLevelPct, v.mafGps, v.mapKpa, v.engineLoadPct,
        v.intakeTempC, v.fuelTrimPct, v.timingAdv, v.ltFuelTrimPct,
    };
    for (int i = 0; i < 12; i++) update_tile(i, vals[i]);

    if (g_status_lbl) {
        switch (v.status) {
        case OBD_CONNECTED:
            lv_label_set_text(g_status_lbl, "OBD");
            lv_obj_set_style_text_color(g_status_lbl, C_OK, 0);
            break;
        case OBD_CONNECTING:
            lv_label_set_text(g_status_lbl, "CONN...");
            lv_obj_set_style_text_color(g_status_lbl, C_WARN, 0);
            break;
        default:
            lv_label_set_text(g_status_lbl, "NO OBD");
            lv_obj_set_style_text_color(g_status_lbl, C_CRIT, 0);
        }
    }

    if (g_odom_lbl) {
        if (v.odomValid) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%lu km", (unsigned long)v.odomKm);
            lv_label_set_text(g_odom_lbl, buf);
            lv_obj_set_style_text_color(g_odom_lbl, C_OK, 0);
        } else if (v.odomErr) {
            lv_label_set_text(g_odom_lbl, "ERR");
            lv_obj_set_style_text_color(g_odom_lbl, C_CRIT, 0);
        }
    }

    if (v.dtcScanDone && v.dtcCount != g_last_dtccount) {
        g_last_dtccount = v.dtcCount;
        update_dtc_list(v);
    }
}
