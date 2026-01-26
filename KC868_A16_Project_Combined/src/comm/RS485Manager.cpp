// RS485Manager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void initRS485() {
    // Configure with current RS485 settings
    int configParity = SERIAL_8N1; // Default

    // Set correct parity
    if (rs485DataBits == 8) {
        if (rs485Parity == 0) {
            if (rs485StopBits == 1) configParity = SERIAL_8N1;
            else configParity = SERIAL_8N2;
        }
        else if (rs485Parity == 1) {
            if (rs485StopBits == 1) configParity = SERIAL_8O1;
            else configParity = SERIAL_8O2;
        }
        else if (rs485Parity == 2) {
            if (rs485StopBits == 1) configParity = SERIAL_8E1;
            else configParity = SERIAL_8E2;
        }
    }
    else if (rs485DataBits == 7) {
        if (rs485Parity == 0) {
            if (rs485StopBits == 1) configParity = SERIAL_7N1;
            else configParity = SERIAL_7N2;
        }
        else if (rs485Parity == 1) {
            if (rs485StopBits == 1) configParity = SERIAL_7O1;
            else configParity = SERIAL_7O2;
        }
        else if (rs485Parity == 2) {
            if (rs485StopBits == 1) configParity = SERIAL_7E1;
            else configParity = SERIAL_7E2;
        }
    }

    rs485.begin(rs485BaudRate, configParity, RS485_RX_PIN, RS485_TX_PIN);
    debugPrintln("RS485 initialized with baud rate: " + String(rs485BaudRate));
}

void processRS485Commands() {
    if (rs485.available()) {
        String command = rs485.readStringUntil('\n');
        command.trim();

        String response = processCommand(command);
        rs485.println(response);
    }
}

String processCommand(String command) {
    if (command.startsWith("RELAY ")) {
        // Relay control command
        command = command.substring(6);

        if (command.startsWith("STATUS")) {
            // Return relay status
            String response = "RELAY STATUS:\n";
            for (int i = 0; i < 16; i++) {
                response += String(i + 1) + " (Relay " + String(i + 1) + "): ";
                response += outputStates[i] ? "ON" : "OFF";
                response += "\n";
            }
            return response;
        }
        else if (command.startsWith("ALL ON")) {
            // Turn all relays on
            for (int i = 0; i < 16; i++) {
                outputStates[i] = true;
            }
            if (writeOutputs()) {
                broadcastUpdate();
                return "All relays turned ON";
            }
            else {
                return "ERROR: Failed to turn all relays ON";
            }
        }
        else if (command.startsWith("ALL OFF")) {
            // Turn all relays off
            for (int i = 0; i < 16; i++) {
                outputStates[i] = false;
            }
            if (writeOutputs()) {
                broadcastUpdate();
                return "All relays turned OFF";
            }
            else {
                return "ERROR: Failed to turn all relays OFF";
            }
        }
        else {
            // Individual relay control: RELAY <number> <ON|OFF>
            int spacePos = command.indexOf(' ');
            if (spacePos > 0) {
                int relayNum = command.substring(0, spacePos).toInt();
                String action = command.substring(spacePos + 1);

                if (relayNum >= 1 && relayNum <= 16) {
                    int index = relayNum - 1;
                    if (action == "ON") {
                        outputStates[index] = true;
                        if (writeOutputs()) {
                            broadcastUpdate();
                            return "Relay " + String(relayNum) + " turned ON";
                        }
                        else {
                            return "ERROR: Failed to turn relay ON";
                        }
                    }
                    else if (action == "OFF") {
                        outputStates[index] = false;
                        if (writeOutputs()) {
                            broadcastUpdate();
                            return "Relay " + String(relayNum) + " turned OFF";
                        }
                        else {
                            return "ERROR: Failed to turn relay OFF";
                        }
                    }
                }
            }
        }

        return "ERROR: Invalid relay command";
    }
    else if (command.startsWith("INPUT STATUS")) {
        // Return input status
        String response = "INPUT STATUS:\n";
        for (int i = 0; i < 16; i++) {
            response += String(i + 1) + " (Input " + String(i + 1) + "): ";
            response += inputStates[i] ? "HIGH" : "LOW";
            response += "\n";
        }
        return response;
    }
    else if (command.startsWith("INTERRUPT STATUS")) {
        // Return interrupt configuration status
        String response = "INTERRUPT CONFIGURATIONS:\n";
        for (int i = 0; i < 16; i++) {
            response += String(i + 1) + " (Input " + String(i + 1) + "): ";
            response += interruptConfigs[i].enabled ? "Enabled" : "Disabled";
            response += ", Priority: ";

            switch (interruptConfigs[i].priority) {
            case INPUT_PRIORITY_HIGH: response += "High"; break;
            case INPUT_PRIORITY_MEDIUM: response += "Medium"; break;
            case INPUT_PRIORITY_LOW: response += "Low"; break;
            case INPUT_PRIORITY_NONE: response += "None (Polling)"; break;
            default: response += "Unknown";
            }

            response += ", Trigger: ";
            switch (interruptConfigs[i].triggerType) {
            case INTERRUPT_TRIGGER_RISING: response += "Rising Edge"; break;
            case INTERRUPT_TRIGGER_FALLING: response += "Falling Edge"; break;
            case INTERRUPT_TRIGGER_CHANGE: response += "Change (Any Edge)"; break;
            case INTERRUPT_TRIGGER_HIGH_LEVEL: response += "High Level"; break;
            case INTERRUPT_TRIGGER_LOW_LEVEL: response += "Low Level"; break;
            default: response += "Unknown";
            }

            response += "\n";
        }
        response += "\nInterrupt System: " + String(inputInterruptsEnabled ? "Active" : "Inactive");
        return response;
    }
    else if (command.startsWith("ANALOG STATUS")) {
        // Return analog input status with voltage values
        String response = "ANALOG STATUS:\n";
        for (int i = 0; i < 4; i++) {
            response += String(i + 1) + " (Analog " + String(i + 1) + "): ";
            response += String(analogValues[i]) + " (Raw), ";
            response += String(analogVoltages[i], 2) + "V, ";
            response += String(calculatePercentage(analogVoltages[i])) + "% (of 5V scale)";
            response += "\n";
        }
        return response;
    }
    else if (command.startsWith("COMM STATUS")) {
        // Return communication status
        String response = "COMMUNICATION STATUS:\n";
        response += "Active Protocol: " + currentCommunicationProtocol + "\n";
        response += "WiFi Connected: " + String(wifiConnected ? "Yes" : "No") + "\n";
        response += "Ethernet Connected: " + String(ethConnected ? "Yes" : "No") + "\n";
        response += "RS485 Available: Yes\n";
        response += "USB Available: Yes\n";

        // Add protocol-specific details
        if (currentCommunicationProtocol == "wifi") {
            response += "\nWIFI DETAILS:\n";
            response += "SSID: " + wifiSSID + "\n";
            response += "Security: " + wifiSecurity + "\n";
            response += "Channel: " + String(wifiChannel) + "\n";
            if (wifiConnected) {
                response += "IP: " + WiFi.localIP().toString() + "\n";
                response += "Signal: " + String(WiFi.RSSI()) + " dBm\n";
            }
        }
        else if (currentCommunicationProtocol == "ethernet") {
            response += "\nETHERNET DETAILS:\n";
            if (ethConnected) {
                response += "MAC: " + ETH.macAddress() + "\n";
                response += "IP: " + ETH.localIP().toString() + "\n";
                response += "Speed: " + String(ETH.linkSpeed()) + " Mbps\n";
                response += "Duplex: " + String(ETH.fullDuplex() ? "Full" : "Half") + "\n";
            }
            else {
                response += "Status: Disconnected\n";
            }
        }
        else if (currentCommunicationProtocol == "rs485") {
            response += "\nRS485 DETAILS:\n";
            response += "Baud Rate: " + String(rs485BaudRate) + "\n";
            response += "Protocol: " + rs485Protocol + "\n";
            response += "Mode: " + rs485Mode + "\n";
            response += "Address: " + String(rs485DeviceAddress) + "\n";
        }
        else if (currentCommunicationProtocol == "usb") {
            response += "\nUSB DETAILS:\n";
            response += "COM Port: " + String(usbComPort) + "\n";
            response += "Baud Rate: " + String(usbBaudRate) + "\n";
            response += "Data Bits: " + String(usbDataBits) + "\n";
            response += "Parity: " + String(usbParity == 0 ? "None" : usbParity == 1 ? "Odd" : "Even") + "\n";
            response += "Stop Bits: " + String(usbStopBits) + "\n";
        }

        return response;
    }
    else if (command.startsWith("SCAN I2C")) {
        // Scan I2C bus
        String response = "I2C DEVICES:\n";
        int deviceCount = 0;

        for (uint8_t address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            uint8_t error = Wire.endTransmission();

            if (error == 0) {
                deviceCount++;
                response += "0x" + String(address, HEX) + " - ";

                // Identify known devices
                if (address == PCF8574_INPUTS_1_8) {
                    response += "PCF8574 Inputs 1-8";
                }
                else if (address == PCF8574_INPUTS_9_16) {
                    response += "PCF8574 Inputs 9-16";
                }
                else if (address == PCF8574_OUTPUTS_1_8) {
                    response += "PCF8574 Outputs 1-8";
                }
                else if (address == PCF8574_OUTPUTS_9_16) {
                    response += "PCF8574 Outputs 9-16";
                }
                else if (address == 0x68) {
                    response += "DS3231 RTC";
                }
                else {
                    response += "Unknown device";
                }

                response += "\n";
            }
        }

        response += "Found " + String(deviceCount) + " device(s)\n";
        return response;
    }
    else if (command == "STATUS") {
        // Return system status
        String response = "KC868-A16 System Status\n";
        response += "---------------------\n";
        response += "Device: " + deviceName + "\n";
        response += "Firmware: " + firmwareVersion + "\n";
        response += "Uptime: " + String(millis() / 1000) + " seconds\n";

        if (wifiConnected) {
            response += "WiFi: Connected, IP: " + WiFi.localIP().toString() + "\n";
        }
        else {
            response += "WiFi: Not connected\n";
        }

        if (ethConnected) {
            response += "Ethernet: Connected, IP: " + ETH.localIP().toString() + "\n";
        }
        else {
            response += "Ethernet: Not connected\n";
        }

        response += "Active Protocol: " + currentCommunicationProtocol + "\n";
        response += "I2C errors: " + String(i2cErrorCount) + "\n";
        response += "RTC available: " + String(rtcInitialized ? "Yes" : "No") + "\n";
        response += "Current time: " + getTimeString() + "\n";
        response += "Free heap: " + String(ESP.getFreeHeap()) + " bytes\n";

        // Add analog inputs status with voltage values
        response += "\nAnalog Inputs (0-5V range):\n";
        for (int i = 0; i < 4; i++) {
            response += "A" + String(i + 1) + ": ";
            response += String(analogValues[i]) + " (Raw), ";
            response += String(analogVoltages[i], 2) + "V, ";
            response += String(calculatePercentage(analogVoltages[i])) + "%\n";
        }

        // Add interrupt status summary
        response += "\nInterrupt System: " + String(inputInterruptsEnabled ? "Active" : "Inactive") + "\n";
        int highCount = 0, medCount = 0, lowCount = 0, noneCount = 0;
        for (int i = 0; i < 16; i++) {
            if (!interruptConfigs[i].enabled) continue;

            switch (interruptConfigs[i].priority) {
            case INPUT_PRIORITY_HIGH: highCount++; break;
            case INPUT_PRIORITY_MEDIUM: medCount++; break;
            case INPUT_PRIORITY_LOW: lowCount++; break;
            case INPUT_PRIORITY_NONE: noneCount++; break;
            }
        }
        response += "Configured interrupts: High=" + String(highCount) +
            ", Med=" + String(medCount) +
            ", Low=" + String(lowCount) +
            ", Polling=" + String(noneCount) + "\n";

        return response;
    }
    else if (command == "HELP") {
        // Return help information
        String response = "KC868-A16 Controller Command Help\n";
        response += "---------------------\n";
        response += "RELAY STATUS - Show all relay states\n";
        response += "RELAY ALL ON - Turn all relays on\n";
        response += "RELAY ALL OFF - Turn all relays off\n";
        response += "RELAY <num> ON - Turn relay on (1-16)\n";
        response += "RELAY <num> OFF - Turn relay off (1-16)\n";
        response += "INPUT STATUS - Show all input states\n";
        response += "INTERRUPT STATUS - Show interrupt configurations\n";
        response += "ANALOG STATUS - Show all analog input values\n";
        response += "COMM STATUS - Show communication interface status\n";
        response += "SCAN I2C - Scan for I2C devices\n";
        response += "STATUS - Show system status\n";
        response += "DEBUG ON - Enable debug mode\n";
        response += "DEBUG OFF - Disable debug mode\n";
        response += "SET TIME <yyyy-mm-dd hh:mm:ss> - Set system time\n";
        response += "INTERRUPT ENABLE <num> - Enable interrupt for input (1-16)\n";
        response += "INTERRUPT DISABLE <num> - Disable interrupt for input (1-16)\n";
        response += "INTERRUPT PRIORITY <num> <priority> - Set input interrupt priority (HIGH/MEDIUM/LOW/NONE)\n";
        response += "INTERRUPT TRIGGER <num> <type> - Set input trigger type (RISING/FALLING/CHANGE/HIGH_LEVEL/LOW_LEVEL)\n";
        response += "REBOOT - Restart the system\n";
        response += "VERSION - Show firmware version\n";

        return response;
    }
    else if (command.startsWith("SET TIME ")) {
        // Set time command
        String timeStr = command.substring(9);

        // Format should be "yyyy-mm-dd hh:mm:ss"
        if (timeStr.length() == 19) {
            int year = timeStr.substring(0, 4).toInt();
            int month = timeStr.substring(5, 7).toInt();
            int day = timeStr.substring(8, 10).toInt();
            int hour = timeStr.substring(11, 13).toInt();
            int minute = timeStr.substring(14, 16).toInt();
            int second = timeStr.substring(17, 19).toInt();

            if (year >= 2023 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
                hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {

                syncTimeFromClient(year, month, day, hour, minute, second);
                return "Time set successfully: " + getTimeString();
            }
        }

        return "ERROR: Invalid time format. Use SET TIME yyyy-mm-dd hh:mm:ss";
    }
    else if (command.startsWith("INTERRUPT ENABLE ")) {
        int inputNum = command.substring(17).toInt();
        if (inputNum >= 1 && inputNum <= 16) {
            int index = inputNum - 1;
            interruptConfigs[index].enabled = true;
            saveInterruptConfigs();

            if (inputInterruptsEnabled) {
                setupInputInterrupts(); // Reconfigure interrupts
            }
            else {
                inputInterruptsEnabled = true; // Enable the interrupt system
                setupInputInterrupts();
            }

            return "Interrupt enabled for input " + String(inputNum);
        }
        return "ERROR: Invalid input number. Must be between 1-16.";
    }
    else if (command.startsWith("INTERRUPT DISABLE ")) {
        int inputNum = command.substring(18).toInt();
        if (inputNum >= 1 && inputNum <= 16) {
            int index = inputNum - 1;
            interruptConfigs[index].enabled = false;
            saveInterruptConfigs();

            // Check if any interrupts are still enabled
            bool anyEnabled = false;
            for (int i = 0; i < 16; i++) {
                if (interruptConfigs[i].enabled) {
                    anyEnabled = true;
                    break;
                }
            }

            if (anyEnabled && inputInterruptsEnabled) {
                setupInputInterrupts(); // Reconfigure interrupts
            }
            else if (!anyEnabled) {
                disableInputInterrupts(); // Disable the entire interrupt system
            }

            return "Interrupt disabled for input " + String(inputNum);
        }
        return "ERROR: Invalid input number. Must be between 1-16.";
    }
    else if (command.startsWith("INTERRUPT PRIORITY ")) {
        String params = command.substring(19);
        int spacePos = params.indexOf(' ');

        if (spacePos > 0) {
            int inputNum = params.substring(0, spacePos).toInt();
            String priorityStr = params.substring(spacePos + 1);
            priorityStr.toUpperCase();

            uint8_t priority = INPUT_PRIORITY_MEDIUM; // Default medium
            if (priorityStr == "HIGH") {
                priority = INPUT_PRIORITY_HIGH;
            }
            else if (priorityStr == "MEDIUM") {
                priority = INPUT_PRIORITY_MEDIUM;
            }
            else if (priorityStr == "LOW") {
                priority = INPUT_PRIORITY_LOW;
            }
            else if (priorityStr == "NONE") {
                priority = INPUT_PRIORITY_NONE;
            }
            else {
                return "ERROR: Invalid priority. Use HIGH, MEDIUM, LOW, or NONE.";
            }

            if (inputNum >= 1 && inputNum <= 16) {
                int index = inputNum - 1;
                interruptConfigs[index].priority = priority;
                saveInterruptConfigs();

                if (inputInterruptsEnabled) {
                    setupInputInterrupts(); // Reconfigure interrupts
                }

                return "Priority for input " + String(inputNum) + " set to " + priorityStr;
            }
            return "ERROR: Invalid input number. Must be between 1-16.";
        }
        return "ERROR: Invalid format. Use INTERRUPT PRIORITY <input_num> <HIGH|MEDIUM|LOW|NONE>";
    }
    else if (command.startsWith("INTERRUPT TRIGGER ")) {
        String params = command.substring(18);
        int spacePos = params.indexOf(' ');

        if (spacePos > 0) {
            int inputNum = params.substring(0, spacePos).toInt();
            String triggerStr = params.substring(spacePos + 1);
            triggerStr.toUpperCase();

            uint8_t triggerType = INTERRUPT_TRIGGER_CHANGE; // Default to change
            if (triggerStr == "RISING" || triggerStr == "RISE") {
                triggerType = INTERRUPT_TRIGGER_RISING;
            }
            else if (triggerStr == "FALLING" || triggerStr == "FALL") {
                triggerType = INTERRUPT_TRIGGER_FALLING;
            }
            else if (triggerStr == "CHANGE" || triggerStr == "BOTH") {
                triggerType = INTERRUPT_TRIGGER_CHANGE;
            }
            else if (triggerStr == "HIGH" || triggerStr == "HIGH_LEVEL") {
                triggerType = INTERRUPT_TRIGGER_HIGH_LEVEL;
            }
            else if (triggerStr == "LOW" || triggerStr == "LOW_LEVEL") {
                triggerType = INTERRUPT_TRIGGER_LOW_LEVEL;
            }
            else {
                return "ERROR: Invalid trigger type. Use RISING, FALLING, CHANGE, HIGH_LEVEL, or LOW_LEVEL.";
            }

            if (inputNum >= 1 && inputNum <= 16) {
                int index = inputNum - 1;
                interruptConfigs[index].triggerType = triggerType;
                saveInterruptConfigs();

                if (inputInterruptsEnabled) {
                    setupInputInterrupts(); // Reconfigure interrupts
                }

                return "Trigger type for input " + String(inputNum) + " set to " + triggerStr;
            }
            return "ERROR: Invalid input number. Must be between 1-16.";
        }
        return "ERROR: Invalid format. Use INTERRUPT TRIGGER <input_num> <RISING|FALLING|CHANGE|HIGH_LEVEL|LOW_LEVEL>";
    }
    else if (command == "DEBUG ON") {
        debugMode = true;
        return "Debug mode enabled";
    }
    else if (command == "DEBUG OFF") {
        debugMode = false;
        return "Debug mode disabled";
    }
    else if (command == "VERSION") {
        return "KC868-A16 Controller firmware version " + firmwareVersion;
    }
    else if (command == "REBOOT") {
        String response = "Rebooting system...";
        delay(100);
        ESP.restart();
        return response;
    }

    return "ERROR: Unknown command. Type HELP for commands.";
}

