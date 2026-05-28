#include <Arduino.h>
#include <lvgl.h>
#include <Preferences.h>
#include "vehicle_data.h"
#include "obd_task.h"
#include "ui/display.h"
#include "ui/dashboard.h"
#include "ui/language.h"

VehicleData gVehicleData;

void setup() {
    Serial.begin(115200);

    Preferences prefs;
    prefs.begin("obd", true);
    g_lang = (Language)prefs.getUChar("lang", (uint8_t)LANG_EN);
    prefs.end();

    display_init();
    dashboard_init(&gVehicleData);

    xTaskCreatePinnedToCore(
        obd_task,
        "OBD",
        8192,
        &gVehicleData,
        1,
        nullptr,
        1
    );
}

void loop() {
    lv_timer_handler();

    static uint32_t last_update = 0;
    uint32_t now = millis();
    if (now - last_update >= 50) {
        last_update = now;
        VehicleValues snap = gVehicleData.snapshot();
        dashboard_update(snap);
    }

    delay(5);
}
