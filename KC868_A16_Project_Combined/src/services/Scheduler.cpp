// Scheduler.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void checkInputBasedSchedules() {
    // Calculate current state of all inputs as a single 32-bit value
    uint32_t currentInputState = 0;

    // Add digital inputs (bits 0-15)
    for (int i = 0; i < 16; i++) {
        if (inputStates[i]) {
            currentInputState |= (1UL << i);
        }
    }

    // Add direct inputs HT1-HT3 (bits 16-18)
    for (int i = 0; i < 3; i++) {
        if (directInputStates[i]) {
            currentInputState |= (1UL << (16 + i));
        }
    }

    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!schedules[i].enabled) continue;

        // We now care about input-based, combined, and sensor-based schedules
        if (schedules[i].triggerType != 1 && schedules[i].triggerType != 2 && schedules[i].triggerType != 3) continue;

        // Skip if this is an input-based schedule with no inputs
        if ((schedules[i].triggerType == 1 || schedules[i].triggerType == 2) && schedules[i].inputMask == 0) continue;

        bool conditionMet = false;
        bool timeConditionMet = true;  // Default true for input-based/sensor-based, will check for combined

        if (schedules[i].triggerType == 2) { // Combined type
            // Get current time
            DateTime now;
            if (rtcInitialized) {
                now = rtc.now();
            }
            else {
                // Use ESP32 time if RTC not available
                time_t nowTime;
                struct tm timeinfo;
                time(&nowTime);
                localtime_r(&nowTime, &timeinfo);
                now = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            }

            // Calculate day of week bit (1=Sunday, 2=Monday, 4=Tuesday, etc.)
            uint8_t currentDayOfWeek = now.dayOfTheWeek();  // 0=Sunday, 1=Monday, etc.
            uint8_t currentDayBit = (1 << currentDayOfWeek);

            // Check if schedule should run today
            if (!(schedules[i].days & currentDayBit)) {
                timeConditionMet = false;
            }
            else {
                // Check if it's the right hour and minute
                timeConditionMet = (now.hour() == schedules[i].hour && now.minute() == schedules[i].minute);
            }

            if (!timeConditionMet) {
                continue; // Skip to next schedule if time condition not met for combined type
            }
        }

        // Track inputs with TRUE and FALSE matches to handle both conditions
        uint16_t highMatchingInputs = 0;
        uint16_t lowMatchingInputs = 0;

        // Check conditions based on trigger type
        if (schedules[i].triggerType == 3) { // Sensor-based
            // Get the sensor index and ensure it's valid
            uint8_t sensorIndex = schedules[i].sensorIndex;
            if (sensorIndex >= 3) continue; // Invalid sensor index

            // Skip if sensor is configured as digital input
            if (htSensorConfig[sensorIndex].sensorType == SENSOR_TYPE_DIGITAL) continue;

            bool sensorConditionMet = false;

            // Check temperature threshold
            if (schedules[i].sensorTriggerType == 0) { // Temperature
                float currentTemp = htSensorConfig[sensorIndex].temperature;
                float threshold = schedules[i].sensorThreshold;

                switch (schedules[i].sensorCondition) {
                case 0: // Above
                    sensorConditionMet = (currentTemp > threshold);
                    break;
                case 1: // Below
                    sensorConditionMet = (currentTemp < threshold);
                    break;
                case 2: // Equal (with tolerance)
                    sensorConditionMet = (abs(currentTemp - threshold) < 0.5f);
                    break;
                }
            }
            // Check humidity threshold (only for DHT sensors)
            else if (schedules[i].sensorTriggerType == 1 &&
                (htSensorConfig[sensorIndex].sensorType == SENSOR_TYPE_DHT11 ||
                    htSensorConfig[sensorIndex].sensorType == SENSOR_TYPE_DHT22)) {

                float currentHumidity = htSensorConfig[sensorIndex].humidity;
                float threshold = schedules[i].sensorThreshold;

                switch (schedules[i].sensorCondition) {
                case 0: // Above
                    sensorConditionMet = (currentHumidity > threshold);
                    break;
                case 1: // Below
                    sensorConditionMet = (currentHumidity < threshold);
                    break;
                case 2: // Equal (with tolerance)
                    sensorConditionMet = (abs(currentHumidity - threshold) < 2.0f);
                    break;
                }
            }

            conditionMet = sensorConditionMet;

            // For sensor conditions, we'll use the targetId for the true condition
            // and targetIdLow for the false condition (like high and low for digital inputs)
            if (sensorConditionMet) {
                highMatchingInputs = 1; // Just a non-zero value to trigger the action
            }
            else {
                lowMatchingInputs = 1;
            }
        }
        else { // Input-based or combined with input
            // Evaluate input conditions based on logic type
            if (schedules[i].logic == 0) {  // AND logic
                // All conditions must be met
                conditionMet = true;  // Start with true for AND logic

                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;

                    // If this bit is part of our input mask, check its state
                    if (schedules[i].inputMask & bitMask) {
                        bool desiredState = (schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;

                        if (currentState != desiredState) {
                            conditionMet = false;
                            break; // Break early for AND logic if one condition fails
                        }

                        // Track which inputs match which state for relay control
                        if (currentState) {
                            highMatchingInputs |= bitMask;
                        }
                        else {
                            lowMatchingInputs |= bitMask;
                        }
                    }
                }
            }
            else {  // OR logic
                // Any condition can trigger
                conditionMet = false;  // Start with false for OR logic

                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;

                    // If this bit is part of our input mask, check its state
                    if (schedules[i].inputMask & bitMask) {
                        bool desiredState = (schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;

                        // Track which inputs match which state for relay control
                        if (currentState) {
                            highMatchingInputs |= bitMask;
                        }
                        else {
                            lowMatchingInputs |= bitMask;
                        }

                        if (currentState == desiredState) {
                            conditionMet = true;
                            // Don't break early for OR logic - we need to track all matching inputs
                        }
                    }
                }
            }
        }

        // If all conditions are met, execute the schedule
        if (conditionMet && timeConditionMet) {
            debugPrintln("Trigger conditions met for schedule " + String(i) + ": " + String(schedules[i].name));

            // Execute actions for HIGH inputs if we have any inputs in HIGH state
            if (highMatchingInputs && schedules[i].targetId > 0) {
                executeScheduleAction(i, schedules[i].targetId);
            }

            // Execute actions for LOW inputs if we have any inputs in LOW state
            if (lowMatchingInputs && schedules[i].targetIdLow > 0) {
                executeScheduleAction(i, schedules[i].targetIdLow);
            }
        }
    }
}

void executeScheduleAction(int scheduleIndex, uint16_t targetId) {
    if (scheduleIndex < 0 || scheduleIndex >= MAX_SCHEDULES) return;

    debugPrintln("Executing schedule: " + String(schedules[scheduleIndex].name));

    // Perform the scheduled action
    if (schedules[scheduleIndex].targetType == 0) {
        // Single output - targetId should be a relay index
        uint8_t relay = targetId;
        if (relay < 16) {
            debugPrintln("Setting single relay " + String(relay) + " to " +
                (schedules[scheduleIndex].action == 0 ? "OFF" :
                    schedules[scheduleIndex].action == 1 ? "ON" : "TOGGLE"));

            if (schedules[scheduleIndex].action == 0) {        // OFF
                outputStates[relay] = false;
            }
            else if (schedules[scheduleIndex].action == 1) {   // ON
                outputStates[relay] = true;
            }
            else if (schedules[scheduleIndex].action == 2) {   // TOGGLE
                outputStates[relay] = !outputStates[relay];
            }
        }
    }
    else if (schedules[scheduleIndex].targetType == 1) {
        // Multiple outputs (using bitmask)
        debugPrintln("Setting multiple relays with mask: " + String(targetId, BIN));

        for (int j = 0; j < 16; j++) {
            if (targetId & (1 << j)) {
                debugPrintln("Setting relay " + String(j) + " to " +
                    (schedules[scheduleIndex].action == 0 ? "OFF" :
                        schedules[scheduleIndex].action == 1 ? "ON" : "TOGGLE"));

                if (schedules[scheduleIndex].action == 0) {        // OFF
                    outputStates[j] = false;
                }
                else if (schedules[scheduleIndex].action == 1) {   // ON
                    outputStates[j] = true;
                }
                else if (schedules[scheduleIndex].action == 2) {   // TOGGLE
                    outputStates[j] = !outputStates[j];
                }
            }
        }
    }

    // Update outputs
    if (!writeOutputs()) {
        debugPrintln("ERROR: Failed to write outputs when executing schedule");
    }

    // Broadcast update to UI
    broadcastUpdate();
}

void executeScheduleAction(int scheduleIndex) {
    executeScheduleAction(scheduleIndex, schedules[scheduleIndex].targetId);
}

void checkInputBasedSchedules(int changedInputIndex, bool newState) {
    // Calculate the bit mask for this input
    uint16_t changedInputMask = (1UL << changedInputIndex);

    // Calculate current state of all inputs as a single 32-bit value
    uint32_t currentInputState = 0;

    // Add digital inputs (bits 0-15)
    for (int i = 0; i < 16; i++) {
        if (inputStates[i]) {
            currentInputState |= (1UL << i);
        }
    }

    // Add direct inputs HT1-HT3 (bits 16-18)
    for (int i = 0; i < 3; i++) {
        if (directInputStates[i]) {
            currentInputState |= (1UL << (16 + i));
        }
    }

    debugPrintln("Checking input-based schedules for input " + String(changedInputIndex) +
        " (state: " + String(newState ? "HIGH" : "LOW") + ")");

    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!schedules[i].enabled) continue;

        // We only care about input-based or combined schedules
        if (schedules[i].triggerType != 1 && schedules[i].triggerType != 2) continue;

        // Skip if this input isn't part of the schedule's input mask
        if (!(schedules[i].inputMask & changedInputMask)) continue;

        debugPrintln("Evaluating schedule " + String(i) + ": " + String(schedules[i].name));

        bool inputConditionMet = false;

        // For combined schedules, we also need to check the time condition
        bool timeConditionMet = true;  // Default true, will be overridden if it's a combined schedule

        if (schedules[i].triggerType == 2) { // Combined type
            DateTime now;
            if (rtcInitialized) {
                now = rtc.now();
            }
            else {
                // Use ESP32 time if RTC not available
                time_t nowTime;
                struct tm timeinfo;
                time(&nowTime);
                localtime_r(&nowTime, &timeinfo);
                now = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            }

            // Calculate day of week bit (1=Sunday, 2=Monday, 4=Tuesday, etc.)
            uint8_t currentDayOfWeek = now.dayOfTheWeek();  // 0=Sunday, 1=Monday, etc.
            uint8_t currentDayBit = (1 << currentDayOfWeek);

            // Check if schedule should run today
            if (!(schedules[i].days & currentDayBit)) {
                timeConditionMet = false;
            }
            else {
                // Check if it's time to run
                timeConditionMet = (now.hour() == schedules[i].hour && now.minute() == schedules[i].minute);
            }

            if (!timeConditionMet) {
                debugPrintln("Time condition not met for combined schedule " + String(i));
                continue; // Skip to next schedule if time condition not met for combined type
            }
        }

        // Check input conditions
        if (schedules[i].inputMask != 0) {
            // Evaluate input conditions based on logic type
            if (schedules[i].logic == 0) {  // AND logic
              // All conditions must be met
                inputConditionMet = true;  // Start with true for AND logic

                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;

                    // If this bit is part of our input mask, check its state
                    if (schedules[i].inputMask & bitMask) {
                        bool desiredState = (schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;

                        if (currentState != desiredState) {
                            inputConditionMet = false;
                            break; // Break early for AND logic if one condition fails
                        }
                    }
                }
            }
            else {  // OR logic
              // Any condition can trigger
                inputConditionMet = false;  // Start with false for OR logic

                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;

                    // If this bit is part of our input mask, check its state
                    if (schedules[i].inputMask & bitMask) {
                        bool desiredState = (schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;

                        if (currentState == desiredState) {
                            inputConditionMet = true;
                            break; // Break early for OR logic if one condition is met
                        }
                    }
                }
            }
        }

        debugPrintln("Input condition " + String(inputConditionMet ? "met" : "not met") +
            " for schedule " + String(i));

        // For input-based trigger, we need the input condition to be met
        if (inputConditionMet) {
            debugPrintln("Executing schedule: " + String(schedules[i].name));

            // Perform the scheduled action
            if (schedules[i].targetType == 0) {
                // Single output
                uint8_t relay = schedules[i].targetId;
                if (relay < 16) {
                    debugPrintln("Setting single relay " + String(relay) + " to " +
                        (schedules[i].action == 0 ? "OFF" :
                            schedules[i].action == 1 ? "ON" : "TOGGLE"));

                    if (schedules[i].action == 0) {        // OFF
                        outputStates[relay] = false;
                    }
                    else if (schedules[i].action == 1) {   // ON
                        outputStates[relay] = true;
                    }
                    else if (schedules[i].action == 2) {   // TOGGLE
                        outputStates[relay] = !outputStates[relay];
                    }
                }
            }
            else if (schedules[i].targetType == 1) {
                // Multiple outputs (using bitmask)
                debugPrintln("Setting multiple relays with mask: " + String(schedules[i].targetId, BIN));

                for (int j = 0; j < 16; j++) {
                    if (schedules[i].targetId & (1 << j)) {
                        debugPrintln("Setting relay " + String(j) + " to " +
                            (schedules[i].action == 0 ? "OFF" :
                                schedules[i].action == 1 ? "ON" : "TOGGLE"));

                        if (schedules[i].action == 0) {        // OFF
                            outputStates[j] = false;
                        }
                        else if (schedules[i].action == 1) {   // ON
                            outputStates[j] = true;
                        }
                        else if (schedules[i].action == 2) {   // TOGGLE
                            outputStates[j] = !outputStates[j];
                        }
                    }
                }
            }

            // Update outputs
            if (!writeOutputs()) {
                debugPrintln("ERROR: Failed to write outputs when executing schedule");
            }

            // Broadcast update
            broadcastUpdate();
        }
    }
}

void checkSchedules() {
    // Get current time
    DateTime now;
    if (rtcInitialized) {
        now = rtc.now();
    }
    else {
        // Use ESP32 time if RTC not available
        time_t nowTime;
        struct tm timeinfo;
        time(&nowTime);
        localtime_r(&nowTime, &timeinfo);

        now = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    // Calculate day of week bit (1=Sunday, 2=Monday, 4=Tuesday, etc.)
    uint8_t currentDayOfWeek = now.dayOfTheWeek();  // 0=Sunday, 1=Monday, etc.
    uint8_t currentDayBit = (1 << currentDayOfWeek);

    // Print current time info every minute for debugging
    static int lastMinutePrinted = -1;
    if (now.minute() != lastMinutePrinted) {
        lastMinutePrinted = now.minute();
        debugPrintln("Current time: " + String(now.year()) + "-" +
            String(now.month()) + "-" +
            String(now.day()) + " " +
            String(now.hour()) + ":" +
            String(now.minute()) + ":" +
            String(now.second()));
        debugPrintln("Day of week: " + String(currentDayOfWeek) + ", Day bit: " + String(currentDayBit, BIN));
    }

    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!schedules[i].enabled) {
            continue;
        }

        // We only care about time-based schedules here (type 0) or combined schedules (type 2)
        if (schedules[i].triggerType == 0 || schedules[i].triggerType == 2) {
            // Check if schedule should run today
            if (schedules[i].days & currentDayBit) {
                // Check if it's time to run (only check the first 5 seconds of the minute)
                if (now.hour() == schedules[i].hour && now.minute() == schedules[i].minute && now.second() < 5) {
                    debugPrintln("Time trigger met for schedule " + String(i) + ": " + String(schedules[i].name));

                    // For time-only schedules, execute directly
                    if (schedules[i].triggerType == 0) {
                        executeScheduleAction(i);
                    }
                    // For combined schedules, check input conditions too
                    else if (schedules[i].triggerType == 2) {
                        // Call checkInputBasedSchedules which handles combined schedules
                        checkInputBasedSchedules();
                    }
                }
            }
        }
    }
}

void executeSchedule(int scheduleIndex) {
    if (scheduleIndex < 0 || scheduleIndex >= MAX_SCHEDULES || !schedules[scheduleIndex].enabled) {
        return;
    }

    debugPrintln("Executing schedule: " + String(schedules[scheduleIndex].name));

    // Perform the scheduled action
    if (schedules[scheduleIndex].targetType == 0) {
        // Single output
        uint8_t relay = schedules[scheduleIndex].targetId;
        if (relay < 16) {
            debugPrintln("Setting single relay " + String(relay) + " to " +
                (schedules[scheduleIndex].action == 0 ? "OFF" :
                    schedules[scheduleIndex].action == 1 ? "ON" : "TOGGLE"));

            if (schedules[scheduleIndex].action == 0) {        // OFF
                outputStates[relay] = false;
            }
            else if (schedules[scheduleIndex].action == 1) {   // ON
                outputStates[relay] = true;
            }
            else if (schedules[scheduleIndex].action == 2) {   // TOGGLE
                outputStates[relay] = !outputStates[relay];
            }
        }
    }
    else if (schedules[scheduleIndex].targetType == 1) {
        // Multiple outputs (using bitmask)
        debugPrintln("Setting multiple relays with mask: " + String(schedules[scheduleIndex].targetId, BIN));

        for (int j = 0; j < 16; j++) {
            if (schedules[scheduleIndex].targetId & (1 << j)) {
                debugPrintln("Setting relay " + String(j) + " to " +
                    (schedules[scheduleIndex].action == 0 ? "OFF" :
                        schedules[scheduleIndex].action == 1 ? "ON" : "TOGGLE"));

                if (schedules[scheduleIndex].action == 0) {        // OFF
                    outputStates[j] = false;
                }
                else if (schedules[scheduleIndex].action == 1) {   // ON
                    outputStates[j] = true;
                }
                else if (schedules[scheduleIndex].action == 2) {   // TOGGLE
                    outputStates[j] = !outputStates[j];
                }
            }
        }
    }

    // Update outputs
    if (!writeOutputs()) {
        debugPrintln("ERROR: Failed to write outputs when executing schedule");
    }

    // Broadcast update
    broadcastUpdate();
}

