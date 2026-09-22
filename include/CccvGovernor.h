#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "EvccTypes.h"

class CccvGovernor {
public:
    static CccvGovernor& getInstance() {
        static CccvGovernor instance;
        return instance;
    }

    void begin();
    void loadFromPreferences();
    void saveToPreferences();

    const CccvProfile& getProfile() const { return _profile; }
    void setProfile(const CccvProfile& profile);

    const CccvGovernorStatus& getStatus() const { return _status; }
    void setEnabled(bool enabled);

    // Evaluates the curve for a given measured pack voltage and current
    // Returns target current in Amps
    float update(float rawPackVoltage, float totalCurrent, bool chargingActive);

    // Preset loaders
    void loadPresetTesla36SConservative(); // 4.10V = 147.6V
    void loadPresetTesla36SStandard();     // 4.15V = 149.4V
    void loadPresetTesla36SMaxRange();     // 4.20V = 151.2V

    void recordEepromWrite() { _status.writesThisSession++; }
    void resetSessionWrites() { _status.writesThisSession = 0; }
    void resetCompletion() { _chargeCompleted = false; }
    bool isChargeCompleted() const { return _chargeCompleted; }

private:
    CccvGovernor();
    ~CccvGovernor() = default;
    CccvGovernor(const CccvGovernor&) = delete;
    CccvGovernor& operator=(const CccvGovernor&) = delete;

    CccvProfile _profile;
    CccvGovernorStatus _status;
    Preferences _prefs;

    float _filteredVoltage = 0.0f;
    uint32_t _lastCheckMs = 0;
    bool _chargeCompleted = false;
};
