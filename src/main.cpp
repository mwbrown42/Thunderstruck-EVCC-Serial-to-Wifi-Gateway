#include <Arduino.h>
#include "Config.h"
#include "EvccTypes.h"
#include "WebServerManager.h"
#include "EvccParser.h"
#include "EvccSimulator.h"
#include "CccvGovernor.h"
#include "WiFiConfigManager.h"
#include "USBSerialHost.h"
#include "StatusLedManager.h"
#include "EventLogger.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static const char* TAG = "Main";

void onSerialActivity(bool isTx) {
    (void)isTx;
}

void onRawLineReceived(const String& line) {
    StatusLedManager::getInstance().notifyDataTransmitted();
    WebServerManager::getInstance().broadcastRawLine(line, false);
}

void onTelemetryUpdated(const ChargerTelemetry chargers[NUM_CHARGERS], const EvccSystemState& state) {
    WebServerManager::getInstance().broadcastTelemetry(chargers, state);
}


void onQueryResponseReceived(const String& queryType, const String& responseText) {
    StatusLedManager::getInstance().notifyDataTransmitted();
    WebServerManager::getInstance().broadcastQueryResponse(queryType, responseText);
}

void setup() {
    // Disable brownout detector to prevent reboot loops when powered from weak OTG sources
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Initialize Onboard RGB Status LED immediately
    StatusLedManager::getInstance().begin(ONBOARD_LED_PIN);

    Serial.begin(115200);
    delay(500);

    // Initialize Persistent Event Logger in Flash
    EventLogger::getInstance().begin();

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "Thunderstruck EVCC Serial to WiFi Gateway v3.0");
    ESP_LOGI(TAG, "Target: ESP32-S3-DevKitC-1 with USB OTG Host");
    ESP_LOGI(TAG, "==================================================");

    // 1. Initialize WiFi & Network Config
    WiFiConfigManager::getInstance().begin();

    // 2. Initialize EVCC Parser & Callbacks
    EvccParser& parser = EvccParser::getInstance();
    parser.begin();
    parser.setRawLineCallback(onRawLineReceived);
    parser.setTelemetryCallback(onTelemetryUpdated);
    parser.setQueryResponseCallback(onQueryResponseReceived);

    // Initialize Simulator
    EvccSimulator::getInstance().begin();

    // Initialize CC/CV Dynamic Tapering Governor
    CccvGovernor::getInstance().begin();

    // 3. Initialize USB Serial Host (or Hardware UART fallback)
    USBSerialHost& usbHost = USBSerialHost::getInstance();
    usbHost.setActivityCallback(onSerialActivity);
    usbHost.begin(EVCC_BAUD_RATE);

    // 4. Initialize Web Server & WebSockets
    WebServerManager::getInstance().begin();

    ESP_LOGI(TAG, "System initialization complete. Web dashboard active.");
}

void processSerialInput() {
    static String serialCmd = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            serialCmd.trim();
            if (serialCmd.length() > 0) {
                if (serialCmd.equalsIgnoreCase("wifi") || serialCmd.equalsIgnoreCase("show wifi") || serialCmd.equalsIgnoreCase("ip")) {
                    WiFiConfigManager& mgr = WiFiConfigManager::getInstance();
                    const NetworkConfig& cfg = mgr.getConfig();
                    Serial.println("\n----------------------------------------");
                    Serial.printf("WiFi Mode:    %s\n", mgr.isApMode() ? "Access Point (SoftAP)" : "Station");
                    Serial.printf("SSID:         %s\n", cfg.ssid.c_str());
                    Serial.printf("Static IP:    %s\n", cfg.useStaticIp ? "Yes" : "No (DHCP)");
                    Serial.printf("IP Address:   %s (current: %s)\n", cfg.staticIp.toString().c_str(), mgr.getIpAddress().c_str());
                    Serial.printf("Subnet Mask:  %s\n", cfg.subnet.toString().c_str());
                    Serial.printf("Gateway:      %s\n", cfg.gateway.toString().c_str());
                    Serial.printf("DNS:          %s\n", cfg.dns.toString().c_str());
                    Serial.printf("RSSI:         %d dBm\n", mgr.getRssi());
                    Serial.println("----------------------------------------");
                    Serial.println("Commands:");
                    Serial.println("  set ip <ip> [gateway] [subnet]");
                    Serial.println("  set wifi <ssid> <password> [ip]");
                    Serial.println("  reset wifi");
                    Serial.println("----------------------------------------");
                } else if (serialCmd.startsWith("set ip ") || serialCmd.startsWith("SET IP ")) {
                    String rest = serialCmd.substring(7);
                    rest.trim();
                    int sp1 = rest.indexOf(' ');
                    String ipStr = (sp1 > 0) ? rest.substring(0, sp1) : rest;
                    IPAddress newIp;
                    if (newIp.fromString(ipStr)) {
                        NetworkConfig cfg = WiFiConfigManager::getInstance().getConfig();
                        cfg.useStaticIp = true;
                        cfg.staticIp = newIp;
                        if (sp1 > 0) {
                            String rest2 = rest.substring(sp1 + 1);
                            rest2.trim();
                            int sp2 = rest2.indexOf(' ');
                            String gwStr = (sp2 > 0) ? rest2.substring(0, sp2) : rest2;
                            cfg.gateway.fromString(gwStr);
                            if (sp2 > 0) {
                                String snStr = rest2.substring(sp2 + 1);
                                snStr.trim();
                                cfg.subnet.fromString(snStr);
                            }
                        } else {
                            cfg.gateway = IPAddress(newIp[0], newIp[1], newIp[2], 1);
                            cfg.subnet = IPAddress(255, 255, 255, 0);
                            cfg.dns = cfg.gateway;
                        }
                        WiFiConfigManager::getInstance().saveConfig(cfg);
                        Serial.printf("[Serial] IP updated to %s. Rebooting in 500ms...\n", newIp.toString().c_str());
                        WebServerManager::getInstance().scheduleRestart(500);
                    } else {
                        Serial.println("[Serial] Invalid IP format!");
                    }
                } else if (serialCmd.startsWith("set wifi ") || serialCmd.startsWith("SET WIFI ")) {
                    String rest = serialCmd.substring(9);
                    rest.trim();
                    int sp1 = rest.indexOf(' ');
                    if (sp1 > 0) {
                        String ssid = rest.substring(0, sp1);
                        String rest2 = rest.substring(sp1 + 1);
                        rest2.trim();
                        int sp2 = rest2.indexOf(' ');
                        String pass = (sp2 > 0) ? rest2.substring(0, sp2) : rest2;
                        NetworkConfig cfg = WiFiConfigManager::getInstance().getConfig();
                        cfg.ssid = ssid;
                        cfg.password = pass;
                        if (sp2 > 0) {
                            String ipStr = rest2.substring(sp2 + 1);
                            ipStr.trim();
                            IPAddress newIp;
                            if (newIp.fromString(ipStr)) {
                                cfg.useStaticIp = true;
                                cfg.staticIp = newIp;
                                cfg.gateway = IPAddress(newIp[0], newIp[1], newIp[2], 1);
                                cfg.subnet = IPAddress(255, 255, 255, 0);
                                cfg.dns = cfg.gateway;
                            }
                        }
                        WiFiConfigManager::getInstance().saveConfig(cfg);
                        Serial.printf("[Serial] WiFi credentials updated for '%s'. Rebooting in 500ms...\n", ssid.c_str());
                        WebServerManager::getInstance().scheduleRestart(500);
                    } else {
                        Serial.println("[Serial] Usage: set wifi <ssid> <password> [ip]");
                    }
                } else if (serialCmd.equalsIgnoreCase("scan") || serialCmd.equalsIgnoreCase("scan wifi")) {
                    Serial.println("[WiFi] Pausing STA connection to scan available 2.4GHz WiFi networks...");
                    WiFi.disconnect(true);
                    delay(200);
                    int n = WiFi.scanNetworks(false, false, false, 300);
                    Serial.printf("[WiFi] Found %d networks:\n", n);
                    for (int i = 0; i < n; ++i) {
                        Serial.printf("  %2d: '%s' (RSSI: %d dBm, Ch: %d, Auth: %d)\n",
                                      i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), (int)WiFi.encryptionType(i));
                    }
                    WiFi.scanDelete();
                } else if (serialCmd.equalsIgnoreCase("reset wifi")) {
                    WiFiConfigManager::getInstance().resetToFactoryDefaults();
                    Serial.println("[Serial] Reset to factory SoftAP. Rebooting in 500ms...");
                    WebServerManager::getInstance().scheduleRestart(500);
                } else if (serialCmd.equalsIgnoreCase("log") || serialCmd.equalsIgnoreCase("show log") || serialCmd.equalsIgnoreCase("logs")) {
                    EventLogger::getInstance().dumpToSerial(Serial);
                } else if (serialCmd.equalsIgnoreCase("clear log") || serialCmd.equalsIgnoreCase("reset log")) {
                    EventLogger::getInstance().clear();
                    Serial.println("[Serial] Flash log cleared.");
                } else if (serialCmd.equalsIgnoreCase("governor") || serialCmd.equalsIgnoreCase("show governor")) {
                    const auto& gov = EvccParser::getInstance().getThermalGovernorStatus();
                    Serial.println("\n----------------------------------------");
                    Serial.println("Thermal Governor Status:");
                    Serial.printf("  Enabled:        %s\n", gov.enabled ? "YES" : "NO");
                    Serial.printf("  State:          %s\n", gov.isDerated ? "DERATED" : "OPTIMAL");
                    Serial.printf("  Baseline Max C: %.1f A\n", gov.baselineMaxc);
                    Serial.printf("  Active Max C:   %.1f A (%d%%)\n", gov.activeMaxc, gov.deratePercent);
                    Serial.printf("  Peak Heatsink:  %.1f deg C (%s)\n", gov.peakTemp, gov.hottestCharger.c_str());
                    Serial.printf("  Status:         %s\n", gov.statusText.c_str());
                    Serial.println("----------------------------------------");
                } else if (serialCmd.equalsIgnoreCase("set governor on") || serialCmd.equalsIgnoreCase("set governor enable")) {
                    EvccParser::getInstance().setThermalGovernorEnabled(true);
                    Serial.println("[Serial] Thermal Governor ENABLED.");
                } else if (serialCmd.equalsIgnoreCase("set governor off") || serialCmd.equalsIgnoreCase("set governor disable")) {
                    EvccParser::getInstance().setThermalGovernorEnabled(false);
                    Serial.println("[Serial] Thermal Governor DISABLED.");
                } else {
                    // Forward any other command to the EVCC
                    USBSerialHost::getInstance().println(serialCmd);
                }
                serialCmd = "";
            }
        } else {
            serialCmd += c;
        }
    }
}

void loop() {
    // Update onboard RGB Status LED
    StatusLedManager::getInstance().process();

    // Process WiFi / DNS Captive Portal / Button
    WiFiConfigManager::getInstance().process();

    // Process USB Serial stream & EVCC command parser
    EvccParser::getInstance().process();

    // Process AsyncWebServer & WebSocket cleanup
    WebServerManager::getInstance().process();

    // Process local console serial input
    processSerialInput();

    delay(2);
}
