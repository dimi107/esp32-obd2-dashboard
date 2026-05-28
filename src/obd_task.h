#pragma once
#include "vehicle_data.h"

// Runs on Core 1. Parameter must be a pointer to the global VehicleData.
void obd_task(void* pvParameters);
