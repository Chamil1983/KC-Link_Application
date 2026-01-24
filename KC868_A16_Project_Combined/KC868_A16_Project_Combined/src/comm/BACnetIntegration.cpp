#include "BACnetIntegration.h"
#include "../Globals.h"
#include "../Definitions.h"
#include <Wire.h>

// Static member initialization
bool BACnetIntegration::_enabled = false;
uint32_t BACnetIntegration::_lastSync = 0;

void BACnetIntegration::initialize() {
    Serial.println(F("[BACnet Integration] Ready"));
}

void BACnetIntegration::update() {
    if (!_enabled || !bacnetDriver.isRunning()) {
        return;
    }

    // Process BACnet stack
    bacnetDriver.task();

    // Periodic sync
    uint32_t currentTime = millis();
    if (currentTime - _lastSync >= SYNC_INTERVAL) {
        syncFromHardware();
        syncToHardware();
        _lastSync = currentTime;
    }
}

void BACnetIntegration::syncFromHardware() {
    updateAnalogInputs();
    updateBinaryInputs();
    updateBinaryOutputs();
    updateSensorValues();
}

void BACnetIntegration::syncToHardware() {
    applyBinaryOutputCommands();
}

void BACnetIntegration::updateAnalogInputs() {
    // Update Analog Inputs (4 channels) from global analogVoltages array
    for (uint8_t i = 0; i < 4; i++) {
        float voltage = analogVoltages[i];  // Global variable from Globals.h
        bacnetDriver.updateAnalogInput(i, voltage);
    }
}

void BACnetIntegration::updateBinaryInputs() {
    // Update 16 main digital inputs from global inputStates array
    for (uint8_t i = 0; i < 16; i++) {
        bool state = inputStates[i];  // Global variable from Globals.h
        bacnetDriver.updateBinaryInput(i, state);
    }

    // Update 3 direct inputs (HT1, HT2, HT3) from global directInputStates array
    for (uint8_t i = 0; i < 3; i++) {
        bool state = directInputStates[i];  // Global variable from Globals.h
        bacnetDriver.updateBinaryInput(16 + i, state);
    }
}

void BACnetIntegration::updateBinaryOutputs() {
    // Update Binary Outputs (current state) from global outputStates array
    for (uint8_t i = 0; i < 16; i++) {
        bool state = outputStates[i];  // Global variable from Globals.h
        bacnetDriver.updateBinaryOutput(i, state);
    }
}

void BACnetIntegration::updateSensorValues() {
    // Update sensor values from global dhtSensors and ds18b20Sensors arrays

    // DHT Sensor 1 (HT1)
    if (dhtSensors[0] != nullptr && htSensorConfig[0].type.indexOf("DHT") >= 0) {
        float temp = dhtSensors[0]->readTemperature();
        float hum = dhtSensors[0]->readHumidity();

        if (!isnan(temp) && !isnan(hum)) {
            char desc[64];
            snprintf(desc, sizeof(desc), "DHT1: %.1f°C, %.1f%%RH", temp, hum);
            bacnetDriver.updateSensorValue(0, temp, desc);
        }
    }

    // DHT Sensor 2 (HT2)
    if (dhtSensors[1] != nullptr && htSensorConfig[1].type.indexOf("DHT") >= 0) {
        float temp = dhtSensors[1]->readTemperature();
        float hum = dhtSensors[1]->readHumidity();

        if (!isnan(temp) && !isnan(hum)) {
            char desc[64];
            snprintf(desc, sizeof(desc), "DHT2: %.1f°C, %.1f%%RH", temp, hum);
            bacnetDriver.updateSensorValue(1, temp, desc);
        }
    }

    // DS18B20 Sensor (HT3)
    if (ds18b20Sensors[2] != nullptr && htSensorConfig[2].type.indexOf("DS18B20") >= 0) {
        ds18b20Sensors[2]->requestTemperatures();
        float temp = ds18b20Sensors[2]->getTempCByIndex(0);

        if (temp != DEVICE_DISCONNECTED_C && temp > -55.0 && temp < 125.0) {
            char desc[64];
            snprintf(desc, sizeof(desc), "DS18B20: %.1f°C", temp);
            bacnetDriver.updateSensorValue(2, temp, desc);
        }
    }
}

void BACnetIntegration::applyBinaryOutputCommands() {
    // Read Binary Output commands from BACnet and apply to hardware
    for (uint8_t i = 0; i < 16; i++) {
        bool value = false;
        if (bacnetDriver.getBinaryOutputCommand(i, value)) {
            // Check if value changed and update hardware
            if (outputStates[i] != value) {
                outputStates[i] = value;

                // Apply to hardware via PCF8574
                if (i < 8) {
                    // Outputs 0-7 on outputIC3 (address 0x24)
                    outputIC3.digitalWrite(i, value ? HIGH : LOW);
                }
                else {
                    // Outputs 8-15 on outputIC4 (address 0x25)
                    outputIC4.digitalWrite(i - 8, value ? HIGH : LOW);
                }

                Serial.printf("[BACnet] Output %d set to %d\n", i + 1, value);
            }
        }
    }
}

void BACnetIntegration::setEnabled(bool enabled) {
    _enabled = enabled;
    Serial.printf("[BACnet Integration] %s\n", enabled ? "Enabled" : "Disabled");
}

bool BACnetIntegration::isEnabled() {
    return _enabled;
}

String BACnetIntegration::getStatus() {
    if (!_enabled) {
        return "Disabled";
    }
    if (!bacnetDriver.isRunning()) {
        return "Not Running";
    }
    return "Running";
}

uint32_t BACnetIntegration::getRequestCount() {
    return bacnetDriver.getRequestCount();
}

uint32_t BACnetIntegration::getResponseCount() {
    return bacnetDriver.getResponseCount();
}