// AnalogTriggerService.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void checkAnalogTriggers() {
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        if (!analogTriggers[i].enabled) continue;

        bool triggerConditionMet = false;

        // Check for HT sensor based analog trigger
        if (analogTriggers[i].analogInput >= 100) { // Special value for HT sensors (e.g., 100 = HT1 temp, 101 = HT1 humidity)
            uint8_t sensorIndex = analogTriggers[i].htSensorIndex;

            if (sensorIndex < 3 && htSensorConfig[sensorIndex].sensorType != SENSOR_TYPE_DIGITAL) {
                float sensorValue;

                // Get the appropriate sensor value based on type (temperature/humidity)
                if (analogTriggers[i].sensorTriggerType == 0) { // Temperature
                    sensorValue = htSensorConfig[sensorIndex].temperature;
                }
                else { // Humidity
                    sensorValue = htSensorConfig[sensorIndex].humidity;
                }

                // Check condition
                switch (analogTriggers[i].sensorCondition) {
                case 0: // Above
                    triggerConditionMet = (sensorValue > analogTriggers[i].sensorThreshold);
                    break;
                case 1: // Below
                    triggerConditionMet = (sensorValue < analogTriggers[i].sensorThreshold);
                    break;
                case 2: // Equal (with tolerance)
                    triggerConditionMet = (abs(sensorValue - analogTriggers[i].sensorThreshold) < 0.5);
                    break;
                }
            }
        }
        // Standard analog input check
        else if (analogTriggers[i].analogInput < 4) {
            int value = analogValues[analogTriggers[i].analogInput];

            // Check condition
            switch (analogTriggers[i].condition) {
            case 0: // Above
                triggerConditionMet = (value > analogTriggers[i].threshold);
                break;
            case 1: // Below
                triggerConditionMet = (value < analogTriggers[i].threshold);
                break;
            case 2: // Equal (with tolerance)
                triggerConditionMet = (abs(value - analogTriggers[i].threshold) < 50);
                break;
            }
        }

        // If this is a combined trigger, check digital inputs too
        if (analogTriggers[i].combinedMode) {
            bool inputConditionMet = false;

            // Calculate current state of all inputs as a single 32-bit value
            uint32_t currentInputState = 0;

            // Add digital inputs (bits 0-15)
            for (int j = 0; j < 16; j++) {
                if (inputStates[j]) {
                    currentInputState |= (1UL << j);
                }
            }

            // Add direct inputs HT1-HT3 (bits 16-18)
            for (int j = 0; j < 3; j++) {
                if (directInputStates[j]) {
                    currentInputState |= (1UL << (16 + j));
                }
            }

            // Check inputs based on logic
            if (analogTriggers[i].logic == 0) { // AND logic
                inputConditionMet = true; // Start with true

                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;

                    // If this bit is part of our input mask, check its state
                    if (analogTriggers[i].inputMask & bitMask) {
                        bool desiredState = (analogTriggers[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;

                        if (currentState != desiredState) {
                            inputConditionMet = false;
                            break; // Break early for AND logic if one condition fails
                        }
                    }
                }
            }
            else { // OR logic
                inputConditionMet = false; // Start with false

                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;

                    // If this bit is part of our input mask, check its state
                    if (analogTriggers[i].inputMask & bitMask) {
                        bool desiredState = (analogTriggers[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;

                        if (currentState == desiredState) {
                            inputConditionMet = true;
                            break; // Break early for OR logic if one condition is true
                        }
                    }
                }
            }

            // For combined mode, both analog and input conditions must be met
            triggerConditionMet = triggerConditionMet && inputConditionMet;
        }

        // If all conditions are met, execute the action
        if (triggerConditionMet) {
            debugPrintln("Analog trigger activated: " + String(analogTriggers[i].name));

            // Perform the trigger action
            if (analogTriggers[i].targetType == 0) {
                // Single output
                uint8_t relay = analogTriggers[i].targetId;
                if (relay < 16) {
                    if (analogTriggers[i].action == 0) {        // OFF
                        outputStates[relay] = false;
                    }
                    else if (analogTriggers[i].action == 1) { // ON
                        outputStates[relay] = true;
                    }
                    else if (analogTriggers[i].action == 2) { // TOGGLE
                        outputStates[relay] = !outputStates[relay];
                    }
                }
            }
            else if (analogTriggers[i].targetType == 1) {
                // Multiple outputs (using bitmask)
                for (int j = 0; j < 16; j++) {
                    if (analogTriggers[i].targetId & (1 << j)) {
                        if (analogTriggers[i].action == 0) {        // OFF
                            outputStates[j] = false;
                        }
                        else if (analogTriggers[i].action == 1) { // ON
                            outputStates[j] = true;
                        }
                        else if (analogTriggers[i].action == 2) { // TOGGLE
                            outputStates[j] = !outputStates[j];
                        }
                    }
                }
            }

            // Update outputs
            writeOutputs();

            // Broadcast update
            broadcastUpdate();
        }
    }
}

