#pragma once
/**
 * Types.h
 * Data structures used across modules.
 */

#include "Definitions.h"

// Structure for interrupt configuration
struct InterruptConfig {
    bool enabled;
    uint8_t priority;     // 0=disabled, 1=high, 2=medium, 3=low
    uint8_t inputIndex;   // 0-15 for 16 digital inputs
    uint8_t triggerType;  // 0=rising, 1=falling, 2=change, 3=high level, 4=low level
    char name[32];        // Name for this interrupt
};

// Structure for HT pin configuration
struct HTSensorConfig {
    uint8_t sensorType;     // 0=Digital, 1=DHT11, 2=DHT22, 3=DS18B20
    float temperature;      // Last temperature reading (�C)
    float humidity;         // Last humidity reading (% - only for DHT sensors)
    bool configured;        // Whether sensor has been configured
    unsigned long lastReadTime; // Last time sensor was read
};

struct TimeSchedule {
    bool enabled;
    uint8_t triggerType;  // 0=Time-based, 1=Input-based, 2=Combined, 3=Sensor-based
    uint8_t days;         // Bit field: bit 0=Sunday, bit 1=Monday, ..., bit 6=Saturday (for time-based)
    uint8_t hour;         // Hour for time-based trigger
    uint8_t minute;       // Minute for time-based trigger
    uint16_t inputMask;   // Bit mask for inputs (bits 0-15 for digital inputs, bits 16-18 for HT1-HT3)
    uint16_t inputStates; // Required state for each input (0=LOW, 1=HIGH)
    uint8_t logic;        // 0=AND (all conditions must be met), 1=OR (any condition can trigger)
    uint8_t action;       // 0=OFF, 1=ON, 2=TOGGLE
    uint8_t targetType;   // 0=Output, 1=Multiple outputs
    uint16_t targetId;    // Output number (0-15) or bitmask for multiple outputs
    uint16_t targetIdLow; // Additional target for LOW state (when input is FALSE)
    char name[32];        // Name/description of the schedule

    // New fields for sensor triggers
    uint8_t sensorIndex;      // HT sensor index (0-2 for HT1-HT3)
    uint8_t sensorTriggerType; // 0=Temperature, 1=Humidity
    uint8_t sensorCondition;   // 0=Above, 1=Below, 2=Equal
    float sensorThreshold;     // Temperature or humidity threshold value
};

struct AnalogTrigger {
    bool enabled;
    uint8_t analogInput;    // 0-3 (A1-A4)
    uint16_t threshold;     // Analog threshold value (0-4095)
    uint8_t condition;      // 0=Above, 1=Below, 2=Equal
    uint8_t action;         // 0=OFF, 1=ON, 2=TOGGLE
    uint8_t targetType;     // 0=Output, 1=Multiple outputs
    uint16_t targetId;      // Output number (0-15) or bitmask
    char name[32];          // Name/description of trigger

    // New fields for combined triggers
    bool combinedMode;      // Enable combined mode with digital inputs
    uint16_t inputMask;     // Bit mask for digital inputs (bits 0-15 for digital inputs, 16-18 for HT1-HT3)
    uint16_t inputStates;   // Required state for each input (0=LOW, 1=HIGH)
    uint8_t logic;          // 0=AND (all conditions must be met), 1=OR (any condition can trigger)

    // Support for HT analog sensors
    uint8_t htSensorIndex;  // 0-2 for HT1-HT3
    uint8_t sensorTriggerType; // 0=Temperature, 1=Humidity
    uint8_t sensorCondition;   // 0=Above, 1=Below, 2=Equal
    float sensorThreshold;     // Temperature or humidity threshold value
};
