// ApiDebug.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleDebug() {
    DynamicJsonDocument doc(4096);

    doc["i2c_errors"] = i2cErrorCount;
    doc["last_error"] = lastErrorMessage;
    doc["uptime_ms"] = millis();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["firmware_version"] = firmwareVersion;

    // Check internet connectivity
    time_t now;
    time(&now);
    doc["internet_connected"] = (now > 1600000000);  // Reasonable timestamp indicates NTP sync worked

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleDebugCommand() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("command")) {
            String command = doc["command"].as<String>();
            String commandResponse = processCommand(command);

            DynamicJsonDocument responseDoc(1024);
            responseDoc["status"] = "success";
            responseDoc["response"] = commandResponse;

            String jsonResponse;
            serializeJson(responseDoc, jsonResponse);

            response = jsonResponse;
        }
    }

    server.send(200, "application/json", response);
}

