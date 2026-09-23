#pragma once

#include <Arduino.h>
#include <vector>
#include "EvccTypes.h"
#include "Config.h"

typedef void (*RawLineCallback)(const String& line);
typedef void (*TelemetryCallback)(const ChargerTelemetry chargers[NUM_CHARGERS], const EvccSystemState& state);
typedef void (*QueryResponseCallback)(const String& queryType, const String& responseText);

class EvccParser {
public:
    static EvccParser& getInstance();

    void begin();
    void process(); // Call in loop to read and parse serial data

    // Command API
    void sendRawCommand(const String& cmd);
    void setTraceCharger(bool enable);
    void setTraceCan(bool enable);
    void setTraceState(bool enable);
    void setTraceOff();
    void setMaxVoltage(float volts);
    void setMaxCurrent(float amps);
    void setTermCurrent(float amps);
    void requestQuery(const String& queryType); // "show", "config", "history"
    void injectLine(const String& line); // Inject simulated or raw line into parser

    // Telemetry & State getters
    const ChargerTelemetry& getCharger(size_t idx) const { return _chargers[idx < NUM_CHARGERS ? idx : 0]; }
    const ChargerTelemetry& getCharger1() const { return _chargers[0]; }
    const ChargerTelemetry& getCharger2() const { return _chargers[1]; }
    const ChargerTelemetry& getCharger3() const { return _chargers[2]; }
    const ChargerTelemetry& getCharger4() const { return _chargers[3]; }
    const ChargerTelemetry* getAllChargers() const { return _chargers; }
    const EvccSystemState& getSystemState() const { return _systemState; }
    const std::vector<SessionDataPoint>& getSessionHistory() const { return _sessionHistory; }
    
    // Trace state flags
    bool isTraceChargerEnabled() const { return _traceChargerActive; }
    bool isTraceCanEnabled() const { return _traceCanActive; }
    bool isTraceStateEnabled() const { return _traceStateActive; }

    // Thermal Governor API
    const ThermalGovernorStatus& getThermalGovernorStatus() const { return _governor; }
    void setThermalGovernorEnabled(bool enable);
    bool isThermalGovernorEnabled() const { return _governor.enabled; }
    void setGovernorMaxTemp(float temp);
    float getGovernorMaxTemp() const { return _governor.maxTemp; }

    // Callbacks
    void setRawLineCallback(RawLineCallback cb) { _rawLineCb = cb; }
    void setTelemetryCallback(TelemetryCallback cb) { _telemetryCb = cb; }
    void setQueryResponseCallback(QueryResponseCallback cb) { _queryRespCb = cb; }

    void resetSessionHistory();

private:
    EvccParser();

    void parseLine(const String& line);
    void parseTraceChargerLine(const String& line);
    void parseTraceStateLine(const String& line);
    void parseShowStatusLine(const String& line);
    void parseShowConfigLine(const String& line);
    void recordSessionPoint();
    void updateThermalGovernor();

    ChargerTelemetry _chargers[NUM_CHARGERS];
    EvccSystemState _systemState;
    std::vector<SessionDataPoint> _sessionHistory;
    ThermalGovernorStatus _governor;
    uint32_t _lastGovernorCheckMs = 0;
    uint32_t _lastGovernorAdjustMs = 0;
    uint32_t _lastDerateTimeMs = 0;
    float _filteredPeakTemp = 0.0f;
    float _filteredChargerTemp[NUM_CHARGERS] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t _chargeSessionStartMs = 0;

    bool _traceChargerActive;
    bool _traceCanActive;
    bool _traceStateActive;

    String _currentLine;
    uint32_t _sessionStartTime;
    uint32_t _lastHistogramSampleMs;
    uint32_t _lastTelemetryBroadcastMs;

    // Momentary query capturing
    bool _capturingQuery;
    String _queryTarget;
    String _queryBuffer;
    uint32_t _queryStartMs;

    RawLineCallback _rawLineCb;
    TelemetryCallback _telemetryCb;
    QueryResponseCallback _queryRespCb;
};
