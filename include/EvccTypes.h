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

// Closed-Loop Thermal Governor Status
struct ThermalGovernorStatus {
    bool enabled            = true;    // User toggleable via UI or API
    bool isDerated          = false;   // True if activeMaxc < baselineMaxc
    float baselineMaxc      = 0.0f;    // User-configured baseline target (e.g. 40.0A or 44.0A)
    float activeMaxc        = 0.0f;    // Active commanded setpoint after thermal derate
    float deratePercent     = 0.0f;    // Current reduction percentage (e.g. 30.0%)
    float peakTemp          = 0.0f;    // Highest charger heatsink temperature currently observed
    String hottestCharger   = "";      // Name of the charger currently running hottest
    String statusText       = "Optimal"; // "Optimal", "Warm", "Derated (-30%)", "Emergency Floor"
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
