\
#include "BACnetIntegration.h"
#include "../Globals.h"
#include "../Definitions.h"
#include <WiFi.h>
#include <Wire.h>

// Static member initialization
bool BACnetIntegration::_enabled = true;
bool BACnetIntegration::_started = false;
uint32_t BACnetIntegration::_lastSync = 0;

void BACnetIntegration::initialize() {
    Serial.println(F("[BACnet Integration] Initializing..."));

    // Device metadata (visible to BACnet client)
    bacnetDriver.setDeviceID(BACNET_DEVICE_ID);

    // Centralized device identity strings (Globals.cpp)
    bacnetDriver.setDeviceName(deviceNameStr.c_str());
    bacnetDriver.setDescription(deviceDescriptionStr.c_str());
    bacnetDriver.setLocation(deviceLocationStr.c_str());

    Serial.println(F("[BACnet Integration] Ready"));
}

void BACnetIntegration::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        bacnetDriver.end();
        _started = false;
        Serial.println(F("[BACnet Integration] Disabled"));
    } else {
        Serial.println(F("[BACnet Integration] Enabled"));
    }
}

bool BACnetIntegration::isEnabled() {
    return _enabled;
}

bool BACnetIntegration::isNetworkReady(IPAddress& ip, IPAddress& gw, IPAddress& mask) {
    // Prefer Ethernet when available
    if (ethConnected) {
        ip = ETH.localIP();
        gw = ETH.gatewayIP();
        mask = ETH.subnetMask();
        return (ip[0] != 0);
    }

    // Fall back to WiFi STA
    if (WiFi.status() == WL_CONNECTED) {
        ip = WiFi.localIP();
        gw = WiFi.gatewayIP();
        mask = WiFi.subnetMask();
        return (ip[0] != 0);
    }

    return false;
}

void BACnetIntegration::startIfNeeded() {
    if (_started && bacnetDriver.isRunning()) return;

    IPAddress ip, gw, mask;
    if (!isNetworkReady(ip, gw, mask)) return;

    if (bacnetDriver.begin(ip, gw, mask)) {
        _started = true;
        Serial.println(F("[BACnet Integration] BACnet/IP Started"));
    }
}

void BACnetIntegration::update() {
    if (!_enabled) return;

    // Start BACnet once network is ready
    startIfNeeded();

    if (!bacnetDriver.isRunning()) return;

    // Always handle BACnet network traffic
    bacnetDriver.task();

    // Sync hardware <-> BACnet at a controlled rate
    const uint32_t now = millis();
    if ((now - _lastSync) < SYNC_INTERVAL) {
        return;
    }
    _lastSync = now;

    // 1) Apply any BO write commands from BACnet client to hardware first
    applyBinaryOutputCommands();

    // 2) Push current hardware states to BACnet objects
    updateBinaryInputs();
    updateBinaryOutputs();
    updateAnalogInputs();
    updateSensorValues();
}

void BACnetIntegration::updateAnalogInputs() {
    // A1..A4 from global analogVoltages array (0..5V scaled)
    for (uint8_t i = 0; i < 4; i++) {
        bacnetDriver.updateAnalogInput(i, analogVoltages[i]);
    }
}

void BACnetIntegration::updateBinaryInputs() {
    // Digital Inputs 1..16 from global inputStates array
    for (uint8_t i = 0; i < 16; i++) {
        bacnetDriver.updateBinaryInput(i, inputStates[i]);
    }
}

void BACnetIntegration::updateBinaryOutputs() {
    // MOSFET Outputs 1..16 from global outputStates array
    for (uint8_t i = 0; i < 16; i++) {
        bacnetDriver.updateBinaryOutput(i, outputStates[i]);
    }
}

void BACnetIntegration::updateSensorValues() {
    // Sensor mapping to AI objects:
    //  AI101 DHT1 Temp, AI102 DHT1 RH
    //  AI103 DHT2 Temp, AI104 DHT2 RH
    //  AI105 DS18B20 Temp

    // DHT 1 (HT1)
    if (dhtSensors[0] != nullptr &&
        htSensorConfig[0].configured &&
        (htSensorConfig[0].sensorType == 1 || htSensorConfig[0].sensorType == 2)) {

        const float temp = dhtSensors[0]->readTemperature();
        const float hum  = dhtSensors[0]->readHumidity();

        if (!isnan(temp)) {
            char desc[64];
            snprintf(desc, sizeof(desc), "HT1 Temp: %.1f C", temp);
            bacnetDriver.updateSensorAnalog(101, temp, UNITS_DEGREES_CELSIUS, desc);
            htSensorConfig[0].temperature = temp;
        }
        if (!isnan(hum)) {
            char desc[64];
            snprintf(desc, sizeof(desc), "HT1 RH: %.1f %%", hum);
            bacnetDriver.updateSensorAnalog(102, hum, UNITS_PERCENT, desc);
            htSensorConfig[0].humidity = hum;
        }
    }

    // DHT 2 (HT2)
    if (dhtSensors[1] != nullptr &&
        htSensorConfig[1].configured &&
        (htSensorConfig[1].sensorType == 1 || htSensorConfig[1].sensorType == 2)) {

        const float temp = dhtSensors[1]->readTemperature();
        const float hum  = dhtSensors[1]->readHumidity();

        if (!isnan(temp)) {
            char desc[64];
            snprintf(desc, sizeof(desc), "HT2 Temp: %.1f C", temp);
            bacnetDriver.updateSensorAnalog(103, temp, UNITS_DEGREES_CELSIUS, desc);
            htSensorConfig[1].temperature = temp;
        }
        if (!isnan(hum)) {
            char desc[64];
            snprintf(desc, sizeof(desc), "HT2 RH: %.1f %%", hum);
            bacnetDriver.updateSensorAnalog(104, hum, UNITS_PERCENT, desc);
            htSensorConfig[1].humidity = hum;
        }
    }

    // DS18B20 (HT3)
    if (ds18b20Sensors[2] != nullptr &&
        htSensorConfig[2].configured &&
        htSensorConfig[2].sensorType == 3) {

        ds18b20Sensors[2]->requestTemperatures();
        const float temp = ds18b20Sensors[2]->getTempCByIndex(0);

        if (!isnan(temp) && temp > -127.0f) {
            char desc[64];
            snprintf(desc, sizeof(desc), "HT3 DS18B20: %.1f C", temp);
            bacnetDriver.updateSensorAnalog(105, temp, UNITS_DEGREES_CELSIUS, desc);
            htSensorConfig[2].temperature = temp;
        }
    }
}

void BACnetIntegration::applyBinaryOutputCommands() {
    // Read NEW Binary Output commands from BACnet and apply to hardware
    for (uint8_t i = 0; i < 16; i++) {
        bool value = false;
        if (bacnetDriver.getBinaryOutputCommand(i, value)) {

            // Respect master enable global flag
            if (!outputsMasterEnable) {
                Serial.println(F("[BACnet] Outputs blocked - Master Enable is OFF"));
                continue;
            }

            if (outputStates[i] != value) {
                outputStates[i] = value;

                // Write to PCF8574 output expanders
                if (i < 8) {
                    // Outputs 1-8 (PCF8574 0x24)
                    outputIC3.digitalWrite(i, value ? HIGH : LOW);
                } else {
                    // Outputs 9-16 (PCF8574 0x25)
                    outputIC4.digitalWrite(i - 8, value ? HIGH : LOW);
                }

                Serial.printf("[BACnet] Output %u set to %u\n", (unsigned)(i + 1), (unsigned)value);

                // Also update BACnet object immediately (faster UI feedback)
                bacnetDriver.updateBinaryOutput(i, value);
            }
        }
    }
}
