/*
 * Display driver – Arduino_GFX AXS15231B (QSPI) + Canvas full-frame buffer.
 * Board: JC3248W535C, 320×480 physical portrait.
 * Landscape 480×320: LVGL renders into a separate 480×320 PSRAM buffer; flush_cb
 * rotates it 90° CW into the Canvas portrait framebuffer, then pushes to hardware.
 * AXS15231B requires a full-frame write; Canvas satisfies that requirement.
 * Touch: AXS15231B I2C 0x3B (GPIO3=INT); AXS command protocol, portrait→landscape transform.
 */
#include <Arduino_DataBus.h>
#include <databus/Arduino_ESP32QSPI.h>
#include <Arduino_GFX.h>
#include <display/Arduino_AXS15231B.h>
#include <canvas/Arduino_Canvas.h>
#include <Wire.h>
#include <lvgl.h>
#include "display.h"
#include "../config.h"

// AXS15231B: same IC as display controller, touch function at 0x3B
#define AXS15231B_TOUCH_ADDR  0x3B
static const uint8_t AXS_READ_CMD[8] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08};

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    DISP_CS, DISP_SCLK, DISP_IO0, DISP_IO1, DISP_IO2, DISP_IO3);

// rotation=0: physical portrait 320×480. IPS=false confirmed for AXS15231B on JC3248W535C.
static Arduino_GFX *output_display = new Arduino_AXS15231B(
    bus, DISP_RST /*-1*/, 0 /*rotation*/, false /*IPS*/, 320, 480);

// Canvas matches the physical 320×480 portrait frame.
static Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, output_display);

// LVGL landscape render buffer (480×320), allocated in PSRAM during init.
static lv_color_t* lvgl_buf = nullptr;


// ── LVGL callbacks ─────────────────────────────────────────────────────────

// Rotate LVGL landscape buffer (480×320) 90° CW into portrait Canvas (320×480).
// landscape(lx, ly) → portrait(319-ly, lx)
// portrait index = lx*320 + (319-ly)
static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* /*area*/, lv_color_t* color_p) {
    const uint16_t* src = reinterpret_cast<const uint16_t*>(color_p);
    uint16_t*       dst = reinterpret_cast<uint16_t*>(gfx->getFramebuffer());
    for (int ly = 0; ly < 320; ly++) {
        const uint16_t* row = src + (size_t)ly * 480;
        for (int lx = 0; lx < 480; lx++) {
            dst[(size_t)lx * 320 + (319 - ly)] = row[lx];
        }
    }
    gfx->flush();
    lv_disp_flush_ready(drv);
}

static void touch_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
    uint8_t buf[8] = {0};

    Wire.beginTransmission(AXS15231B_TOUCH_ADDR);
    Wire.write(AXS_READ_CMD, sizeof(AXS_READ_CMD));
    if (Wire.endTransmission() != 0) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    delayMicroseconds(50);

    Wire.requestFrom((uint8_t)AXS15231B_TOUCH_ADDR, (uint8_t)8);
    int avail = 0;
    while (Wire.available() && avail < 8) buf[avail++] = Wire.read();

    // Touch present: buf[0]==0 && buf[1]!=0
    if (avail < 8 || buf[0] != 0 || buf[1] == 0) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    int16_t raw_x = (int16_t)(((buf[2] & 0x0F) << 8) | buf[3]);
    int16_t raw_y = (int16_t)(((buf[4] & 0x0F) << 8) | buf[5]);

    // Portrait touch → landscape (90° CW): lv_x=raw_y, lv_y=319-raw_x
    data->point.x = raw_y;
    data->point.y = 319 - raw_x;
    data->state   = LV_INDEV_STATE_PR;
}

// ── Public init ────────────────────────────────────────────────────────────
void display_init(void) {
    pinMode(DISP_BL, OUTPUT);
    digitalWrite(DISP_BL, HIGH);

    pinMode(TOUCH_INT, INPUT);  // GPIO3, AXS15231B interrupt (optional, not gated)

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);

    gfx->begin();

    lv_init();

    lvgl_buf = reinterpret_cast<lv_color_t*>(
        heap_caps_malloc(480 * 320 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!lvgl_buf) { while(1) delay(1000); }

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, nullptr, 480 * 320);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = DISP_WIDTH;   // 480
    disp_drv.ver_res      = DISP_HEIGHT;  // 320
    disp_drv.flush_cb     = flush_cb;
    disp_drv.draw_buf     = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_cb;
    lv_indev_drv_register(&indev_drv);

    lv_disp_t* disp = lv_disp_get_default();
    lv_theme_t* th  = lv_theme_default_init(disp,
                          lv_palette_main(LV_PALETTE_BLUE),
                          lv_palette_main(LV_PALETTE_CYAN),
                          true, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);

}
