// DigitalIO.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void printIOStates() {
    Serial.println("--- Current I/O States ---");

    // Print input states
    Serial.println("Input States (1=HIGH/OFF, 0=LOW/ON):");
    Serial.print("Inputs 1-8:  ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(inputStates[i] ? "1" : "0");
    }
    Serial.println();

    Serial.print("Inputs 9-16: ");
    for (int i = 15; i >= 8; i--) {
        Serial.print(inputStates[i] ? "1" : "0");
    }
    Serial.println();

    // Print output states
    Serial.println("Output States (1=HIGH/ON, 0=LOW/OFF):");
    Serial.print("Outputs 1-8:  ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(outputStates[i] ? "1" : "0");
    }
    Serial.println();

    Serial.print("Outputs 9-16: ");
    for (int i = 15; i >= 8; i--) {
        Serial.print(outputStates[i] ? "1" : "0");
    }
    Serial.println();

    // Print analog inputs with voltage values
    Serial.println("Analog Inputs (0-5V range):");
    for (int i = 0; i < 4; i++) {
        Serial.print("A");
        Serial.print(i + 1);
        Serial.print(": Raw=");
        Serial.print(analogValues[i]);
        Serial.print(", Voltage=");
        Serial.print(analogVoltages[i], 2); // Display with 2 decimal places
        Serial.print("V, ");
        Serial.print(calculatePercentage(analogVoltages[i]));
        Serial.println("%");
    }

    Serial.println("----------------------------");
}

bool readInputs() {
    bool anyChanged = false;
    bool success = true;

    // Store previous input states to check for changes
    bool prevInputStates[16];
    bool prevDirectInputStates[3];

    // Copy current states to previous states
    for (int i = 0; i < 16; i++) {
        prevInputStates[i] = inputStates[i];
    }

    for (int i = 0; i < 3; i++) {
        prevDirectInputStates[i] = directInputStates[i];
    }

    // Read from PCF8574 input expanders using the PCF8574 library
    // Inputs 1-8 (IC1)
    for (int i = 0; i < 8; i++) {
        bool newState = false;
        try {
            newState = inputIC1.digitalRead(i);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Error reading from Input IC1";
            success = false;
            debugPrintln("Error reading from Input IC1: " + String(e.what()));
            continue;
        }

        // Invert because of the pull-up configuration (LOW = active/true)
        newState = !newState;

        if (inputStates[i] != newState) {
            inputStates[i] = newState;
            anyChanged = true;
            debugPrintln("Input " + String(i + 1) + " changed to " + String(newState ? "HIGH" : "LOW"));

            // Process this specific input change
            if (inputInterruptsEnabled && interruptConfigs[i].enabled) {
                processInputChange(i, newState);
            }
        }
    }

    // Inputs 9-16 (IC2)
    for (int i = 0; i < 8; i++) {
        bool newState = false;
        try {
            newState = inputIC2.digitalRead(i);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Error reading from Input IC2";
            success = false;
            debugPrintln("Error reading from Input IC2: " + String(e.what()));
            continue;
        }

        // Invert because of the pull-up configuration (LOW = active/true)
        newState = !newState;

        if (inputStates[i + 8] != newState) {
            inputStates[i + 8] = newState;
            anyChanged = true;
            debugPrintln("Input " + String(i + 9) + " changed to " + String(newState ? "HIGH" : "LOW"));

            // Process this specific input change
            if (inputInterruptsEnabled && interruptConfigs[i + 8].enabled) {
                processInputChange(i + 8, newState);
            }
        }
    }

    // Read direct GPIO inputs with inversion (LOW = active/true)
    bool ht1 = !digitalRead(HT1_PIN);
    bool ht2 = !digitalRead(HT2_PIN);
    bool ht3 = !digitalRead(HT3_PIN);

    if (directInputStates[0] != ht1) {
        directInputStates[0] = ht1;
        anyChanged = true;
        debugPrintln("HT1 changed to " + String(ht1 ? "HIGH" : "LOW"));
    }

    if (directInputStates[1] != ht2) {
        directInputStates[1] = ht2;
        anyChanged = true;
        debugPrintln("HT2 changed to " + String(ht2 ? "HIGH" : "LOW"));
    }

    if (directInputStates[2] != ht3) {
        directInputStates[2] = ht3;
        anyChanged = true;
        debugPrintln("HT3 changed to " + String(ht3 ? "HIGH" : "LOW"));
    }

    // If any changes detected but not already processed by interrupt handlers,
    // check if there are any input-based schedules to run
    if (anyChanged && !inputInterruptsEnabled) {
        checkInputBasedSchedules();
    }

    // If any changes detected, print the current I/O states for debugging
    if (anyChanged && debugMode) {
        printIOStates();
    }

    return anyChanged;
}

bool writeOutputs() {
    bool success = true;

    // Set outputs 1-8 (IC4)
    for (int i = 0; i < 8; i++) {
        try {
            // Write HIGH when output state is false (relays are active LOW)
            outputIC4.digitalWrite(i, outputStates[i] ? LOW : HIGH);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Failed to write to Output IC4";
            success = false;
            debugPrintln("Error writing to Output IC4: " + String(e.what()));
        }
    }

    // Set outputs 9-16 (IC3)
    for (int i = 0; i < 8; i++) {
        try {
            // Write HIGH when output state is false (relays are active LOW)
            outputIC3.digitalWrite(i, outputStates[i + 8] ? LOW : HIGH);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Failed to write to Output IC3";
            success = false;
            debugPrintln("Error writing to Output IC3: " + String(e.what()));
        }
    }

    if (success) {
        debugPrintln("Successfully updated all relays");
        if (debugMode) {
            printIOStates();
        }
    }
    else {
        debugPrintln("ERROR: Failed to write to some output expanders");
        // Try to recover I2C bus
        Wire.flush();
        delay(50);
    }

    return success;
}

