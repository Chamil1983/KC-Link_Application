/**
 * RelayDriver.h - Relay control driver for Cortex Link A8R-M ESP32 IoT Smart Home Controller
 * Uses MCP23017 I/C IO Expander for relay control
 */

#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <MCP23017.h>
#include "Debug.h"
#include "Config.h"

 // Relay pin definitions (MCP23017 GPA pins)
#define RELAY_1     0  // GPA0
#define RELAY_2     1  // GPA1
#define RELAY_3     2  // GPA2
#define RELAY_4     3  // GPA3
#define RELAY_5     4  // GPA4
#define RELAY_6     5  // GPA5

// Relay states (Logic HIGH = OFF, Logic LOW = ON)
#define RELAY_ON    LOW
#define RELAY_OFF   HIGH

// Bit masks for relay operations
#define RELAY_ALL_BITS      0x3F  // 00111111 (bits 0-5 for the 6 relays)

class RelayDriver {
public:
    // Initialize the relay driver
    bool begin();

    // Reset the MCP23017 device
    bool resetDevice();

    // I2C scan function to check connection
    bool scanI2C();

    // Single relay control
    bool turnOn(uint8_t relayNum);  // relayNum: 1-6
    bool turnOff(uint8_t relayNum);
    bool toggle(uint8_t relayNum);
    bool setState(uint8_t relayNum, uint8_t state); // state: RELAY_ON or RELAY_OFF

    // Get relay state
    uint8_t getState(uint8_t relayNum); // returns RELAY_ON, RELAY_OFF, or 0xFF on error

    // Multiple relay control
    bool setMultiple(uint8_t relayMask, uint8_t state);
    uint8_t getAllStates();  // Returns current state of all relays

    // All relay control
    bool turnOnAll();
    bool turnOffAll();  // Safety reset function

    // State verification
    bool verifyState(uint8_t relayNum, uint8_t desiredState);

    // Connection status
    bool isConnected() const { return _initialized; }

private:
    MCP23017 _mcp = MCP23017(MCP23017_OUTPUT_ADDR);
    bool _initialized = false;
    uint8_t _relayStates = 0xFF;  // Default all relays OFF (HIGH)

    // Helper methods
    bool _isValidRelayNum(uint8_t relayNum);
    uint8_t _relayNumToPin(uint8_t relayNum);
    void _updateRelayState(uint8_t relayPin, uint8_t state);

    // Robust single-bit write to GPIO_A (atomic read-modify-write)
    bool _writeRelayPin(uint8_t relayPin, uint8_t state);
};

#endif // RELAY_DRIVER_H