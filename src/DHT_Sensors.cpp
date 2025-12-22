#include "DHT_Sensors.h"
#include "Debug.h"

DHTSensors::DHTSensors()
    : readInterval(2000),
    tempUnit(UNIT_C),
    oneWire(nullptr),
    ds18Sensors(nullptr),
    dsCount(0),
    lastDSBusRead(0)
{
    dhtPins[0] = PIN_DHT_SENSOR1;
    dhtPins[1] = PIN_DHT_SENSOR2;
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
        dhtInstances[i] = nullptr;
        chType[i] = DHT_TYPE_DHT22;
        chTempC[i] = NAN;
        chHumidity[i] = NAN;
        chConnected[i] = false;
        lastDHTRead[i] = 0;
    }
    for (uint8_t i = 0; i < MAX_DS18B20_SENSORS; i++)
        dsTempC[i] = -127.0f;
}

DHTSensors::~DHTSensors() {
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++)
        destroyDHT(i);
    if (ds18Sensors) { delete ds18Sensors; ds18Sensors = nullptr; }
    if (oneWire) { delete oneWire; oneWire = nullptr; }
}

void DHTSensors::allocateDHT(uint8_t ch) {
    destroyDHT(ch);
    if (ch >= NUM_DHT_SENSORS) return;
    if (chType[ch] == DHT_TYPE_NONE || chType[ch] == DHT_TYPE_DS18B20) return;
    uint8_t libType = (chType[ch] == DHT_TYPE_DHT11) ? DHT11 : DHT22;
    dhtInstances[ch] = new DHT(dhtPins[ch], libType);
    if (dhtInstances[ch]) {
        dhtInstances[ch]->begin();
        // Warm-up delay for DHT can be beneficial on some boards
        delay(20);
    }
}

void DHTSensors::destroyDHT(uint8_t ch) {
    if (ch >= NUM_DHT_SENSORS) return;
    if (dhtInstances[ch]) {
        delete dhtInstances[ch];
        dhtInstances[ch] = nullptr;
    }
}

bool DHTSensors::begin() {
    // Allocate DHT instances
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) allocateDHT(i);

    // DS18B20 bus init with quick retry
    for (int attempt = 0; attempt < 3; attempt++) {
        oneWire = new OneWire(PIN_DS18B20);
        if (oneWire) {
            ds18Sensors = new DallasTemperature(oneWire);
            if (ds18Sensors) {
                ds18Sensors->begin();
                dsCount = ds18Sensors->getDeviceCount();
                if (dsCount > MAX_DS18B20_SENSORS) dsCount = MAX_DS18B20_SENSORS;
                for (uint8_t i = 0; i < dsCount; i++) {
                    if (ds18Sensors->getAddress(dsAddresses[i], i)) {
                        ds18Sensors->setResolution(dsAddresses[i], 11); // faster conversions
                    }
                }
                break;
            }
        }
        if (ds18Sensors) { delete ds18Sensors; ds18Sensors = nullptr; }
        if (oneWire) { delete oneWire; oneWire = nullptr; }
        delay(120);
    }

    // Initial full forced read
    readAll(true);
    return true;
}

void DHTSensors::setTemperatureUnit(TempUnit u) { tempUnit = u; }
void DHTSensors::setReadInterval(uint32_t ms) { readInterval = max<uint32_t>(500, ms); }

bool DHTSensors::setSensorType(uint8_t ch, DHTSensorType type) {
    if (ch >= NUM_DHT_SENSORS) return false;
    if (chType[ch] == type) return true;
    chType[ch] = type;
    chTempC[ch] = NAN;
    chHumidity[ch] = NAN;
    chConnected[ch] = false;
    lastDHTRead[ch] = 0;
    allocateDHT(ch);
    return true;
}

DHTSensorType DHTSensors::getSensorType(uint8_t ch) const {
    if (ch >= NUM_DHT_SENSORS) return DHT_TYPE_NONE;
    return chType[ch];
}

float DHTSensors::getTemperatureC(uint8_t ch) {
    if (ch >= NUM_DHT_SENSORS) return NAN;
    return chTempC[ch];
}

float DHTSensors::getTemperature(uint8_t ch) {
    float c = getTemperatureC(ch);
    if (isnan(c)) return NAN;
    return (tempUnit == UNIT_F) ? (c * 9.0f / 5.0f + 32.0f) : c;
}

float DHTSensors::getHumidity(uint8_t ch) {
    if (ch >= NUM_DHT_SENSORS) return NAN;
    if (chType[ch] == DHT_TYPE_DS18B20) return NAN;
    return chHumidity[ch];
}

bool DHTSensors::isSensorConnected(uint8_t ch) {
    if (ch >= NUM_DHT_SENSORS) return false;
    return chConnected[ch];
}

void DHTSensors::refreshDS18B20(bool force) {
    if (!ds18Sensors || dsCount == 0) return;
    unsigned long now = millis();
    if (!force && (now - lastDSBusRead < DS_INTERVAL_MS)) return;
    ds18Sensors->requestTemperatures();
    lastDSBusRead = now;
    for (uint8_t i = 0; i < dsCount; i++)
        dsTempC[i] = ds18Sensors->getTempC(dsAddresses[i]);
}

void DHTSensors::mapDS18B20ToChannels() {
    if (dsCount == 0) {
        for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
            if (chType[i] == DHT_TYPE_DS18B20) {
                chConnected[i] = false;
                chTempC[i] = NAN;
                chHumidity[i] = NAN;
            }
        }
        return;
    }
    float ref = dsTempC[0];
    bool ok = (ref > -100.0f);
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
        if (chType[i] == DHT_TYPE_DS18B20) {
            if (ok) {
                chTempC[i] = ref;
                chConnected[i] = true;
            }
            else {
                chTempC[i] = NAN;
                chConnected[i] = false;
            }
            chHumidity[i] = NAN;
        }
    }
}

bool DHTSensors::readChannel(uint8_t ch, bool force) {
    if (ch >= NUM_DHT_SENSORS) return false;

    if (chType[ch] == DHT_TYPE_DS18B20) {
        refreshDS18B20(force);
        mapDS18B20ToChannels();
        return chConnected[ch];
    }

    if (chType[ch] == DHT_TYPE_NONE) {
        chConnected[ch] = false; chTempC[ch] = NAN; chHumidity[ch] = NAN;
        return false;
    }

    if (!dhtInstances[ch]) {
        chConnected[ch] = false; chTempC[ch] = NAN; chHumidity[ch] = NAN;
        return false;
    }

    unsigned long now = millis();
    if (!force && (now - lastDHTRead[ch] < readInterval)) {
        return chConnected[ch]; // use cached results
    }

    // Perform read (Adafruit DHT returns NAN on failure)
    float t = dhtInstances[ch]->readTemperature(); // Celsius
    float h = dhtInstances[ch]->readHumidity();
    lastDHTRead[ch] = now;

    if (!isnan(t)) {
        chTempC[ch] = t;
        chConnected[ch] = true;
    }
    else {
        chTempC[ch] = NAN;
        chConnected[ch] = false;
    }

    if (!isnan(h)) chHumidity[ch] = h;
    else if (!chConnected[ch]) chHumidity[ch] = NAN;

    return chConnected[ch];
}

bool DHTSensors::readAll(bool force) {
    bool dsNeeded = false;
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++)
        if (chType[i] == DHT_TYPE_DS18B20) { dsNeeded = true; break; }

    if (dsNeeded) {
        refreshDS18B20(force);
        mapDS18B20ToChannels();
    }

    bool ok = true;
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
        if (chType[i] == DHT_TYPE_DS18B20) continue;
        if (!readChannel(i, force)) ok = false;
    }
    return ok;
}

void DHTSensors::update() {
    // Poll DHT channels respecting interval
    unsigned long now = millis();
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
        if (chType[i] == DHT_TYPE_DS18B20 || chType[i] == DHT_TYPE_NONE) continue;
        if (now - lastDHTRead[i] >= readInterval) {
            readChannel(i, false);
        }
    }
    refreshDS18B20(false);
    mapDS18B20ToChannels();
}

uint8_t DHTSensors::getDS18B20Count() { return dsCount; }
float DHTSensors::getDS18B20Temperature(uint8_t index) {
    if (index >= dsCount) return -127.0f;
    return dsTempC[index];
}
DeviceAddress* DHTSensors::getDS18B20Address(uint8_t index) {
    if (index >= dsCount) return nullptr;
    return &dsAddresses[index];
}
bool DHTSensors::isDS18B20Connected(uint8_t index) {
    if (index >= dsCount) return false;
    return dsTempC[index] > -100.0f;
}
void DHTSensors::printDS18B20Address(uint8_t index) {
    if (index >= dsCount) return;
    Serial.print("0x");
    for (uint8_t i = 0; i < 8; i++) {
        if (dsAddresses[index][i] < 16) Serial.print('0');
        Serial.print(dsAddresses[index][i], HEX);
    }
}
void DHTSensors::setDS18B20Resolution(uint8_t resolution) {
    if (!ds18Sensors) return;
    if (resolution < 9) resolution = 9;
    else if (resolution > 12) resolution = 12;
    for (uint8_t i = 0; i < dsCount; i++)
        ds18Sensors->setResolution(dsAddresses[i], resolution);
}