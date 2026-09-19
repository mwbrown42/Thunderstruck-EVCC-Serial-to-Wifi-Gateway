#pragma once

#include <Arduino.h>
#include "Config.h"

#include <USBHostSerial.h>

typedef void (*SerialActivityCallback)(bool isTx);
typedef void (*ConnectionChangeCallback)(bool connected);

class USBSerialHost {
public:
    static USBSerialHost& getInstance();

    bool begin(uint32_t baudRate = EVCC_BAUD_RATE);
    void update(); // Called from main loop or task

    // Stream interface
    size_t write(const uint8_t* buffer, size_t size);
    size_t write(uint8_t ch);
    size_t print(const String& str);
    size_t println(const String& str);

    int available();
    int read();
    int peek();
    void flush();

    bool isConnected() const { return _connected; }
    int getNumUsbDevices() const { return _usbHost.getNumDevices(); }
    size_t getTxBytes() const { return _txBytes; }
    size_t getRxBytes() const { return _rxBytes; }
    String getDiagnostics();

    void setActivityCallback(SerialActivityCallback cb) { _activityCb = cb; }
    void setConnectionCallback(ConnectionChangeCallback cb) { _connCb = cb; }
    void handleConnectionChange(bool connected);

    // Fallback UART support
    void setUseHardwareUart(bool useUart) { _useHardwareUart = useUart; }
    bool isUsingHardwareUart() const { return _useHardwareUart; }

private:
    USBSerialHost();
    ~USBSerialHost();

    bool initUsbHost();
    bool initHardwareUart();

    bool _initialized;
    bool _connected;
    bool _useHardwareUart;
    uint32_t _baudRate;
    size_t _txBytes;
    size_t _rxBytes;

    SerialActivityCallback _activityCb;
    ConnectionChangeCallback _connCb;

    // Internal ring buffer for received data
    static const size_t RX_BUFFER_SIZE = 2048;
    uint8_t _rxBuffer[RX_BUFFER_SIZE];
    volatile size_t _rxHead;
    volatile size_t _rxTail;

    void pushRxByte(uint8_t b);

    USBHostSerial _usbHost;
};
