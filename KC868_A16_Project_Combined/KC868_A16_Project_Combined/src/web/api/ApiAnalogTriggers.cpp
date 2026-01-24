// ApiAnalogTriggers.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleAnalogTriggers() {
    DynamicJsonDocument doc(4096);
    JsonArray triggersArray = doc.createNestedArray("triggers");

    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        JsonObject trigger = triggersArray.createNestedObject();
        trigger["id"] = i;
        trigger["enabled"] = analogTriggers[i].enabled;
        trigger["name"] = analogTriggers[i].name;
        trigger["analogInput"] = analogTriggers[i].analogInput;
        trigger["threshold"] = analogTriggers[i].threshold;
        trigger["condition"] = analogTriggers[i].condition;
        trigger["action"] = analogTriggers[i].action;
        trigger["targetType"] = analogTriggers[i].targetType;
        trigger["targetId"] = analogTriggers[i].targetId;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateAnalogTriggers() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("trigger")) {
            JsonObject triggerJson = doc["trigger"];

            // Extract trigger data
            int id = triggerJson.containsKey("id") ? triggerJson["id"].as<int>() : -1;

            if (id >= 0 && id < MAX_ANALOG_TRIGGERS) {
                analogTriggers[id].enabled = triggerJson["enabled"];
                strlcpy(analogTriggers[id].name, triggerJson["name"] | "Trigger", 32);
                analogTriggers[id].analogInput = triggerJson["analogInput"];
                analogTriggers[id].threshold = triggerJson["threshold"];
                analogTriggers[id].condition = triggerJson["condition"];
                analogTriggers[id].action = triggerJson["action"];
                analogTriggers[id].targetType = triggerJson["targetType"];
                analogTriggers[id].targetId = triggerJson["targetId"];

                // New combined trigger fields
                analogTriggers[id].combinedMode = triggerJson["combinedMode"] | false;
                if (analogTriggers[id].combinedMode) {
                    analogTriggers[id].inputMask = triggerJson["inputMask"] | 0;
                    analogTriggers[id].inputStates = triggerJson["inputStates"] | 0;
                    analogTriggers[id].logic = triggerJson["logic"] | 0;
                }

                // HT sensor fields for temperature/humidity triggering
                if (triggerJson.containsKey("htSensorIndex")) {
                    analogTriggers[id].htSensorIndex = triggerJson["htSensorIndex"];
                    analogTriggers[id].sensorTriggerType = triggerJson["sensorTriggerType"];
                    analogTriggers[id].sensorCondition = triggerJson["sensorCondition"];
                    analogTriggers[id].sensorThreshold = triggerJson["sensorThreshold"];
                }

                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("enabled")) {
            // Handle simple enable/disable
            int id = doc["id"].as<int>();
            bool enabled = doc["enabled"];

            if (id >= 0 && id < MAX_ANALOG_TRIGGERS) {
                analogTriggers[id].enabled = enabled;
                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("delete")) {
            // Handle delete operation
            int id = doc["id"].as<int>();
            bool deleteTrigger = doc["delete"];

            if (id >= 0 && id < MAX_ANALOG_TRIGGERS && deleteTrigger) {
                // Reset this trigger slot to default values
                analogTriggers[id].enabled = false;
                analogTriggers[id].analogInput = 0;
                analogTriggers[id].threshold = 2048;  // Middle value
                analogTriggers[id].condition = 0;
                analogTriggers[id].action = 0;
                analogTriggers[id].targetType = 0;
                analogTriggers[id].targetId = 0;
                // Reset combined mode fields
                analogTriggers[id].combinedMode = false;
                analogTriggers[id].inputMask = 0;
                analogTriggers[id].inputStates = 0;
                analogTriggers[id].logic = 0;
                // Reset HT sensor fields
                analogTriggers[id].htSensorIndex = 0;
                analogTriggers[id].sensorTriggerType = 0;
                analogTriggers[id].sensorCondition = 0;
                analogTriggers[id].sensorThreshold = 25.0;

                snprintf(analogTriggers[id].name, 32, "Trigger %d", id + 1);

                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

