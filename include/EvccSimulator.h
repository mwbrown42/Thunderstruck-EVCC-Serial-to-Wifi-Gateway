#pragma once

#include <Arduino.h>
#include "EvccTypes.h"
#include "Config.h"

enum SimScenario {
    SIM_QUAD_CHARGE = 0,
    SIM_DUAL_CHARGE,
    SIM_SINGLE_CHARGE,
    SIM_TAPERING,
    SIM_OVERTEMP_FAULT,
    SIM_CAN_RXERR,
    SIM_INPUT_VOLTAGE_ERR,
    SIM_PACK_VOLTAGE_ERR,
    SIM_STANDBY,
    SIM_CUSTOM
};

class EvccSimulator {
public:
    static EvccSimulator& getInstance();

    void begin();
    void process();

    // Enable / Disable simulation
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // Preset Scenarios
    void applyScenario(SimScenario scenario);
    SimScenario getCurrentScenario() const { return _currentScenario; }

    // Live Value Adjustments
    void setCharger1Values(float voltage, float current, float temperature);
    void setCharger2Values(float voltage, float current, float temperature);
    void setCharger3Values(float voltage, float current, float temperature);
    void setCharger4Values(float voltage, float current, float temperature);
    void setChargerValues(uint8_t chargerNum, float voltage, float current, float temperature);
    void setChargerFault(uint8_t chargerNum, const String& faultKey, bool value);
    void clearAllFaults();

    // State Adjustments
    void setSystemState(const String& state, const String& j1772);

    // Command Simulation & Line Injection
    void handleCommand(const String& cmd);
    void injectRawLine(const String& line);

    // Getter for simulator state to report back to Web UI
    float getC1Voltage() const { return _c1_v; }
    float getC1Current() const { return _c1_a; }
    float getC1Temp() const { return _c1_temp; }
    const ChargerFaults& getC1Faults() const { return _c1_faults; }

    float getC2Voltage() const { return _c2_v; }
    float getC2Current() const { return _c2_a; }
    float getC2Temp() const { return _c2_temp; }
    const ChargerFaults& getC2Faults() const { return _c2_faults; }

    float getC3Voltage() const { return _c3_v; }
    float getC3Current() const { return _c3_a; }
    float getC3Temp() const { return _c3_temp; }
    const ChargerFaults& getC3Faults() const { return _c3_faults; }

    float getC4Voltage() const { return _c4_v; }
    float getC4Current() const { return _c4_a; }
    float getC4Temp() const { return _c4_temp; }
    const ChargerFaults& getC4Faults() const { return _c4_faults; }

private:
    EvccSimulator();

    void emitTraceChargerLines();
    void emitTraceStateLine();
    void emitTraceCanLine();
    void emitLine(const String& line);

    bool _enabled;
    SimScenario _currentScenario;

    // Simulation Data for 4 chargers
    float _c1_v;
    float _c1_a;
    float _c1_wh;
    float _c1_temp;
    ChargerFaults _c1_faults;

    float _c2_v;
    float _c2_a;
    float _c2_wh;
    float _c2_temp;
    ChargerFaults _c2_faults;

    float _c3_v;
    float _c3_a;
    float _c3_wh;
    float _c3_temp;
    ChargerFaults _c3_faults;

    float _c4_v;
    float _c4_a;
    float _c4_wh;
    float _c4_temp;
    ChargerFaults _c4_faults;

    String _sysState;
    String _j1772State;
    String _cellLoop;
    String _proximity;
    String _buzzer;
    uint32_t _simUptimeSec;

    float _simMaxv;
    float _simMaxc;
    float _simTermc;
    float _simTermt;

    bool _traceCharger;
    bool _traceState;
    bool _traceCan;

    uint32_t _lastTickMs;
    uint32_t _lastTraceChargerMs;
    uint32_t _lastTraceStateMs;
    uint32_t _lastTraceCanMs;
};
