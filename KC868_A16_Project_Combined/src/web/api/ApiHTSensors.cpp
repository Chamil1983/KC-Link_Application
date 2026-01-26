// ApiHTSensors.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleHTSensors() {
    DynamicJsonDocument doc(1024);
    JsonArray sensorsArray = doc.createNestedArray("htSensors");

    const char* sensorTypeNames[] = {
        "Digital Input", "DHT11", "DHT22", "DS18B20"
    };

    for (int i = 0; i < 3; i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["index"] = i;
        sensor["pin"] = "HT" + String(i + 1);
        sensor["sensorType"] = htSensorConfig[i].sensorType;
        sensor["sensorTypeName"] = sensorTypeNames[htSensorConfig[i].sensorType];

        // Add appropriate readings based on sensor type
        if (htSensorConfig[i].sensorType == SENSOR_TYPE_DIGITAL) {
            sensor["value"] = directInputStates[i] ? "HIGH" : "LOW";
        }
        else if (htSensorConfig[i].sensorType == SENSOR_TYPE_DHT11 ||
            htSensorConfig[i].sensorType == SENSOR_TYPE_DHT22) {
            sensor["temperature"] = htSensorConfig[i].temperature;
            sensor["humidity"] = htSensorConfig[i].humidity;
        }
        else if (htSensorConfig[i].sensorType == SENSOR_TYPE_DS18B20) {
            sensor["temperature"] = htSensorConfig[i].temperature;
        }
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateHTSensor() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        debugPrintln("Received HT sensor update request: " + body); // Add debug output

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("sensor")) {
            JsonObject sensorJson = doc["sensor"];
            debugPrintln("Parsed sensor JSON successfully"); // Add debug output

            if (sensorJson.containsKey("index") && sensorJson.containsKey("sensorType")) {
                int index = sensorJson["index"];
                int sensorType = sensorJson["sensorType"];

                debugPrintln("Configuring HT" + String(index + 1) + " as type " + String(sensorType)); // Add debug output

                if (index >= 0 && index < 3 &&
                    sensorType >= 0 && sensorType <= SENSOR_TYPE_DS18B20) {

                    // Update sensor configuration
                    htSensorConfig[index].sensorType = sensorType;

                    // Initialize the sensor with new configuration
                    initializeSensor(index);

                    // Save configuration
                    saveHTSensorConfig();

                    debugPrintln("Sensor configuration updated successfully"); // Add debug output
                    response = "{\"status\":\"success\",\"message\":\"Sensor configuration updated\"}";
                }
                else {
                    debugPrintln("Invalid index or sensor type"); // Add debug output
                    response = "{\"status\":\"error\",\"message\":\"Invalid index or sensor type\"}";
                }
            }
            else {
                debugPrintln("Missing index or sensorType in request"); // Add debug output
                response = "{\"status\":\"error\",\"message\":\"Missing required fields\"}";
            }
        }
        else {
            debugPrintln("JSON parse error or missing sensor object"); // Add debug output
            response = "{\"status\":\"error\",\"message\":\"Invalid JSON or missing sensor object\"}";
        }
    }
    else {
        debugPrintln("No plain body in request"); // Add debug output
    }

    server.send(200, "application/json", response);
}

