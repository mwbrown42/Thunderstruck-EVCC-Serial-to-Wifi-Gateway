#pragma once

#include <Arduino.h>

// ============================================================================
// Hardware & Pin Configuration (ESP32-S3 DevKitC-1)
// ============================================================================
#define EVCC_BAUD_RATE            9600

// Native USB OTG Pins (ESP32-S3 Hardware standard)
#define USB_DM_GPIO               19
#define USB_DP_GPIO               20

// Optional Hardware UART Fallback (if USB Host not used or for direct 3.5mm TTL connection)
#define EVCC_UART_NUM             1
#define EVCC_UART_RX_PIN          18
#define EVCC_UART_TX_PIN          17

// Factory Reset / WiFi Reset Button (Boot button on DevKitC-1)
#define RESET_BUTTON_PIN          0
#define RESET_BUTTON_HOLD_MS      5000

// Onboard Status LED (DevKitC-1 addressable RGB on GPIO 48 or standard LED on GPIO 2)
#define ONBOARD_LED_PIN           48

// ============================================================================
// EVCC System Configuration
// ============================================================================
#define NUM_CHARGERS              4
#define CHARGER1_CAN_ID           40    // 0x28: 'tsm2500' / 'charger'
#define CHARGER2_CAN_ID           41    // 0x29: 'tsm2500_41' / 'charger2'
#define CHARGER3_CAN_ID           42    // 0x2A: 'tsm2500_42' / 'charger3'
#define CHARGER4_CAN_ID           43    // 0x2B: 'tsm2500_43' / 'charger4'

// Telemetry & Buffer Settings
#define RAW_SERIAL_BUFFER_SIZE    4096
#define HISTOGRAM_MAX_POINTS      360   // E.g., 1 sample every 5s = 30 minutes, or 10s = 1 hour
#define HISTOGRAM_SAMPLE_MS       5000  // Sample every 5 seconds during active charge
#define TELEMETRY_BROADCAST_MS    500   // Send parsed WS updates every 500ms

// ============================================================================
// Default Network Settings
// ============================================================================
#define DEFAULT_AP_SSID           "EVCC-Gateway-AP"
#define DEFAULT_AP_PASS           ""    // Open network (no password)
#define DEFAULT_AP_IP             IPAddress(192, 168, 4, 1)
#define DEFAULT_AP_GATEWAY        IPAddress(192, 168, 4, 1)
#define DEFAULT_AP_SUBNET         IPAddress(255, 255, 255, 0)

// Default Target Station Network Settings
// (Leave blank by default so new installations boot into Open SoftAP mode "EVCC-Gateway-AP",
// allowing users to configure their own SSID, password, and IP via the captive web portal).
#define DEFAULT_STA_SSID          ""
#define DEFAULT_STA_PASS          ""
#define DEFAULT_STA_USE_STATIC    false
#define DEFAULT_STA_IP            IPAddress(192, 168, 1, 3)
#define DEFAULT_STA_GATEWAY       IPAddress(192, 168, 1, 1)
#define DEFAULT_STA_SUBNET        IPAddress(255, 255, 255, 0)
#define DEFAULT_STA_DNS           IPAddress(192, 168, 1, 1)
#define STA_CONNECT_TIMEOUT_MS    60000 // 60 seconds (1 minute) timeout to connect to user's Wi-Fi network

#define DNS_PORT                  53
#define HTTP_PORT                 80
#define WS_PORT                   80

#define PREF_NAMESPACE            "evcc_cfg"

