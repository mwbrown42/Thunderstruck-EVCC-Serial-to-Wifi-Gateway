#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "EvccTypes.h"
#include "Config.h"

class WebServerManager {
public:
    static WebServerManager& getInstance();

    void begin();
    void process();
    void scheduleRestart(uint32_t delayMs = 1000);

    // WebSocket & UDP Broadcasting
    void broadcastRawLine(const String& line, bool isTx = false);
    void broadcastTelemetry(const ChargerTelemetry chargers[NUM_CHARGERS], const EvccSystemState& state);
    void broadcastQueryResponse(const String& queryType, const String& responseText);

private:
    WebServerManager();

    void setupRoutes();
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client);

    AsyncWebServer _server;
    AsyncWebSocket _ws;
    WiFiUDP _udp;
    uint32_t _lastCleanupMs;
    volatile bool _pendingRestart;
    volatile uint32_t _restartMs;

    friend void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
};
