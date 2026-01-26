// InterruptManager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void initInterruptConfigs() {
    for (int i = 0; i < 16; i++) {
        interruptConfigs[i].enabled = false;
        interruptConfigs[i].priority = INPUT_PRIORITY_MEDIUM;  // Default medium priority
        interruptConfigs[i].inputIndex = i;
        interruptConfigs[i].triggerType = INTERRUPT_TRIGGER_CHANGE;  // Default to change (both edges)
        snprintf(interruptConfigs[i].name, 32, "Input %d", i + 1);
    }

    // Load any saved configurations from EEPROM
    loadInterruptConfigs();
}

void setupInputInterrupts() {
    // Disable any existing interrupts first
    disableInputInterrupts();

    // Check if any interrupt is enabled
    bool anyEnabled = false;
    for (int i = 0; i < 16; i++) {
        if (interruptConfigs[i].enabled && interruptConfigs[i].priority != INPUT_PRIORITY_NONE) {
            anyEnabled = true;
            break;
        }
    }

    if (!anyEnabled) {
        debugPrintln("No input interrupts enabled");
        return;
    }

    // Setup task notifications for input interrupts
    debugPrintln("Setting up input interrupts");

    // For I2C expanders, we need to use a different approach since we can't
    // directly attach interrupts to those pins. We'll use a polling approach
    // with priorities determining the order of checking.

    // Reset interrupt flags
    for (int i = 0; i < 16; i++) {
        inputStateChanged[i] = false;
    }

    inputInterruptsEnabled = true;
}

void disableInputInterrupts() {
    inputInterruptsEnabled = false;

    // Reset interrupt flags
    for (int i = 0; i < 16; i++) {
        inputStateChanged[i] = false;
    }

    debugPrintln("Input interrupts disabled");
}

void processInputInterrupts() {
    if (!inputInterruptsEnabled) return;

    // Keep track of previous states for edge detection
    static bool prevInputStates[16] = { false };

    unsigned long currentMillis = millis();

    // Read all digital inputs into a temporary array to avoid multiple I2C transactions
    bool currentInputs[16];
    bool anyChange = false;

    // Read inputs from I2C expanders
    try {
        // Read inputs 1-8
        for (int i = 0; i < 8; i++) {
            bool newState = !inputIC1.digitalRead(i);  // Inverted because of pull-up
            currentInputs[i] = newState;

            // Determine if this input should be processed based on its trigger type
            bool shouldProcess = false;

            if (interruptConfigs[i].enabled) {
                switch (interruptConfigs[i].triggerType) {
                case INTERRUPT_TRIGGER_RISING:
                    // Process on rising edge (LOW to HIGH)
                    shouldProcess = !prevInputStates[i] && newState;
                    break;

                case INTERRUPT_TRIGGER_FALLING:
                    // Process on falling edge (HIGH to LOW)
                    shouldProcess = prevInputStates[i] && !newState;
                    break;

                case INTERRUPT_TRIGGER_CHANGE:
                    // Process on any edge (change)
                    shouldProcess = prevInputStates[i] != newState;
                    break;

                case INTERRUPT_TRIGGER_HIGH_LEVEL:
                    // Process when the input is HIGH
                    shouldProcess = newState;
                    break;

                case INTERRUPT_TRIGGER_LOW_LEVEL:
                    // Process when the input is LOW
                    shouldProcess = !newState;
                    break;
                }

                if (shouldProcess) {
                    anyChange = true;
                    inputStateChanged[i] = true;
                }
            }

            // Update previous state for next iteration
            prevInputStates[i] = newState;
        }

        // Read inputs 9-16
        for (int i = 0; i < 8; i++) {
            bool newState = !inputIC2.digitalRead(i);  // Inverted because of pull-up
            currentInputs[i + 8] = newState;

            // Determine if this input should be processed based on its trigger type
            bool shouldProcess = false;

            if (interruptConfigs[i + 8].enabled) {
                switch (interruptConfigs[i + 8].triggerType) {
                case INTERRUPT_TRIGGER_RISING:
                    // Process on rising edge (LOW to HIGH)
                    shouldProcess = !prevInputStates[i + 8] && newState;
                    break;

                case INTERRUPT_TRIGGER_FALLING:
                    // Process on falling edge (HIGH to LOW)
                    shouldProcess = prevInputStates[i + 8] && !newState;
                    break;

                case INTERRUPT_TRIGGER_CHANGE:
                    // Process on any edge (change)
                    shouldProcess = prevInputStates[i + 8] != newState;
                    break;

                case INTERRUPT_TRIGGER_HIGH_LEVEL:
                    // Process when the input is HIGH
                    shouldProcess = newState;
                    break;

                case INTERRUPT_TRIGGER_LOW_LEVEL:
                    // Process when the input is LOW
                    shouldProcess = !newState;
                    break;
                }

                if (shouldProcess) {
                    anyChange = true;
                    inputStateChanged[i + 8] = true;
                }
            }

            // Update previous state for next iteration
            prevInputStates[i + 8] = newState;
        }
    }
    catch (const std::exception& e) {
        i2cErrorCount++;
        lastErrorMessage = "Error reading from Input ICs during interrupt processing";
        debugPrintln("Error reading inputs for interrupt processing: " + String(e.what()));
        return;
    }

    // If no changes detected, nothing to do
    if (!anyChange) return;

    // Process changes based on priority levels
    // First HIGH priority
    for (int i = 0; i < 16; i++) {
        if (interruptConfigs[i].enabled &&
            interruptConfigs[i].priority == INPUT_PRIORITY_HIGH &&
            inputStateChanged[i]) {

            // Process this input change - this now handles schedule checking
            processInputChange(i, currentInputs[i]);
            inputStateChanged[i] = false;
        }
    }

    // Then MEDIUM priority
    for (int i = 0; i < 16; i++) {
        if (interruptConfigs[i].enabled &&
            interruptConfigs[i].priority == INPUT_PRIORITY_MEDIUM &&
            inputStateChanged[i]) {

            // Process this input change - this now handles schedule checking
            processInputChange(i, currentInputs[i]);
            inputStateChanged[i] = false;
        }
    }

    // Finally LOW priority
    for (int i = 0; i < 16; i++) {
        if (interruptConfigs[i].enabled &&
            interruptConfigs[i].priority == INPUT_PRIORITY_LOW &&
            inputStateChanged[i]) {

            // Process this input change - this now handles schedule checking
            processInputChange(i, currentInputs[i]);
            inputStateChanged[i] = false;
        }
    }

    // Update all input states after processing
    for (int i = 0; i < 16; i++) {
        inputStates[i] = currentInputs[i];
    }

    // Broadcast the update after processing
    broadcastUpdate();
}

void processInputChange(int inputIndex, bool newState) {
    debugPrintln("Input " + String(inputIndex + 1) + " changed to " + String(newState ? "HIGH" : "LOW"));

    // Update corresponding input state
    inputStates[inputIndex] = newState;

    // Check for any schedules that use this input and evaluate if they should be triggered
    checkInputBasedSchedules();

    // Broadcast update to ensure UI reflects current state
    broadcastUpdate();
}

void pollNonInterruptInputs() {
    unsigned long currentMillis = millis();

    // Only poll at the specified interval
    if (currentMillis - lastInputReadTime < INPUT_READ_INTERVAL) {
        return;
    }

    lastInputReadTime = currentMillis;

    // Identify which inputs need polling (priority NONE)
    bool needsPolling[16] = { false };
    bool anyNeedPolling = false;
    bool anyChanged = false;

    for (int i = 0; i < 16; i++) {
        if (interruptConfigs[i].priority == INPUT_PRIORITY_NONE) {
            needsPolling[i] = true;
            anyNeedPolling = true;
        }
    }

    // If no inputs need polling, exit
    if (!anyNeedPolling) return;

    // Poll only the required inputs
    try {
        // Poll inputs 1-8 if needed
        for (int i = 0; i < 8; i++) {
            if (needsPolling[i]) {
                bool newState = !inputIC1.digitalRead(i); // Inverted because of pull-up
                if (newState != inputStates[i]) {
                    inputStates[i] = newState;
                    anyChanged = true;
                    debugPrintln("Polled Input " + String(i + 1) + " changed to " + String(newState ? "HIGH" : "LOW"));

                    // Process this input change directly
                    processInputChange(i, newState);
                }
            }
        }

        // Poll inputs 9-16 if needed
        for (int i = 0; i < 8; i++) {
            if (needsPolling[i + 8]) {
                bool newState = !inputIC2.digitalRead(i); // Inverted because of pull-up
                if (newState != inputStates[i + 8]) {
                    inputStates[i + 8] = newState;
                    anyChanged = true;
                    debugPrintln("Polled Input " + String(i + 9) + " changed to " + String(newState ? "HIGH" : "LOW"));

                    // Process this input change directly
                    processInputChange(i + 8, newState);
                }
            }
        }
    }
    catch (const std::exception& e) {
        i2cErrorCount++;
        lastErrorMessage = "Error polling non-interrupt inputs";
        debugPrintln("Error polling inputs: " + String(e.what()));
    }

    // If any inputs changed, check if we need to update schedules
    // (processInputChange will handle this for individual inputs)
}

