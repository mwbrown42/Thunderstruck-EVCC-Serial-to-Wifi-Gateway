#include "WebServerManager.h"
#include "WebAssets.h"
#include "EvccParser.h"
#include "EvccSimulator.h"
#include "CccvGovernor.h"
#include "WiFiConfigManager.h"
#include "USBSerialHost.h"
#include "EventLogger.h"
#include <LittleFS.h>
#include "esp_log.h"

static const char* TAG = "WebServerManager";

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    (void)server;
    if (type == WS_EVT_CONNECT) {
        ESP_LOGI(TAG, "WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        EventLogger::getInstance().log("WS", "Client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        // Send initial telemetry immediately
        EvccParser& parser = EvccParser::getInstance();
        WebServerManager::getInstance().broadcastTelemetry(parser.getAllChargers(), parser.getSystemState());

        // Send welcome diagnostics banner
        JsonDocument diagDoc;
        diagDoc["type"] = "raw";
        diagDoc["line"] = USBSerialHost::getInstance().getDiagnostics();
        diagDoc["isTx"] = false;
        String diagStr;
        serializeJson(diagDoc, diagStr);
        client->text(diagStr);
    } else if (type == WS_EVT_DISCONNECT) {
        ESP_LOGI(TAG, "WebSocket client #%u disconnected", client->id());
        EventLogger::getInstance().log("WS", "Client #%u disconnected", client->id());
    } else if (type == WS_EVT_DATA) {
        WebServerManager::getInstance().handleWebSocketMessage(arg, data, len, client);
    }
}

WebServerManager& WebServerManager::getInstance() {
    static WebServerManager instance;
    return instance;
}

WebServerManager::WebServerManager()
    : _server(HTTP_PORT),
      _ws("/ws"),
      _lastCleanupMs(0),
      _pendingRestart(false),
      _restartMs(0) {
}

void WebServerManager::setupRoutes() {
    _ws.onEvent(onWsEvent);
    _server.addHandler(&_ws);

    // Root page - stream gzipped asset directly from PROGMEM with zero heap allocation
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    // Captive Portal Redirects for easy AP setup (Android, iOS, Windows, macOS)
    _server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    _server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    _server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    _server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    _server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    _server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });

    // API: System Status
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        EvccParser& parser = EvccParser::getInstance();
        WiFiConfigManager& wifi = WiFiConfigManager::getInstance();

        JsonDocument doc;
        doc["ip"] = wifi.getIpAddress();
        doc["apMode"] = wifi.isApMode();
        doc["rssi"] = wifi.getRssi();
        doc["uptime"] = millis() / 1000;

        JsonObject st = doc["state"].to<JsonObject>();
        st["state"] = parser.getSystemState().state;
        st["j1772"] = parser.getSystemState().j1772State;
        st["cellLoop"] = parser.getSystemState().cellLoop;
        st["proximity"] = parser.getSystemState().proximityStatus;
        st["buzzer"] = parser.getSystemState().buzzer;
        st["termReason"] = parser.getSystemState().termReason;

        JsonArray chargersArr = doc["chargers"].to<JsonArray>();
        for (int i = 0; i < NUM_CHARGERS; i++) {
            const ChargerTelemetry& ch = parser.getCharger(i);
            JsonObject chObj = chargersArr.add<JsonObject>();
            chObj["id"] = i + 1;
            chObj["name"] = ch.name;
            chObj["canId"] = ch.canId;
            chObj["v"] = ch.voltage;
            chObj["a"] = ch.current;
            chObj["w"] = ch.watts;
            chObj["wh"] = ch.wattHours;
            chObj["tmp"] = ch.temperature;
            chObj["active"] = ch.active;
        }

        // Backwards-compatible c1, c2, c3, c4 objects
        const char* cKeys[4] = {"c1", "c2", "c3", "c4"};
        for (int i = 0; i < NUM_CHARGERS; i++) {
            const ChargerTelemetry& ch = parser.getCharger(i);
            JsonObject cObj = doc[cKeys[i]].to<JsonObject>();
            cObj["name"] = ch.name;
            cObj["canId"] = ch.canId;
            cObj["v"] = ch.voltage;
            cObj["a"] = ch.current;
            cObj["w"] = ch.watts;
            cObj["wh"] = ch.wattHours;
            cObj["tmp"] = ch.temperature;
            cObj["active"] = ch.active;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Session History Points
    _server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest *request) {
        EvccParser& parser = EvccParser::getInstance();
        const auto& hist = parser.getSessionHistory();

        JsonDocument doc;
        JsonArray arr = doc["points"].to<JsonArray>();
        for (const auto& pt : hist) {
            JsonObject obj = arr.add<JsonObject>();
            obj["t"] = pt.elapsedSec;
            obj["v1"] = pt.c1_voltage;
            obj["a1"] = pt.c1_current;
            obj["v2"] = pt.c2_voltage;
            obj["a2"] = pt.c2_current;
            obj["v3"] = pt.c3_voltage;
            obj["a3"] = pt.c3_current;
            obj["v4"] = pt.c4_voltage;
            obj["a4"] = pt.c4_current;
            obj["w"] = pt.total_watts;
        }


        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Hardware & USB Diagnostics
    _server.on("/api/diag", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["wifi_mode"] = WiFiConfigManager::getInstance().isApMode() ? "SoftAP" : "Station";
        doc["ip"] = WiFiConfigManager::getInstance().getIpAddress();
        doc["rssi"] = WiFiConfigManager::getInstance().getRssi();
        doc["usb_devices"] = USBSerialHost::getInstance().getNumUsbDevices();
        doc["vcp_connected"] = USBSerialHost::getInstance().isConnected();
        doc["uart_active"] = true;
        doc["tx_bytes"] = USBSerialHost::getInstance().getTxBytes();
        doc["rx_bytes"] = USBSerialHost::getInstance().getRxBytes();
        doc["report"] = USBSerialHost::getInstance().getDiagnostics();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Get Network Config
    _server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        const NetworkConfig& cfg = WiFiConfigManager::getInstance().getConfig();
        JsonDocument doc;
        doc["ssid"] = cfg.ssid;
        doc["pass"] = cfg.password;
        doc["static"] = cfg.useStaticIp;
        doc["ip"] = cfg.staticIp.toString();
        doc["sn"] = cfg.subnet.toString();
        doc["gw"] = cfg.gateway.toString();
        doc["dns"] = cfg.dns.toString();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Save Network Config
    _server.on("/api/config", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            // Nothing to do in request handler, body handler processes the data
            (void)request;
        },
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                request->_tempObject = new String();
            }
            String* body = static_cast<String*>(request->_tempObject);
            if (body) {
                for (size_t i = 0; i < len; i++) {
                    body->concat((char)data[i]);
                }
            }

            if (index + len >= total) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, body ? *body : "");
                if (body) {
                    delete body;
                    request->_tempObject = nullptr;
                }

                if (err) {
                    Serial.printf("[Web] JSON parse error: %s\n", err.c_str());
                    request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
                    return;
                }

                NetworkConfig currentCfg = WiFiConfigManager::getInstance().getConfig();
                NetworkConfig cfg;

                String newSsid = doc["ssid"] | "";
                cfg.ssid = newSsid.length() > 0 ? newSsid : currentCfg.ssid;

                String newPass = doc["pass"] | "";
                if (newPass.length() > 0) {
                    cfg.password = newPass;
                } else {
                    // Preserve existing password if user left it blank
                    cfg.password = currentCfg.password;
                }

                cfg.useStaticIp = doc["static"] | false;

                String ipStr = doc["ip"] | "";
                String snStr = doc["sn"] | "";
                String gwStr = doc["gw"] | "";
                String dnsStr = doc["dns"] | "";

                if (cfg.useStaticIp) {
                    if (ipStr.length() > 0) {
                        cfg.staticIp.fromString(ipStr);
                    } else {
                        cfg.staticIp = currentCfg.staticIp;
                    }

                    if (snStr.length() > 0) {
                        cfg.subnet.fromString(snStr);
                    } else if (currentCfg.subnet != IPAddress(0,0,0,0)) {
                        cfg.subnet = currentCfg.subnet;
                    } else {
                        cfg.subnet = IPAddress(255, 255, 255, 0);
                    }

                    if (gwStr.length() > 0) {
                        cfg.gateway.fromString(gwStr);
                    } else if (currentCfg.gateway != IPAddress(0,0,0,0)) {
                        cfg.gateway = currentCfg.gateway;
                    } else if (cfg.staticIp != IPAddress(0,0,0,0)) {
                        cfg.gateway = IPAddress(cfg.staticIp[0], cfg.staticIp[1], cfg.staticIp[2], 1);
                    }

                    if (dnsStr.length() > 0) {
                        cfg.dns.fromString(dnsStr);
                    } else if (currentCfg.dns != IPAddress(0,0,0,0)) {
                        cfg.dns = currentCfg.dns;
                    } else {
                        cfg.dns = cfg.gateway;
                    }
                }

                Serial.printf("[Web] Received configuration request: SSID='%s', PassLen=%d, Static=%d, IP=%s\n",
                              cfg.ssid.c_str(), (int)cfg.password.length(), cfg.useStaticIp, cfg.staticIp.toString().c_str());

                bool saved = WiFiConfigManager::getInstance().saveConfig(cfg);

                JsonDocument res;
                res["success"] = saved;
                res["message"] = "Configuration saved! Rebooting to connect to " + cfg.ssid + "...";
                String response;
                serializeJson(res, response);
                request->send(200, "application/json", response);

                // Schedule restart after sending response
                WebServerManager::getInstance().scheduleRestart(1000);
            }
        }
    );

    // API: Reset WiFi to AP Mode
    _server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        WiFiConfigManager::getInstance().resetToFactoryDefaults();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi reset to SoftAP mode\"}");
        WebServerManager::getInstance().scheduleRestart(1000);
    });

    // API: Persistent Flash Log Download
    _server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!LittleFS.exists("/evcc_log.txt")) {
            request->send(200, "text/plain", "No log file found in flash.");
            return;
        }
        request->send(LittleFS, "/evcc_log.txt", "text/plain");
    });

    // API: Clear Flash Log
    _server.on("/api/log/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        EventLogger::getInstance().clear();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Flash log cleared\"}");
    });

    // Captive portal fallback redirect / 404 handler
    _server.onNotFound([](AsyncWebServerRequest *request) {
        if (WiFiConfigManager::getInstance().isApMode()) {
            request->redirect("http://192.168.4.1/");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });

    _server.begin();
    ESP_LOGI(TAG, "HTTP Server and WebSocket started on port %d", HTTP_PORT);
}

void WebServerManager::begin() {
    setupRoutes();
    _udp.begin(8888);
}

void WebServerManager::scheduleRestart(uint32_t delayMs) {
    _restartMs = millis() + delayMs;
    _pendingRestart = true;
    Serial.printf("[System] Reboot scheduled in %u ms...\n", delayMs);
}

void WebServerManager::process() {
    if (millis() - _lastCleanupMs > 2000) {
        _ws.cleanupClients();
        _lastCleanupMs = millis();
    }
    if (_pendingRestart && (millis() >= _restartMs)) {
        _pendingRestart = false;
        Serial.println("[System] Rebooting ESP32 now...");
        delay(150);
        ESP.restart();
    }
}

void WebServerManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client) {
    (void)client;
    AwsFrameInfo *info = static_cast<AwsFrameInfo*>(arg);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            ESP_LOGE(TAG, "WS JSON deserialization error: %s", err.c_str());
            return;
        }

        EvccParser& parser = EvccParser::getInstance();
        String action = doc["action"] | "";

        if (action == "raw") {
            String cmd = doc["command"] | "";
            if (cmd.length() > 0) {
                parser.sendRawCommand(cmd);
            }
        } else if (action == "trace") {
            String traceType = doc["trace"] | "";
            if (traceType == "can") {
                parser.setTraceCan(!parser.isTraceCanEnabled());
            } else if (traceType == "state") {
                parser.setTraceState(!parser.isTraceStateEnabled());
            } else if (traceType == "charger") {
                parser.setTraceCharger(!parser.isTraceChargerEnabled());
            } else if (traceType == "off") {
                parser.setTraceOff();
            }
        } else if (action == "query") {
            String q = doc["query"] | "";
            parser.requestQuery(q);
        } else if (action == "set_param") {
            String param = doc["param"] | "";
            float val = doc["value"] | 0.0f;
            if (param == "maxv") {
                parser.setMaxVoltage(val);
            } else if (param == "maxc") {
                parser.setMaxCurrent(val);
            } else if (param == "termc") {
                parser.setTermCurrent(val);
            }
        } else if (action == "set_governor") {
            bool enabled = doc["enabled"] | true;
            parser.setThermalGovernorEnabled(enabled);
        } else if (action == "sim_toggle") {
            bool enabled = doc["enabled"] | false;
            EvccSimulator::getInstance().setEnabled(enabled);
        } else if (action == "sim_preset") {
            int sc = doc["scenario"] | 0;
            EvccSimulator::getInstance().applyScenario((SimScenario)sc);
        } else if (action == "sim_values") {
            uint8_t ch = doc["charger"] | 1;
            float v = doc["v"] | 0.0f;
            float a = doc["a"] | 0.0f;
            float tmp = doc["tmp"] | 25.0f;
            EvccSimulator::getInstance().setChargerValues(ch, v, a, tmp);
        } else if (action == "sim_fault") {
            uint8_t ch = doc["charger"] | 1;
            String fault = doc["fault"] | "";
            bool val = doc["value"] | false;
            EvccSimulator::getInstance().setChargerFault(ch, fault, val);
        } else if (action == "sim_state") {
            String state = doc["state"] | "";
            String j1772 = doc["j1772"] | "";
            EvccSimulator::getInstance().setSystemState(state, j1772);
        } else if (action == "sim_inject") {
            String line = doc["line"] | "";
            if (line.length() > 0) {
                EvccSimulator::getInstance().injectRawLine(line);
            }
        } else if (action == "toggle_cccv") {
            bool enabled = doc["enabled"] | true;
            CccvGovernor::getInstance().setEnabled(enabled);
        } else if (action == "set_cccv_preset") {
            String preset = doc["preset"] | "";
            if (preset == "conservative") {
                CccvGovernor::getInstance().loadPresetTesla36SConservative();
            } else if (preset == "standard") {
                CccvGovernor::getInstance().loadPresetTesla36SStandard();
            } else if (preset == "max_range") {
                CccvGovernor::getInstance().loadPresetTesla36SMaxRange();
            }
        } else if (action == "set_cccv_profile") {
            CccvProfile p = CccvGovernor::getInstance().getProfile();
            if (doc["enabled"].is<bool>()) p.enabled = doc["enabled"].as<bool>();
            if (doc["cellCount"].is<uint8_t>()) p.cellCount = doc["cellCount"].as<uint8_t>();
            if (doc["smoothLinear"].is<bool>()) p.smoothLinear = doc["smoothLinear"].as<bool>();
            if (doc["fastCutoff"].is<bool>()) p.fastCutoff = doc["fastCutoff"].as<bool>();
            if (doc["termAmps"].is<float>()) p.terminationAmps = doc["termAmps"].as<float>();
            JsonArray pts = doc["points"].as<JsonArray>();
            if (!pts.isNull()) {
                int idx = 0;
                for (JsonObject pt : pts) {
                    if (idx < CCCV_NUM_POINTS) {
                        p.points[idx].voltage = pt["v"] | p.points[idx].voltage;
                        p.points[idx].current = pt["a"] | p.points[idx].current;
                        idx++;
                    }
                }
            }
            CccvGovernor::getInstance().setProfile(p);
        }
    }
}

void WebServerManager::broadcastRawLine(const String& line, bool isTx) {
    if (_ws.count() == 0) return;

    JsonDocument doc;
    doc["type"] = "raw";
    doc["line"] = line;
    doc["isTx"] = isTx;

    String jsonStr;
    serializeJson(doc, jsonStr);
    _ws.textAll(jsonStr);
}

void WebServerManager::broadcastTelemetry(const ChargerTelemetry chargers[NUM_CHARGERS], const EvccSystemState& state) {
    JsonDocument doc;
    doc["type"] = "telemetry";
    doc["ip"] = WiFiConfigManager::getInstance().getIpAddress();

    JsonObject st = doc["state"].to<JsonObject>();
    st["state"] = state.state;
    st["j1772"] = state.j1772State;
    st["cellLoop"] = state.cellLoop;
    st["proximity"] = state.proximityStatus;
    st["buzzer"] = state.buzzer;
    st["uptime"] = state.uptimeStr;
    st["termReason"] = state.termReason;
    st["pilotDuty"] = state.pilotDutyCycle;
    st["lineCurrentAvail"] = state.lineCurrentAvail;
    st["maxv"] = state.maxv;
    st["maxc"] = state.maxc;
    st["sessionSec"] = state.sessionElapsedSec;

    // Trace button states
    JsonObject tr = doc["traces"].to<JsonObject>();
    EvccParser& parser = EvccParser::getInstance();
    tr["can"] = parser.isTraceCanEnabled();
    tr["state"] = parser.isTraceStateEnabled();
    tr["charger"] = parser.isTraceChargerEnabled();

    // Broadcast individual c1, c2, c3, c4 for backwards compatibility
    const char* cKeys[4] = {"c1", "c2", "c3", "c4"};
    for (int i = 0; i < NUM_CHARGERS; i++) {
        JsonObject chObj = doc[cKeys[i]].to<JsonObject>();
        chObj["name"] = chargers[i].name;
        chObj["canId"] = chargers[i].canId;
        chObj["v"] = chargers[i].voltage;
        chObj["a"] = chargers[i].current;
        chObj["w"] = chargers[i].watts;
        chObj["wh"] = chargers[i].wattHours;
        chObj["tmp"] = chargers[i].temperature;
        chObj["active"] = chargers[i].active;
        JsonObject f = chObj["faults"].to<JsonObject>();
        f["rxerr"] = chargers[i].faults.rxerr;
        f["hwfail"] = chargers[i].faults.hwfail;
        f["overtemp"] = chargers[i].faults.overtemp;
        f["not_charging"] = chargers[i].faults.not_charging;
        f["input_voltage_err"] = chargers[i].faults.input_voltage_err;
        f["pack_voltage_err"] = chargers[i].faults.pack_voltage_err;
    }

    // Also broadcast full chargers array
    JsonArray chargersArr = doc["chargers"].to<JsonArray>();
    for (int i = 0; i < NUM_CHARGERS; i++) {
        JsonObject chObj = chargersArr.add<JsonObject>();
        chObj["id"] = i + 1;
        chObj["name"] = chargers[i].name;
        chObj["canId"] = chargers[i].canId;
        chObj["v"] = chargers[i].voltage;
        chObj["a"] = chargers[i].current;
        chObj["w"] = chargers[i].watts;
        chObj["wh"] = chargers[i].wattHours;
        chObj["tmp"] = chargers[i].temperature;
        chObj["active"] = chargers[i].active;
        JsonObject f = chObj["faults"].to<JsonObject>();
        f["rxerr"] = chargers[i].faults.rxerr;
        f["hwfail"] = chargers[i].faults.hwfail;
        f["overtemp"] = chargers[i].faults.overtemp;
        f["not_charging"] = chargers[i].faults.not_charging;
        f["input_voltage_err"] = chargers[i].faults.input_voltage_err;
        f["pack_voltage_err"] = chargers[i].faults.pack_voltage_err;
    }

    // Thermal Governor telemetry
    JsonObject gov = doc["governor"].to<JsonObject>();
    const ThermalGovernorStatus& govStatus = parser.getThermalGovernorStatus();
    gov["enabled"] = govStatus.enabled;
    gov["isDerated"] = govStatus.isDerated;
    gov["baselineMaxc"] = govStatus.baselineMaxc;
    gov["activeMaxc"] = govStatus.activeMaxc;
    gov["deratePercent"] = govStatus.deratePercent;
    gov["peakTemp"] = govStatus.peakTemp;
    gov["hottestCharger"] = govStatus.hottestCharger;
    gov["statusText"] = govStatus.statusText;

    JsonArray govChargers = gov["chargers"].to<JsonArray>();
    for (int i = 0; i < NUM_CHARGERS; i++) {
        JsonObject gch = govChargers.add<JsonObject>();
        gch["id"] = i + 1;
        gch["isDerated"] = govStatus.chargers[i].isDerated;
        gch["temp"] = govStatus.chargers[i].currentTemp;
        gch["scale"] = govStatus.chargers[i].targetScale;
        gch["targetAmps"] = govStatus.chargers[i].targetAmps;
        gch["statusText"] = govStatus.chargers[i].statusText;
    }

    // CC/CV Dynamic Tapering Telemetry
    JsonObject cccv = doc["cccv"].to<JsonObject>();
    const CccvGovernorStatus& cccvStatus = CccvGovernor::getInstance().getStatus();
    const CccvProfile& cccvProf = CccvGovernor::getInstance().getProfile();
    cccv["enabled"] = cccvStatus.enabled;
    cccv["isTapering"] = cccvStatus.isTapering;
    cccv["packVoltage"] = cccvStatus.packVoltage;
    cccv["cellVoltage"] = cccvStatus.cellVoltageEquiv;
    cccv["targetAmps"] = cccvStatus.targetAmps;
    cccv["activeCurrent"] = cccvStatus.activeCurrent;
    cccv["phase"] = cccvStatus.phase;
    cccv["statusText"] = cccvStatus.statusText;
    cccv["cellCount"] = cccvProf.cellCount;
    cccv["smoothLinear"] = cccvProf.smoothLinear;
    cccv["fastCutoff"] = cccvProf.fastCutoff;
    cccv["termAmps"] = cccvProf.terminationAmps;
    cccv["writesThisSession"] = cccvStatus.writesThisSession;
    JsonArray pts = cccv["points"].to<JsonArray>();
    for (int i = 0; i < CCCV_NUM_POINTS; i++) {
        JsonObject pt = pts.add<JsonObject>();
        pt["v"] = cccvProf.points[i].voltage;
        pt["a"] = cccvProf.points[i].current;
    }

    // Simulation metadata
    JsonObject sim = doc["sim"].to<JsonObject>();
    EvccSimulator& simInst = EvccSimulator::getInstance();
    sim["enabled"] = simInst.isEnabled();
    sim["scenario"] = (int)simInst.getCurrentScenario();
    sim["c1_v"] = simInst.getC1Voltage();
    sim["c1_a"] = simInst.getC1Current();
    sim["c1_tmp"] = simInst.getC1Temp();
    sim["c2_v"] = simInst.getC2Voltage();
    sim["c2_a"] = simInst.getC2Current();
    sim["c2_tmp"] = simInst.getC2Temp();
    sim["c3_v"] = simInst.getC3Voltage();
    sim["c3_a"] = simInst.getC3Current();
    sim["c3_tmp"] = simInst.getC3Temp();
    sim["c4_v"] = simInst.getC4Voltage();
    sim["c4_a"] = simInst.getC4Current();
    sim["c4_tmp"] = simInst.getC4Temp();

    String jsonStr;
    serializeJson(doc, jsonStr);

    // Send over WebSocket if any browser / client is connected
    if (_ws.count() > 0) {
        _ws.textAll(jsonStr);
    }

    // Broadcast over UDP to entire subnet on port 8888 (Item 11: Zero-IP configuration)
    if (WiFi.status() == WL_CONNECTED || WiFiConfigManager::getInstance().isApMode()) {
        IPAddress bcast = WiFi.broadcastIP();
        if (bcast == IPAddress(0, 0, 0, 0)) {
            bcast = IPAddress(255, 255, 255, 255);
        }
        _udp.beginPacket(bcast, 8888);
        _udp.write((const uint8_t*)jsonStr.c_str(), jsonStr.length());
        _udp.endPacket();
    }
}


void WebServerManager::broadcastQueryResponse(const String& queryType, const String& responseText) {
    if (_ws.count() == 0) return;

    JsonDocument doc;
    doc["type"] = "query_resp";
    doc["query"] = queryType;
    doc["text"] = responseText;

    String jsonStr;
    serializeJson(doc, jsonStr);
    _ws.textAll(jsonStr);
}
