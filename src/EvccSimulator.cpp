#include "EvccSimulator.h"
#include "EvccParser.h"
#include "esp_log.h"

static const char* TAG = "EvccSimulator";

EvccSimulator& EvccSimulator::getInstance() {
    static EvccSimulator instance;
    return instance;
}

EvccSimulator::EvccSimulator()
    : _enabled(false),
      _currentScenario(SIM_DUAL_CHARGE),
      _c1_v(116.4f),
      _c1_a(18.2f),
      _c1_wh(450.2f),
      _c1_temp(38.0f),
      _c2_v(116.5f),
      _c2_a(18.0f),
      _c2_wh(446.8f),
      _c2_temp(39.0f),
      _c3_v(116.3f),
      _c3_a(18.1f),
      _c3_wh(448.1f),
      _c3_temp(37.0f),
      _c4_v(116.6f),
      _c4_a(17.9f),
      _c4_wh(445.0f),
      _c4_temp(38.0f),
      _sysState("CHARGE"),
      _j1772State("LOCKED"),
      _cellLoop("OK"),
      _proximity("EVSE Connected and locked"),
      _buzzer("OFF"),
      _simUptimeSec(1840),
      _simMaxv(134.4f),
      _simMaxc(80.0f),
      _simTermc(1.5f),
      _simTermt(360.0f),
      _traceCharger(true),
      _traceState(true),
      _traceCan(false),
      _lastTickMs(0),
      _lastTraceChargerMs(0),
      _lastTraceStateMs(0),
      _lastTraceCanMs(0) {
}

void EvccSimulator::begin() {
    applyScenario(SIM_QUAD_CHARGE);
}

void EvccSimulator::setEnabled(bool enabled) {
    _enabled = enabled;
    ESP_LOGI(TAG, "EVCC Simulation %s", enabled ? "ENABLED" : "DISABLED");
    if (enabled) {
        emitLine("[Simulator] EVCC Serial Port Simulator Activated");
        emitLine("evcc> ");
    } else {
        emitLine("[Simulator] EVCC Serial Port Simulator Deactivated (Live Hardware Active)");
    }
}

void EvccSimulator::clearAllFaults() {
    _c1_faults.reset();
    _c2_faults.reset();
    _c3_faults.reset();
    _c4_faults.reset();
}


void EvccSimulator::applyScenario(SimScenario scenario) {
    _currentScenario = scenario;
    clearAllFaults();

    switch (scenario) {
        case SIM_QUAD_CHARGE:
            _c1_v = 116.4f;
            _c1_a = 20.0f;
            _c1_temp = 38.0f;
            _c1_wh = 450.2f;

            _c2_v = 116.5f;
            _c2_a = 20.0f;
            _c2_temp = 39.0f;
            _c2_wh = 446.8f;

            _c3_v = 116.3f;
            _c3_a = 20.0f;
            _c3_temp = 37.0f;
            _c3_wh = 448.1f;

            _c4_v = 116.6f;
            _c4_a = 20.0f;
            _c4_temp = 38.0f;
            _c4_wh = 445.0f;

            _sysState = "CHARGE";
            _j1772State = "LOCKED";
            _proximity = "EVSE Connected and locked";
            _cellLoop = "OK";
            _buzzer = "OFF";
            break;

        case SIM_DUAL_CHARGE:
            _c1_v = 116.4f;
            _c1_a = 20.0f;
            _c1_temp = 38.0f;
            _c1_wh = 450.2f;

            _c2_v = 116.5f;
            _c2_a = 20.0f;
            _c2_temp = 39.0f;
            _c2_wh = 446.8f;

            _c3_v = 0.0f;
            _c3_a = 0.0f;
            _c3_temp = 22.0f;
            _c3_wh = 0.0f;

            _c4_v = 0.0f;
            _c4_a = 0.0f;
            _c4_temp = 22.0f;
            _c4_wh = 0.0f;

            _sysState = "CHARGE";
            _j1772State = "LOCKED";
            _proximity = "EVSE Connected and locked";
            _cellLoop = "OK";
            _buzzer = "OFF";
            break;

        case SIM_SINGLE_CHARGE:
            _c1_v = 116.4f;
            _c1_a = 20.0f;
            _c1_temp = 38.0f;
            _c1_wh = 520.0f;

            _c2_v = 0.0f;
            _c2_a = 0.0f;
            _c2_temp = 22.0f;
            _c2_wh = 0.0f;

            _c3_v = 0.0f;
            _c3_a = 0.0f;
            _c3_temp = 22.0f;
            _c3_wh = 0.0f;

            _c4_v = 0.0f;
            _c4_a = 0.0f;
            _c4_temp = 22.0f;
            _c4_wh = 0.0f;

            _sysState = "CHARGE";
            _j1772State = "LOCKED";
            _proximity = "EVSE Connected and locked";
            _cellLoop = "OK";
            break;

        case SIM_TAPERING:
            _c1_v = 133.8f;
            _c1_a = 2.0f;
            _c1_temp = 42.0f;
            _c1_wh = 14520.0f;

            _c2_v = 133.8f;
            _c2_a = 2.0f;
            _c2_temp = 41.0f;
            _c2_wh = 14210.0f;

            _c3_v = 133.8f;
            _c3_a = 2.0f;
            _c3_temp = 40.5f;
            _c3_wh = 14350.0f;

            _c4_v = 133.8f;
            _c4_a = 2.0f;
            _c4_temp = 41.5f;
            _c4_wh = 14280.0f;

            _sysState = "FINISH CHARGE";
            _j1772State = "LOCKED";
            _proximity = "EVSE Connected and locked";
            _cellLoop = "OK";
            break;

        case SIM_OVERTEMP_FAULT:
            _c1_v = 116.0f;
            _c1_a = 0.0f;
            _c1_temp = 68.0f;
            _c1_faults.overtemp = true;
            _c1_faults.not_charging = true;

            _c2_v = 116.0f;
            _c2_a = 0.0f;
            _c2_temp = 66.0f;
            _c2_faults.overtemp = true;
            _c2_faults.not_charging = true;

            _c3_v = 116.0f;
            _c3_a = 0.0f;
            _c3_temp = 67.0f;
            _c3_faults.overtemp = true;
            _c3_faults.not_charging = true;

            _c4_v = 116.0f;
            _c4_a = 0.0f;
            _c4_temp = 65.0f;
            _c4_faults.overtemp = true;
            _c4_faults.not_charging = true;

            _sysState = "FAULT";
            _j1772State = "LOCKED";
            _buzzer = "ON";
            break;

        case SIM_CAN_RXERR:
            _c1_faults.rxerr = true;
            _c2_faults.rxerr = true;
            _c3_faults.rxerr = true;
            _c4_faults.rxerr = true;
            _c1_a = 0.0f;
            _c2_a = 0.0f;
            _c3_a = 0.0f;
            _c4_a = 0.0f;
            _sysState = "FAULT";
            _buzzer = "ON";
            break;

        case SIM_INPUT_VOLTAGE_ERR:
            _c1_faults.input_voltage_err = true;
            _c1_faults.not_charging = true;
            _c2_faults.input_voltage_err = true;
            _c2_faults.not_charging = true;
            _c3_faults.input_voltage_err = true;
            _c3_faults.not_charging = true;
            _c4_faults.input_voltage_err = true;
            _c4_faults.not_charging = true;
            _c1_a = 0.0f;
            _c2_a = 0.0f;
            _c3_a = 0.0f;
            _c4_a = 0.0f;
            _sysState = "FAULT";
            break;

        case SIM_PACK_VOLTAGE_ERR:
            _c1_faults.pack_voltage_err = true;
            _c1_faults.not_charging = true;
            _c2_faults.pack_voltage_err = true;
            _c2_faults.not_charging = true;
            _c3_faults.pack_voltage_err = true;
            _c3_faults.not_charging = true;
            _c4_faults.pack_voltage_err = true;
            _c4_faults.not_charging = true;
            _c1_a = 0.0f;
            _c2_a = 0.0f;
            _c3_a = 0.0f;
            _c4_a = 0.0f;
            _sysState = "FAULT";
            break;

        case SIM_STANDBY:
            _c1_v = 0.0f;
            _c1_a = 0.0f;
            _c1_temp = 24.0f;
            _c2_v = 0.0f;
            _c2_a = 0.0f;
            _c2_temp = 24.0f;
            _c3_v = 0.0f;
            _c3_a = 0.0f;
            _c3_temp = 24.0f;
            _c4_v = 0.0f;
            _c4_a = 0.0f;
            _c4_temp = 24.0f;
            _sysState = "STANDBY";
            _j1772State = "CONNECTED";
            _proximity = "EVSE Connected";
            _cellLoop = "OK";
            _buzzer = "OFF";
            break;

        case SIM_CUSTOM:
        default:
            break;
    }
}

void EvccSimulator::setCharger1Values(float voltage, float current, float temperature) {
    _c1_v = voltage;
    _c1_a = current;
    _c1_temp = temperature;
    _currentScenario = SIM_CUSTOM;
}

void EvccSimulator::setCharger2Values(float voltage, float current, float temperature) {
    _c2_v = voltage;
    _c2_a = current;
    _c2_temp = temperature;
    _currentScenario = SIM_CUSTOM;
}

void EvccSimulator::setCharger3Values(float voltage, float current, float temperature) {
    _c3_v = voltage;
    _c3_a = current;
    _c3_temp = temperature;
    _currentScenario = SIM_CUSTOM;
}

void EvccSimulator::setCharger4Values(float voltage, float current, float temperature) {
    _c4_v = voltage;
    _c4_a = current;
    _c4_temp = temperature;
    _currentScenario = SIM_CUSTOM;
}

void EvccSimulator::setChargerValues(uint8_t chargerNum, float voltage, float current, float temperature) {
    if (chargerNum == 1) setCharger1Values(voltage, current, temperature);
    else if (chargerNum == 2) setCharger2Values(voltage, current, temperature);
    else if (chargerNum == 3) setCharger3Values(voltage, current, temperature);
    else if (chargerNum == 4) setCharger4Values(voltage, current, temperature);
}

void EvccSimulator::setChargerFault(uint8_t chargerNum, const String& faultKey, bool value) {
    ChargerFaults* tgt = &_c1_faults;
    if (chargerNum == 2) tgt = &_c2_faults;
    else if (chargerNum == 3) tgt = &_c3_faults;
    else if (chargerNum == 4) tgt = &_c4_faults;

    if (faultKey == "rxerr") tgt->rxerr = value;
    else if (faultKey == "hwfail") tgt->hwfail = value;
    else if (faultKey == "overtemp") tgt->overtemp = value;
    else if (faultKey == "not_charging") tgt->not_charging = value;
    else if (faultKey == "input_voltage_err") tgt->input_voltage_err = value;
    else if (faultKey == "pack_voltage_err") tgt->pack_voltage_err = value;
    _currentScenario = SIM_CUSTOM;
}

void EvccSimulator::setSystemState(const String& state, const String& j1772) {
    if (state.length() > 0) _sysState = state;
    if (j1772.length() > 0) _j1772State = j1772;
    _currentScenario = SIM_CUSTOM;
}

void EvccSimulator::emitLine(const String& line) {
    EvccParser::getInstance().injectLine(line);
}

void EvccSimulator::injectRawLine(const String& line) {
    emitLine(line);
}

void EvccSimulator::emitTraceChargerLines() {
    // Charger 1 (tsm2500 / ID 40)
    String line1 = "tsm2500: V=" + String(_c1_v, 1) + ", A=" + String(_c1_a, 1) +
                   ", W=" + String((int)(_c1_v * _c1_a)) + ", Wh=" + String(_c1_wh, 1) +
                   ", TMP=" + String((int)_c1_temp) + "C";
    if (_c1_faults.rxerr) line1 += ", rxerr";
    if (_c1_faults.hwfail) line1 += ", hwfail";
    if (_c1_faults.overtemp) line1 += ", overtemp";
    if (_c1_faults.not_charging) line1 += ", not charging";
    if (_c1_faults.input_voltage_err) line1 += ", input voltage err";
    if (_c1_faults.pack_voltage_err) line1 += ", pack voltage err";
    emitLine(line1);

    // Charger 2 (tsm2500_41 / ID 41)
    String line2 = "tsm2500_41: V=" + String(_c2_v, 1) + ", A=" + String(_c2_a, 1) +
                   ", W=" + String((int)(_c2_v * _c2_a)) + ", Wh=" + String(_c2_wh, 1) +
                   ", TMP=" + String((int)_c2_temp) + "C";
    if (_c2_faults.rxerr) line2 += ", rxerr";
    if (_c2_faults.hwfail) line2 += ", hwfail";
    if (_c2_faults.overtemp) line2 += ", overtemp";
    if (_c2_faults.not_charging) line2 += ", not charging";
    if (_c2_faults.input_voltage_err) line2 += ", input voltage err";
    if (_c2_faults.pack_voltage_err) line2 += ", pack voltage err";
    emitLine(line2);

    // Charger 3 (tsm2500_42 / ID 42)
    String line3 = "tsm2500_42: V=" + String(_c3_v, 1) + ", A=" + String(_c3_a, 1) +
                   ", W=" + String((int)(_c3_v * _c3_a)) + ", Wh=" + String(_c3_wh, 1) +
                   ", TMP=" + String((int)_c3_temp) + "C";
    if (_c3_faults.rxerr) line3 += ", rxerr";
    if (_c3_faults.hwfail) line3 += ", hwfail";
    if (_c3_faults.overtemp) line3 += ", overtemp";
    if (_c3_faults.not_charging) line3 += ", not charging";
    if (_c3_faults.input_voltage_err) line3 += ", input voltage err";
    if (_c3_faults.pack_voltage_err) line3 += ", pack voltage err";
    emitLine(line3);

    // Charger 4 (tsm2500_43 / ID 43)
    String line4 = "tsm2500_43: V=" + String(_c4_v, 1) + ", A=" + String(_c4_a, 1) +
                   ", W=" + String((int)(_c4_v * _c4_a)) + ", Wh=" + String(_c4_wh, 1) +
                   ", TMP=" + String((int)_c4_temp) + "C";
    if (_c4_faults.rxerr) line4 += ", rxerr";
    if (_c4_faults.hwfail) line4 += ", hwfail";
    if (_c4_faults.overtemp) line4 += ", overtemp";
    if (_c4_faults.not_charging) line4 += ", not charging";
    if (_c4_faults.input_voltage_err) line4 += ", input voltage err";
    if (_c4_faults.pack_voltage_err) line4 += ", pack voltage err";
    emitLine(line4);
}


void EvccSimulator::emitTraceStateLine() {
    String line = "j1772=" + _j1772State + ", new state=" + _sysState + ", term rsn=0";
    emitLine(line);
}

void EvccSimulator::emitTraceCanLine() {
    char buf[128];
    snprintf(buf, sizeof(buf), "can rx: id=0x18FF50E5 len=8 d=28 %02X %02X %02X 00 00 00 00",
             (uint8_t)_c1_v, (uint8_t)(_c1_a * 10), (uint8_t)_c1_temp);
    emitLine(String(buf));
}

void EvccSimulator::handleCommand(const String& cmd) {
    String c = cmd;
    c.trim();
    String cLower = c;
    cLower.toLowerCase();

    if (cLower == "show" || cLower == "status") {
        uint32_t hrs = _simUptimeSec / 3600;
        uint32_t mins = (_simUptimeSec % 3600) / 60;
        uint32_t secs = _simUptimeSec % 60;
        char uptimeBuf[32];
        snprintf(uptimeBuf, sizeof(uptimeBuf), "%luh %lum %lus", (unsigned long)hrs, (unsigned long)mins, (unsigned long)secs);

        emitLine("state : " + _sysState);
        emitLine("cell loop: " + _cellLoop);
        emitLine("proximity: " + _proximity);
        emitLine("buzzer : " + _buzzer);
        emitLine("uptime : " + String(uptimeBuf));
        emitLine(String("OUT1 : ") + ((_sysState == "CHARGE") ? "ON" : "OFF"));
        emitLine(String("OUT2 : ") + ((_sysState == "CHARGE") ? "ON" : "OFF"));
        emitLine("OUT3 : OFF");
        emitLine("J1772 : duty cycle=30.0%, line current available=18.0A");
        emitLine("evcc> ");
    } else if (cLower == "show config" || cLower == "config") {
        emitLine("charger : tsm2500");
        emitLine("charger2 : tsm2500_41");
        emitLine("charger3 : tsm2500_42");
        emitLine("charger4 : tsm2500_43");
        emitLine("maxv : " + String(_simMaxv, 1));
        emitLine("maxc : " + String(_simMaxc, 1));
        emitLine("termc : " + String(_simTermc, 1));
        emitLine("termt : " + String((int)_simTermt));
        emitLine("bms : loop");
        emitLine("canbr : 250k");
        emitLine("options : canterm, topbalance");
        emitLine("evcc> ");
    } else if (cLower == "show history" || cLower == "history") {
        emitLine("=== Session History Log ===");
        emitLine("Session 1: Time=02:15:30, Wh=6450, Term=maxv reached");
        emitLine("Session 2: Time=01:42:10, Wh=4980, Term=user stopped");
        emitLine("Current: Elapsed=00:32:15, Wh=" + String(_c1_wh + _c2_wh + _c3_wh + _c4_wh, 1));
        emitLine("evcc> ");
    } else if (cLower == "trace charger") {
        _traceCharger = true;
        emitLine("trace charger enabled");
        emitLine("evcc> ");
    } else if (cLower == "trace state") {
        _traceState = true;
        emitLine("trace state enabled");
        emitLine("evcc> ");
    } else if (cLower == "trace can") {
        _traceCan = true;
        emitLine("trace can enabled");
        emitLine("evcc> ");
    } else if (cLower == "trace off") {
        _traceCharger = false;
        _traceState = false;
        _traceCan = false;
        emitLine("trace off");
        emitLine("evcc> ");
    } else if (cLower.startsWith("set maxv ")) {
        _simMaxv = c.substring(9).toFloat();
        emitLine("maxv set to " + String(_simMaxv, 1));
        emitLine("evcc> ");
    } else if (cLower.startsWith("set maxc ")) {
        _simMaxc = c.substring(9).toFloat();
        emitLine("maxc set to " + String(_simMaxc, 1));
        emitLine("evcc> ");
    } else if (cLower.startsWith("set termc ")) {
        _simTermc = c.substring(10).toFloat();
        emitLine("termc set to " + String(_simTermc, 1));
        emitLine("evcc> ");
    } else {
        emitLine("OK: " + c);
        emitLine("evcc> ");
    }
}

void EvccSimulator::process() {
    if (!_enabled) return;

    uint32_t now = millis();

    // 1-second dynamic physics update
    if (now - _lastTickMs >= 1000) {
        _lastTickMs = now;
        _simUptimeSec++;

        if (_sysState == "CHARGE" && !_c1_faults.hasAnyFault() && !_c2_faults.hasAnyFault() && !_c3_faults.hasAnyFault() && !_c4_faults.hasAnyFault()) {
            // Wh accumulation
            _c1_wh += (_c1_v * _c1_a) / 3600.0f;
            _c2_wh += (_c2_v * _c2_a) / 3600.0f;
            _c3_wh += (_c3_v * _c3_a) / 3600.0f;
            _c4_wh += (_c4_v * _c4_a) / 3600.0f;

            // Slow voltage ramp
            if (_c1_v > 0.0f && _c1_v < _simMaxv) _c1_v += 0.02f;
            if (_c2_v > 0.0f && _c2_v < _simMaxv) _c2_v += 0.02f;
            if (_c3_v > 0.0f && _c3_v < _simMaxv) _c3_v += 0.02f;
            if (_c4_v > 0.0f && _c4_v < _simMaxv) _c4_v += 0.02f;

            // Tapering current when near maxv
            if (_c1_v >= (_simMaxv - 2.0f) && _c1_a > _simTermc) {
                _c1_a -= 0.05f;
                if (_c1_a < _simTermc) _c1_a = _simTermc;
            }
            if (_c2_v >= (_simMaxv - 2.0f) && _c2_a > _simTermc) {
                _c2_a -= 0.05f;
                if (_c2_a < _simTermc) _c2_a = _simTermc;
            }
            if (_c3_v >= (_simMaxv - 2.0f) && _c3_a > _simTermc) {
                _c3_a -= 0.05f;
                if (_c3_a < _simTermc) _c3_a = _simTermc;
            }
            if (_c4_v >= (_simMaxv - 2.0f) && _c4_a > _simTermc) {
                _c4_a -= 0.05f;
                if (_c4_a < _simTermc) _c4_a = _simTermc;
            }

            // Charge complete
            if (_c1_v >= _simMaxv && _c1_a <= _simTermc && _c2_a <= _simTermc && _c3_a <= _simTermc && _c4_a <= _simTermc) {
                _sysState = "FINISH CHARGE";
            }
        }
    }

    // Periodic trace streams
    if (_traceCharger && (now - _lastTraceChargerMs >= 1000)) {
        _lastTraceChargerMs = now;
        emitTraceChargerLines();
    }

    if (_traceState && (now - _lastTraceStateMs >= 3000)) {
        _lastTraceStateMs = now;
        emitTraceStateLine();
    }

    if (_traceCan && (now - _lastTraceCanMs >= 2000)) {
        _lastTraceCanMs = now;
        emitTraceCanLine();
    }
}
