/**
 * Debug.cpp - Implementation of Debug utilities
 */

#include "Debug.h"
#include <stdarg.h>

bool Debug::initialized = false;
uint8_t Debug::currentLevel = DEBUG_LEVEL; // default from compile-time setting
const char* Debug::levelNames[] = { "NONE", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE" };

void Debug::begin(unsigned long baudRate) {
    if (!initialized) {
        Serial.begin(baudRate);
        delay(50);
        Serial.println("Debug started");
        initialized = true;
    }
}

void Debug::log(uint8_t level, const char* format, ...) {
    if (!initialized) {
        begin();
    }

    // Runtime level gating
    if (level > currentLevel) return;

    char buffer[160];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    switch (level) {
    case DEBUG_LEVEL_ERROR:   Serial.print("[ERROR] "); break;
    case DEBUG_LEVEL_WARNING: Serial.print("[WARN] ");  break;
    case DEBUG_LEVEL_INFO:    Serial.print("[INFO] ");  break;
    case DEBUG_LEVEL_DEBUG:   Serial.print("[DEBUG] "); break;
    case DEBUG_LEVEL_TRACE:   Serial.print("[TRACE] "); break;
    default: break;
    }
    Serial.println(buffer);
}

void Debug::logMemoryUsage() {
#ifdef ESP32
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
#endif
}

void Debug::scanI2CDevices(TwoWire& wire) {
    Serial.println("I2C scan:");
    uint8_t deviceCount = 0;
    for (uint8_t address = 1; address < 127; address++) {
        wire.beginTransmission(address);
        uint8_t error = wire.endTransmission();
        if (error == 0) {
            Serial.print("  Device at 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            deviceCount++;
        }
    }
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" devices");
}

void Debug::debugAssert(bool condition, const char* message) {
    if (!condition && message) {
        Serial.print("ASSERT: ");
        Serial.println(message);
    }
}

void Debug::MSGPrintln(String message) { if (currentLevel >= DEBUG_LEVEL_INFO)    Serial.println(message); }
void Debug::errorPrintln(String message) { if (currentLevel >= DEBUG_LEVEL_ERROR)   Serial.print("[ERROR] "), Serial.println(message); }
void Debug::warningPrintln(String message) { if (currentLevel >= DEBUG_LEVEL_WARNING) Serial.print("[WARN] "), Serial.println(message); }
void Debug::infoPrintln(String message) { if (currentLevel >= DEBUG_LEVEL_INFO)    Serial.print("[INFO] "), Serial.println(message); }
void Debug::debugPrintln(String message) { if (currentLevel >= DEBUG_LEVEL_DEBUG)   Serial.print("[DEBUG] "), Serial.println(message); }

void Debug::setLevel(uint8_t level) {
    if (level > DEBUG_LEVEL_TRACE) level = DEBUG_LEVEL_TRACE;
    currentLevel = level;
    Serial.print("Debug level changed to ");
    Serial.println(levelNames[(level <= DEBUG_LEVEL_TRACE) ? level : DEBUG_LEVEL_TRACE]);
}
uint8_t Debug::getLevel() {
    return currentLevel;
}