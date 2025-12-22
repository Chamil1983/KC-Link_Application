/**
 * DigitalInputDriver.h (Corrected)
 * Added inputsChanged flag + clear method for real-time MQTT publishing.
 */
#ifndef DIGITAL_INPUT_DRIVER_H
#define DIGITAL_INPUT_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <MCP23017.h>
#include "Config.h"
#include "Debug.h"

#define INPUT_INT_DISABLE    0
#define INPUT_INT_CHANGE     1
#define INPUT_INT_FALLING    2
#define INPUT_INT_RISING     3

#define MCP_ETH_RESET_PIN_CALC ((MCP_ETH_RESET_PORT == MCP23017Port::A) ? MCP_ETH_RESET_PIN : (MCP_ETH_RESET_PIN + 8))

class DigitalInputDriver {
public:
    DigitalInputDriver();

    bool begin(TwoWire& wire = Wire);
    void reset();
    bool scanI2C(TwoWire& wire = Wire);

    bool readInput(uint8_t inputNum);
    uint8_t readAllInputs();
    void updateAllInputs();
    uint8_t getInputState() const;

    void setupInterrupts(uint8_t mode = INPUT_INT_CHANGE);
    void processInterrupts();

    // Change detection (new)
    bool inputsChanged() const { return _inputsChanged; }
    void clearInputsChanged() { _inputsChanged = false; }

    // Port A special pins
    void ethernetReset(bool state);
    void pulseEthernetReset(unsigned long duration = ETH_RESET_DURATION);
    bool getEthernetResetState() const;

    bool readRtcSqwPin() const;
    void enableRtcSqwInterrupt(bool enable);
    bool hasRtcSqwChanged() const;

    void writePortA(uint8_t value);
    void writePortAPin(uint8_t pin, bool value);
    uint8_t readPortA() const;
    bool readPortAPin(uint8_t pin) const;

    bool isConnected() const { return connected; }

    friend void IRAM_ATTR isrDigitalInputA();
    friend void IRAM_ATTR isrDigitalInputB();

private:
    MCP23017 mcp;
    uint8_t  inputState;
    uint8_t  portAState;
    bool     connected;
    volatile bool interruptFlagA;
    volatile bool interruptFlagB;
    unsigned long lastInterruptTime;
    bool rtcSqwChanged;

    // New flag set when digital inputs actually change
    volatile bool _inputsChanged = false;

    void configurePins();
    bool isValidInput(uint8_t inputNum);
};

void IRAM_ATTR isrDigitalInputA();
void IRAM_ATTR isrDigitalInputB();

extern DigitalInputDriver digitalInputs;

#endif // DIGITAL_INPUT_DRIVER_H