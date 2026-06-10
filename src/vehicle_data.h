#pragma once
#include <cmath>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const int MAX_DTCS = 20;

enum OBDStatus {
    OBD_DISCONNECTED,
    OBD_CONNECTING,
    OBD_CONNECTED,
    OBD_ERROR
};

// Plain copyable data – used for snapshots passed to the UI.
struct VehicleValues {
    float rpm            = NAN;
    float speedKmh       = NAN;
    float coolantTempC   = NAN;
    float batteryV       = NAN;  // Mode 01 PID 0x42
    float fuelLevelPct   = NAN;
    float mafGps         = NAN;  // Mode 01 PID 0x10, g/s
    float mapKpa         = NAN;
    float engineLoadPct  = NAN;  // Mode 01 PID 0x04
    float intakeTempC    = NAN;
    float fuelTrimPct    = NAN;
    float timingAdv      = NAN;
    float ltFuelTrimPct  = NAN;  // Mode 01 PID 0x07

    OBDStatus status    = OBD_DISCONNECTED;

    char  dtcCodes[MAX_DTCS][8];  // "P0128\0" (OBD-II) or "D01A08\0" (BMW proprietary)
    int   dtcCount     = 0;
    bool     dtcScanDone = false;   // set by OBD task after scan completes

    uint32_t odomKm    = 0;
    bool     odomValid = false;     // false until first successful DME read
    bool     odomErr   = false;     // true if last READ_ODO attempt failed

    VehicleValues() { memset(dtcCodes, 0, sizeof(dtcCodes)); }
};

// Thread-safe wrapper used by the OBD task and main loop.
class VehicleData : public VehicleValues {
    SemaphoreHandle_t mutex_;
public:
    // Flags written by UI, read/cleared by OBD task
    volatile bool dtcScanReq  = false;
    volatile bool dtcClearReq = false;
    volatile bool odoReadReq  = false;

    VehicleData() : mutex_(xSemaphoreCreateMutex()) {}

    bool lock(uint32_t ms = 10) {
        return xSemaphoreTake(mutex_, pdMS_TO_TICKS(ms)) == pdTRUE;
    }
    void unlock() { xSemaphoreGive(mutex_); }

    // Returns a safe copy of all data fields (no mutex).
    VehicleValues snapshot() {
        VehicleValues v;
        if (lock(20)) {
            static_cast<VehicleValues&>(v) = static_cast<const VehicleValues&>(*this);
            unlock();
        }
        return v;
    }
};
