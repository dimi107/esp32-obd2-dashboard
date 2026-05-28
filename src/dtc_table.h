#pragma once
#include <cstring>

// Common OBD-II / BMW / MINI fault code descriptions.
// Covers the most frequent P-codes seen on N12, N14, N16, N18,
// B38, B48 engines and standard powertrain systems.
struct DtcEntry { const char* code; const char* desc; };

static const DtcEntry DTC_TABLE[] = {
    // Fuel / air
    {"P0087", "Fuel Rail Pressure Too Low"},
    {"P0088", "Fuel Rail Pressure Too High"},
    {"P0089", "Fuel Pressure Regulator Performance"},
    {"P0090", "Fuel Pressure Reg. Control Valve"},
    {"P0171", "System Too Lean  (Bank 1)"},
    {"P0172", "System Too Rich  (Bank 1)"},
    {"P0174", "System Too Lean  (Bank 2)"},
    {"P0175", "System Too Rich  (Bank 2)"},
    // Misfire
    {"P0300", "Random / Multiple Cylinder Misfire"},
    {"P0301", "Cylinder 1 Misfire"},
    {"P0302", "Cylinder 2 Misfire"},
    {"P0303", "Cylinder 3 Misfire"},
    {"P0304", "Cylinder 4 Misfire"},
    {"P0305", "Cylinder 5 Misfire"},
    {"P0306", "Cylinder 6 Misfire"},
    // Catalytic converter
    {"P0420", "Catalyst Efficiency Low  (Bank 1)"},
    {"P0430", "Catalyst Efficiency Low  (Bank 2)"},
    // Sensors
    {"P0100", "Mass Air Flow Sensor Circuit"},
    {"P0101", "MAF Sensor Range / Performance"},
    {"P0102", "MAF Sensor Circuit Low"},
    {"P0103", "MAF Sensor Circuit High"},
    {"P0110", "Intake Air Temp Sensor Circuit"},
    {"P0115", "Coolant Temp Sensor Circuit"},
    {"P0116", "Coolant Temp Range / Performance"},
    {"P0117", "Coolant Temp Sensor Low"},
    {"P0118", "Coolant Temp Sensor High"},
    {"P0120", "Throttle Position Sensor Circuit"},
    {"P0122", "Throttle Position Sensor Low"},
    {"P0123", "Throttle Position Sensor High"},
    {"P0128", "Coolant Below Thermostat Temp"},
    {"P0130", "O2 Sensor Circuit  (Bank 1, Sensor 1)"},
    {"P0136", "O2 Sensor Circuit  (Bank 1, Sensor 2)"},
    {"P0180", "Fuel Temp Sensor Circuit"},
    {"P0190", "Fuel Rail Pressure Sensor Circuit"},
    {"P0191", "Fuel Rail Pressure Sensor Performance"},
    // Crankshaft / camshaft
    {"P0335", "Crankshaft Position Sensor A Circuit"},
    {"P0336", "Crankshaft Position Sensor A Range"},
    {"P0340", "Camshaft Position Sensor A Circuit"},
    {"P0341", "Camshaft Position Sensor A Range"},
    {"P0365", "Camshaft Position Sensor B Circuit"},
    // VANOS / VVT
    {"P0010", "Intake Camshaft Actuator Circuit  (Bank 1)"},
    {"P0011", "Intake Camshaft Timing Over-Adv  (Bank 1)"},
    {"P0012", "Intake Camshaft Timing Over-Ret  (Bank 1)"},
    {"P0014", "Exhaust Camshaft Timing Over-Adv (Bank 1)"},
    {"P0015", "Exhaust Camshaft Timing Over-Ret (Bank 1)"},
    // Knock
    {"P0325", "Knock Sensor 1 Circuit  (Bank 1)"},
    {"P0330", "Knock Sensor 2 Circuit  (Bank 2)"},
    // Ignition / coil
    {"P0350", "Ignition Coil Primary/Secondary Circuit"},
    {"P0351", "Ignition Coil A Primary/Secondary"},
    {"P0352", "Ignition Coil B Primary/Secondary"},
    {"P0353", "Ignition Coil C Primary/Secondary"},
    {"P0354", "Ignition Coil D Primary/Secondary"},
    // EVAP
    {"P0440", "EVAP System Malfunction"},
    {"P0441", "EVAP System Incorrect Purge Flow"},
    {"P0442", "EVAP System Small Leak"},
    {"P0446", "EVAP Vent Control Circuit"},
    {"P0455", "EVAP System Large Leak"},
    {"P0456", "EVAP System Very Small Leak"},
    // EGR
    {"P0400", "EGR Flow Malfunction"},
    {"P0401", "EGR Insufficient Flow"},
    {"P0402", "EGR Excessive Flow"},
    // Turbo / boost (Cooper S / JCW)
    {"P0234", "Turbo Overboost Condition"},
    {"P0236", "Turbocharger Boost Sensor Performance"},
    {"P0237", "Turbocharger Boost Sensor Low"},
    {"P0238", "Turbocharger Boost Sensor High"},
    {"P0243", "Turbocharger Wastegate Solenoid A"},
    {"P0299", "Turbocharger Underboost Condition"},
    // Battery / charging
    {"P0560", "System Voltage Malfunction"},
    {"P0562", "System Voltage Low"},
    {"P0563", "System Voltage High"},
    {"P0601", "Internal Control Module Memory"},
    // BMW-specific / manufacturer
    {"P1340", "Camshaft / TDC Sensor (BMW)"},
    {"P1421", "Secondary Air Injection Valve (BMW)"},
    {nullptr, nullptr}  // table terminator
};

// Returns a human-readable description for a DTC code, or "Unknown Fault Code".
inline const char* dtc_description(const char* code) {
    for (int i = 0; DTC_TABLE[i].code != nullptr; i++) {
        if (strcmp(DTC_TABLE[i].code, code) == 0)
            return DTC_TABLE[i].desc;
    }
    return "Unknown Fault Code";
}
