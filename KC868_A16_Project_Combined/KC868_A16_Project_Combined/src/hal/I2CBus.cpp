// I2CBus.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void initI2C() {

    // Configure input pins (set as inputs with pull-ups)
    for (int i = 0; i < 8; i++) {
        inputIC1.pinMode(i, INPUT);
        inputIC2.pinMode(i, INPUT);
    }

    // Configure output pins (set as outputs)
    for (int i = 0; i < 8; i++) {
        outputIC3.pinMode(i, OUTPUT);
        outputIC4.pinMode(i, OUTPUT);
    }


    // Initialize PCF8574 ICs
    if (!inputIC1.begin()) {
        Serial.println("Error: Could not initialize Input IC1 (0x22)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Input IC1";
    }

    if (!inputIC2.begin()) {
        Serial.println("Error: Could not initialize Input IC2 (0x21)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Input IC2";
    }

    if (!outputIC3.begin()) {
        Serial.println("Error: Could not initialize Output IC3 (0x25)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Output IC3";
    }

    if (!outputIC4.begin()) {
        Serial.println("Error: Could not initialize Output IC4 (0x24)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Output IC4";
    }


    // Initialize all outputs to HIGH (OFF state due to inverted logic)
    for (int i = 0; i < 8; i++) {
        outputIC3.digitalWrite(i, HIGH);
        outputIC4.digitalWrite(i, HIGH);
    }

    // Initialize input state arrays
    for (int i = 0; i < 16; i++) {
        inputStates[i] = true;   // Default HIGH (pull-up)
    }

    debugPrintln("I2C and PCF8574 expanders initialized successfully");
}

