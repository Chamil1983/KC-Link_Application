// ApiSystem.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleReboot() {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Rebooting device...\"}");
    delay(500);
    ESP.restart();
}

