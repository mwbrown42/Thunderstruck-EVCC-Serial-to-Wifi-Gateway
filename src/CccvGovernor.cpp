#include "CccvGovernor.h"
#include "EventLogger.h"
#include <cmath>

CccvGovernor::CccvGovernor() {
    _profile.setDefaultTesla36S();
    _status.enabled = true;
    _status.targetAmps = _profile.points[0].current;
    _status.phase = "BULK_CC";
    _status.statusText = "Bulk Constant Current";
}

void CccvGovernor::begin() {
    loadFromPreferences();
    EventLogger::getInstance().log("CCCV", "Initialized CC/CV Governor (%s, %dS, Term: %.1fA)",
        _profile.enabled ? "ENABLED" : "DISABLED", _profile.cellCount, _profile.terminationAmps);
}

void CccvGovernor::loadFromPreferences() {
    if (!_prefs.begin("cccv", true)) {
        // First boot or preferences error, use default
        _profile.setDefaultTesla36S();
        return;
    }

    if (!_prefs.isKey("cfg_init")) {
        _prefs.end();
        saveToPreferences();
        return;
    }

    _profile.enabled = _prefs.getBool("enabled", true);
    _profile.cellCount = _prefs.getUChar("cell_count", 36);
    _profile.smoothLinear = _prefs.getBool("smooth_lin", false); // Default false: discrete steps to protect EEPROM
    _profile.fastCutoff = _prefs.getBool("fast_cut", true);      // Default true: clean immediate cutoff at ceiling
    _profile.terminationAmps = _prefs.getFloat("term_amps", 4.0f);

    for (int i = 0; i < CCCV_NUM_POINTS; i++) {
        char keyV[16], keyC[16];
        snprintf(keyV, sizeof(keyV), "p%d_v", i);
        snprintf(keyC, sizeof(keyC), "p%d_c", i);
        _profile.points[i].voltage = _prefs.getFloat(keyV, _profile.points[i].voltage);
        _profile.points[i].current = _prefs.getFloat(keyC, _profile.points[i].current);
    }
    _prefs.end();

    _status.enabled = _profile.enabled;
}

void CccvGovernor::saveToPreferences() {
    if (!_prefs.begin("cccv", false)) {
        EventLogger::getInstance().log("CCCV", "Failed to open NVS for saving CC/CV profile");
        return;
    }

    _prefs.putBool("cfg_init", true);
    _prefs.putBool("enabled", _profile.enabled);
    _prefs.putUChar("cell_count", _profile.cellCount);
    _prefs.putBool("smooth_lin", _profile.smoothLinear);
    _prefs.putBool("fast_cut", _profile.fastCutoff);
    _prefs.putFloat("term_amps", _profile.terminationAmps);

    for (int i = 0; i < CCCV_NUM_POINTS; i++) {
        char keyV[16], keyC[16];
        snprintf(keyV, sizeof(keyV), "p%d_v", i);
        snprintf(keyC, sizeof(keyC), "p%d_c", i);
        _prefs.putFloat(keyV, _profile.points[i].voltage);
        _prefs.putFloat(keyC, _profile.points[i].current);
    }
    _prefs.end();

    EventLogger::getInstance().log("CCCV", "Saved profile to Flash: 36S, %.1fV-%.1fV, %.1fA-%.1fA, FastCut=%s, Stepped=%s",
        _profile.points[0].voltage, _profile.points[CCCV_NUM_POINTS - 1].voltage,
        _profile.points[CCCV_NUM_POINTS - 1].current, _profile.points[0].current,
        _profile.fastCutoff ? "YES" : "NO", !_profile.smoothLinear ? "YES" : "NO");
}

void CccvGovernor::setProfile(const CccvProfile& profile) {
    _profile = profile;
    _status.enabled = profile.enabled;
    saveToPreferences();
}

void CccvGovernor::setEnabled(bool enabled) {
    _profile.enabled = enabled;
    _status.enabled = enabled;
    if (_prefs.begin("cccv", false)) {
        _prefs.putBool("enabled", enabled);
        _prefs.end();
    }
    EventLogger::getInstance().log("CCCV", "CC/CV Governor %s", enabled ? "ENABLED" : "DISABLED");
}

void CccvGovernor::loadPresetTesla36SConservative() {
    _profile.setDefaultTesla36S();
    saveToPreferences();
}

void CccvGovernor::loadPresetTesla36SStandard() {
    _profile.enabled = true;
    _profile.cellCount = 36;
    _profile.smoothLinear = false;
    _profile.fastCutoff = true;
    _profile.terminationAmps = 4.0f;
    // 36S Standard (4.15V / cell = 149.4V)
    _profile.points[0] = CccvPoint(145.0f, 50.0f); // 4.03V/cell
    _profile.points[1] = CccvPoint(147.0f, 35.0f); // 4.08V/cell
    _profile.points[2] = CccvPoint(148.0f, 20.0f); // 4.11V/cell
    _profile.points[3] = CccvPoint(148.8f, 10.0f); // 4.13V/cell
    _profile.points[4] = CccvPoint(149.4f,  4.0f); // 4.15V/cell
    saveToPreferences();
}

void CccvGovernor::loadPresetTesla36SMaxRange() {
    _profile.enabled = true;
    _profile.cellCount = 36;
    _profile.smoothLinear = false;
    _profile.fastCutoff = true;
    _profile.terminationAmps = 4.0f;
    // 36S Max Range (4.20V / cell = 151.2V)
    _profile.points[0] = CccvPoint(146.5f, 50.0f); // 4.07V/cell
    _profile.points[1] = CccvPoint(148.5f, 35.0f); // 4.12V/cell
    _profile.points[2] = CccvPoint(149.8f, 20.0f); // 4.16V/cell
    _profile.points[3] = CccvPoint(150.5f, 10.0f); // 4.18V/cell
    _profile.points[4] = CccvPoint(151.2f,  4.0f); // 4.20V/cell
    saveToPreferences();
}

float CccvGovernor::update(float rawPackVoltage, float totalCurrent, bool chargingActive) {
    if (!chargingActive) {
        _chargeCompleted = false; // Reset completion latch when charging stops
    }

    if (rawPackVoltage <= 20.0f) {
        // No valid pack connected
        _status.packVoltage = 0.0f;
        _status.cellVoltageEquiv = 0.0f;
        _status.phase = "STANDBY";
        _status.statusText = "Standby (No Pack Voltage)";
        _status.isTapering = false;
        _chargeCompleted = false;
        return _profile.points[0].current;
    }

    // 3-point EMA smoothing on pack voltage
    if (_filteredVoltage <= 20.0f || !chargingActive) {
        _filteredVoltage = rawPackVoltage;
    } else {
        _filteredVoltage = (_filteredVoltage * 2.0f + rawPackVoltage) / 3.0f;
    }

    _status.packVoltage = round(_filteredVoltage * 10.0f) / 10.0f;
    _status.cellVoltageEquiv = round((_filteredVoltage / (float)_profile.cellCount) * 1000.0f) / 1000.0f;
    _status.activeCurrent = totalCurrent;

    if (!_profile.enabled) {
        _status.phase = "DISABLED";
        _status.statusText = "CC/CV Governor Disabled";
        _status.isTapering = false;
        _chargeCompleted = false;
        _status.targetAmps = _profile.points[0].current;
        return _profile.points[0].current;
    }

    // If already latched as completed, stay at 0A until charging session resets
    if (_chargeCompleted) {
        _status.phase = "COMPLETE";
        _status.statusText = "Charge Complete (Cutoff Ceiling Reached)";
        _status.isTapering = true;
        _status.targetAmps = 0.0f;
        return 0.0f;
    }

    float target = _profile.points[0].current;

    // 1. Below first taper point -> Full bulk CC current
    if (_filteredVoltage < _profile.points[0].voltage) {
        target = _profile.points[0].current;
        _status.phase = "BULK_CC";
        char buf[64];
        snprintf(buf, sizeof(buf), "Bulk CC (%.1fV / %.3fV/cell)", _filteredVoltage, _status.cellVoltageEquiv);
        _status.statusText = String(buf);
        _status.isTapering = false;
    }
    // 2. Above top cutoff voltage -> Clean Immediate Cutoff
    else if (_filteredVoltage >= _profile.points[CCCV_NUM_POINTS - 1].voltage) {
        if (_profile.fastCutoff || (chargingActive && totalCurrent <= _profile.terminationAmps)) {
            _chargeCompleted = true;
            target = 0.0f;
            _status.phase = "COMPLETE";
            _status.statusText = "Charge Complete (Ceiling Reached - Fast Cutoff)";
            _status.isTapering = true;
        } else {
            target = _profile.points[CCCV_NUM_POINTS - 1].current;
            _status.phase = "BALANCE";
            char buf[64];
            snprintf(buf, sizeof(buf), "Balance Top-Off (%.1fA limit @ %.1fV)", target, _filteredVoltage);
            _status.statusText = String(buf);
            _status.isTapering = true;
        }
    }
    // 3. In between taper points -> interpolate or discrete step
    else {
        _status.isTapering = true;
        for (int i = 0; i < CCCV_NUM_POINTS - 1; i++) {
            float v0 = _profile.points[i].voltage;
            float v1 = _profile.points[i + 1].voltage;
            float i0 = _profile.points[i].current;
            float i1 = _profile.points[i + 1].current;

            if (_filteredVoltage >= v0 && _filteredVoltage < v1) {
                if (_profile.smoothLinear && (v1 - v0) > 0.01f) {
                    float ratio = (_filteredVoltage - v0) / (v1 - v0);
                    target = i0 + ratio * (i1 - i0);
                } else {
                    target = i1; // Discrete stepped mode (protects EEPROM)
                }

                if (i == 0) _status.phase = "TAPER_1";
                else if (i == 1) _status.phase = "TAPER_2";
                else _status.phase = "TAPER_3";

                char buf[64];
                snprintf(buf, sizeof(buf), "Taper %d (%.1fA @ %.1fV / %.3fV/cell)",
                         i + 1, target, _filteredVoltage, _status.cellVoltageEquiv);
                _status.statusText = String(buf);
                break;
            }
        }
    }

    // Floor clamp at 3.0A unless COMPLETE
    if (_status.phase != "COMPLETE" && target < 3.0f) {
        target = 3.0f;
    }

    _status.targetAmps = round(target * 10.0f) / 10.0f;
    return _status.targetAmps;
}
