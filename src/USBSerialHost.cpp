#include "USBSerialHost.h"
#include "WebServerManager.h"
#include "WiFiConfigManager.h"
#include "EventLogger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "esp_log.h"

static const char* TAG = "USBSerialHost";

static HardwareSerial SerialEVCC(EVCC_UART_NUM);
static SemaphoreHandle_t s_rxMutex = nullptr;

static void usbLoggerCallback(const char* msg) {
    Serial.printf("[USBHost] %s\n", msg);
    ESP_LOGI("USBHost", "%s", msg);
    EventLogger::getInstance().log("USB", "%s", msg);
    // Broadcast directly to tablet and web terminal
    WebServerManager::getInstance().broadcastRawLine(String(msg) + "\n", false);
}

USBSerialHost& USBSerialHost::getInstance() {
    static USBSerialHost instance;
    return instance;
}

USBSerialHost::USBSerialHost()
    : _initialized(false),
      _connected(false),
      _useHardwareUart(false),
      _baudRate(EVCC_BAUD_RATE),
      _txBytes(0),
      _rxBytes(0),
      _activityCb(nullptr),
      _connCb(nullptr),
      _rxHead(0),
      _rxTail(0) {
    if (!s_rxMutex) {
        s_rxMutex = xSemaphoreCreateMutex();
    }
}

USBSerialHost::~USBSerialHost() {
    if (s_rxMutex) {
        vSemaphoreDelete(s_rxMutex);
        s_rxMutex = nullptr;
    }
}

void USBSerialHost::pushRxByte(uint8_t b) {
    _rxBytes++;
    if (s_rxMutex && xSemaphoreTake(s_rxMutex, portMAX_DELAY) == pdTRUE) {
        size_t nextHead = (_rxHead + 1) % RX_BUFFER_SIZE;
        if (nextHead != _rxTail) {
            _rxBuffer[_rxHead] = b;
            _rxHead = nextHead;
        }
        xSemaphoreGive(s_rxMutex);
    }
}

void USBSerialHost::handleConnectionChange(bool connected) {
    _connected = connected;
    EventLogger::getInstance().log("USB", "VCP Serial Link changed: %s", connected ? "CONNECTED" : "DISCONNECTED");
    if (_connCb) {
        _connCb(connected);
    }
}

bool USBSerialHost::initUsbHost() {
    ESP_LOGI(TAG, "Initializing USB Host stack for EVCC at %d baud...", _baudRate);
    Serial.printf("[USBHost] Initializing USB Host stack for EVCC at %d baud...\n", _baudRate);
    _usbHost.setLogger(usbLoggerCallback);
    bool ok = _usbHost.begin(_baudRate, 0, 0, 8);
    if (ok) {
        ESP_LOGI(TAG, "USB Host stack initialized. Waiting for EVCC USB adapter enumeration.");
        Serial.println("[USBHost] USB Host stack initialized. Waiting for EVCC USB adapter enumeration.");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to initialize USB Host stack.");
        Serial.println("[USBHost] Failed to initialize USB Host stack.");
        return false;
    }
}

bool USBSerialHost::initHardwareUart() {
    ESP_LOGI(TAG, "Initializing Hardware UART on RX:%d, TX:%d at %d baud",
             EVCC_UART_RX_PIN, EVCC_UART_TX_PIN, _baudRate);
    SerialEVCC.begin(_baudRate, SERIAL_8N1, EVCC_UART_RX_PIN, EVCC_UART_TX_PIN);
    return true;
}

bool USBSerialHost::begin(uint32_t baudRate) {
    _baudRate = baudRate;
    if (_initialized) return true;

    // Always initialize Hardware UART so pins 18/17 are active simultaneously
    initHardwareUart();

    bool ok = initUsbHost();
    if (!ok) {
        ESP_LOGW(TAG, "USB Host init failed; operating in Hardware UART mode.");
        _useHardwareUart = true;
    }
    _initialized = true;
    return _initialized;
}

void USBSerialHost::update() {
    // 1. Read from Hardware UART (Pins RX=18, TX=17)
    while (SerialEVCC.available() > 0) {
        int ch = SerialEVCC.read();
        if (ch >= 0) {
            pushRxByte((uint8_t)ch);
            if (_activityCb) _activityCb(false); // RX activity
        }
    }

    // 2. Read from USB Host
    bool conn = (bool)_usbHost;
    if (conn != _connected) {
        handleConnectionChange(conn);
        String msg = String("[USBHost] Device connection changed: ") + (conn ? "CONNECTED" : "DISCONNECTED");
        Serial.println(msg);
        WebServerManager::getInstance().broadcastRawLine(msg + "\n", false);
    }
    while (_usbHost.available() > 0) {
        uint8_t ch = _usbHost.read();
        pushRxByte(ch);
        if (_activityCb) _activityCb(false); // RX activity
    }
}

size_t USBSerialHost::write(const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    
    if (_activityCb) _activityCb(true); // TX activity

    size_t written = 0;
    // Always mirror write to Hardware UART
    SerialEVCC.write(buffer, size);

    // If USB Host is connected, write to USB Host
    if (_usbHost.isConnected()) {
        written = _usbHost.write(buffer, size);
    }

    _txBytes += size;

    // Also echo to Serial for local debug console
    Serial.write(buffer, size);
    return (written > 0) ? written : size;
}

size_t USBSerialHost::write(uint8_t ch) {
    return write(&ch, 1);
}

size_t USBSerialHost::print(const String& str) {
    return write(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
}

size_t USBSerialHost::println(const String& str) {
    size_t n = print(str);
    n += print("\r\n");
    return n;
}

int USBSerialHost::available() {
    int count = 0;
    if (s_rxMutex && xSemaphoreTake(s_rxMutex, portMAX_DELAY) == pdTRUE) {
        if (_rxHead >= _rxTail) {
            count = static_cast<int>(_rxHead - _rxTail);
        } else {
            count = static_cast<int>(RX_BUFFER_SIZE - _rxTail + _rxHead);
        }
        xSemaphoreGive(s_rxMutex);
    }
    return count;
}

int USBSerialHost::read() {
    int result = -1;
    if (s_rxMutex && xSemaphoreTake(s_rxMutex, portMAX_DELAY) == pdTRUE) {
        if (_rxHead != _rxTail) {
            result = _rxBuffer[_rxTail];
            _rxTail = (_rxTail + 1) % RX_BUFFER_SIZE;
        }
        xSemaphoreGive(s_rxMutex);
    }
    return result;
}

int USBSerialHost::peek() {
    int result = -1;
    if (s_rxMutex && xSemaphoreTake(s_rxMutex, portMAX_DELAY) == pdTRUE) {
        if (_rxHead != _rxTail) {
            result = _rxBuffer[_rxTail];
        }
        xSemaphoreGive(s_rxMutex);
    }
    return result;
}

void USBSerialHost::flush() {
    if (s_rxMutex && xSemaphoreTake(s_rxMutex, portMAX_DELAY) == pdTRUE) {
        _rxHead = 0;
        _rxTail = 0;
        xSemaphoreGive(s_rxMutex);
    }
}

String USBSerialHost::getDiagnostics() {
    int numDevs = getNumUsbDevices();
    bool vcpConn = _usbHost.isConnected();

    String out = "\n================== EVCC GATEWAY DIAGNOSTICS ==================\n";
    out += "WiFi Mode:       " + String(WiFiConfigManager::getInstance().isApMode() ? "SoftAP" : "Station (Connected)") + "\n";
    out += "WiFi IP:         " + WiFiConfigManager::getInstance().getIpAddress() + "\n";
    out += "WiFi RSSI:       " + String(WiFiConfigManager::getInstance().getRssi()) + " dBm\n";
    out += "USB Host Stack:  ACTIVE (9600 baud, 8N1)\n";
    out += "USB Phys Devs:   " + String(numDevs) + "\n";
    if (numDevs == 0) {
        out += "  --> WARNING: 0 USB devices detected on USB port!\n";
        out += "  --> NOTE: ESP32-S3-DevKitC-1 isolates VBUS between USB ports.\n";
        out += "  --> When powered via right port (UART), the left port (USB) has 0V!\n";
        out += "  --> Connect 5V to the FTDI cable VBUS or bridge 5V header pin.\n";
    } else {
        out += "  --> Physical USB connection detected on D+/D-!\n";
    }
    out += "VCP Serial Link: " + String(vcpConn ? "CONNECTED (Ready)" : "WAITING FOR VCP OPEN") + "\n";
    out += "Hardware UART:   ACTIVE on RX:GPIO" + String(EVCC_UART_RX_PIN) + ", TX:GPIO" + String(EVCC_UART_TX_PIN) + "\n";
    out += "Total Bytes TX:  " + String(_txBytes) + "\n";
    out += "Total Bytes RX:  " + String(_rxBytes) + "\n";
    out += "==============================================================\n";
    return out;
}
