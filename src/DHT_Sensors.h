#ifndef DHT_SENSORS_H
#define DHT_SENSORS_H

#include <Arduino.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
#include "Config.h"

// Logical sensor types assignable to each channel
enum DHTSensorType : uint8_t {
    DHT_TYPE_NONE = 0,
    DHT_TYPE_DHT11 = 11,
    DHT_TYPE_DHT22 = 22,
    DHT_TYPE_DS18B20 = 18    // maps channel temperature to first DS18B20 found
};

enum TempUnit : uint8_t {
    UNIT_C = 0,
    UNIT_F = 1
};

class DHTSensors {
public:
    DHTSensors();
    ~DHTSensors();

    bool begin();                       // Initialize DHT channels + DS18B20 bus
    void update();                      // Periodic maintenance (call each loop)

    // Access
    float getTemperatureC(uint8_t ch);
    float getTemperature(uint8_t ch);
    float getHumidity(uint8_t ch);      // NAN for DS18B20 / NONE / invalid
    bool  isSensorConnected(uint8_t ch);

    // Configuration
    bool setSensorType(uint8_t ch, DHTSensorType type);
    DHTSensorType getSensorType(uint8_t ch) const;

    void setTemperatureUnit(TempUnit u);
    TempUnit getTemperatureUnit() const { return tempUnit; }

    void setReadInterval(uint32_t ms);  // ≥500 ms (for DHT reliability)
    uint32_t getReadInterval() const { return readInterval; }

    // Forced/on-demand reads
    bool readChannel(uint8_t ch, bool force = false);
    bool readAll(bool force = false);

    // DS18B20 raw
    uint8_t getDS18B20Count();
    float getDS18B20Temperature(uint8_t index);
    DeviceAddress* getDS18B20Address(uint8_t index);
    bool isDS18B20Connected(uint8_t index);
    void printDS18B20Address(uint8_t index);
    void setDS18B20Resolution(uint8_t resolution = 11);

    // Additional helper functions for the test program
    uint8_t getConnectedCount() {
        uint8_t count = 0;
        for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
            if (chConnected[i]) count++;
        }
        return count;
    }

private:
    void allocateDHT(uint8_t ch);
    void destroyDHT(uint8_t ch);
    void refreshDS18B20(bool force);
    void mapDS18B20ToChannels();

    DHT* dhtInstances[NUM_DHT_SENSORS];
    uint8_t       dhtPins[NUM_DHT_SENSORS];
    DHTSensorType chType[NUM_DHT_SENSORS];

    float         chTempC[NUM_DHT_SENSORS];
    float         chHumidity[NUM_DHT_SENSORS];
    bool          chConnected[NUM_DHT_SENSORS];
    unsigned long lastDHTRead[NUM_DHT_SENSORS];

    uint32_t readInterval;  // per-channel min poll interval for DHTs
    TempUnit tempUnit;

    // DS18B20
    OneWire* oneWire;
    DallasTemperature* ds18Sensors;
    DeviceAddress     dsAddresses[MAX_DS18B20_SENSORS];
    float             dsTempC[MAX_DS18B20_SENSORS];
    uint8_t           dsCount;
    unsigned long     lastDSBusRead;
    const unsigned long DS_INTERVAL_MS = 1000;
};

#endif // DHT_SENSORS_H