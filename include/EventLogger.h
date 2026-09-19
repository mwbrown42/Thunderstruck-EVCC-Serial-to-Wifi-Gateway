#pragma once

#include <Arduino.h>

class EventLogger {
public:
    static EventLogger& getInstance() {
        static EventLogger instance;
        return instance;
    }

    void begin();
    void log(const char* tag, const char* format, ...) __attribute__((format(printf, 3, 4)));
    void log(const String& tag, const String& msg);
    
    String getRecentLogs(size_t maxBytes = 8192);
    void dumpToSerial(Stream& out = Serial);
    void clear();
    size_t getLogSize();
    bool isFsAvailable() const { return _fsMounted; }

private:
    EventLogger();
    ~EventLogger() = default;

    void appendToFile(const String& entry);
    String getResetReason();

    bool _fsMounted = false;
    SemaphoreHandle_t _mutex = nullptr;
    size_t _currentLogSize = 0;
    static const size_t MAX_LOG_FILE_SIZE = 65536; // 64 KB per chunk (128 KB total circular buffer)
};

