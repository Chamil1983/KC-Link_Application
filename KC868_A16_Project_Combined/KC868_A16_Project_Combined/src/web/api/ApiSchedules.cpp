// ApiSchedules.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleSchedules() {
    DynamicJsonDocument doc(4096);
    JsonArray schedulesArray = doc.createNestedArray("schedules");

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        JsonObject schedule = schedulesArray.createNestedObject();
        schedule["id"] = i;
        schedule["enabled"] = schedules[i].enabled;
        schedule["name"] = schedules[i].name;
        schedule["triggerType"] = schedules[i].triggerType;
        schedule["days"] = schedules[i].days;
        schedule["hour"] = schedules[i].hour;
        schedule["minute"] = schedules[i].minute;
        schedule["inputMask"] = schedules[i].inputMask;
        schedule["inputStates"] = schedules[i].inputStates;
        schedule["logic"] = schedules[i].logic;
        schedule["action"] = schedules[i].action;
        schedule["targetType"] = schedules[i].targetType;
        schedule["targetId"] = schedules[i].targetId;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateSchedule() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("schedule")) {
            JsonObject scheduleJson = doc["schedule"];

            // Extract schedule data
            int id = scheduleJson.containsKey("id") ? scheduleJson["id"].as<int>() : -1;

            if (id >= 0 && id < MAX_SCHEDULES) {
                schedules[id].enabled = scheduleJson["enabled"];
                strlcpy(schedules[id].name, scheduleJson["name"] | "Schedule", 32);
                schedules[id].triggerType = scheduleJson["triggerType"] | 0;

                // Time-based fields
                schedules[id].days = scheduleJson["days"] | 0;
                schedules[id].hour = scheduleJson["hour"] | 0;
                schedules[id].minute = scheduleJson["minute"] | 0;

                // Input-based fields
                schedules[id].inputMask = scheduleJson["inputMask"] | 0;
                schedules[id].inputStates = scheduleJson["inputStates"] | 0;
                schedules[id].logic = scheduleJson["logic"] | 0;

                // Common fields
                schedules[id].action = scheduleJson["action"] | 0;
                schedules[id].targetType = scheduleJson["targetType"] | 0;
                schedules[id].targetId = scheduleJson["targetId"] | 0;
                schedules[id].targetIdLow = scheduleJson["targetIdLow"] | 0;  // Added for dual condition support

                // Sensor-based fields (for type 3 and 4)
                if (schedules[id].triggerType == 3 || schedules[id].triggerType == 4) {
                    schedules[id].sensorIndex = scheduleJson["sensorIndex"] | 0;
                    schedules[id].sensorTriggerType = scheduleJson["sensorTriggerType"] | 0;
                    schedules[id].sensorCondition = scheduleJson["sensorCondition"] | 0;
                    schedules[id].sensorThreshold = scheduleJson["sensorThreshold"] | 25.0f;
                }

                // Save schedules to EEPROM
                saveSchedulesToEEPROM();

                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("enabled")) {
            // Handle simple enable/disable
            int id = doc["id"].as<int>();
            bool enabled = doc["enabled"];

            if (id >= 0 && id < MAX_SCHEDULES) {
                schedules[id].enabled = enabled;
                saveSchedulesToEEPROM();
                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("delete")) {
            // Handle delete operation
            int id = doc["id"].as<int>();
            bool deleteSchedule = doc["delete"];

            if (id >= 0 && id < MAX_SCHEDULES && deleteSchedule) {
                // Reset this schedule slot to default values
                schedules[id].enabled = false;
                schedules[id].triggerType = 0;
                schedules[id].days = 0;
                schedules[id].hour = 0;
                schedules[id].minute = 0;
                schedules[id].inputMask = 0;
                schedules[id].inputStates = 0;
                schedules[id].logic = 0;
                schedules[id].action = 0;
                schedules[id].targetType = 0;
                schedules[id].targetId = 0;
                schedules[id].targetIdLow = 0;
                schedules[id].sensorIndex = 0;
                schedules[id].sensorTriggerType = 0;
                schedules[id].sensorCondition = 0;
                schedules[id].sensorThreshold = 25.0f;
                snprintf(schedules[id].name, 32, "Schedule %d", id + 1);

                saveSchedulesToEEPROM();
                response = "{\"status\":\"success\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

void handleEvaluateInputSchedules() {
    checkInputBasedSchedules();

    // Send response
    String response = "{\"status\":\"success\",\"message\":\"Input-based schedules evaluated\"}";
    server.send(200, "application/json", response);
}

