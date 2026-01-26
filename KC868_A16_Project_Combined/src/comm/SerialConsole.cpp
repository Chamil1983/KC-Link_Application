// SerialConsole.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        String response = processCommand(command);
        Serial.println(response);
    }
}

