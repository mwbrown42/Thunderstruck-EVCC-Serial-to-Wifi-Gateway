#pragma once

#include <Arduino.h>
#include "Config.h"

enum class StatusLedMode {
    OFF,
    SOLID_RED,       // Error
    BLINK_RED,       // Broadcasting on EVCC-Gateway-AP
    BLINK_YELLOW,    // Changing to / connecting to user's selected SSID
    BLINK_GREEN,     // Connected to user's selected SSID
    SOLID_GREEN      // Transmitting data from EVCC over WiFi
};

class StatusLedManager {
public:
    static StatusLedManager& getInstance();

    void begin(uint8_t pin = ONBOARD_LED_PIN);
    void setMode(StatusLedMode mode);
    StatusLedMode getMode() const { return _mode; }

    // Trigger active data transmission state (stays solid green while transmitting)
    void notifyDataTransmitted();

    // Call periodically in loop()
    void process();

private:
    StatusLedManager();

    void writeRgb(uint8_t r, uint8_t g, uint8_t b);

    uint8_t _pin;
    StatusLedMode _mode;
    uint32_t _lastBlinkMs;
    bool _blinkState;
    uint32_t _lastTxMs;
};
