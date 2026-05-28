#pragma once
#include "../vehicle_data.h"
#include "language.h"

// Create all LVGL widgets. vd is used for DTC scan/clear button callbacks.
void dashboard_init(VehicleData* vd);

// Update all displayed values from a snapshot.  Call from the main loop.
void dashboard_update(const VehicleValues& v);

// Switch display language and persist the choice to NVS.
void set_language(Language lang);
