#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "EvccTypes.h"
#include "Config.h"

class WiFiConfigManager {
public:
    static WiFiConfigManager& getInstance();

    void begin();
    void process(); // Call in main loop for DNS & button checks

    bool saveConfig(const NetworkConfig& cfg);
    void resetToFactoryDefaults();
    const NetworkConfig& getConfig() const { return _config; }

    bool isApMode() const { return _isAp; }
    String getIpAddress() const;
    int8_t getRssi() const;
    String getSsid() const;

private:
    WiFiConfigManager();

    void loadConfig();
    bool startStation();
    void startSoftAP();
    void checkResetButton();

    NetworkConfig _config;
    Preferences _prefs;
    DNSServer _dnsServer;
    bool _isAp;
    uint32_t _btnPressStartMs;
    uint32_t _lastReportSec;
    bool _btnHeld;
};
