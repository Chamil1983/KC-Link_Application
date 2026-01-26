// ApiRelay.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleRelayControl() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        debugPrintln("Relay control request body: " + body);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("relay") && doc.containsKey("state")) {
                int relay = doc["relay"];
                bool state = doc["state"];

                debugPrintln("Request to set relay " + String(relay) + " to " + String(state ? "ON" : "OFF"));

                if (relay >= 0 && relay < 16) {
                    outputStates[relay] = state;
                    if (writeOutputs()) {
                        debugPrintln("Relay control successful");
                        response = "{\"status\":\"success\",\"relay\":" + String(relay) +
                            ",\"state\":" + String(state ? "true" : "false") + "}";

                        // Broadcast update
                        broadcastUpdate();
                    }
                    else {
                        debugPrintln("Failed to write to relay");
                        response = "{\"status\":\"error\",\"message\":\"Failed to write to relay\"}";
                    }
                }
                else if (relay == 99) {  // Special case for all relays
                    debugPrintln("Setting all relays to " + String(state ? "ON" : "OFF"));

                    for (int i = 0; i < 16; i++) {
                        outputStates[i] = state;
                    }
                    if (writeOutputs()) {
                        response = "{\"status\":\"success\",\"relay\":\"all\",\"state\":" +
                            String(state ? "true" : "false") + "}";

                        // Broadcast update
                        broadcastUpdate();
                    }
                    else {
                        debugPrintln("Failed to write to relays");
                        response = "{\"status\":\"error\",\"message\":\"Failed to write to relays\"}";
                    }
                }
                else {
                    debugPrintln("Invalid relay number: " + String(relay));
                }
            }
            else {
                debugPrintln("Missing relay or state in request");
            }
        }
        else {
            debugPrintln("Invalid JSON in request: " + String(error.c_str()));
        }
    }
    else {
        debugPrintln("No plain body in request");
    }

    server.send(200, "application/json", response);
}

