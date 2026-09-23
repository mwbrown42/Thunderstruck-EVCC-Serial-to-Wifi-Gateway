#include "EvccParser.h"
#include "USBSerialHost.h"
#include "EvccSimulator.h"
#include "EventLogger.h"
#include "CccvGovernor.h"

EvccParser& EvccParser::getInstance() {
    static EvccParser instance;
    return instance;
}

EvccParser::EvccParser()
    : _traceChargerActive(false),
      _traceCanActive(false),
      _traceStateActive(false),
      _currentLine(""),
      _sessionStartTime(0),
      _lastHistogramSampleMs(0),
      _lastTelemetryBroadcastMs(0),
      _capturingQuery(false),
      _queryTarget(""),
      _queryBuffer(""),
      _queryStartMs(0),
      _rawLineCb(nullptr),
      _telemetryCb(nullptr),
      _queryRespCb(nullptr) {
    _chargers[0].name = "tsm2500";
    _chargers[0].canId = CHARGER1_CAN_ID;
    _chargers[1].name = "tsm2500_41";
    _chargers[1].canId = CHARGER2_CAN_ID;
    _chargers[2].name = "tsm2500_42";
    _chargers[2].canId = CHARGER3_CAN_ID;
    _chargers[3].name = "tsm2500_43";
    _chargers[3].canId = CHARGER4_CAN_ID;
}


void EvccParser::begin() {
    resetSessionHistory();
}

void EvccParser::resetSessionHistory() {
    _sessionHistory.clear();
    _sessionStartTime = millis();
    _lastHistogramSampleMs = millis();
}

void EvccParser::sendRawCommand(const String& cmd) {
    String trimmed = cmd;
    trimmed.trim();

    EventLogger::getInstance().log("CMD_TX", "%s", trimmed.c_str());

    if (trimmed.equalsIgnoreCase("log") || trimmed.equalsIgnoreCase("logs") || trimmed.equalsIgnoreCase("show log")) {
        String recent = EventLogger::getInstance().getRecentLogs(4096);
        if (_rawLineCb) {
            _rawLineCb("\n=== EVCC PERSISTENT FLASH LOG ===\n");
            _rawLineCb(recent);
            _rawLineCb("\n=== END OF LOG ===\n");
        }
        return;
    }
    if (trimmed.equalsIgnoreCase("clear log") || trimmed.equalsIgnoreCase("reset log")) {
        EventLogger::getInstance().clear();
        if (_rawLineCb) _rawLineCb("[Gateway] Flash log cleared.\n");
        return;
    }

    if (trimmed.equalsIgnoreCase("usb") || trimmed.equalsIgnoreCase("diag") || trimmed.equalsIgnoreCase("status")) {
        String diag = USBSerialHost::getInstance().getDiagnostics();
        if (_rawLineCb) _rawLineCb(diag);
        return;
    }
    if (trimmed.equalsIgnoreCase("uart on")) {
        USBSerialHost::getInstance().setUseHardwareUart(true);
        if (_rawLineCb) _rawLineCb("[Gateway] Hardware UART force enabled.\n");
        return;
    }
    if (trimmed.equalsIgnoreCase("uart off")) {
        USBSerialHost::getInstance().setUseHardwareUart(false);
        if (_rawLineCb) _rawLineCb("[Gateway] Hardware UART force disabled.\n");
        return;
    }

    String echoLine = "[TX] --> " + trimmed;
    if (_rawLineCb) _rawLineCb(echoLine);
    Serial.println(echoLine);

    if (EvccSimulator::getInstance().isEnabled()) {
        EvccSimulator::getInstance().handleCommand(trimmed);
    } else {
        bool connected = USBSerialHost::getInstance().isConnected();
        int devs = USBSerialHost::getInstance().getNumUsbDevices();
        if (!connected) {
            String warn = "[TX WARNING] EVCC USB Serial is NOT connected! (Phys devs: " + String(devs) + "). Sent to Hardware UART fallback (Pins 18/17).";
            if (_rawLineCb) _rawLineCb(warn);
            Serial.println(warn);
        }
        USBSerialHost::getInstance().println(trimmed);
    }
    _systemState.lastTxMs = millis();
}

void EvccParser::setTraceCharger(bool enable) {
    if (enable) {
        sendRawCommand("trace charger");
        _traceChargerActive = true;
    } else {
        sendRawCommand("trace off");
        _traceChargerActive = false;
    }
}

void EvccParser::setTraceCan(bool enable) {
    if (enable) {
        sendRawCommand("trace can");
        _traceCanActive = true;
    } else {
        sendRawCommand("trace off");
        _traceCanActive = false;
    }
}

void EvccParser::setTraceState(bool enable) {
    if (enable) {
        sendRawCommand("trace state");
        _traceStateActive = true;
    } else {
        sendRawCommand("trace off");
        _traceStateActive = false;
    }
}

void EvccParser::setTraceOff() {
    sendRawCommand("trace off");
    _traceChargerActive = false;
    _traceCanActive = false;
    _traceStateActive = false;
}

void EvccParser::setMaxVoltage(float volts) {
    char buf[32];
    snprintf(buf, sizeof(buf), "set maxv %.1f", volts);
    sendRawCommand(buf);
}

void EvccParser::setMaxCurrent(float amps) {
    _governor.baselineMaxc = amps;
    if (_governor.isDerated && _governor.deratePercent > 0.0f) {
        float targetScale = 1.0f - (_governor.deratePercent / 100.0f);
        _governor.activeMaxc = round((amps * targetScale) * 10.0f) / 10.0f;
        if (_governor.activeMaxc < 5.0f && amps >= 5.0f) _governor.activeMaxc = 5.0f;
        char buf[32];
        snprintf(buf, sizeof(buf), "set maxc %.1f", _governor.activeMaxc);
        sendRawCommand(buf);
        EventLogger::getInstance().log("GOVERNOR", "User updated baseline maxc to %.1fA (active throttled: %.1fA)", amps, _governor.activeMaxc);
    } else {
        _governor.activeMaxc = amps;
        char buf[32];
        snprintf(buf, sizeof(buf), "set maxc %.1f", amps);
        sendRawCommand(buf);
    }
}

void EvccParser::setTermCurrent(float amps) {
    char buf[32];
    snprintf(buf, sizeof(buf), "set termc %.1f", amps);
    sendRawCommand(buf);
}

void EvccParser::requestQuery(const String& queryType) {
    _capturingQuery = true;
    _queryTarget = queryType;
    _queryBuffer = "";
    _queryStartMs = millis();

    if (queryType == "status" || queryType == "show") {
        sendRawCommand("show");
    } else if (queryType == "config" || queryType == "show config") {
        sendRawCommand("show config");
    } else if (queryType == "history" || queryType == "show history") {
        sendRawCommand("show history");
    } else {
        sendRawCommand(queryType);
    }
}

static float extractValueAfter(const String& line, const String& key, char delimiter = ',') {
    int idx = line.indexOf(key);
    if (idx < 0) return 0.0f;
    idx += key.length();
    while (idx < line.length() && (line[idx] == ' ' || line[idx] == '=')) idx++;
    int endIdx = line.indexOf(delimiter, idx);
    if (endIdx < 0) endIdx = line.length();
    String valStr = line.substring(idx, endIdx);
    valStr.trim();
    // remove units if present (V, A, W, Wh, C, etc.)
    while (valStr.length() > 0 && !isDigit(valStr[valStr.length()-1]) && valStr[valStr.length()-1] != '.') {
        valStr.remove(valStr.length()-1);
    }
    return valStr.toFloat();
}

static void checkFaultFlags(const String& lineLower, float current, ChargerFaults& faults) {
    faults.rxerr = (lineLower.indexOf("rxerr") >= 0);
    faults.hwfail = (lineLower.indexOf("hwfail") >= 0);
    faults.overtemp = (lineLower.indexOf("overtemp") >= 0);
    // If current is actively flowing (> 0.5A), the charger is definitively charging.
    // "not charging" is only flagged if reported in the line AND current is near zero.
    faults.not_charging = (lineLower.indexOf("not charging") >= 0 && current < 0.5f);
    faults.input_voltage_err = (lineLower.indexOf("input voltage err") >= 0);
    faults.pack_voltage_err = (lineLower.indexOf("pack voltage err") >= 0);
}

void EvccParser::parseTraceChargerLine(const String& line) {
    String lineLower = line;
    lineLower.toLowerCase();

    // Determine which of the 4 chargers this line belongs to:
    int chargerIdx = 0;
    if (lineLower.indexOf("tsm2500_43") >= 0 || lineLower.indexOf("charger4") >= 0 || lineLower.indexOf("elcon") >= 0) {
        chargerIdx = 3;
    } else if (lineLower.indexOf("tsm2500_42") >= 0 || lineLower.indexOf("charger3") >= 0) {
        chargerIdx = 2;
    } else if (lineLower.indexOf("tsm2500_41") >= 0 || lineLower.indexOf("charger2") >= 0) {
        chargerIdx = 1;
    } else {
        chargerIdx = 0;
    }

    ChargerTelemetry& tgt = _chargers[chargerIdx];
    tgt.active = true;
    tgt.lastUpdateMs = millis();

    // Parse V=, A=, W=, Wh=, TMP=
    if (line.indexOf("V=") >= 0 || line.indexOf("V =") >= 0) {
        tgt.voltage = extractValueAfter(line, "V=");
        if (tgt.voltage == 0.0f) tgt.voltage = extractValueAfter(line, "V =");
    }
    if (line.indexOf("A=") >= 0 || line.indexOf("A =") >= 0) {
        tgt.current = extractValueAfter(line, "A=");
        if (tgt.current == 0.0f) tgt.current = extractValueAfter(line, "A =");
    }
    if (line.indexOf("W=") >= 0 || line.indexOf("W =") >= 0) {
        tgt.watts = extractValueAfter(line, "W=");
        if (tgt.watts == 0.0f) tgt.watts = extractValueAfter(line, "W =");
    }
    if (line.indexOf("Wh=") >= 0 || line.indexOf("Wh =") >= 0) {
        tgt.wattHours = extractValueAfter(line, "Wh=");
        if (tgt.wattHours == 0.0f) tgt.wattHours = extractValueAfter(line, "Wh =");
    }
    if (line.indexOf("TMP =") >= 0 || line.indexOf("TMP=") >= 0 || line.indexOf("TMP ") >= 0) {
        tgt.temperature = extractValueAfter(line, "TMP");
    }

    // Check for fault strings directly reflecting this frame's status
    checkFaultFlags(lineLower, tgt.current, tgt.faults);

    // Delta-logging: Log volts and amps as they ramp up and ramp down,
    // filtering out 0.1A-0.2A sensor noise jitter during steady-state.
    bool hasFault = tgt.faults.hasAnyFault() || lineLower.indexOf("err") >= 0 || lineLower.indexOf("fail") >= 0;
    float deltaV = fabs(tgt.voltage - tgt.lastLoggedV);
    float deltaA = fabs(tgt.current - tgt.lastLoggedA);
    uint32_t now = millis();
    bool timeHeartbeat = (now - tgt.lastLoggedMs >= 60000); // 1-min periodic heartbeat during steady charge

    if (hasFault || deltaA >= 0.8f || deltaV >= 1.0f || timeHeartbeat) {
        tgt.lastLoggedV = tgt.voltage;
        tgt.lastLoggedA = tgt.current;
        tgt.lastLoggedMs = now;
        EventLogger::getInstance().log("EVCC_RX", "%s", line.c_str());
    }
}

void EvccParser::parseTraceStateLine(const String& line) {
    // Parse j1772 state
    int jIdx = line.indexOf("j1772=");
    if (jIdx >= 0) {
        String jState = line.substring(jIdx + 6);
        jState.trim();
        _systemState.j1772State = jState;
    }

    // Parse new state
    int stateIdx = line.indexOf("new state=");
    if (stateIdx >= 0) {
        int endIdx = line.indexOf(",", stateIdx);
        if (endIdx < 0) endIdx = line.length();
        String newState = line.substring(stateIdx + 10, endIdx);
        newState.trim();
        
        // If transitioning into CHARGE from another state, start fresh histogram session
        if (newState == "CHARGE" && _systemState.state != "CHARGE") {
            resetSessionHistory();
            CccvGovernor::getInstance().resetSessionWrites();
            CccvGovernor::getInstance().resetCompletion();
        }
        _systemState.state = newState;
    }

    // Parse term rsn
    int rsnIdx = line.indexOf("term rsn=");
    if (rsnIdx >= 0) {
        String rsn = line.substring(rsnIdx + 9);
        rsn.trim();
        if (rsn != "0") {
            _systemState.termReason = rsn;
        }
    }
}

void EvccParser::parseShowStatusLine(const String& line) {
    String lineTrim = line;
    lineTrim.trim();

    if (lineTrim.startsWith("state :")) {
        _systemState.state = lineTrim.substring(7);
        _systemState.state.trim();
    } else if (lineTrim.startsWith("cell loop:")) {
        _systemState.cellLoop = lineTrim.substring(10);
        _systemState.cellLoop.trim();
    } else if (lineTrim.startsWith("proximity:")) {
        _systemState.proximityStatus = lineTrim.substring(10);
        _systemState.proximityStatus.trim();
    } else if (lineTrim.startsWith("buzzer :")) {
        _systemState.buzzer = lineTrim.substring(8);
        _systemState.buzzer.trim();
    } else if (lineTrim.startsWith("uptime :")) {
        _systemState.uptimeStr = lineTrim.substring(8);
        _systemState.uptimeStr.trim();
    } else if (lineTrim.startsWith("OUT1 :")) {
        _systemState.out1 = lineTrim.substring(6);
        _systemState.out1.trim();
    } else if (lineTrim.startsWith("OUT2 :")) {
        _systemState.out2 = lineTrim.substring(6);
        _systemState.out2.trim();
    } else if (lineTrim.startsWith("OUT3 :")) {
        _systemState.out3 = lineTrim.substring(6);
        _systemState.out3.trim();
    } else if (lineTrim.startsWith("J1772 :")) {
        _systemState.pilotDutyCycle = extractValueAfter(lineTrim, "duty cycle=");
        _systemState.lineCurrentAvail = extractValueAfter(lineTrim, "line current available=");
    }
}

void EvccParser::parseShowConfigLine(const String& line) {
    String lineTrim = line;
    lineTrim.trim();

    if (lineTrim.startsWith("maxv :")) {
        _systemState.maxv = extractValueAfter(lineTrim, "maxv :");
    } else if (lineTrim.startsWith("maxc :")) {
        _systemState.maxc = extractValueAfter(lineTrim, "maxc :");
        if (_governor.baselineMaxc <= 0.0f || !_governor.isDerated) {
            _governor.baselineMaxc = _systemState.maxc;
            _governor.activeMaxc = _systemState.maxc;
        }
    } else if (lineTrim.startsWith("termc :")) {
        _systemState.termc = extractValueAfter(lineTrim, "termc :");
    } else if (lineTrim.startsWith("termt :")) {
        _systemState.termt = extractValueAfter(lineTrim, "termt :");
    } else if (lineTrim.startsWith("bms :")) {
        _systemState.bmsType = lineTrim.substring(5);
        _systemState.bmsType.trim();
    } else if (lineTrim.startsWith("canbr :")) {
        _systemState.canBaud = lineTrim.substring(7);
        _systemState.canBaud.trim();
    } else if (lineTrim.startsWith("options :")) {
        _systemState.options = lineTrim.substring(9);
        _systemState.options.trim();
    }
}

void EvccParser::recordSessionPoint() {
    SessionDataPoint pt;
    pt.elapsedSec = (millis() - _sessionStartTime) / 1000;
    pt.c1_voltage = _chargers[0].voltage;
    pt.c1_current = _chargers[0].current;
    pt.c2_voltage = _chargers[1].voltage;
    pt.c2_current = _chargers[1].current;
    pt.c3_voltage = _chargers[2].voltage;
    pt.c3_current = _chargers[2].current;
    pt.c4_voltage = _chargers[3].voltage;
    pt.c4_current = _chargers[3].current;
    pt.total_watts = (_chargers[0].voltage * _chargers[0].current) +
                     (_chargers[1].voltage * _chargers[1].current) +
                     (_chargers[2].voltage * _chargers[2].current) +
                     (_chargers[3].voltage * _chargers[3].current);

    if (_sessionHistory.size() >= HISTOGRAM_MAX_POINTS) {
        _sessionHistory.erase(_sessionHistory.begin());
    }
    _sessionHistory.push_back(pt);
}

void EvccParser::parseLine(const String& line) {
    _systemState.lastRxMs = millis();
    _systemState.evccOnline = true;

    // Charger telemetry lines are delta-filtered inside parseTraceChargerLine.
    // Raw CAN frames are skipped from persistent flash to prevent storage thrashing.
    bool isChargerLine = (line.indexOf("V=") >= 0 && (line.indexOf("A=") >= 0 || line.indexOf("W=") >= 0));
    bool isRoutineCan = (line.indexOf("18eb24") >= 0 || line.indexOf("18e54") >= 0);

    if (!isChargerLine && !isRoutineCan) {
        EventLogger::getInstance().log("EVCC_RX", "%s", line.c_str());
    }

    // Send to raw line callback for live terminal
    if (_rawLineCb) {
        _rawLineCb(line);
    }

    // Accumulate into query buffer if capturing momentary response
    if (_capturingQuery) {
        _queryBuffer += line + "\n";
        if (line.indexOf("evcc>") >= 0 || line.startsWith("evcc>")) {
            _capturingQuery = false;
            EventLogger::getInstance().log("QUERY", "Query '%s' response complete (%u bytes)", _queryTarget.c_str(), _queryBuffer.length());
            if (_queryRespCb) {
                _queryRespCb(_queryTarget, _queryBuffer);
            }
        }
    }

    // Classify and parse line
    if (line.indexOf("V=") >= 0 && (line.indexOf("A=") >= 0 || line.indexOf("W=") >= 0)) {
        parseTraceChargerLine(line);
    } else if (line.indexOf("j1772=") >= 0 || line.indexOf("state=") >= 0) {
        parseTraceStateLine(line);
    } else if (line.indexOf("state :") >= 0 || line.indexOf("cell loop:") >= 0 || line.indexOf("proximity:") >= 0 || line.indexOf("uptime :") >= 0) {
        parseShowStatusLine(line);
    } else if (line.indexOf("maxv :") >= 0 || line.indexOf("maxc :") >= 0 || line.indexOf("termc :") >= 0 || line.indexOf("options :") >= 0) {
        parseShowConfigLine(line);
    }
}

void EvccParser::injectLine(const String& line) {
    int start = 0;
    while (start < line.length()) {
        int nextNl = line.indexOf('\n', start);
        if (nextNl < 0) {
            String sub = line.substring(start);
            sub.trim();
            if (sub.length() > 0) parseLine(sub);
            break;
        } else {
            String sub = line.substring(start, nextNl);
            sub.trim();
            if (sub.length() > 0) parseLine(sub);
            start = nextNl + 1;
        }
    }
}

void EvccParser::process() {
    if (EvccSimulator::getInstance().isEnabled()) {
        EvccSimulator::getInstance().process();
    } else {
        USBSerialHost& serial = USBSerialHost::getInstance();
        serial.update();

        // Read incoming characters from USB Serial
        while (serial.available() > 0) {
            int c = serial.read();
            if (c < 0) break;

            char ch = (char)c;
            if (ch == '\r') continue;

            if (ch == '\n') {
                if (_currentLine.length() > 0) {
                    parseLine(_currentLine);
                    _currentLine = "";
                }
            } else {
                _currentLine += ch;
                // Check for prompt
                if (_currentLine.endsWith("evcc> ")) {
                    parseLine(_currentLine);
                    _currentLine = "";
                } else if (_currentLine.length() > 256) {
                    // safeguard long buffer
                    parseLine(_currentLine);
                    _currentLine = "";
                }
            }
        }
    }

    // Query capture timeout check (3 seconds)
    if (_capturingQuery && (millis() - _queryStartMs > 3000)) {
        _capturingQuery = false;
        EventLogger::getInstance().log("QUERY", "Query '%s' TIMED OUT after 3000ms", _queryTarget.c_str());
        if (_queryRespCb) {
            if (_queryBuffer.length() == 0) {
                _queryBuffer = "No response from EVCC within 3 seconds.\n" + USBSerialHost::getInstance().getDiagnostics();
            }
            _queryRespCb(_queryTarget, _queryBuffer);
        }
    }

    // Inactive charger watchdog: if no CAN message received from a charger for 3.5 seconds,
    // mark it inactive and zero its current/watts so stale data never lingers.
    uint32_t nowMs = millis();
    for (int i = 0; i < NUM_CHARGERS; i++) {
        if (_chargers[i].active && (nowMs - _chargers[i].lastUpdateMs > 3500)) {
            _chargers[i].active = false;
            _chargers[i].current = 0.0f;
            _chargers[i].watts = 0.0f;
            _chargers[i].faults.reset();
        }
    }

    // Session elapsed timer calculation
    bool anyChargerActive = false;
    for (int i = 0; i < NUM_CHARGERS; i++) {
        if (_chargers[i].active && _chargers[i].current > 0.5f) anyChargerActive = true;
    }
    bool isCharging = (anyChargerActive || _systemState.state.indexOf("CHARGE") >= 0);
    if (isCharging) {
        if (_chargeSessionStartMs == 0) {
            _chargeSessionStartMs = millis();
        }
        _systemState.sessionElapsedSec = (millis() - _chargeSessionStartMs) / 1000;
    } else {
        if (_chargeSessionStartMs != 0) {
            _chargeSessionStartMs = 0;
        }
    }

    // Periodic charging session histogram sampling
    if (millis() - _lastHistogramSampleMs >= HISTOGRAM_SAMPLE_MS) {
        _lastHistogramSampleMs = millis();
        // Record if any of the 4 chargers are active or state is CHARGE
        bool anyActive = false;
        for (int i = 0; i < NUM_CHARGERS; i++) {
            if (_chargers[i].active) anyActive = true;
        }
        if (anyActive || _systemState.state.indexOf("CHARGE") >= 0) {
            recordSessionPoint();
        }
    }

    // Closed-Loop Thermal Governor processing
    updateThermalGovernor();

    // Periodic telemetry broadcast
    if (millis() - _lastTelemetryBroadcastMs >= TELEMETRY_BROADCAST_MS) {
        _lastTelemetryBroadcastMs = millis();
        if (_telemetryCb) {
            _telemetryCb(_chargers, _systemState);
        }
    }
}

void EvccParser::setThermalGovernorEnabled(bool enable) {
    _governor.enabled = enable;
    EventLogger::getInstance().log("GOVERNOR", "Thermal Governor %s", enable ? "ENABLED" : "DISABLED");
    if (!enable && _governor.isDerated) {
        // Restore full baseline current immediately
        _governor.isDerated = false;
        _governor.deratePercent = 0.0f;
        _governor.statusText = "Disabled";
        if (_governor.baselineMaxc > 0.0f) {
            _governor.activeMaxc = _governor.baselineMaxc;
            char buf[32];
            snprintf(buf, sizeof(buf), "set maxc %.1f", _governor.baselineMaxc);
            sendRawCommand(buf);
            EventLogger::getInstance().log("GOVERNOR", "Restored baseline maxc %.1fA on disable", _governor.baselineMaxc);
        }
    }
}

void EvccParser::setGovernorMaxTemp(float temp) {
    if (temp < 45.0f) temp = 45.0f;
    if (temp > 84.0f) temp = 84.0f;
    _governor.maxTemp = temp;
    EventLogger::getInstance().log("GOVERNOR", "Max Temp cap set to %.1f°C", temp);
}

void EvccParser::updateThermalGovernor() {
    uint32_t now = millis();
    if (now - _lastGovernorCheckMs < 1000) return;
    _lastGovernorCheckMs = now;

    // Scan all chargers for activity, peak temperature, and active count
    float rawPeakT = 0.0f;
    String hottest = "";
    bool chargingActive = false;
    int numActiveChargers = 0;

    for (int i = 0; i < NUM_CHARGERS; i++) {
        bool isActive = _chargers[i].active || _chargers[i].current > 0.5f || _chargers[i].voltage > 20.0f;
        if (isActive) {
            numActiveChargers++;
        }
        if (_chargers[i].current > 0.5f || _systemState.state.indexOf("CHARGE") >= 0) {
            chargingActive = true;
        }
        if (_chargers[i].temperature > rawPeakT) {
            rawPeakT = _chargers[i].temperature;
            hottest = _chargers[i].name;
        }
    }
    if (numActiveChargers == 0) numActiveChargers = 2; // Default dual-charger baseline

    // Ensure baseline is established from parsed config or fallback to 80.0A
    if (_governor.baselineMaxc <= 0.0f) {
        if (_systemState.maxc > 0.0f) {
            _governor.baselineMaxc = _systemState.maxc;
            _governor.activeMaxc = _systemState.maxc;
        } else {
            _governor.baselineMaxc = 80.0f;
            _governor.activeMaxc = 80.0f;
        }
    }

    float basePerCharger = _governor.baselineMaxc / (float)numActiveChargers;

    if (!_governor.enabled) {
        _governor.statusText = "Disabled";
        _governor.deratePercent = 100.0f;
        _governor.activeMaxc = _governor.baselineMaxc;
        for (int i = 0; i < NUM_CHARGERS; i++) {
            _governor.chargers[i].isDerated = false;
            _governor.chargers[i].targetScale = 1.0f;
            _governor.chargers[i].targetAmps = basePerCharger;
            _governor.chargers[i].statusText = "Disabled";
        }
        return;
    }

    // 1. Calculate individual thermal governor status for EACH charger separately
    // TSM-2500 internal trip ceiling is 85°C.
    // Dynamic throttling knee based on user-configured maxTemp:
    float knee = _governor.maxTemp;
    if (knee < 45.0f) knee = 45.0f;
    if (knee > 83.0f) knee = 83.0f;

    float z1 = knee;
    float z2 = knee + (84.0f - knee) * 0.35f;
    float z3 = knee + (84.0f - knee) * 0.70f;
    float z4 = 84.0f;
    float recov = knee - 4.0f;

    float minAllowedScale = 1.0f;
    int limitingChargerIdx = -1;

    for (int i = 0; i < NUM_CHARGERS; i++) {
        float rawT = _chargers[i].temperature;
        if (_filteredChargerTemp[i] <= 0.0f || !chargingActive) {
            _filteredChargerTemp[i] = rawT;
        } else {
            _filteredChargerTemp[i] = (_filteredChargerTemp[i] * 2.0f + rawT) / 3.0f;
        }
        float chTemp = _filteredChargerTemp[i];
        _governor.chargers[i].currentTemp = round(chTemp * 10.0f) / 10.0f;

        float chScale = 1.0f;
        String chStatus = "Optimal";

        if (chTemp >= z4) {
            chScale = 0.30f;
            char sbuf[48];
            snprintf(sbuf, sizeof(sbuf), "Emergency Floor (30%% @ %.0f°C)", chTemp);
            chStatus = String(sbuf);
        } else if (chTemp >= z3) {
            chScale = 0.50f;
            char sbuf[48];
            snprintf(sbuf, sizeof(sbuf), "Heavy Derate (50%% @ %.0f°C)", chTemp);
            chStatus = String(sbuf);
        } else if (chTemp >= z2) {
            chScale = 0.70f;
            char sbuf[48];
            snprintf(sbuf, sizeof(sbuf), "Moderate Derate (70%% @ %.0f°C)", chTemp);
            chStatus = String(sbuf);
        } else if (chTemp >= z1) {
            chScale = 0.85f;
            char sbuf[48];
            snprintf(sbuf, sizeof(sbuf), "Mild Derate (85%% @ %.0f°C)", chTemp);
            chStatus = String(sbuf);
        } else if (chTemp <= recov) {
            chScale = 1.0f;
            char sbuf[48];
            snprintf(sbuf, sizeof(sbuf), "Optimal (%.0f°C)", chTemp);
            chStatus = String(sbuf);
        } else {
            // In hysteresis band (recov to z1): maintain current individual scale
            if (_governor.chargers[i].isDerated) {
                chScale = _governor.chargers[i].targetScale;
                chStatus = _governor.chargers[i].statusText;
            } else {
                chScale = 1.0f;
                char sbuf[48];
                snprintf(sbuf, sizeof(sbuf), "Optimal (%.0f°C)", chTemp);
                chStatus = String(sbuf);
            }
        }

        _governor.chargers[i].isDerated = (chScale < 0.99f);
        _governor.chargers[i].targetScale = chScale;
        _governor.chargers[i].targetAmps = round((basePerCharger * chScale) * 10.0f) / 10.0f;
        _governor.chargers[i].statusText = chStatus;

        // Check against active chargers for system-level constraint
        bool isActive = _chargers[i].active || _chargers[i].current > 0.5f || numActiveChargers <= 2;
        if (isActive && chScale < minAllowedScale) {
            minAllowedScale = chScale;
            limitingChargerIdx = i;
        }
    }

    _governor.peakTemp = round(rawPeakT * 10.0f) / 10.0f;
    _governor.hottestCharger = hottest;

    if (!chargingActive && _governor.peakTemp < 60.0f) {
        if (_governor.isDerated) {
            _governor.isDerated = false;
            _governor.deratePercent = 100.0f;
            _governor.activeMaxc = _governor.baselineMaxc;
            _governor.statusText = "Optimal";
            char buf[32];
            snprintf(buf, sizeof(buf), "set maxc %.1f", _governor.baselineMaxc);
            sendRawCommand(buf);
            EventLogger::getInstance().log("GOVERNOR", "Restored baseline maxc %.1fA (Charging idle)", _governor.baselineMaxc);
        } else {
            _governor.statusText = "Optimal";
            _governor.deratePercent = 100.0f;
            _governor.activeMaxc = _governor.baselineMaxc;
        }
        return;
    }

    // 2. Compute CC/CV Tapering target current from measured pack voltage:
    float packV = 0.0f;
    float totalA = 0.0f;
    for (int i = 0; i < NUM_CHARGERS; i++) {
        if (_chargers[i].voltage > packV) {
            packV = _chargers[i].voltage;
        }
        totalA += _chargers[i].current;
    }
    float cccvTargetAmps = CccvGovernor::getInstance().update(packV, totalA, chargingActive);

    // 3. Determine target commanded total current for EVCC:
    // EVCC distributes maxc equally to all active chargers.
    // Thermal governor target:
    float thermalTargetAmps = round((_governor.baselineMaxc * minAllowedScale) * 10.0f) / 10.0f;
    if (thermalTargetAmps < 5.0f && _governor.baselineMaxc >= 5.0f) {
        thermalTargetAmps = 5.0f; // Absolute safe floor
    }

    // Arbitration: Take the MINIMUM of Thermal Governor and CC/CV Governor
    float targetAmps = thermalTargetAmps;
    bool cccvGoverning = false;
    if (CccvGovernor::getInstance().getProfile().enabled && cccvTargetAmps < thermalTargetAmps) {
        targetAmps = cccvTargetAmps;
        cccvGoverning = true;
    }

    bool isTerminating = (targetAmps <= 0.1f);
    bool needAdjustment = false;
    if (isTerminating) {
        needAdjustment = (_governor.activeMaxc > 0.0f);
    } else if (!CccvGovernor::getInstance().getProfile().smoothLinear) {
        needAdjustment = (fabs(targetAmps - _governor.activeMaxc) >= 1.0f);
    } else {
        needAdjustment = (fabs(targetAmps - _governor.activeMaxc) >= 0.5f);
    }

    bool isSteppingDown = (targetAmps < _governor.activeMaxc);
    bool isSteppingUp   = (targetAmps > _governor.activeMaxc);

    // Dwell time: When stepped down, hold derated level for at least 90s before stepping up!
    bool dwellSatisfied = (now - _lastDerateTimeMs >= 90000);

    // Emergency throttle (>=82°C) or CC/CV taper stepping down acts in 15s.
    // Immediate cutoff terminates without waiting.
    bool canAdjust = false;
    if (isTerminating) {
        canAdjust = true;
    } else if (_governor.peakTemp >= 82.0f && isSteppingDown) {
        canAdjust = true;
    } else if (isSteppingDown && (now - _lastGovernorAdjustMs >= 15000)) {
        canAdjust = true;
    } else if (isSteppingUp && (now - _lastGovernorAdjustMs >= 15000) && dwellSatisfied) {
        canAdjust = true;
    } else if (_governor.activeMaxc <= 0.0f) {
        canAdjust = true;
    }

    if (needAdjustment && canAdjust) {
        _lastGovernorAdjustMs = now;
        if (isSteppingDown) {
            _lastDerateTimeMs = now;
        }
        float prevMaxc = _governor.activeMaxc > 0.0f ? _governor.activeMaxc : _governor.baselineMaxc;
        _governor.activeMaxc = targetAmps;
        _governor.isDerated = (targetAmps < _governor.baselineMaxc - 0.5f);
        _governor.deratePercent = round((targetAmps / _governor.baselineMaxc) * 100.0f);

        if (cccvGoverning) {
            _governor.statusText = String("CC/CV: ") + CccvGovernor::getInstance().getStatus().statusText;
            EventLogger::getInstance().log("GOVERNOR", "CC/CV Taper active: %.1fA (Pack %.1fV, %s)",
                targetAmps, packV, CccvGovernor::getInstance().getStatus().phase.c_str());
        } else if (_governor.isDerated && limitingChargerIdx >= 0) {
            char statusBuf[64];
            snprintf(statusBuf, sizeof(statusBuf), "Thermal Derated (%d%% by %s @ %.0f°C)",
                     (int)_governor.deratePercent,
                     _chargers[limitingChargerIdx].name.c_str(),
                     _governor.chargers[limitingChargerIdx].currentTemp);
            _governor.statusText = String(statusBuf);
            EventLogger::getInstance().log("GOVERNOR", "Throttled maxc from %.1fA to %.1fA (%d%%) [%s at %.0fC]",
                prevMaxc, targetAmps, (int)_governor.deratePercent,
                _chargers[limitingChargerIdx].name.c_str(),
                _governor.chargers[limitingChargerIdx].currentTemp);
        } else {
            _governor.statusText = "Optimal";
            _governor.deratePercent = 100.0f;
            EventLogger::getInstance().log("GOVERNOR", "Restored baseline maxc %.1fA (Cool: %.0fC)", targetAmps, _governor.peakTemp);
        }

        // Send adjusted maxc to EVCC
        char cmdBuf[32];
        snprintf(cmdBuf, sizeof(cmdBuf), "set maxc %.1f", targetAmps);
        sendRawCommand(cmdBuf);
        CccvGovernor::getInstance().recordEepromWrite();
    } else {
        // Update live status text
        if (cccvGoverning) {
            _governor.statusText = String("CC/CV: ") + CccvGovernor::getInstance().getStatus().statusText;
        } else if (_governor.isDerated && limitingChargerIdx >= 0) {
            char statusBuf[64];
            snprintf(statusBuf, sizeof(statusBuf), "Thermal Derated (%d%% by %s @ %.0f°C)",
                     (int)_governor.deratePercent,
                     _chargers[limitingChargerIdx].name.c_str(),
                     _governor.chargers[limitingChargerIdx].currentTemp);
            _governor.statusText = String(statusBuf);
        } else if (_governor.peakTemp >= 75.0f) {
            _governor.statusText = "Warm (Monitoring)";
        } else {
            _governor.statusText = "Optimal";
        }
    }
}


