// HTSensorManager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

// NOTE: These functions are referenced from multiple translation units.
// Their signatures must exactly match FunctionPrototypes.h to avoid
// C++ name-mangling/linker errors.

void initializeSensor(uint8_t htIndex) {
    // Define the pin mapping for HT1-HT3
    if (htIndex >= 3) return;

    const int htPins[3] = { HT1_PIN, HT2_PIN, HT3_PIN };
    int pin = htPins[htIndex];

    // Clean up previous sensor objects if they exist
    if (dhtSensors[htIndex] != NULL) {
        delete dhtSensors[htIndex];
        dhtSensors[htIndex] = NULL;
    }
    if (oneWireBuses[htIndex] != NULL) {
        delete oneWireBuses[htIndex];
        oneWireBuses[htIndex] = NULL;
    }
    if (ds18b20Sensors[htIndex] != NULL) {
        delete ds18b20Sensors[htIndex];
        ds18b20Sensors[htIndex] = NULL;
    }

    // Configure pin based on sensor type
    switch (htSensorConfig[htIndex].sensorType) {
    case SENSOR_TYPE_DIGITAL:
        pinMode(pin, INPUT_PULLUP);
        break;

    case SENSOR_TYPE_DHT11:
        dhtSensors[htIndex] = new DHT(pin, DHT11);
        dhtSensors[htIndex]->begin();
        break;

    case SENSOR_TYPE_DHT22:
        dhtSensors[htIndex] = new DHT(pin, DHT22);
        dhtSensors[htIndex]->begin();
        break;

    case SENSOR_TYPE_DS18B20:
        oneWireBuses[htIndex] = new OneWire(pin);
        ds18b20Sensors[htIndex] = new DallasTemperature(oneWireBuses[htIndex]);
        ds18b20Sensors[htIndex]->begin();
        break;
    }

    htSensorConfig[htIndex].configured = true;
    htSensorConfig[htIndex].lastReadTime = 0;

    debugPrintln("HT" + String(htIndex + 1) + " sensor initialized as type " +
        String(htSensorConfig[htIndex].sensorType));
}

void readSensor(uint8_t htIndex) {
    // Only read at appropriate intervals based on sensor type
    unsigned long currentMillis = millis();

    if (htIndex >= 3) return;

    // Define minimum read intervals for different sensor types (in ms)
    const unsigned long DHT_READ_INTERVAL = 2000;  // DHT sensors should be read max once every 2 seconds
    const unsigned long DS18B20_READ_INTERVAL = 1000;  // DS18B20 every 1 second
    const unsigned long DIGITAL_READ_INTERVAL = 100;  // Digital inputs every 100ms

    unsigned long minInterval;
    switch (htSensorConfig[htIndex].sensorType) {
    case SENSOR_TYPE_DHT11:
    case SENSOR_TYPE_DHT22:
        minInterval = DHT_READ_INTERVAL;
        break;
    case SENSOR_TYPE_DS18B20:
        minInterval = DS18B20_READ_INTERVAL;
        break;
    case SENSOR_TYPE_DIGITAL:
    default:
        minInterval = DIGITAL_READ_INTERVAL;
        break;
    }

    // Return if it's not time to read yet
    if (currentMillis - htSensorConfig[htIndex].lastReadTime < minInterval) {
        return;
    }

    // Update last read time
    htSensorConfig[htIndex].lastReadTime = currentMillis;

    // Read sensor based on type
    const int htPins[3] = { HT1_PIN, HT2_PIN, HT3_PIN };
    int pin = htPins[htIndex];

    switch (htSensorConfig[htIndex].sensorType) {
    case SENSOR_TYPE_DIGITAL:
        // For digital input, just update the directInputStates array
        directInputStates[htIndex] = !digitalRead(pin); // Invert for active LOW logic
        break;

    case SENSOR_TYPE_DHT11:
    case SENSOR_TYPE_DHT22:
        if (dhtSensors[htIndex] != NULL) {
            // Read humidity and temperature
            float newHumidity = dhtSensors[htIndex]->readHumidity();
            float newTemperature = dhtSensors[htIndex]->readTemperature();

            // Check if readings are valid
            if (!isnan(newHumidity) && !isnan(newTemperature)) {
                htSensorConfig[htIndex].humidity = newHumidity;
                htSensorConfig[htIndex].temperature = newTemperature;
                debugPrintln("HT" + String(htIndex + 1) + " DHT: " +
                    String(newTemperature, 1) + "�C, " +
                    String(newHumidity, 1) + "%");
            }
            else {
                debugPrintln("HT" + String(htIndex + 1) + " DHT read error");
            }
        }
        break;

    case SENSOR_TYPE_DS18B20:
        if (ds18b20Sensors[htIndex] != NULL) {
            // Request temperature reading
            ds18b20Sensors[htIndex]->requestTemperatures();

            // Get temperature in Celsius
            float newTemperature = ds18b20Sensors[htIndex]->getTempCByIndex(0);

            // Check if reading is valid
            if (newTemperature != DEVICE_DISCONNECTED_C) {
                htSensorConfig[htIndex].temperature = newTemperature;
                debugPrintln("HT" + String(htIndex + 1) + " DS18B20: " +
                    String(newTemperature, 1) + "�C");
            }
            else {
                debugPrintln("HT" + String(htIndex + 1) + " DS18B20 read error");
            }
        }
        break;
    }
}

