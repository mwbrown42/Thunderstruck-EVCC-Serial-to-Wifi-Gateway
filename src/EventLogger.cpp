#include "EventLogger.h"
#include <LittleFS.h>
#include <esp_system.h>
#include <rom/rtc.h>

static const char* LOG_FILE_PATH = "/evcc_log.txt";
static const char* LOG_FILE_OLD_PATH = "/evcc_log.old";

EventLogger::EventLogger() {
    _mutex = xSemaphoreCreateMutex();
}

String EventLogger::getResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXTERNAL_PIN";
        case ESP_RST_SW:        return "SW_RESET";
        case ESP_RST_PANIC:     return "EXCEPTION_PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "OTHER_WDT";
        case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN (" + String((int)reason) + ")";
    }
}

void EventLogger::begin() {
    if (_mutex && xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        if (!LittleFS.begin(true)) {
            Serial.println("[EventLogger] LittleFS mount failed even with formatOnFail!");
            _fsMounted = false;
        } else {
            _fsMounted = true;
            Serial.println("[EventLogger] LittleFS mounted successfully. Circular buffer logging active.");
            
            // Check active log size; roll if over chunk threshold
            if (LittleFS.exists(LOG_FILE_PATH)) {
                File f = LittleFS.open(LOG_FILE_PATH, "r");
                if (f) {
                    size_t sz = f.size();
                    f.close();
                    if (sz >= MAX_LOG_FILE_SIZE) {
                        if (LittleFS.exists(LOG_FILE_OLD_PATH)) LittleFS.remove(LOG_FILE_OLD_PATH);
                        LittleFS.rename(LOG_FILE_PATH, LOG_FILE_OLD_PATH);
                        _currentLogSize = 0;
                        Serial.printf("[EventLogger] Active log reached %u bytes, rolled to %s\n", MAX_LOG_FILE_SIZE, LOG_FILE_OLD_PATH);
                    } else {
                        _currentLogSize = sz;
                    }
                }
            } else {
                _currentLogSize = 0;
            }
        }
        xSemaphoreGive(_mutex);
    }

    // Log boot banner
    log("SYSTEM", "==================================================");
    log("SYSTEM", "ESP32-S3 N16R8 BOOT: Reset Reason=%s, FreeHeap=%u, PSRAM=%u, Freq=%uMHz",
        getResetReason().c_str(),
        ESP.getFreeHeap(),
        ESP.getPsramSize(),
        ESP.getCpuFreqMHz());
    log("SYSTEM", "==================================================");
}

void EventLogger::log(const char* tag, const char* format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    log(String(tag), String(buf));
}

void EventLogger::log(const String& tag, const String& msg) {
    unsigned long ms = millis();
    unsigned long sec = ms / 1000;
    unsigned long remMs = ms % 1000;

    char timePrefix[32];
    snprintf(timePrefix, sizeof(timePrefix), "[%4lu.%03lu] [%s] ", sec, remMs, tag.c_str());

    String line = String(timePrefix) + msg;
    
    // Always print to hardware serial monitor for real-time visibility
    Serial.println(line);

    appendToFile(line + "\n");
}

void EventLogger::appendToFile(const String& entry) {
    if (!_fsMounted) return;

    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (_currentLogSize + entry.length() > MAX_LOG_FILE_SIZE) {
            // Circular buffer roll:
            // 1. Discard oldest chunk (/evcc_log.old)
            // 2. Promote current active log to /evcc_log.old
            // 3. Open fresh /evcc_log.txt for subsequent samples
            if (LittleFS.exists(LOG_FILE_OLD_PATH)) {
                LittleFS.remove(LOG_FILE_OLD_PATH);
            }
            LittleFS.rename(LOG_FILE_PATH, LOG_FILE_OLD_PATH);
            _currentLogSize = 0;

            File rf = LittleFS.open(LOG_FILE_PATH, "w");
            if (rf) {
                String rotMsg = "[SYSTEM] Log chunk rolled (previous 64KB preserved in " + String(LOG_FILE_OLD_PATH) + ")\n";
                rf.print(rotMsg);
                _currentLogSize += rotMsg.length();
                rf.close();
            }
        }
        File f = LittleFS.open(LOG_FILE_PATH, "a");
        if (f) {
            f.print(entry);
            _currentLogSize += entry.length();
            f.close(); // Immediate flush and close so power cuts don't lose data
        }
        xSemaphoreGive(_mutex);
    }
}

size_t EventLogger::getLogSize() {
    if (!_fsMounted) return 0;
    size_t sz = 0;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (LittleFS.exists(LOG_FILE_OLD_PATH)) {
            File oldF = LittleFS.open(LOG_FILE_OLD_PATH, "r");
            if (oldF) {
                sz += oldF.size();
                oldF.close();
            }
        }
        if (LittleFS.exists(LOG_FILE_PATH)) {
            File f = LittleFS.open(LOG_FILE_PATH, "r");
            if (f) {
                sz += f.size();
                f.close();
            }
        }
        xSemaphoreGive(_mutex);
    }
    return sz;
}

String EventLogger::getRecentLogs(size_t maxBytes) {
    if (!_fsMounted) return "[EventLogger] Flash storage not available.\n";

    String result = "";
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        size_t curSz = 0;
        if (LittleFS.exists(LOG_FILE_PATH)) {
            File f = LittleFS.open(LOG_FILE_PATH, "r");
            if (f) {
                curSz = f.size();
                f.close();
            }
        }

        // Read older chunk if needed to fulfill requested history buffer
        if (curSz < maxBytes && LittleFS.exists(LOG_FILE_OLD_PATH)) {
            size_t neededFromOld = maxBytes - curSz;
            File oldF = LittleFS.open(LOG_FILE_OLD_PATH, "r");
            if (oldF) {
                size_t oldSz = oldF.size();
                if (oldSz > neededFromOld) {
                    oldF.seek(oldSz - neededFromOld);
                    oldF.readStringUntil('\n'); // Align to line boundary
                }
                char buf[257];
                while (oldF.available()) {
                    int r = oldF.read((uint8_t*)buf, sizeof(buf) - 1);
                    if (r > 0) {
                        buf[r] = '\0';
                        result += buf;
                    }
                }
                oldF.close();
            }
        }

        // Read current chunk (latest samples)
        if (LittleFS.exists(LOG_FILE_PATH)) {
            File f = LittleFS.open(LOG_FILE_PATH, "r");
            if (f) {
                size_t sz = f.size();
                if (result.length() == 0 && sz > maxBytes) {
                    f.seek(sz - maxBytes);
                    f.readStringUntil('\n');
                }
                char buf[257];
                while (f.available()) {
                    int r = f.read((uint8_t*)buf, sizeof(buf) - 1);
                    if (r > 0) {
                        buf[r] = '\0';
                        result += buf;
                    }
                }
                f.close();
            }
        }
        xSemaphoreGive(_mutex);
    }
    if (result.length() == 0) {
        result = "[EventLogger] Log is currently empty.\n";
    }
    return result;
}

void EventLogger::dumpToSerial(Stream& out) {
    if (!_fsMounted) {
        out.println("[EventLogger] Flash storage not available.");
        return;
    }

    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        out.println("\n--- DUMPING EVCC GATEWAY LOG (CIRCULAR BUFFER) ---");
        if (LittleFS.exists(LOG_FILE_OLD_PATH)) {
            File oldF = LittleFS.open(LOG_FILE_OLD_PATH, "r");
            if (oldF) {
                out.printf("--- [PREVIOUS CHUNK: %s (%u bytes)] ---\n", LOG_FILE_OLD_PATH, oldF.size());
                uint8_t buf[256];
                while (oldF.available()) {
                    size_t r = oldF.read(buf, sizeof(buf));
                    out.write(buf, r);
                }
                oldF.close();
                out.println();
            }
        }
        if (LittleFS.exists(LOG_FILE_PATH)) {
            File f = LittleFS.open(LOG_FILE_PATH, "r");
            if (f) {
                out.printf("--- [ACTIVE CHUNK: %s (%u bytes)] ---\n", LOG_FILE_PATH, f.size());
                uint8_t buf[256];
                while (f.available()) {
                    size_t r = f.read(buf, sizeof(buf));
                    out.write(buf, r);
                }
                f.close();
            }
        }
        out.println("\n--- END OF EVCC GATEWAY LOG ---");
        xSemaphoreGive(_mutex);
    }
}

void EventLogger::clear() {
    if (!_fsMounted) return;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (LittleFS.exists(LOG_FILE_OLD_PATH)) {
            LittleFS.remove(LOG_FILE_OLD_PATH);
        }
        if (LittleFS.exists(LOG_FILE_PATH)) {
            LittleFS.remove(LOG_FILE_PATH);
        }
        _currentLogSize = 0;
        xSemaphoreGive(_mutex);
    }
    log("SYSTEM", "Circular log cleared by user request.");
}
