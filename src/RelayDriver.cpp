/**
 * RelayDriver.cpp - Implementation of relay control for Cortex Link A8R-M
 */

#include "RelayDriver.h"

bool RelayDriver::begin() {

    // Initialize MCP23017 immediately - matching example
    _mcp = MCP23017(MCP23017_OUTPUT_ADDR);

    Debug::infoPrintln("Initializing RelayDriver...");

    // Initialize I2C - follow the example's simpler approach first
    Wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);
    delay(100);

    _mcp.init();

    Debug::infoPrintln("MCP23017 initialized");

    // Configure relay pins as outputs (exactly like the example)
    for (uint8_t i = RELAY_1; i <= RELAY_6; i++) {
        _mcp.pinMode(i, OUTPUT);
    }
    Debug::infoPrintln("Relay pins configured as outputs");

    // Reset MCP23017 ports
    _mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    _mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);
    Debug::infoPrintln("MCP23017 ports reset");

    // Safety - set all relays OFF - HIGH is OFF
    for (uint8_t i = RELAY_1; i <= RELAY_6; i++) {
        // Use robust masked write instead of digitalWrite
        _writeRelayPin(i, RELAY_OFF); // HIGH = OFF
    }
    Debug::infoPrintln("All relays turned OFF for safety");

    _initialized = true;
    Debug::infoPrintln("RelayDriver initialized successfully");
    return true;
}

bool RelayDriver::scanI2C() {
    Debug::infoPrintln("Scanning I2C bus...");

    bool mcpFound = false;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Debug::infoPrintln("I2C device found at address 0x" + String(address, HEX));
            if (address == MCP23017_OUTPUT_ADDR) {
                Debug::infoPrintln("MCP23017 found at address 0x" + String(address, HEX));
                mcpFound = true;
            }
        }
    }

    if (!mcpFound) {
        Debug::errorPrintln("MCP23017 not found at address 0x" + String(MCP23017_OUTPUT_ADDR, HEX));
    }

    return mcpFound;
}

bool RelayDriver::resetDevice() {
    Debug::infoPrintln("Resetting MCP23017 device...");

    // Re-initialize and reset like the example
    _mcp.init();

    // Reset the ports
    _mcp.writeRegister(MCP23017Register::GPIO_A, 0x00);
    _mcp.writeRegister(MCP23017Register::GPIO_B, 0x00);

    // Set all relays to OFF state (HIGH)
    for (uint8_t i = RELAY_1; i <= RELAY_6; i++) {
        _writeRelayPin(i, RELAY_OFF);
    }

    _relayStates = 0xFF; // All relays off in our tracking

    Debug::infoPrintln("MCP23017 device reset complete");
    return true;
}

bool RelayDriver::_writeRelayPin(uint8_t relayPin, uint8_t state) {
    // Atomic read-modify-write on GPIO_A to avoid glitches
    uint8_t portValue = _mcp.readRegister(MCP23017Register::GPIO_A);
    uint8_t bitMask = (1 << relayPin);

    uint8_t before = portValue;

    if (state == RELAY_ON) {
        portValue &= ~bitMask;   // LOW = ON (clear the bit)
    }
    else {
        portValue |= bitMask;    // HIGH = OFF (set the bit)
    }

    // Some MCP23017 libraries return void for writeRegister; do not capture return value
    _mcp.writeRegister(MCP23017Register::GPIO_A, portValue);
    bool ok = true;

    uint8_t after = _mcp.readRegister(MCP23017Register::GPIO_A);

    // Update internal cache
    _updateRelayState(relayPin, state);

    // Detailed debug trace (use hex for portability)
    INFO_LOG("Relay pin %u write: state=%s before=0x%02X after=0x%02X",
        relayPin,
        (state == RELAY_ON ? "ON" : "OFF"),
        (unsigned)(before & RELAY_ALL_BITS),
        (unsigned)(after & RELAY_ALL_BITS));

    // Verify by reading the pin
    uint8_t readBack = _mcp.digitalRead(relayPin);
    INFO_LOG("Relay pin %u verify: digitalRead=%u (expected %u)", relayPin, readBack, state);

    return ok && (readBack == state);
}

bool RelayDriver::turnOn(uint8_t relayNum) {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return false;
    }

    if (!_isValidRelayNum(relayNum)) {
        Debug::errorPrintln("Invalid relay number: " + String(relayNum));
        return false;
    }

    uint8_t pin = _relayNumToPin(relayNum);
    Debug::debugPrintln("Turning ON relay " + String(relayNum) + " (pin " + String(pin) + ")");
    return _writeRelayPin(pin, RELAY_ON);
}

bool RelayDriver::turnOff(uint8_t relayNum) {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return false;
    }

    if (!_isValidRelayNum(relayNum)) {
        Debug::errorPrintln("Invalid relay number: " + String(relayNum));
        return false;
    }

    uint8_t pin = _relayNumToPin(relayNum);
    Debug::debugPrintln("Turning OFF relay " + String(relayNum) + " (pin " + String(pin) + ")");
    return _writeRelayPin(pin, RELAY_OFF);
}

bool RelayDriver::toggle(uint8_t relayNum) {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return false;
    }

    if (!_isValidRelayNum(relayNum)) {
        Debug::errorPrintln("Invalid relay number: " + String(relayNum));
        return false;
    }

    uint8_t pin = _relayNumToPin(relayNum);
    uint8_t currentState = _mcp.digitalRead(pin);
    uint8_t nextState = (currentState == RELAY_ON) ? RELAY_OFF : RELAY_ON;

    String msg = String("Toggling relay ") + String(relayNum) +
        " (pin " + String(pin) + ") from " +
        (currentState == RELAY_ON ? String("ON") : String("OFF")) +
        " to " +
        (nextState == RELAY_ON ? String("ON") : String("OFF"));
    Debug::debugPrintln(msg);

    return _writeRelayPin(pin, nextState);
}

bool RelayDriver::setState(uint8_t relayNum, uint8_t state) {
    return (state == RELAY_ON) ? turnOn(relayNum) : turnOff(relayNum);
}

uint8_t RelayDriver::getState(uint8_t relayNum) {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return 0xFF;
    }

    if (!_isValidRelayNum(relayNum)) {
        Debug::errorPrintln("Invalid relay number: " + String(relayNum));
        return 0xFF;
    }

    uint8_t pin = _relayNumToPin(relayNum);
    uint8_t state = _mcp.digitalRead(pin);

    Debug::debugPrintln("Relay " + String(relayNum) + " state: " +
        (state == RELAY_ON ? "ON" : "OFF"));

    return state;
}

bool RelayDriver::setMultiple(uint8_t relayMask, uint8_t state) {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return false;
    }

    if (relayMask > RELAY_ALL_BITS) {
        Debug::errorPrintln("Invalid relay mask: " + String(relayMask, HEX));
        return false;
    }

    Debug::debugPrintln("Setting multiple relays with mask " + String(relayMask, BIN) +
        " to " + (state == RELAY_ON ? "ON" : "OFF"));

    // Read current port value
    uint8_t portValue = _mcp.readRegister(MCP23017Register::GPIO_A);

    if (state == RELAY_ON) {
        // For ON, we need to clear the bits (set to LOW)
        portValue &= ~relayMask;
    }
    else {
        // For OFF, we need to set the bits (set to HIGH)
        portValue |= relayMask;
    }

    // Write back to port
    _mcp.writeRegister(MCP23017Register::GPIO_A, portValue);

    // Update internal state tracking
    if (state == RELAY_ON) {
        _relayStates &= ~relayMask;  // Clear bit (ON is LOW)
    }
    else {
        _relayStates |= relayMask;   // Set bit (OFF is HIGH)
    }

    return true;
}

uint8_t RelayDriver::getAllStates() {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return 0xFF;
    }

    // Read port A value (relays are on pins 0-5)
    uint8_t portValue = _mcp.readRegister(MCP23017Register::GPIO_A);

    // Mask to only include relay bits (0-5)
    uint8_t relayStates = portValue & RELAY_ALL_BITS;

    Debug::debugPrintln("All relay states: " + String(relayStates, BIN));

    // Update internal state cache
    _relayStates = relayStates;

    return relayStates;
}

bool RelayDriver::turnOnAll() {
    Debug::infoPrintln("Turning ON all relays");
    return setMultiple(RELAY_ALL_BITS, RELAY_ON);
}

bool RelayDriver::turnOffAll() {
    Debug::infoPrintln("Turning OFF all relays (safe reset)");
    return setMultiple(RELAY_ALL_BITS, RELAY_OFF);
}

bool RelayDriver::verifyState(uint8_t relayNum, uint8_t desiredState) {
    if (!_initialized) {
        Debug::errorPrintln("RelayDriver not initialized");
        return false;
    }

    if (!_isValidRelayNum(relayNum)) {
        Debug::errorPrintln("Invalid relay number: " + String(relayNum));
        return false;
    }

    uint8_t pin = _relayNumToPin(relayNum);
    uint8_t actualState = _mcp.digitalRead(pin);

    bool match = (actualState == desiredState);
    if (!match) {
        Debug::warningPrintln("Relay " + String(relayNum) + " state mismatch: expected " +
            String(desiredState) + ", actual " + String(actualState));
    }

    return match;
}

bool RelayDriver::_isValidRelayNum(uint8_t relayNum) {
    return (relayNum >= 1 && relayNum <= 6);
}

uint8_t RelayDriver::_relayNumToPin(uint8_t relayNum) {
    // Convert from 1-based relay number to 0-based pin number
    return relayNum - 1;
}

void RelayDriver::_updateRelayState(uint8_t relayPin, uint8_t state) {
    // Update the bit in our internal state tracking
    uint8_t relayBit = (1 << relayPin);

    if (state == RELAY_ON) {
        _relayStates &= ~relayBit;  // Clear bit (ON is LOW)
    }
    else {
        _relayStates |= relayBit;   // Set bit (OFF is HIGH)
    }
}