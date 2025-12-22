/**
 * Debug.h - Debug utilities for Cortex Link A8R-M ESP32 IoT Smart Home Controller
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include <Wire.h>

 // Debug levels
#define DEBUG_LEVEL_NONE    0
#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_WARNING 2
#define DEBUG_LEVEL_INFO    3
#define DEBUG_LEVEL_DEBUG   4
#define DEBUG_LEVEL_TRACE   5

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_ERROR
#endif

// Always route through Debug::log; runtime level controls output.
// Do NOT compile-out logs so we can change level at runtime.
#define DEBUG_LOG(level, format, ...) Debug::log(level, format, ##__VA_ARGS__)
#define ERROR_LOG(format, ...)        Debug::log(DEBUG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define WARNING_LOG(format, ...)      Debug::log(DEBUG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define INFO_LOG(format, ...)         Debug::log(DEBUG_LEVEL_INFO, format, ##__VA_ARGS__)
#define DEBUG_LOG_MSG(format, ...)    Debug::log(DEBUG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define TRACE_LOG(format, ...)        Debug::log(DEBUG_LEVEL_TRACE, format, ##__VA_ARGS__)

#define LOG_MEMORY() Debug::logMemoryUsage()

class Debug {
public:
    static void begin(unsigned long baudRate = 115200);
    static void log(uint8_t level, const char* format, ...);
    static void logMemoryUsage();
    static void scanI2CDevices(TwoWire& wire = Wire);
    static void debugAssert(bool condition, const char* message = nullptr);
    static void MSGPrintln(String message);
    static void errorPrintln(String message);
    static void warningPrintln(String message);
    static void infoPrintln(String message);
    static void debugPrintln(String message);

    // Runtime level control
    static void setLevel(uint8_t level);
    static uint8_t getLevel();

private:
    static bool initialized;
    static const char* levelNames[];
    static uint8_t currentLevel;
};

#endif // DEBUG_H