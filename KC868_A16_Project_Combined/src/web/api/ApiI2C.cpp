// ApiI2C.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleI2CScan() {
    DynamicJsonDocument doc(1024);
    JsonArray devices = doc.createNestedArray("devices");

    // Scan I2C bus
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            JsonObject device = devices.createNestedObject();
            device["address"] = "0x" + String(address, HEX);

            // Identify known devices
            if (address == PCF8574_INPUTS_1_8) {
                device["name"] = "PCF8574 Inputs 1-8";
            }
            else if (address == PCF8574_INPUTS_9_16) {
                device["name"] = "PCF8574 Inputs 9-16";
            }
            else if (address == PCF8574_OUTPUTS_1_8) {
                device["name"] = "PCF8574 Outputs 1-8";
            }
            else if (address == PCF8574_OUTPUTS_9_16) {
                device["name"] = "PCF8574 Outputs 9-16";
            }
            else if (address == 0x68) {
                device["name"] = "DS3231 RTC";
            }
            else {
                device["name"] = "Unknown device";
            }
        }
    }

    doc["total_devices"] = devices.size();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

