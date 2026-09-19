#include "StatusLedManager.h"
#include <esp32-hal-rgb-led.h>

StatusLedManager& StatusLedManager::getInstance() {
    static StatusLedManager instance;
    return instance;
}

StatusLedManager::StatusLedManager()
    : _pin(ONBOARD_LED_PIN),
      _mode(StatusLedMode::OFF),
      _lastBlinkMs(0),
      _blinkState(false),
      _lastTxMs(0) {
}

void StatusLedManager::begin(uint8_t pin) {
    _pin = pin;
    _mode = StatusLedMode::OFF;
    _lastBlinkMs = millis();
    _blinkState = false;
    _lastTxMs = 0;
    writeRgb(0, 0, 0);
}

void StatusLedManager::writeRgb(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(_pin, r, g, b);
}

void StatusLedManager::setMode(StatusLedMode mode) {
    if (_mode != mode) {
        _mode = mode;
        _lastBlinkMs = millis();
        _blinkState = true;
    }
}

void StatusLedManager::notifyDataTransmitted() {
    _lastTxMs = millis();
}

void StatusLedManager::process() {
    uint32_t now = millis();

    // Check if we are actively transmitting data over WiFi (stays Solid Green)
    if (_mode == StatusLedMode::SOLID_GREEN || (_mode == StatusLedMode::BLINK_GREEN && _lastTxMs > 0 && (now - _lastTxMs < 1200))) {
        writeRgb(0, 60, 0); // Solid Green
        return;
    }

    switch (_mode) {
        case StatusLedMode::SOLID_RED:
            writeRgb(60, 0, 0); // Solid Red
            break;

        case StatusLedMode::BLINK_RED:
            if (now - _lastBlinkMs >= 400) {
                _lastBlinkMs = now;
                _blinkState = !_blinkState;
                if (_blinkState) {
                    writeRgb(60, 0, 0); // Red ON
                } else {
                    writeRgb(0, 0, 0);  // OFF
                }
            }
            break;

        case StatusLedMode::BLINK_YELLOW:
            if (now - _lastBlinkMs >= 250) {
                _lastBlinkMs = now;
                _blinkState = !_blinkState;
                if (_blinkState) {
                    writeRgb(60, 45, 0); // Yellow ON
                } else {
                    writeRgb(0, 0, 0);   // OFF
                }
            }
            break;

        case StatusLedMode::BLINK_GREEN:
            if (now - _lastBlinkMs >= 400) {
                _lastBlinkMs = now;
                _blinkState = !_blinkState;
                if (_blinkState) {
                    writeRgb(0, 60, 0); // Green ON
                } else {
                    writeRgb(0, 0, 0);  // OFF
                }
            }
            break;

        case StatusLedMode::OFF:
        default:
            writeRgb(0, 0, 0);
            break;
    }
}
