/*
 * OBD task – runs on Core 1.
 * Connects to the ELM327 WiFi AP, cycles through 12 PIDs non-blocking,
 * and handles DTC scan / clear on demand.
 *
 * If compilation fails with "no member … in ELM327", open
 *   .pio/libdeps/<env>/ELMduino/src/ELMduino.h
 * and verify the exact function names for your installed version.
 */
#include "obd_task.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ELMduino.h>
#include <cstring>
#include <cstdlib>

struct UdsDid { uint8_t d1, d2; float scale; float offset; bool two_byte; const char* label; };

// ── UDS Mode 22 odometer DID table ────────────────────────────────────────
// DID 0xF452 is the ISO 14229 standard odometer identifier; try it first.
// Response: 62 F4 52 [km2] [km1] [km0]  – 3-byte big-endian km value.
// If BMW returns 4 bytes the extra byte at offset 3 shifts the triplet; the
// sanity check (< 2 000 000 km) catches misparses automatically.
static const UdsDid ODO_DIDS[] = {
    { 0xF4, 0x52, 0.0f, 0.0f, false, "odometer (ISO 14229)" },
    { 0xF1, 0x0E, 0.0f, 0.0f, false, "odometer alt" },
};
static const int N_ODO_DIDS = 2;

// Consecutive failures before a PID is permanently skipped for this session
static constexpr int      PID_FAIL_THRESHOLD = 10;
// Hard time limit for ELM_GETTING_MSG – if no response in this window, force-advance
static constexpr uint32_t PID_STUCK_MS       = 1500;

// ── PID polling table (12 metrics in VehicleValues order) ──────────────────
typedef float (*PidFn)(ELM327&);
static const PidFn POLL_FNS[12] = {
    [](ELM327& e) { return e.rpm(); },
    [](ELM327& e) { return (float)e.kph(); },
    [](ELM327& e) { return e.engineCoolantTemp(); },
    [](ELM327& e) { return e.batteryVoltage(); },            // [3] PID 0x42
    [](ELM327& e) { return e.fuelLevel(); },
    [](ELM327& e) { return e.mafRate(); },                   // [5] PID 0x10
    [](ELM327& e) { return (float)e.manifoldPressure(); },
    [](ELM327& e) { return e.engineLoad(); },               // [7] PID 0x04
    [](ELM327& e) { return e.intakeAirTemp(); },
    [](ELM327& e) { return e.shortTermFuelTrimBank_1(); },
    [](ELM327& e) { return e.timingAdvance(); },
    [](ELM327& e) { return e.longTermFuelTrimBank_1(); },   // [11] PID 0x07
};
static const int NUM_PIDS = 12;

// ── Physical sanity bounds per PID – values outside these are garbage ──────
// Wider than the display range so legitimate edge readings are never dropped.
//              rpm   spd  cool  batt  fuel  maf  MAP  load  iat   strim  ign  ltrim
static const float PID_LO[12] = {  0,   0,  -40,   6,   0,   0,  10,   0,  -40,  -30,  -64,  -30 };
static const float PID_HI[12] = { 9000, 300, 150,  20, 100, 200, 255, 100,   80,   30,   64,   30 };

static void write_pid(VehicleData* vd, int idx, float val) {
    // NaN is allowed (clears the field → shows "--" on the UI).
    // Reject inf and out-of-range readings from garbled ELM327 responses.
    if (!isnan(val) && (!isfinite(val) || val < PID_LO[idx] || val > PID_HI[idx])) return;
    if (!vd->lock()) return;
    float* const fields[NUM_PIDS] = {
        &vd->rpm, &vd->speedKmh, &vd->coolantTempC, &vd->batteryV,
        &vd->fuelLevelPct, &vd->mafGps, &vd->mapKpa, &vd->engineLoadPct,
        &vd->intakeTempC, &vd->fuelTrimPct, &vd->timingAdv, &vd->ltFuelTrimPct,
    };
    *fields[idx] = val;
    vd->unlock();
}

// ── Read ELM327 raw response until '>' prompt or timeout ───────────────────
static int read_prompt(WiFiClient& c, char* buf, int maxlen, uint32_t ms) {
    int n = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < ms && n < maxlen - 1) {
        while (c.available() && n < maxlen - 1) {
            char ch = c.read();
            if (ch == '>') { buf[n] = '\0'; return n; }
            buf[n++] = ch;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    buf[n] = '\0';
    return n;
}

// ── Collect hex bytes from an ELM327 response string ──────────────────────
static int parse_hex_bytes(const char* buf, uint8_t* out, int maxout) {
    int n = 0;
    const char* p = buf;
    while (*p && n < maxout) {
        while (*p && !isxdigit((unsigned char)*p)) p++;
        if (!*p || !isxdigit((unsigned char)p[1])) { if (*p) p++; continue; }
        char tmp[3] = { p[0], p[1], '\0' };
        out[n++] = (uint8_t)strtol(tmp, nullptr, 16);
        p += 2;
    }
    return n;
}

// ── Parse UDS Mode 19 (SID 0x19 subfn 0x02) response bytes ─────────────────
// Returns number of DTCs added, or -1 if no valid 59 02 response was found.
// Caller must reset dtcCount=0 before the first call for a scan session.
static int parse_dtcs_uds(const uint8_t* bytes, int n, VehicleData* vd) {
    // Locate 0x59 0x02 positive response header
    int start = 0;
    while (start < n - 1 && !(bytes[start] == 0x59 && bytes[start+1] == 0x02)) start++;
    if (start >= n - 1) return -1;

    start += 3;  // skip 0x59, 0x02, DTCStatusAvailabilityMask

    if (!vd->lock(50)) return -1;

    int added = 0;
    static const char SYS[] = "PCBU";
    while (start + 3 < n && vd->dtcCount < MAX_DTCS) {
        uint8_t h = bytes[start], m = bytes[start+1], l = bytes[start+2];
        start += 4;  // 3-byte DTC + 1-byte status
        if (h == 0 && m == 0 && l == 0) continue;

        if (h == 0x00) {
            // ISO 15031-6 standard OBD-II code (P/C/B/U prefix from mid byte)
            snprintf(vd->dtcCodes[vd->dtcCount++], 8,
                     "%c%d%X%02X",
                     SYS[(m >> 6) & 3], (m >> 4) & 3, m & 0xF, l);
        } else {
            // BMW/manufacturer-specific: display as 6-digit hex (e.g. "D01A08")
            snprintf(vd->dtcCodes[vd->dtcCount++], 8, "%02X%02X%02X", h, m, l);
        }
        added++;
    }
    vd->unlock();
    return added;
}

// ── Main task ──────────────────────────────────────────────────────────────
void obd_task(void* pvParameters) {
    VehicleData* vd = static_cast<VehicleData*>(pvParameters);

    WiFiClient client;
    ELM327     elm;

    enum State { WIFI_CONNECT, OBD_CONNECT, POLL, SCAN_DTC, CLEAR_DTC, RECONNECT, READ_ODO };
    State    state        = WIFI_CONNECT;
    int      poll_idx     = 0;
    int      slow_idx     = 1;   // starts at 1 so first advance lands on idx 2 (coolant)
    int      fast_phase   = 0;   // 0=RPM, 1=Speed, 2=slow PID
    int      err_cnt      = 0;
    char     resp_buf[512];
    int      pid_fail[NUM_PIDS]    = {};
    bool     pid_skip[NUM_PIDS]    = {};
    bool     pid_ever_ok[NUM_PIDS] = {};  // true once a PID has succeeded at least once this session
    uint32_t pid_start_ms = 0;   // millis() when current PID polling started (stuck watchdog)
    int      odo_did_ok   = -1;  // index into ODO_DIDS that last worked; -1 = unknown

    while (true) {
        switch (state) {

        // ── Connect to ELM327's WiFi AP ─────────────────────────────────
        case WIFI_CONNECT: {
            vd->lock(); vd->status = OBD_CONNECTING; vd->unlock();
            WiFi.mode(WIFI_STA);
            WiFi.begin(ELM327_WIFI_SSID, ELM327_WIFI_PASS);
            uint32_t t = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t < 15000)
                vTaskDelay(pdMS_TO_TICKS(300));
            state = (WiFi.status() == WL_CONNECTED) ? OBD_CONNECT : RECONNECT;
            break;
        }

        // ── Open TCP socket + init ELM327 ────────────────────────────────
        case OBD_CONNECT: {
            if (client.connected()) client.stop();
            if (!client.connect(ELM327_IP, ELM327_PORT)) { state = RECONNECT; break; }

            // Flush any stale output buffered in the ELM327 from the previous session
            vTaskDelay(pdMS_TO_TICKS(150));
            while (client.available()) client.read();

            // Re-construct ELM327 to clear internal state machine from the old session
            elm = ELM327{};
            if (!elm.begin(client, false, ELM327_TIMEOUT_MS)) { state = RECONNECT; break; }

            // Reset all measurement fields to NAN so pre-disconnect stale values
            // don't stay on screen after reconnect (power cycles, cable pulls etc.)
            if (vd->lock()) {
                static_cast<VehicleValues&>(*vd) = VehicleValues();
                vd->status = OBD_CONNECTED;
                vd->unlock();
            }

            poll_idx   = 0;
            slow_idx   = 1;
            fast_phase = 0;
            err_cnt    = 0;
            memset(pid_fail,    0, sizeof(pid_fail));
            memset(pid_skip,    0, sizeof(pid_skip));
            memset(pid_ever_ok, 0, sizeof(pid_ever_ok));
            odo_did_ok = -1;
            state = POLL;
            break;
        }

        // ── Cycle through PIDs: RPM → Speed → one slow PID, repeat ──────
        // fast_phase 0=RPM(idx0), 1=Speed(idx1), 2=next slow PID
        // Housekeeping (conn check, DTC/UDS requests) only at phase 0 so
        // RPM and Speed are never interrupted mid-cycle.
        case POLL: {
            if (fast_phase == 0) {
                if (!client.connected() || WiFi.status() != WL_CONNECTED) {
                    vd->lock(); vd->status = OBD_DISCONNECTED; vd->unlock();
                    state = RECONNECT; break;
                }
                bool scan = false, clear = false, odoReq = false;
                if (vd->lock()) { scan = vd->dtcScanReq; clear = vd->dtcClearReq; odoReq = vd->odoReadReq; vd->unlock(); }
                if (clear)   { state = CLEAR_DTC; break; }
                if (scan)    { state = SCAN_DTC;  break; }
                if (odoReq)  { state = READ_ODO;  break; }
                poll_idx     = 0;
                pid_start_ms = millis();
            }

            bool advance = pid_skip[poll_idx];

            if (!advance) {
                float val = POLL_FNS[poll_idx](elm);
                if (elm.nb_rx_state == ELM_SUCCESS) {
                    write_pid(vd, poll_idx, val);
                    pid_fail[poll_idx]    = 0;
                    pid_ever_ok[poll_idx] = true;
                    err_cnt  = 0;
                    advance  = true;
                } else if (elm.nb_rx_state != ELM_GETTING_MSG) {
                    // Error response – count failure.
                    // If this PID worked earlier in the session, treat failure as a
                    // connection regression and reconnect (batteryVoltage uses AT RV and
                    // always "succeeds", which would otherwise mask OBD failures and
                    // prevent err_cnt from reaching the reconnect threshold).
                    if (++pid_fail[poll_idx] >= PID_FAIL_THRESHOLD) {
                        if (pid_ever_ok[poll_idx]) { state = RECONNECT; break; }
                        pid_skip[poll_idx] = true;
                        write_pid(vd, poll_idx, NAN);   // show "--" not stale reading
                    }
                    if (++err_cnt >= 20) { state = RECONNECT; break; }
                    advance = true;
                } else if (millis() - pid_start_ms > PID_STUCK_MS) {
                    // Still ELM_GETTING_MSG after PID_STUCK_MS – adapter wedged or WiFi
                    // degraded. Force advance so RPM/Speed keep updating.
                    Serial.printf("[POLL] PID %d stuck >%ums, forcing advance\n",
                                  poll_idx, PID_STUCK_MS);
                    if (++pid_fail[poll_idx] >= PID_FAIL_THRESHOLD) {
                        if (pid_ever_ok[poll_idx]) { state = RECONNECT; break; }
                        pid_skip[poll_idx] = true;
                        write_pid(vd, poll_idx, NAN);
                    }
                    if (++err_cnt >= 20) { state = RECONNECT; break; }
                    advance = true;
                }
            }

            if (advance) {
                if (fast_phase == 0) {
                    fast_phase   = 1;
                    poll_idx     = 1;
                    pid_start_ms = millis();
                } else if (fast_phase == 1) {
                    bool found = false;
                    for (int i = 0; i < NUM_PIDS - 2; i++) {  // only NUM_PIDS-2 slow slots
                        if (++slow_idx >= NUM_PIDS) slow_idx = 2;
                        if (!pid_skip[slow_idx]) { found = true; break; }
                    }
                    if (found) { fast_phase = 2; poll_idx = slow_idx; pid_start_ms = millis(); }
                    else       { fast_phase = 0; }  // all slow PIDs skipped, stay fast-only
                } else {
                    fast_phase = 0;
                    // poll_idx reset to 0 (with fresh pid_start_ms) at top of next fast_phase==0 entry
                }
            }
            break;
        }

        // ── Read stored DTCs: UDS Mode 19 on DME+EGS ────────────────────
        case SCAN_DTC: {
            // AT commands reply in <50ms; 200ms is comfortably safe
            auto uds_at = [&](const char* cmd) {
                client.print(cmd); client.print("\r");
                read_prompt(client, resp_buf, sizeof(resp_buf), 200);
            };

            while (client.available()) client.read();

            uds_at("AT SP 6");
            uds_at("AT H0");

            if (vd->lock(50)) { vd->dtcCount = 0; vd->unlock(); }

            // Physical addresses: DME 7E0→7E8, EGS 7E1→7E9 (both R56 and F31)
            struct { const char* sh; const char* cra; } ECUS[] = {
                { "AT SH 7E0", "AT CRA 7E8" },
                { "AT SH 7E1", "AT CRA 7E9" },
            };

            for (auto& ecu : ECUS) {
                uds_at(ecu.sh);
                uds_at(ecu.cra);
                client.print("19 02 CF\r");  // ReadDTCByStatusMask, confirmed+pending
                read_prompt(client, resp_buf, sizeof(resp_buf), 2000);

                uint8_t rbuf[256];
                int n = parse_hex_bytes(resp_buf, rbuf, 256);
                int result = (n >= 3) ? parse_dtcs_uds(rbuf, n, vd) : -1;
                Serial.printf("[DTC] Mode19 %s: n=%d result=%d\n", ecu.sh + 6, n, result);
            }

            // Restore ELM327 for normal PID polling
            uds_at("AT D");
            uds_at("AT E0");
            uds_at("AT L0");
            uds_at("AT H0");
            uds_at("AT S1");
            uds_at("AT SP 0");
            while (client.available()) client.read();
            vTaskDelay(pdMS_TO_TICKS(80));

            if (vd->lock(50)) {
                vd->dtcScanDone = true;
                vd->dtcScanReq  = false;
                vd->unlock();
            }

            poll_idx   = 0;
            fast_phase = 0;
            state      = POLL;
            break;
        }

        // ── Clear DTCs (Mode 04) then re-scan via SCAN_DTC ───────────────
        case CLEAR_DTC: {
            client.print("04\r");
            read_prompt(client, resp_buf, sizeof(resp_buf), 3000);
            if (vd->lock()) { vd->dtcClearReq = false; vd->unlock(); }
            // Re-scan to confirm cleared; SCAN_DTC handles all state cleanup
            state = SCAN_DTC;
            break;
        }

        // ── UDS Mode 22: odometer on demand (button-triggered) ────────────
        // NOTE: BMW F-series odometer lives in the instrument cluster (KOMBI),
        // not the DME (7E0). DIDs 0xF452 / 0xF10E on 7E0 will return ERR on F-series.
        // MINI R56/R57 DME does expose odometer so this works on that vehicle.
        case READ_ODO: {
            auto uds_at2 = [&](const char* cmd) {
                client.print(cmd); client.print("\r");
                read_prompt(client, resp_buf, sizeof(resp_buf), 500);
            };

            while (client.available()) client.read();  // flush stale bytes before UDS
            uds_at2("AT SP 6");
            uds_at2("AT H0");
            uds_at2("AT SH 7E0");
            uds_at2("AT CRA 7E8");

            bool found = false;
            int  start = (odo_did_ok >= 0) ? odo_did_ok : 0;
            for (int i = 0; i < N_ODO_DIDS && !found; i++) {
                int idx = (start + i) % N_ODO_DIDS;
                const UdsDid& d = ODO_DIDS[idx];
                char cmd[12];
                snprintf(cmd, sizeof(cmd), "22%02X%02X\r", d.d1, d.d2);
                client.print(cmd);
                read_prompt(client, resp_buf, sizeof(resp_buf), 1500);
                Serial.printf("[ODO] 22%02X%02X (%s) -> %s\n", d.d1, d.d2, d.label, resp_buf);

                uint8_t hb[32];
                int n = parse_hex_bytes(resp_buf, hb, 32);
                for (int j = 0; j + 5 < n && !found; j++) {
                    if (hb[j] == 0x62 && hb[j+1] == d.d1 && hb[j+2] == d.d2) {
                        uint32_t km = ((uint32_t)hb[j+3] << 16)
                                    | ((uint32_t)hb[j+4] <<  8)
                                    |  (uint32_t)hb[j+5];
                        if (km > 0 && km < 2000000UL) {
                            if (vd->lock()) {
                                vd->odomKm    = km;
                                vd->odomValid = true;
                                vd->odomErr   = false;
                                vd->unlock();
                            }
                            Serial.printf("[ODO] OK %lu km\n", (unsigned long)km);
                            odo_did_ok = idx;
                            found = true;
                        }
                    }
                }
            }
            if (!found) {
                Serial.println("[ODO] all DIDs failed");
                if (vd->lock()) { vd->odomErr = true; vd->unlock(); }
            }

            if (vd->lock()) { vd->odoReadReq = false; vd->unlock(); }

            // Restore ELM327
            uds_at2("AT D");
            uds_at2("AT E0");
            uds_at2("AT L0");
            uds_at2("AT H0");
            uds_at2("AT S1");
            uds_at2("AT SP 0");
            while (client.available()) client.read();
            vTaskDelay(pdMS_TO_TICKS(80));

            err_cnt    = 0;
            fast_phase = 0;
            state      = POLL;
            break;
        }

        // ── Wait and reconnect ────────────────────────────────────────────
        case RECONNECT: {
            vd->lock(); vd->status = OBD_DISCONNECTED; vd->unlock();
            if (client.connected()) client.stop();
            vTaskDelay(pdMS_TO_TICKS(2000));
            state = (WiFi.status() == WL_CONNECTED) ? OBD_CONNECT : WIFI_CONNECT;
            break;
        }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
