// ApiInterrupts.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleInterrupts() {
    DynamicJsonDocument doc(4096);
    JsonArray interruptsArray = doc.createNestedArray("interrupts");

    for (int i = 0; i < 16; i++) {
        JsonObject interrupt = interruptsArray.createNestedObject();
        interrupt["id"] = i;
        interrupt["enabled"] = interruptConfigs[i].enabled;
        interrupt["name"] = interruptConfigs[i].name;
        interrupt["priority"] = interruptConfigs[i].priority;
        interrupt["inputIndex"] = interruptConfigs[i].inputIndex;
        interrupt["triggerType"] = interruptConfigs[i].triggerType;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateInterrupts() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("interrupt")) {
            JsonObject interruptJson = doc["interrupt"];

            // Extract interrupt data
            int id = interruptJson.containsKey("id") ? interruptJson["id"].as<int>() : -1;

            if (id >= 0 && id < 16) {
                interruptConfigs[id].enabled = interruptJson["enabled"];
                strlcpy(interruptConfigs[id].name, interruptJson["name"] | "Input", 32);
                interruptConfigs[id].priority = interruptJson["priority"] | INPUT_PRIORITY_MEDIUM;
                interruptConfigs[id].triggerType = interruptJson["triggerType"] | INTERRUPT_TRIGGER_CHANGE;

                // Save configurations
                saveInterruptConfigs();

                // Reconfigure interrupts if needed
                if (inputInterruptsEnabled) {
                    setupInputInterrupts();
                }

                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("enabled")) {
            // Handle simple enable/disable
            int id = doc["id"].as<int>();
            bool enabled = doc["enabled"];

            if (id >= 0 && id < 16) {
                interruptConfigs[id].enabled = enabled;
                saveInterruptConfigs();

                // Reconfigure interrupts if needed
                if (inputInterruptsEnabled) {
                    setupInputInterrupts();
                }

                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("action")) {
            String action = doc["action"];

            if (action == "enable_all") {
                // Enable all interrupts
                for (int i = 0; i < 16; i++) {
                    interruptConfigs[i].enabled = true;
                }
                saveInterruptConfigs();
                setupInputInterrupts();
                response = "{\"status\":\"success\",\"message\":\"All interrupts enabled\"}";
            }
            else if (action == "disable_all") {
                // Disable all interrupts
                for (int i = 0; i < 16; i++) {
                    interruptConfigs[i].enabled = false;
                }
                saveInterruptConfigs();
                disableInputInterrupts();
                response = "{\"status\":\"success\",\"message\":\"All interrupts disabled\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

