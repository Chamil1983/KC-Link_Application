// Logger.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void debugPrintln(String message) {
    if (debugMode) {
        Serial.println(message);
    }
}

