/**
 * DigitalInputDriver.cpp - Implementation of digital input driver
 * For Cortex Link A8R-M ESP32 IoT Smart Home Controller
 */

#include "DigitalInputDriver.h"

 // Create global instance
DigitalInputDriver digitalInputs;

// Pointer to the instance for use in ISR
static DigitalInputDriver* inputDriverInstance = nullptr;

// ISR for Port A interrupt - Keep this extremely minimal
void IRAM_ATTR isrDigitalInputA() {
    if (inputDriverInstance) {
        // Just set a flag - do nothing else in the ISR
        inputDriverInstance->interruptFlagA = true;
    }
}

// ISR for Port B interrupt - Keep this extremely minimal
void IRAM_ATTR isrDigitalInputB() {
    if (inputDriverInstance) {
        // Just set a flag - do nothing else in the ISR
        inputDriverInstance->interruptFlagB = true;
    }
}

DigitalInputDriver::DigitalInputDriver()
    : mcp(MCP23017_INPUT_ADDR),
    inputState(0),
    portAState(0),
    connected(false),
    interruptFlagA(false),
    interruptFlagB(false),
    lastInterruptTime(0),
    rtcSqwChanged(false),
    _inputsChanged(false) {}


bool DigitalInputDriver::begin(TwoWire& wire) {
    DEBUG_LOG(DEBUG_LEVEL_INFO, "Initializing DigitalInputDriver...");

    // Store instance pointer for ISRs
    inputDriverInstance = this;

    // Initialize MCP23017
    wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);
    delay(50);
    mcp.init();
    delay(50); // Short delay for stability

    // Reset the device to ensure clean state
    reset();

    // Configure pins
    configurePins();

    // Verify connection
    connected = scanI2C(wire);
    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_ERROR, "Failed to detect MCP23017 at address 0x%02X", MCP23017_INPUT_ADDR);
        return false;
    }

    // Read initial input states
    updateAllInputs();

    // Read initial port A state
    portAState = mcp.readRegister(MCP23017Register::GPIO_A);

    DEBUG_LOG(DEBUG_LEVEL_INFO, "DigitalInputDriver initialized successfully");
    return true;
}

void DigitalInputDriver::reset() {
    DEBUG_LOG(DEBUG_LEVEL_INFO, "Resetting MCP23017 input device");

    // Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);

    // Software reset by reinitializing the device
    mcp.init();
    delay(50);

    // Clear all registers to default values
    for (uint8_t reg = 0; reg <= 0x1A; reg++) {
        mcp.writeRegister(static_cast<MCP23017Register>(reg), 0x00);
    }

    // Configure pins again
    configurePins();

    // Reset interrupt flags
    interruptFlagA = false;
    interruptFlagB = false;
    rtcSqwChanged = false;

    DEBUG_LOG(DEBUG_LEVEL_INFO, "MCP23017 reset complete");
}

bool DigitalInputDriver::scanI2C(TwoWire& wire) {
    DEBUG_LOG(DEBUG_LEVEL_INFO, "Scanning for MCP23017 input device at address 0x%02X", MCP23017_INPUT_ADDR);

    wire.beginTransmission(MCP23017_INPUT_ADDR);
    uint8_t error = wire.endTransmission();

    if (error == 0) {
        DEBUG_LOG(DEBUG_LEVEL_INFO, "MCP23017 input device found at address 0x%02X", MCP23017_INPUT_ADDR);
        return true;
    }
    else {
        DEBUG_LOG(DEBUG_LEVEL_ERROR, "MCP23017 input device not found. Error: %d", error);
        return false;
    }
}

void DigitalInputDriver::configurePins() {
    DEBUG_LOG(DEBUG_LEVEL_DEBUG, "Configuring MCP23017 pins for digital inputs");

    // Configure all Port B pins (GPB0-GPB7) as inputs with pull-ups
    // Third parameter 'true' inverts the polarity of inputs due to pull-ups
    for (uint8_t pin = 8; pin <= 15; pin++) {
        mcp.pinMode(pin, INPUT_PULLUP, true);
    }

    // Port A configuration for special pins
    // GPA5 (pin 5) - Output for Ethernet W5500 reset
    mcp.pinMode(MCP_ETH_RESET_PIN, OUTPUT);
    // Set initial state HIGH (inactive)
    mcp.digitalWrite(MCP_ETH_RESET_PIN, HIGH);

    // GPA7 (pin 7) - Input for RTC SQW signal
    mcp.pinMode(MCP_RTC_SQW_PIN, INPUT_PULLUP);

    DEBUG_LOG(DEBUG_LEVEL_DEBUG, "MCP23017 pin configuration complete");
}

bool DigitalInputDriver::isValidInput(uint8_t inputNum) {
    // Check if input number is in range (1-8)
    return (inputNum >= 1 && inputNum <= NUM_DIGITAL_INPUTS);
}

bool DigitalInputDriver::readInput(uint8_t inputNum) {
    if (!isValidInput(inputNum)) {
        DEBUG_LOG(DEBUG_LEVEL_ERROR, "Invalid input number: %d", inputNum);
        return false;
    }

    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_WARNING, "MCP23017 not connected");
        return false;
    }

    // Input numbers 1-8 map to GPB0-GPB7 (pins 8-15 in MCP23017 library)
    uint8_t pin = inputNum + 7;  // Convert input number to MCP pin number
    bool state = mcp.digitalRead(pin);

    DEBUG_LOG(DEBUG_LEVEL_DEBUG, "Digital Input %d (GPB%d) = %d", inputNum, inputNum - 1, state);
    return state;
}

uint8_t DigitalInputDriver::readAllInputs() {
    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_WARNING, "MCP23017 not connected");
        return 0;
    }

    // Read the entire Port B at once (more efficient)
    uint8_t portBValue = mcp.readRegister(MCP23017Register::GPIO_B);

    DEBUG_LOG(DEBUG_LEVEL_DEBUG, "All Digital Inputs = 0x%02X", portBValue);
    inputState = portBValue;
    return portBValue;
}

void DigitalInputDriver::updateAllInputs() {
    uint8_t previous = inputState;
    inputState = readAllInputs();
    portAState = mcp.readRegister(MCP23017Register::GPIO_A);
    if (previous != inputState) {
        _inputsChanged = true;
    }
}

uint8_t DigitalInputDriver::getInputState() const {
    return inputState;
}

void DigitalInputDriver::setupInterrupts(uint8_t mode) {
    DEBUG_LOG(DEBUG_LEVEL_INFO, "Setting up MCP23017 interrupts, mode: %d", mode);

    // Configure interrupt pins on ESP32
    pinMode(PIN_MCP_INTA, INPUT_PULLUP);
    pinMode(PIN_MCP_INTB, INPUT_PULLUP);

    // Define iocon variable outside switch statement to avoid initialization crossing
    uint8_t iocon;

    // Configure MCP23017 interrupts
    switch (mode) {
    case INPUT_INT_DISABLE:
        // Disable all interrupts
        mcp.writeRegister(MCP23017Register::GPINTEN_A, 0x00);
        mcp.writeRegister(MCP23017Register::GPINTEN_B, 0x00);
        break;

    case INPUT_INT_CHANGE:
        // Enable interrupt-on-change for all Port B pins
        mcp.writeRegister(MCP23017Register::GPINTEN_B, 0xFF);

        // No need to set INTCON register as default is 0 (interrupt on pin change)
        mcp.writeRegister(MCP23017Register::INTCON_B, 0x00);

        // Don't mirror interrupts - use both INTA and INTB separately
        // This helps reduce interrupt load
        iocon = mcp.readRegister(MCP23017Register::IOCON) & ~0x40;
        mcp.writeRegister(MCP23017Register::IOCON, iocon);
        break;

    case INPUT_INT_FALLING:
        // Enable interrupts for all Port B pins
        mcp.writeRegister(MCP23017Register::GPINTEN_B, 0xFF);

        // Compare against DEFVAL (interrupts when different from DEFVAL)
        mcp.writeRegister(MCP23017Register::INTCON_B, 0xFF);

        // Set DEFVAL to 1 (so 0 will trigger interrupt)
        mcp.writeRegister(MCP23017Register::DEFVAL_B, 0xFF);
        break;

    case INPUT_INT_RISING:
        // Enable interrupts for all Port B pins
        mcp.writeRegister(MCP23017Register::GPINTEN_B, 0xFF);

        // Compare against DEFVAL (interrupts when different from DEFVAL)
        mcp.writeRegister(MCP23017Register::INTCON_B, 0xFF);

        // Set DEFVAL to 0 (so 1 will trigger interrupt)
        mcp.writeRegister(MCP23017Register::DEFVAL_B, 0x00);
        break;
    }

    // Attach interrupt handlers to ESP32 interrupt pins
    if (mode != INPUT_INT_DISABLE) {
        // Clear any pending interrupts before enabling
        mcp.readRegister(MCP23017Register::INTCAP_A);
        mcp.readRegister(MCP23017Register::INTCAP_B);

        attachInterrupt(digitalPinToInterrupt(PIN_MCP_INTA), isrDigitalInputA, FALLING);
        attachInterrupt(digitalPinToInterrupt(PIN_MCP_INTB), isrDigitalInputB, FALLING);
        DEBUG_LOG(DEBUG_LEVEL_INFO, "MCP23017 interrupts enabled");
    }
    else {
        detachInterrupt(digitalPinToInterrupt(PIN_MCP_INTA));
        detachInterrupt(digitalPinToInterrupt(PIN_MCP_INTB));
        DEBUG_LOG(DEBUG_LEVEL_INFO, "MCP23017 interrupts disabled");
    }
}

// This method should be called frequently from the main loop
void DigitalInputDriver::processInterrupts() {
    if (!connected) return;
    unsigned long currentTime = millis();

    if (interruptFlagA) {
        interruptFlagA = false;
        lastInterruptTime = currentTime;
        uint8_t capturedValueA = mcp.readRegister(MCP23017Register::INTCAP_A);
        portAState = capturedValueA;
        rtcSqwChanged = true;
    }

    if (interruptFlagB) {
        interruptFlagB = false;
        // Simple debounce
        if (currentTime - lastInterruptTime >= 50) {
            lastInterruptTime = currentTime;
            uint8_t capturedValueB = mcp.readRegister(MCP23017Register::INTCAP_B);
            if (capturedValueB != inputState) {
                inputState = capturedValueB;
                _inputsChanged = true;
            }
        }
    }
}

// === IMPLEMENTATIONS FOR PORT A SPECIAL PINS ===

// Ethernet W5500 reset control (GPA5)
void DigitalInputDriver::ethernetReset(bool state) {
    if (!connected) return;
    uint8_t pinNumber = MCP_ETH_RESET_PIN;
    mcp.digitalWrite(pinNumber, state);
    if (state) portAState |= (1 << MCP_ETH_RESET_PIN);
    else       portAState &= ~(1 << MCP_ETH_RESET_PIN);
}

void DigitalInputDriver::pulseEthernetReset(unsigned long duration) {
    if (!connected) return;
    ethernetReset(LOW);
    delay(duration);
    ethernetReset(HIGH);
    delay(ETH_RECOVERY_DELAY);
}

bool DigitalInputDriver::getEthernetResetState() const {
    if (!connected) return false;
    return (portAState & (1 << MCP_ETH_RESET_PIN)) != 0;
}

// RTC DS3231 SQW pin state (GPA7)
bool DigitalInputDriver::readRtcSqwPin() const {
    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_WARNING, "MCP23017 not connected");
        return false;
    }

    // Read from cached port state instead of calling mcp methods
    return (portAState & (1 << MCP_RTC_SQW_PIN)) != 0;
}

void DigitalInputDriver::enableRtcSqwInterrupt(bool enable) {
    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_WARNING, "MCP23017 not connected");
        return;
    }

    uint8_t gpintenA = mcp.readRegister(MCP23017Register::GPINTEN_A);

    if (enable) {
        // Enable interrupt on GPA7 (RTC SQW)
        gpintenA |= (1 << MCP_RTC_SQW_PIN);
        DEBUG_LOG(DEBUG_LEVEL_INFO, "RTC SQW pin interrupt enabled");
    }
    else {
        // Disable interrupt on GPA7 (RTC SQW)
        gpintenA &= ~(1 << MCP_RTC_SQW_PIN);
        DEBUG_LOG(DEBUG_LEVEL_INFO, "RTC SQW pin interrupt disabled");
    }

    mcp.writeRegister(MCP23017Register::GPINTEN_A, gpintenA);

    // Clear the flag
    rtcSqwChanged = false;
}

bool DigitalInputDriver::hasRtcSqwChanged() const {
    return rtcSqwChanged;
}

// Direct port access for external drivers
void DigitalInputDriver::writePortA(uint8_t value) {
    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_WARNING, "MCP23017 not connected");
        return;
    }

    mcp.writeRegister(MCP23017Register::GPIO_A, value);
    portAState = value;
}

void DigitalInputDriver::writePortAPin(uint8_t pin, bool value) {
    if (!connected || pin > 7) return;
    mcp.digitalWrite(pin, value);
    if (value) portAState |= (1 << pin);
    else       portAState &= ~(1 << pin);
}

uint8_t DigitalInputDriver::readPortA() const {
    if (!connected) {
        DEBUG_LOG(DEBUG_LEVEL_WARNING, "MCP23017 not connected");
        return 0;
    }

    // Return cached value instead of calling mcp.readRegister()
    return portAState;
}

bool DigitalInputDriver::readPortAPin(uint8_t pin) const {
    if (!connected || pin > 7) return false;
    return (portAState & (1 << pin)) != 0;
}