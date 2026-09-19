#include "WiFiConfigManager.h"
#include "StatusLedManager.h"
#include "EventLogger.h"
#include "esp_log.h"

static const char* TAG = "WiFiConfigManager";

WiFiConfigManager& WiFiConfigManager::getInstance() {
    static WiFiConfigManager instance;
    return instance;
}

WiFiConfigManager::WiFiConfigManager()
    : _isAp(true),
      _btnPressStartMs(0),
      _lastReportSec(0),
      _btnHeld(false) {
}

void WiFiConfigManager::begin() {
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    loadConfig();

    if (_config.ssid.length() > 0) {
        if (!startStation()) {
            Serial.println("[WiFi] Station connection failed, falling back to SoftAP mode.");
            startSoftAP();
        }
    } else {
        Serial.println("[WiFi] No station SSID configured. Starting default Open SoftAP mode.");
        startSoftAP();
    }
}

void WiFiConfigManager::loadConfig() {
    _prefs.begin(PREF_NAMESPACE, true);
    uint32_t cfgVer = _prefs.getUInt("cfg_ver", 0);
    _config.ssid = _prefs.getString("ssid", "");
    _config.password = _prefs.getString("pass", "");
    _config.useStaticIp = _prefs.getBool("static", true);

    String ipStr = _prefs.getString("ip", "");
    String gwStr = _prefs.getString("gw", "");
    String snStr = _prefs.getString("sn", "");
    String dnsStr = _prefs.getString("dns", "");

    if (ipStr.length() > 0) {
        _config.staticIp.fromString(ipStr);
    } else {
        _config.staticIp = DEFAULT_STA_IP;
    }

    if (gwStr.length() > 0) {
        _config.gateway.fromString(gwStr);
    } else {
        _config.gateway = DEFAULT_STA_GATEWAY;
    }

    if (snStr.length() > 0) {
        _config.subnet.fromString(snStr);
    } else {
        _config.subnet = DEFAULT_STA_SUBNET;
    }

    if (dnsStr.length() > 0) {
        _config.dns.fromString(dnsStr);
    } else {
        _config.dns = DEFAULT_STA_DNS;
    }

    _prefs.end();

    // If first boot on blank chip, seed target default:
    if (cfgVer == 0) {
        Serial.println("[WiFi] First boot: initializing default network configuration...");
        _config.ssid = DEFAULT_STA_SSID;
        _config.password = DEFAULT_STA_PASS;
        _config.useStaticIp = DEFAULT_STA_USE_STATIC;
        _config.staticIp = DEFAULT_STA_IP;
        _config.gateway = DEFAULT_STA_GATEWAY;
        _config.subnet = DEFAULT_STA_SUBNET;
        _config.dns = DEFAULT_STA_DNS;
        saveConfig(_config);
    } else {
        Serial.printf("[WiFi] Loaded saved config from NVS: SSID='%s', PassLen=%d, Static=%d, IP=%s\n",
                      _config.ssid.c_str(), (int)_config.password.length(), _config.useStaticIp, _config.staticIp.toString().c_str());
    }
}

bool WiFiConfigManager::saveConfig(const NetworkConfig& cfg) {
    bool ok = _prefs.begin(PREF_NAMESPACE, false);
    if (!ok) {
        ESP_LOGE(TAG, "Failed to open NVS namespace for writing!");
        Serial.println("[WiFi] ERROR: Failed to open Preferences namespace!");
        return false;
    }

    _prefs.putUInt("cfg_ver", 2);
    _prefs.putString("ssid", cfg.ssid);
    _prefs.putString("pass", cfg.password);
    _prefs.putBool("static", cfg.useStaticIp);

    if (cfg.useStaticIp) {
        _prefs.putString("ip", cfg.staticIp.toString());
        _prefs.putString("gw", cfg.gateway.toString());
        _prefs.putString("sn", cfg.subnet.toString());
        _prefs.putString("dns", cfg.dns.toString());
    }
    _prefs.end();

    _config = cfg;
    Serial.printf("[WiFi] Saved config to NVS: SSID='%s', static=%d, IP=%s, GW=%s, SN=%s, DNS=%s\n",
                  cfg.ssid.c_str(), cfg.useStaticIp,
                  cfg.staticIp.toString().c_str(),
                  cfg.gateway.toString().c_str(),
                  cfg.subnet.toString().c_str(),
                  cfg.dns.toString().c_str());
    ESP_LOGI(TAG, "Saved WiFi config for SSID: %s (IP: %s)",
             cfg.ssid.c_str(), cfg.staticIp.toString().c_str());
    return true;
}

void WiFiConfigManager::resetToFactoryDefaults() {
    Serial.println("[WiFi] Factory reset: restoring Open SoftAP (EVCC-Gateway-AP at 192.168.4.1)...");
    _prefs.begin(PREF_NAMESPACE, false);
    _prefs.clear();
    _prefs.putUInt("cfg_ver", 2);
    _prefs.putString("ssid", ""); // Clear station SSID so it STAYS in SoftAP mode
    _prefs.putString("pass", "");
    _prefs.putBool("static", false);
    _prefs.end();

    _config = NetworkConfig();
    _config.ssid = "";
    _config.password = "";
    _config.apMode = true;
    startSoftAP();
}

bool WiFiConfigManager::startStation() {
    Serial.printf("[WiFi] Connecting to SSID: '%s' (password length: %d)...\n",
                  _config.ssid.c_str(), (int)_config.password.length());
    ESP_LOGI(TAG, "Connecting to SSID: %s", _config.ssid.c_str());

    // Blinking Yellow: Changing to / connecting to user's selected SSID
    StatusLedManager::getInstance().setMode(StatusLedMode::BLINK_YELLOW);

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_15dBm); // 15dBm to avoid brownouts on weak OTG power supplies

    if (_config.useStaticIp && _config.staticIp != IPAddress(0,0,0,0)) {
        if (_config.subnet == IPAddress(0,0,0,0)) {
            _config.subnet = IPAddress(255, 255, 255, 0);
        }
        if (_config.gateway == IPAddress(0,0,0,0)) {
            _config.gateway = IPAddress(_config.staticIp[0], _config.staticIp[1], _config.staticIp[2], 1);
        }
        if (_config.dns == IPAddress(0,0,0,0)) {
            _config.dns = _config.gateway;
        }

        Serial.printf("[WiFi] Static IP: %s, Subnet: %s, Gateway: %s, DNS: %s\n",
                      _config.staticIp.toString().c_str(),
                      _config.subnet.toString().c_str(),
                      _config.gateway.toString().c_str(),
                      _config.dns.toString().c_str());

        if (!WiFi.config(_config.staticIp, _config.gateway, _config.subnet, _config.dns)) {
            Serial.println("[WiFi] WARNING: WiFi.config() returned false!");
        }
    }

    WiFi.setAutoReconnect(true);
    WiFi.begin(_config.ssid.c_str(), _config.password.c_str());

    uint32_t startMs = millis();
    uint32_t lastDotMs = 0;
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < STA_CONNECT_TIMEOUT_MS)) {
        delay(50);
        checkResetButton();
        StatusLedManager::getInstance().process();
        if (millis() - lastDotMs >= 1000) {
            lastDotMs = millis();
            Serial.print(".");
        }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _isAp = false;
        _config.apMode = false;
        // Blinking Green: Connected to user's selected SSID
        StatusLedManager::getInstance().setMode(StatusLedMode::BLINK_GREEN);
        Serial.printf("[WiFi] Connected! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        ESP_LOGI(TAG, "Connected to WiFi! IP: %s, RSSI: %d dBm",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
        EventLogger::getInstance().log("WIFI", "Station CONNECTED to '%s', IP: %s, RSSI: %d dBm",
                                       _config.ssid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    }

    // Solid Red: Error connecting to station
    StatusLedManager::getInstance().setMode(StatusLedMode::SOLID_RED);
    delay(1000);

    Serial.printf("[WiFi] Connection to '%s' failed (status: %d).\n", _config.ssid.c_str(), (int)WiFi.status());
    EventLogger::getInstance().log("WIFI", "Station connection to '%s' FAILED (status %d)",
                                   _config.ssid.c_str(), (int)WiFi.status());
    WiFi.disconnect(true);
    delay(200);
    Serial.println("[WiFi] Scanning 2.4GHz WiFi channels to diagnose available networks...");
    int n = WiFi.scanNetworks(false, false, false, 300);
    if (n <= 0) {
        Serial.printf("[WiFi] Scan returned %d networks on 2.4GHz.\n", n);
    } else {
        Serial.printf("[WiFi] Found %d visible 2.4GHz networks:\n", n);
        bool foundTarget = false;
        for (int i = 0; i < n; ++i) {
            bool isTarget = (WiFi.SSID(i) == _config.ssid);
            if (isTarget) foundTarget = true;
            Serial.printf("  %s %2d: '%s' (RSSI: %d dBm, Ch: %d, Auth: %d)\n",
                          isTarget ? "--> MATCH:" : "          ",
                          i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), (int)WiFi.encryptionType(i));
        }
        if (!foundTarget) {
            Serial.printf("[WiFi] Notice: SSID '%s' was NOT detected on 2.4GHz band!\n", _config.ssid.c_str());
            Serial.println("[WiFi] Note: ESP32 only supports 2.4GHz WiFi (not 5GHz). Verify 2.4GHz band is broadcasting.");
            EventLogger::getInstance().log("WIFI", "Scan: SSID '%s' was NOT found in %d visible 2.4GHz networks",
                                           _config.ssid.c_str(), n);
        }
    }
    WiFi.scanDelete();
    return false;
}

void WiFiConfigManager::startSoftAP() {
    if (_config.ssid.length() > 0) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_AP);
    }
    WiFi.softAPConfig(DEFAULT_AP_IP, DEFAULT_AP_GATEWAY, DEFAULT_AP_SUBNET);
    if (strlen(DEFAULT_AP_PASS) > 0) {
        WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASS);
    } else {
        WiFi.softAP(DEFAULT_AP_SSID); // Open network (no password required)
    }
    WiFi.setTxPower(WIFI_POWER_15dBm);
    _isAp = true;
    _config.apMode = true;

    // Blinking Red: Broadcasting on EVCC_Gateway_AP
    StatusLedManager::getInstance().setMode(StatusLedMode::BLINK_RED);

    _dnsServer.start(DNS_PORT, "*", DEFAULT_AP_IP);

    Serial.printf("[WiFi] SoftAP active (Open network, no password). SSID: %s, IP: %s\n",
                  DEFAULT_AP_SSID, DEFAULT_AP_IP.toString().c_str());
    ESP_LOGI(TAG, "SoftAP active (Open network, no password). SSID: %s, IP: %s",
             DEFAULT_AP_SSID, DEFAULT_AP_IP.toString().c_str());
    EventLogger::getInstance().log("WIFI", "SoftAP active. SSID: %s, IP: %s",
                                   DEFAULT_AP_SSID, DEFAULT_AP_IP.toString().c_str());
}

void WiFiConfigManager::checkResetButton() {
    // DevKitC Boot button (active LOW on GPIO 0)
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        if (!_btnHeld) {
            _btnHeld = true;
            _btnPressStartMs = millis();
            _lastReportSec = 0;
            Serial.println("\n[Button] BOOT button pressed! Keep holding for 5 seconds to reset to AP mode (EVCC-Gateway-AP at 192.168.4.1)...");
        } else {
            uint32_t elapsed = millis() - _btnPressStartMs;
            uint32_t sec = elapsed / 1000;
            if (sec > _lastReportSec && sec <= 5) {
                _lastReportSec = sec;
                Serial.printf("[Button] Holding BOOT button... %u / 5 seconds\n", sec);
            }
            if (elapsed >= RESET_BUTTON_HOLD_MS) {
                Serial.println("\n==================================================");
                Serial.println("[Button] 5 SECONDS REACHED! Resetting WiFi configuration...");
                Serial.println("[Button] Restoring Open SoftAP: EVCC-Gateway-AP (192.168.4.1)");
                Serial.println("==================================================");
                _btnHeld = false;
                resetToFactoryDefaults();
                delay(1000);
                ESP.restart();
            }
        }
    } else {
        if (_btnHeld) {
            uint32_t elapsed = millis() - _btnPressStartMs;
            if (elapsed < RESET_BUTTON_HOLD_MS) {
                Serial.printf("[Button] Released after %u ms (needed 5000 ms). Reset cancelled.\n", elapsed);
            }
            _btnHeld = false;
        }
    }
}

void WiFiConfigManager::process() {
    if (_isAp) {
        _dnsServer.processNextRequest();
        // If an SSID is configured, retry connecting in background every 3 minutes (180s)
        // only if no client is currently connected to SoftAP to avoid disrupting user
        static uint32_t lastStaRetryMs = 0;
        if (_config.ssid.length() > 0 && WiFi.softAPgetStationNum() == 0 && (millis() - lastStaRetryMs > 180000)) {
            lastStaRetryMs = millis();
            Serial.printf("[WiFi] Periodic STA retry to '%s'...\n", _config.ssid.c_str());
            if (startStation()) {
                _dnsServer.stop();
            } else {
                startSoftAP();
            }
        }
    }
    checkResetButton();
}

String WiFiConfigManager::getIpAddress() const {
    if (_isAp) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

int8_t WiFiConfigManager::getRssi() const {
    if (_isAp) return 0;
    return WiFi.RSSI();
}

String WiFiConfigManager::getSsid() const {
    if (_isAp) return String(DEFAULT_AP_SSID);
    return _config.ssid;
}
