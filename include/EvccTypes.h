#pragma once

#include <Arduino.h>

// Fault flags for TSM2500 / EVCC chargers
struct ChargerFaults {
    bool rxerr              = false; // Charger communication / CAN rx error
    bool hwfail             = false; // Hardware failure
    bool overtemp           = false; // Over-temperature condition
    bool not_charging       = false; // Not charging
    bool input_voltage_err  = false; // AC input voltage out of spec
    bool pack_voltage_err   = false; // DC battery pack voltage error

    bool hasAnyFault() const {
        return rxerr || hwfail || overtemp || not_charging || input_voltage_err || pack_voltage_err;
    }

    void reset() {
        rxerr = false;
        hwfail = false;
        overtemp = false;
        not_charging = false;
        input_voltage_err = false;
        pack_voltage_err = false;
    }
};

// Telemetry for a single charger
struct ChargerTelemetry {
    String name             = "";
    uint8_t canId           = 0;
    float voltage           = 0.0f; // Volts (V)
    float current           = 0.0f; // Amps (A)
    float watts             = 0.0f; // Watts (W)
    float wattHours         = 0.0f; // Watt-hours (Wh)
    float temperature       = 0.0f; // °C
    uint32_t lastUpdateMs   = 0;
    bool active             = false;
    ChargerFaults faults;
    float lastLoggedV       = -999.0f;
    float lastLoggedA       = -999.0f;
    uint32_t lastLoggedMs   = 0;

    void reset() {
        voltage = 0.0f;
        current = 0.0f;
        watts = 0.0f;
        wattHours = 0.0f;
        temperature = 0.0f;
        lastUpdateMs = 0;
        active = false;
        faults.reset();
        lastLoggedV = -999.0f;
        lastLoggedA = -999.0f;
        lastLoggedMs = 0;
    }
};

// EVCC System State & Indications
struct EvccSystemState {
    String state            = "UNKNOWN";    // DRIVE, CHARGE, FINISH CHARGE, FLOAT CHARGE, STANDBY, EVSE_WAIT, WARMUP
    String j1772State       = "UNKNOWN";    // LOCKED, CONNECTED, DISCONNECTED, WAITING FOR DISC
    String cellLoop         = "OK";         // OK, ERROR
    String proximityStatus  = "UNKNOWN";    // EVSE not connected, EVSE Connected and locked
    String buzzer           = "OFF";        // OFF, ON
    String uptimeStr        = "0h 0m 0s";
    String termReason       = "";           // Termination reason code/text
    float pilotDutyCycle    = 0.0f;         // J1772 Pilot duty cycle %
    float lineCurrentAvail  = 0.0f;         // Available Line Current (A)
    String out1             = "OFF";
    String out2             = "OFF";
    String out3             = "OFF";
    
    // EVCC Config Parameters (parsed from show config)
    float maxv              = 0.0f;
    float maxc              = 0.0f;
    float termc             = 0.0f;
    float termt             = 0.0f;
    String bmsType          = "";
    String canBaud          = "";
    String options          = "";
    
    // Serial link status
    bool evccOnline         = false;
    uint32_t lastRxMs       = 0;
    uint32_t lastTxMs       = 0;
    
    // Charge session duration
    uint32_t sessionElapsedSec = 0;
};

// Individual Charger Thermal Governor Status
struct ChargerGovernorStatus {
    bool isDerated          = false;
    float currentTemp       = 0.0f;
    float targetScale       = 1.0f;     // 1.0, 0.85, 0.70, 0.50, 0.30
    float targetAmps        = 0.0f;     // Safe allowed current allocated for this charger
    String statusText       = "Optimal"; // "Optimal (<65°C)", "Mild Derate (85%)", etc.
};

// Closed-Loop Thermal Governor Status
struct ThermalGovernorStatus {
    bool enabled            = true;    // User toggleable via UI or API
    bool isDerated          = false;   // True if activeMaxc < baselineMaxc
    float baselineMaxc      = 0.0f;    // User-configured baseline target (up to 80.0A for 4 chargers)
    float activeMaxc        = 0.0f;    // Active commanded setpoint after thermal derate
    float deratePercent     = 0.0f;    // Current reduction percentage (e.g. 30.0%)
    float peakTemp          = 0.0f;    // Highest charger heatsink temperature currently observed
    String hottestCharger   = "";      // Name of the charger currently running hottest
    String statusText       = "Optimal"; // "Optimal", "Warm", "Derated (-30%)", "Emergency Floor"
    ChargerGovernorStatus chargers[4];  // Individual throttling status per charger
};

// ============================================================================
// CC/CV Dynamic Tapering Profile & Runtime Status
// ============================================================================
#define CCCV_NUM_POINTS 5

struct CccvPoint {
    float voltage   = 0.0f; // Pack voltage threshold (V)
    float current   = 0.0f; // Target max charging current (A)

    CccvPoint() : voltage(0.0f), current(0.0f) {}
    CccvPoint(float v, float c) : voltage(v), current(c) {}
};

struct CccvProfile {
    bool enabled            = true;
    uint8_t cellCount       = 36;   // e.g. 36S Tesla pack
    bool smoothLinear       = false;// false = Discrete steps (saves EEPROM life!), true = Smooth linear ramp
    bool fastCutoff         = true; // true = Cleanly terminate immediately at voltage ceiling; false = Hold CV
    float terminationAmps   = 4.0f; // Current cutoff threshold (A)
    CccvPoint points[CCCV_NUM_POINTS];

    void setDefaultTesla36S() {
        enabled = true;
        cellCount = 36;
        smoothLinear = false;       // Default discrete steps to protect EEPROM
        fastCutoff = true;          // Clean immediate termination at top of charge
        terminationAmps = 4.0f;
        // 36S Conservative (4.10V / cell = 147.6V)
        points[0] = CccvPoint(143.3f, 50.0f); // Bulk CC (< 3.98V/cell)
        points[1] = CccvPoint(145.4f, 35.0f); // Initial Taper (4.04V/cell)
        points[2] = CccvPoint(146.5f, 20.0f); // Mid Taper (4.07V/cell)
        points[3] = CccvPoint(147.2f, 10.0f); // Fine Taper (4.09V/cell)
        points[4] = CccvPoint(147.6f,  4.0f); // Cutoff Ceiling (4.10V/cell)
    }
};

struct CccvGovernorStatus {
    bool enabled            = true;
    bool isTapering         = false;
    float packVoltage       = 0.0f; // Live measured pack voltage (V)
    float cellVoltageEquiv  = 0.0f; // Pack voltage / cellCount (V)
    float targetAmps        = 50.0f;// Target amps computed by CC/CV curve
    float activeCurrent     = 0.0f; // Total actual measured charging amps
    String phase            = "BULK_CC"; // BULK_CC, TAPER_1, TAPER_2, TAPER_3, BALANCE, COMPLETE
    String statusText       = "Bulk Constant Current";
    uint32_t writesThisSession = 0; // Number of 'set maxc' commands sent to EVCC this session
};

// Data point for real-time charge session histogram
struct SessionDataPoint {
    uint32_t elapsedSec     = 0;
    float c1_voltage        = 0.0f;
    float c1_current        = 0.0f;
    float c2_voltage        = 0.0f;
    float c2_current        = 0.0f;
    float c3_voltage        = 0.0f;
    float c3_current        = 0.0f;
    float c4_voltage        = 0.0f;
    float c4_current        = 0.0f;
    float total_watts       = 0.0f;
};

// WiFi & Network Configuration
struct NetworkConfig {
    String ssid             = "";
    String password         = "";
    bool useStaticIp        = false;
    IPAddress staticIp      = IPAddress(0, 0, 0, 0);
    IPAddress gateway       = IPAddress(0, 0, 0, 0);
    IPAddress subnet        = IPAddress(255, 255, 255, 0);
    IPAddress dns           = IPAddress(8, 8, 8, 8);
    bool apMode             = true;
};
