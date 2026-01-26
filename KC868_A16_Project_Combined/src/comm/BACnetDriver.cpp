\
#include "BACnetDriver.h"
#include <WiFi.h>
#include <string.h>
#include "../Globals.h"
#include "../FunctionPrototypes.h"


// Global instance
BACnetDriver bacnetDriver;

static inline uint16_t u16be(uint8_t hi, uint8_t lo) { return ((uint16_t)hi << 8) | (uint16_t)lo; }

BACnetDriver::BACnetDriver()
    : _initialized(false),
      _deviceID(BACNET_DEVICE_ID),
      _lastIAmMs(0) {

    memset(_deviceName, 0, sizeof(_deviceName));
    memset(_deviceDescription, 0, sizeof(_deviceDescription));
    memset(_deviceLocation, 0, sizeof(_deviceLocation));

    strncpy(_deviceName, "KC868-A16", sizeof(_deviceName) - 1);
    strncpy(_deviceDescription, "KC868-A16 Automation Controller", sizeof(_deviceDescription) - 1);
    strncpy(_deviceLocation, "Factory Floor", sizeof(_deviceLocation) - 1);

    // Clear command queues
    for (uint8_t i = 0; i < BACNET_MAX_BO; i++) {
        _boCmdPending[i] = false;
        _boCmdValue[i] = false;
    }

    // Initialize AI Main (A1..A4)
    for (uint8_t i = 0; i < BACNET_MAX_AI_MAIN; i++) {
        _aiMain[i].instance = (uint32_t)(i + 1);
        _aiMain[i].type = OBJECT_ANALOG_INPUT;
        snprintf(_aiMain[i].name, sizeof(_aiMain[i].name), "Analog Input %u", (unsigned)(i + 1));
        strncpy(_aiMain[i].description, "0-5V Analog Input", sizeof(_aiMain[i].description) - 1);
        _aiMain[i].presentValue = 0.0f;
        _aiMain[i].units = UNITS_VOLTS;
        _aiMain[i].outOfService = false;
        _aiMain[i].lastUpdateMs = 0;
    }

    // Initialize Sensor AIs (AI101..AI105)
    // AI101 DHT1 Temperature
    // AI102 DHT1 Humidity
    // AI103 DHT2 Temperature
    // AI104 DHT2 Humidity
    // AI105 DS18B20 Temperature
    const struct {
        uint32_t inst;
        const char* name;
        const char* desc;
        uint16_t units;
    } sensorDefs[BACNET_MAX_AI_SENSORS] = {
        {101, "DHT1 Temperature", "HT1 DHT Temperature", UNITS_DEGREES_CELSIUS},
        {102, "DHT1 Humidity",    "HT1 DHT Humidity",    UNITS_PERCENT},
        {103, "DHT2 Temperature", "HT2 DHT Temperature", UNITS_DEGREES_CELSIUS},
        {104, "DHT2 Humidity",    "HT2 DHT Humidity",    UNITS_PERCENT},
        {105, "DS18B20 Temp",     "HT3 DS18B20 Temp",    UNITS_DEGREES_CELSIUS},
    };

    for (uint8_t i = 0; i < BACNET_MAX_AI_SENSORS; i++) {
        _aiSensors[i].instance = sensorDefs[i].inst;
        _aiSensors[i].type = OBJECT_ANALOG_INPUT;
        strncpy(_aiSensors[i].name, sensorDefs[i].name, sizeof(_aiSensors[i].name) - 1);
        strncpy(_aiSensors[i].description, sensorDefs[i].desc, sizeof(_aiSensors[i].description) - 1);
        _aiSensors[i].presentValue = 0.0f;
        _aiSensors[i].units = sensorDefs[i].units;
        _aiSensors[i].outOfService = false;
        _aiSensors[i].lastUpdateMs = 0;
    }

    // Initialize BI (Digital Inputs 1..16)
    for (uint8_t i = 0; i < BACNET_MAX_BI; i++) {
        _bi[i].instance = (uint32_t)(i + 1);
        _bi[i].type = OBJECT_BINARY_INPUT;
        snprintf(_bi[i].name, sizeof(_bi[i].name), "Digital Input %u", (unsigned)(i + 1));
        strncpy(_bi[i].description, "Opto-isolated Digital Input", sizeof(_bi[i].description) - 1);
        _bi[i].presentValue = 0.0f;
        _bi[i].units = UNITS_NO_UNITS;
        _bi[i].outOfService = false;
        _bi[i].lastUpdateMs = 0;
    }

    // Initialize BO (MOSFET Outputs 1..16)
    for (uint8_t i = 0; i < BACNET_MAX_BO; i++) {
        _bo[i].instance = (uint32_t)(i + 1);
        _bo[i].type = OBJECT_BINARY_OUTPUT;
        snprintf(_bo[i].name, sizeof(_bo[i].name), "MOSFET Output %u", (unsigned)(i + 1));
        strncpy(_bo[i].description, "Digital MOSFET Output", sizeof(_bo[i].description) - 1);
        _bo[i].presentValue = 0.0f;
        _bo[i].units = UNITS_NO_UNITS;
        _bo[i].outOfService = false;
        _bo[i].lastUpdateMs = 0;
    }
}

BACnetDriver::~BACnetDriver() {
    end();
}

bool BACnetDriver::begin(IPAddress localIP, IPAddress gateway, IPAddress subnet) {
    if (_initialized) {
        return true;
    }

    _localIP = localIP;
    _gateway = gateway;
    _subnet  = subnet;

    Serial.printf("[BACnet] Starting BACnet/IP on %s:%u\n", _localIP.toString().c_str(), (unsigned)BACNET_UDP_PORT);

    if (!_udp.begin(BACNET_UDP_PORT)) {
        Serial.println("[BACnet] UDP begin failed");
        _initialized = false;
        return false;
    }

    _initialized = true;
    _lastIAmMs = 0;

    // Announce once quickly (broadcast)
    sendIAmBroadcast();
    _lastIAmMs = millis();

    return true;
}

void BACnetDriver::end() {
    if (!_initialized) {
        return;
    }
    _udp.stop();
    _initialized = false;
    Serial.println("[BACnet] Stopped");
}

void BACnetDriver::task() {
    if (!_initialized) return;

    processIncomingPacket();

    // Periodic broadcast I-Am (every 15 seconds)
    const uint32_t now = millis();
    if ((now - _lastIAmMs) > 15000UL) {
        sendIAmBroadcast();
        _lastIAmMs = now;
    }
}

// -------------------- Config --------------------
void BACnetDriver::setDeviceID(uint32_t deviceID) {
    _deviceID = deviceID;
}

void BACnetDriver::setDeviceName(const char* name) {
    if (!name) return;
    strncpy(_deviceName, name, sizeof(_deviceName) - 1);
}

void BACnetDriver::setDescription(const char* desc) {
    if (!desc) return;
    strncpy(_deviceDescription, desc, sizeof(_deviceDescription) - 1);
}

void BACnetDriver::setLocation(const char* loc) {
    if (!loc) return;
    strncpy(_deviceLocation, loc, sizeof(_deviceLocation) - 1);
}

// -------------------- Hardware -> BACnet --------------------
void BACnetDriver::updateAnalogInput(uint8_t channel, float volts) {
    if (channel >= BACNET_MAX_AI_MAIN) return;
    _aiMain[channel].presentValue = volts;
    _aiMain[channel].lastUpdateMs = millis();
}

void BACnetDriver::updateBinaryInput(uint8_t channel, bool active) {
    if (channel >= BACNET_MAX_BI) return;
    _bi[channel].presentValue = active ? 1.0f : 0.0f;
    _bi[channel].lastUpdateMs = millis();
}

void BACnetDriver::updateBinaryOutput(uint8_t channel, bool active) {
    if (channel >= BACNET_MAX_BO) return;
    _bo[channel].presentValue = active ? 1.0f : 0.0f;
    _bo[channel].lastUpdateMs = millis();
}

void BACnetDriver::updateSensorAnalog(uint16_t instance, float value, uint16_t units, const char* desc) {
    for (uint8_t i = 0; i < BACNET_MAX_AI_SENSORS; i++) {
        if (_aiSensors[i].instance == instance) {
            _aiSensors[i].presentValue = value;
            _aiSensors[i].units = units;
            if (desc && desc[0]) {
                strncpy(_aiSensors[i].description, desc, sizeof(_aiSensors[i].description) - 1);
            }
            _aiSensors[i].lastUpdateMs = millis();
            return;
        }
    }
}

// -------------------- BACnet -> Hardware Commands --------------------
bool BACnetDriver::getBinaryOutputCommand(uint8_t channel, bool& active) {
    if (channel >= BACNET_MAX_BO) return false;
    if (!_boCmdPending[channel]) return false;

    active = _boCmdValue[channel];
    _boCmdPending[channel] = false; // consume
    return true;
}

// -------------------- Packet Processing --------------------
void BACnetDriver::processIncomingPacket() {
    const int packetSize = _udp.parsePacket();
    if (packetSize <= 0) return;

    IPAddress remoteIP = _udp.remoteIP();
    uint16_t remotePort = (uint16_t)_udp.remotePort();

    const int len = _udp.read(_rxBuffer, BACNET_RX_BUFFER_SIZE);
    if (len < 8) return;

    // BVLL
    if (_rxBuffer[0] != BVLL_TYPE_BACNET_IP) return;

    const uint8_t bvllFunc = _rxBuffer[1];
    const uint16_t bvllLen = u16be(_rxBuffer[2], _rxBuffer[3]);
    if (bvllLen < 8 || bvllLen > (uint16_t)len) {
        // Use actual length if header is inconsistent
        // (some stacks may send larger UDP packet than bvll length)
    }

    if (bvllFunc != BVLL_FUNC_ORIGINAL_UNICAST_NPDU &&
        bvllFunc != BVLL_FUNC_ORIGINAL_BROADCAST_NPDU) {
        return;
    }

    // NPDU
    uint16_t offset = 4;
    if (_rxBuffer[offset++] != 0x01) return; // BACnet Protocol Version

    uint8_t npduCtrl = _rxBuffer[offset++];

    // Skip destination address if present
    if (npduCtrl & 0x20) {
        if (offset + 3 > (uint16_t)len) return;
        offset += 2; // DNET
        uint8_t dlen = _rxBuffer[offset++];
        offset += dlen; // DADR
        if (offset + 1 > (uint16_t)len) return;
        offset += 1; // hop count
    }

    // Skip source address if present
    if (npduCtrl & 0x08) {
        if (offset + 3 > (uint16_t)len) return;
        offset += 2; // SNET
        uint8_t slen = _rxBuffer[offset++];
        offset += slen; // SADR
    }

    if (offset >= (uint16_t)len) return;

    // APDU
    uint8_t pduType = (_rxBuffer[offset] & 0xF0);
    if (pduType == PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST) {
        if (offset + 1 >= (uint16_t)len) return;
        uint8_t serviceChoice = _rxBuffer[offset + 1];
        if (serviceChoice == SERVICE_UNCONFIRMED_WHO_IS) {
            handleWhoIs(remoteIP, remotePort);
        }
        return;
    }

    if (pduType == PDU_TYPE_CONFIRMED_SERVICE_REQUEST) {
        if (offset + 3 >= (uint16_t)len) return;

        // Confirmed request header:
        // [0] PDU Type/flags
        // [1] Max Segments / Max APDU
        // [2] Invoke ID
        // [3] Service Choice
        const uint8_t invokeId = _rxBuffer[offset + 2];
        const uint8_t serviceChoice = _rxBuffer[offset + 3];

        uint8_t* svcData = &_rxBuffer[offset + 4];
        uint16_t svcLen  = (uint16_t)len - (offset + 4);

        if (serviceChoice == SERVICE_CONFIRMED_READ_PROPERTY) {
            handleReadProperty(invokeId, svcData, svcLen, remoteIP, remotePort);
        } else if (serviceChoice == SERVICE_CONFIRMED_WRITE_PROPERTY) {
            handleWriteProperty(invokeId, svcData, svcLen, remoteIP, remotePort);
        } else {
            // Service not supported
            sendError(invokeId, serviceChoice, 2 /*services*/, 9 /*service request denied*/, remoteIP, remotePort);
        }
    }
}

void BACnetDriver::handleWhoIs(IPAddress remoteIP, uint16_t remotePort) {
    // Reply with I-Am unicast (most compatible)
    Serial.printf("[BACnet] Who-Is from %s:%u -> I-Am\n", remoteIP.toString().c_str(), (unsigned)remotePort);
    sendIAmUnicast(remoteIP, remotePort); // respond on BACnet port
}

void BACnetDriver::handleReadProperty(uint8_t invokeId, uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort) {
    // Service parameters:
    // [0] context tag 0: object id (0x0C) + 4 bytes
    // [1] context tag 1: property id (0x19) + 1 byte (most cases)
    if (pduLen < 7) {
        sendError(invokeId, SERVICE_CONFIRMED_READ_PROPERTY, 5 /*communication*/, 1 /*invalid APDU*/, remoteIP, remotePort);
        return;
    }

    uint16_t offset = 0;

    // Object ID
    if (pdu[offset++] != 0x0C) {
        sendError(invokeId, SERVICE_CONFIRMED_READ_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }

    uint8_t objectType = 0;
    uint32_t instance = 0;
    if (!decodeObjectId(&pdu[offset], pduLen - offset, objectType, instance)) {
        sendError(invokeId, SERVICE_CONFIRMED_READ_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }
    offset += 4;

    // Property ID (context tag 1)
    uint32_t propertyId = 0;
    uint16_t consumed = 0;
    if (!decodeContextUnsigned(1, &pdu[offset], pduLen - offset, propertyId, consumed)) {
        sendError(invokeId, SERVICE_CONFIRMED_READ_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }
    offset += consumed;

    // Optional Array index (context tag 2) - ignore for now
    if (offset < pduLen && (pdu[offset] & 0xF0) == 0x20 && (pdu[offset] & 0x08) == 0x08) {
        // context tag 2
        uint32_t dummy = 0;
        uint16_t c2 = 0;
        if (decodeContextUnsigned(2, &pdu[offset], pduLen - offset, dummy, c2)) {
            offset += c2;
        }
    }

    // Build ReadPropertyAck (ComplexAck)
    uint16_t tx = 0;

    // BVLL
    _txBuffer[tx++] = BVLL_TYPE_BACNET_IP;
    _txBuffer[tx++] = BVLL_FUNC_ORIGINAL_UNICAST_NPDU;
    _txBuffer[tx++] = 0x00; // length hi (fill later)
    _txBuffer[tx++] = 0x00; // length lo

    // NPDU (no routing)
    _txBuffer[tx++] = 0x01;
    _txBuffer[tx++] = 0x00;

    // APDU: Complex ACK
    _txBuffer[tx++] = PDU_TYPE_COMPLEX_ACK;
    _txBuffer[tx++] = invokeId;
    _txBuffer[tx++] = SERVICE_CONFIRMED_READ_PROPERTY;

    // Service ACK payload:
    // object-id (tag0)
    _txBuffer[tx++] = 0x0C;
    tx += encodeObjectId(&_txBuffer[tx], objectType, instance);

    // property-id (tag1) - context tag 1 length 1 or 2 (supports >255)
    if (propertyId <= 0xFF) {
        _txBuffer[tx++] = 0x19; // tag1 len1
        _txBuffer[tx++] = (uint8_t)(propertyId & 0xFF);
    } else {
        _txBuffer[tx++] = 0x1A; // tag1 len2
        _txBuffer[tx++] = (uint8_t)((propertyId >> 8) & 0xFF);
        _txBuffer[tx++] = (uint8_t)(propertyId & 0xFF);
    }

    // property-value (opening tag3)
    _txBuffer[tx++] = 0x3E;

    // Encode requested property
    bool encoded = false;

    // Device object
    if (objectType == OBJECT_DEVICE && instance == _deviceID) {
        switch (propertyId) {
            case PROP_OBJECT_IDENTIFIER:
                _txBuffer[tx++] = 0xC4; // Object Identifier (application tag 12, len 4)
                tx += encodeObjectId(&_txBuffer[tx], OBJECT_DEVICE, _deviceID);
                encoded = true;
                break;

            case PROP_OBJECT_NAME:
                tx += encodeAppCharacterString(&_txBuffer[tx], _deviceName);
                encoded = true;
                break;

            case PROP_DESCRIPTION:
                tx += encodeAppCharacterString(&_txBuffer[tx], _deviceDescription);
                encoded = true;
                break;

            case PROP_LOCATION:
                tx += encodeAppCharacterString(&_txBuffer[tx], _deviceLocation);
                encoded = true;
                break;

            case PROP_VENDOR_NAME:
                tx += encodeAppCharacterString(&_txBuffer[tx], manufacturerStr.c_str());
                encoded = true;
                break;

            case PROP_VENDOR_IDENTIFIER:
                tx += encodeAppUnsigned(&_txBuffer[tx], 999); // placeholder vendor id
                encoded = true;
                break;

            case PROP_MODEL_NAME:
                tx += encodeAppCharacterString(&_txBuffer[tx], deviceNameStr.c_str());
                encoded = true;
                break;

            case PROP_SERIAL_NUMBER:
                tx += encodeAppCharacterString(&_txBuffer[tx], getDeviceSerialNumber().c_str());
                encoded = true;
                break;

            case PROP_MESA_MAC_ADDRESS:
                tx += encodeAppCharacterString(&_txBuffer[tx], getBoardMacString().c_str());
                encoded = true;
                break;

            case PROP_MESA_HARDWARE_VER:
                tx += encodeAppCharacterString(&_txBuffer[tx], hardwareVersionStr.c_str());
                encoded = true;
                break;

            case PROP_MESA_YEAR_DEV:
                tx += encodeAppCharacterString(&_txBuffer[tx], yearOfDevelopmentStr.c_str());
                encoded = true;
                break;

            case PROP_MESA_SUBNET_MASK:
                tx += encodeAppCharacterString(&_txBuffer[tx], wifiStaSubnet.toString().c_str());
                encoded = true;
                break;

            case PROP_MESA_GATEWAY:
                tx += encodeAppCharacterString(&_txBuffer[tx], wifiStaGateway.toString().c_str());
                encoded = true;
                break;

            case PROP_MESA_DNS1:
                tx += encodeAppCharacterString(&_txBuffer[tx], wifiStaDns1.toString().c_str());
                encoded = true;
                break;

            case PROP_MESA_DEVICE_DATETIME:
                // Return current device local date/time (RTC-driven) as a formatted string
                // getTimeString() already returns the configured local time in a readable format.
                tx += encodeAppCharacterString(&_txBuffer[tx], getTimeString().c_str());
                encoded = true;
                break;

            case PROP_MESA_AP_SSID:
                tx += encodeAppCharacterString(&_txBuffer[tx], ap_ssid);
                encoded = true;
                break;

            case PROP_MESA_AP_PASSWORD:
                tx += encodeAppCharacterString(&_txBuffer[tx], ap_password);
                encoded = true;
                break;

            case PROP_MESA_AP_IP:
                tx += encodeAppCharacterString(&_txBuffer[tx], "192.168.4.1");
                encoded = true;
                break;

            case PROP_FIRMWARE_REVISION:
                tx += encodeAppCharacterString(&_txBuffer[tx], firmwareVersion.c_str());
                encoded = true;
                break;

            case PROP_APPLICATION_SOFTWARE:
                tx += encodeAppCharacterString(&_txBuffer[tx], "KC868_A16_Controller");
                encoded = true;
                break;

            case PROP_PROTOCOL_VERSION:
                tx += encodeAppUnsigned(&_txBuffer[tx], 1);
                encoded = true;
                break;

            case PROP_PROTOCOL_REVISION:
                tx += encodeAppUnsigned(&_txBuffer[tx], 22);
                encoded = true;
                break;

            case PROP_MAX_APDU_LENGTH_ACCEPTED:
                tx += encodeAppUnsigned(&_txBuffer[tx], BACNET_MAX_APDU);
                encoded = true;
                break;

            case PROP_SEGMENTATION_SUPPORTED:
                tx += encodeAppEnumerated(&_txBuffer[tx], 3 /*noSegmentation*/);
                encoded = true;
                break;

            case PROP_SYSTEM_STATUS:
                tx += encodeAppEnumerated(&_txBuffer[tx], 0 /*operational*/);
                encoded = true;
                break;

            case PROP_OBJECT_LIST:
            {
                // Return list of supported objects (application tag: Object ID repeated)
                // Format: [ObjectID] [ObjectID] ...
                // Device + AI main + AI sensors + BI + BO
                // Device
                _txBuffer[tx++] = 0xC4;
                tx += encodeObjectId(&_txBuffer[tx], OBJECT_DEVICE, _deviceID);

                // AI main
                for (uint8_t i = 0; i < BACNET_MAX_AI_MAIN; i++) {
                    _txBuffer[tx++] = 0xC4;
                    tx += encodeObjectId(&_txBuffer[tx], OBJECT_ANALOG_INPUT, _aiMain[i].instance);
                }
                // AI sensors
                for (uint8_t i = 0; i < BACNET_MAX_AI_SENSORS; i++) {
                    _txBuffer[tx++] = 0xC4;
                    tx += encodeObjectId(&_txBuffer[tx], OBJECT_ANALOG_INPUT, _aiSensors[i].instance);
                }
                // BI
                for (uint8_t i = 0; i < BACNET_MAX_BI; i++) {
                    _txBuffer[tx++] = 0xC4;
                    tx += encodeObjectId(&_txBuffer[tx], OBJECT_BINARY_INPUT, _bi[i].instance);
                }
                // BO
                for (uint8_t i = 0; i < BACNET_MAX_BO; i++) {
                    _txBuffer[tx++] = 0xC4;
                    tx += encodeObjectId(&_txBuffer[tx], OBJECT_BINARY_OUTPUT, _bo[i].instance);
                }

                encoded = true;
                break;
            }

            default:
                break;
        }
    } else {
        // Normal object
        BACnetObject* obj = findObject(objectType, instance);
        if (obj) {
            switch (propertyId) {
                case PROP_OBJECT_IDENTIFIER:
                    _txBuffer[tx++] = 0xC4;
                    tx += encodeObjectId(&_txBuffer[tx], objectType, instance);
                    encoded = true;
                    break;

                case PROP_OBJECT_NAME:
                    tx += encodeAppCharacterString(&_txBuffer[tx], obj->name);
                    encoded = true;
                    break;

                case PROP_DESCRIPTION:
                    tx += encodeAppCharacterString(&_txBuffer[tx], obj->description);
                    encoded = true;
                    break;

                case PROP_OBJECT_TYPE:
                    tx += encodeAppEnumerated(&_txBuffer[tx], objectType);
                    encoded = true;
                    break;

                case PROP_PRESENT_VALUE:
                    if (objectType == OBJECT_ANALOG_INPUT) {
                        tx += encodeAppReal(&_txBuffer[tx], obj->presentValue);
                    } else if (objectType == OBJECT_BINARY_INPUT || objectType == OBJECT_BINARY_OUTPUT) {
                        tx += encodeAppEnumerated(&_txBuffer[tx], (obj->presentValue > 0.5f) ? 1 : 0);
                    }
                    encoded = true;
                    break;

                case PROP_UNITS:
                    if (objectType == OBJECT_ANALOG_INPUT) {
                        tx += encodeAppEnumerated(&_txBuffer[tx], obj->units);
                        encoded = true;
                    }
                    break;

                case PROP_OUT_OF_SERVICE:
                    tx += encodeAppBoolean(&_txBuffer[tx], obj->outOfService);
                    encoded = true;
                    break;

                default:
                    break;
            }
        }
    }

    // Close tag 3
    _txBuffer[tx++] = 0x3F;

    // If not encoded, return error
    if (!encoded) {
        sendError(invokeId, SERVICE_CONFIRMED_READ_PROPERTY, 8 /*property*/, 32 /*unknown property*/, remoteIP, remotePort);
        return;
    }

    // BVLL length
    _txBuffer[2] = (uint8_t)((tx >> 8) & 0xFF);
    _txBuffer[3] = (uint8_t)(tx & 0xFF);

    // Send response
    _udp.beginPacket(remoteIP, remotePort);
    _udp.write(_txBuffer, tx);
    _udp.endPacket();
}

void BACnetDriver::handleWriteProperty(uint8_t invokeId, uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort) {
    // Support: BO Present_Value only
    if (pduLen < 9) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }

    uint16_t offset = 0;

    // Object ID (tag0)
    if (pdu[offset++] != 0x0C) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }

    uint8_t objectType = 0;
    uint32_t instance = 0;
    if (!decodeObjectId(&pdu[offset], pduLen - offset, objectType, instance)) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }
    offset += 4;

    // Property ID (tag1)
    uint32_t propertyId = 0;
    uint16_t consumed = 0;
    if (!decodeContextUnsigned(1, &pdu[offset], pduLen - offset, propertyId, consumed)) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 5, 1, remoteIP, remotePort);
        return;
    }
    offset += consumed;

    // Optional Array index (tag2) - ignore
    if (offset < pduLen && (pdu[offset] & 0xF0) == 0x20 && (pdu[offset] & 0x08) == 0x08) {
        uint32_t dummy = 0;
        uint16_t c2 = 0;
        if (decodeContextUnsigned(2, &pdu[offset], pduLen - offset, dummy, c2)) {
            offset += c2;
        }
    }

    // Opening tag3 for value
    if (offset >= pduLen || pdu[offset] != 0x3E) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 2 /*services*/, 2 /*invalid data*/, remoteIP, remotePort);
        return;
    }
    offset++;

    // -------- Device Date/Time Sync (vendor property) --------
    // Accept WriteProperty(Device, PROP_MESA_DEVICE_DATETIME, "YYYY-MM-DD HH:mm:ss")
    if (objectType == OBJECT_DEVICE && instance == _deviceID && propertyId == PROP_MESA_DEVICE_DATETIME) {
        String dt;
        uint16_t valConsumedStr = 0;
        if (!decodeAnyValueToString(&pdu[offset], pduLen - offset, dt, valConsumedStr)) {
            sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 2, 2, remoteIP, remotePort);
            return;
        }

        int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
        // Expected: YYYY-MM-DD HH:mm:ss
        if (sscanf(dt.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) {
            Serial.printf("[BACnet] Invalid RTC format: %s\n", dt.c_str());
            sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 2, 2, remoteIP, remotePort);
            return;
        }

        // Basic range checks
        if (y < 2000 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
            h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) {
            Serial.printf("[BACnet] RTC out of range: %s\n", dt.c_str());
            sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 2, 2, remoteIP, remotePort);
            return;
        }

        // Store to system time + DS3231 (existing project function)
        syncTimeFromClient(y, mo, d, h, mi, s);
        Serial.printf("[BACnet] RTC synced to: %s\n", getTimeString().c_str());

        sendSimpleAck(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, remoteIP, remotePort);
        return;
    }

    // Value: accept Enumerated (active/inactive) or Boolean
    bool newValue = false;
    uint16_t valConsumed = 0;
    if (!decodeAnyValueToBool(&pdu[offset], pduLen - offset, newValue, valConsumed)) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 2, 2, remoteIP, remotePort);
        return;
    }
    offset += valConsumed;

    // Expect closing tag3 (0x3F) somewhere later; ignore strict.

    // Validate object and property
    if (objectType != OBJECT_BINARY_OUTPUT || propertyId != PROP_PRESENT_VALUE) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 8 /*property*/, 32 /*unknown property*/, remoteIP, remotePort);
        return;
    }

    if (instance < 1 || instance > BACNET_MAX_BO) {
        sendError(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, 8, 42 /*unknown object*/, remoteIP, remotePort);
        return;
    }

    const uint8_t ch = (uint8_t)(instance - 1);

    // Store command pending (do NOT get overwritten by status sync)
    _boCmdValue[ch] = newValue;
    _boCmdPending[ch] = true;

    // Also update BO present value immediately for reads
    _bo[ch].presentValue = newValue ? 1.0f : 0.0f;
    _bo[ch].lastUpdateMs = millis();

    Serial.printf("[BACnet] WriteProperty BO%u Present_Value = %u\n", (unsigned)instance, (unsigned)newValue);

    // ACK
    sendSimpleAck(invokeId, SERVICE_CONFIRMED_WRITE_PROPERTY, remoteIP, remotePort);
}

BACnetObject* BACnetDriver::findObject(uint8_t objectType, uint32_t instance) {
    if (objectType == OBJECT_ANALOG_INPUT) {
        for (uint8_t i = 0; i < BACNET_MAX_AI_MAIN; i++) {
            if (_aiMain[i].instance == instance) return &_aiMain[i];
        }
        for (uint8_t i = 0; i < BACNET_MAX_AI_SENSORS; i++) {
            if (_aiSensors[i].instance == instance) return &_aiSensors[i];
        }
        return nullptr;
    }

    if (objectType == OBJECT_BINARY_INPUT) {
        if (instance < 1 || instance > BACNET_MAX_BI) return nullptr;
        return &_bi[instance - 1];
    }

    if (objectType == OBJECT_BINARY_OUTPUT) {
        if (instance < 1 || instance > BACNET_MAX_BO) return nullptr;
        return &_bo[instance - 1];
    }

    return nullptr;
}

// -------------------- I-Am --------------------
void BACnetDriver::sendIAmBroadcast() {
    // Broadcast to x.x.x.255
    IPAddress broadcastIP(_localIP[0], _localIP[1], _localIP[2], 255);
    sendIAmUnicast(broadcastIP, BACNET_UDP_PORT);
}

void BACnetDriver::sendIAmUnicast(IPAddress remoteIP, uint16_t remotePort) {
    uint16_t tx = 0;

    // BVLL
    _txBuffer[tx++] = BVLL_TYPE_BACNET_IP;
    _txBuffer[tx++] = (remoteIP[3] == 255) ? BVLL_FUNC_ORIGINAL_BROADCAST_NPDU : BVLL_FUNC_ORIGINAL_UNICAST_NPDU;
    _txBuffer[tx++] = 0x00;
    _txBuffer[tx++] = 0x00;

    // NPDU
    _txBuffer[tx++] = 0x01;
    _txBuffer[tx++] = 0x00;

    // APDU Unconfirmed I-Am
    _txBuffer[tx++] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST;
    _txBuffer[tx++] = SERVICE_UNCONFIRMED_I_AM;

    // I-Am payload:
    // Device Object ID (app tag 12, len4)
    _txBuffer[tx++] = 0xC4;
    tx += encodeObjectId(&_txBuffer[tx], OBJECT_DEVICE, _deviceID);

    // Max APDU accepted (unsigned)
    tx += encodeAppUnsigned(&_txBuffer[tx], BACNET_MAX_APDU);

    // Segmentation supported (enumerated)
    tx += encodeAppEnumerated(&_txBuffer[tx], 3 /*noSegmentation*/);

    // Vendor ID (unsigned)
    tx += encodeAppUnsigned(&_txBuffer[tx], 999);

    // BVLL length
    _txBuffer[2] = (uint8_t)((tx >> 8) & 0xFF);
    _txBuffer[3] = (uint8_t)(tx & 0xFF);

    _udp.beginPacket(remoteIP, remotePort);
    _udp.write(_txBuffer, tx);
    _udp.endPacket();
}

// -------------------- ACK/ERROR --------------------
void BACnetDriver::sendSimpleAck(uint8_t invokeId, uint8_t serviceChoice, IPAddress remoteIP, uint16_t remotePort) {
    uint16_t tx = 0;

    _txBuffer[tx++] = BVLL_TYPE_BACNET_IP;
    _txBuffer[tx++] = BVLL_FUNC_ORIGINAL_UNICAST_NPDU;
    _txBuffer[tx++] = 0x00;
    _txBuffer[tx++] = 0x00;

    _txBuffer[tx++] = 0x01;
    _txBuffer[tx++] = 0x00;

    _txBuffer[tx++] = PDU_TYPE_SIMPLE_ACK;
    _txBuffer[tx++] = invokeId;
    _txBuffer[tx++] = serviceChoice;

    _txBuffer[2] = (uint8_t)((tx >> 8) & 0xFF);
    _txBuffer[3] = (uint8_t)(tx & 0xFF);

    _udp.beginPacket(remoteIP, remotePort);
    _udp.write(_txBuffer, tx);
    _udp.endPacket();
}

void BACnetDriver::sendError(uint8_t invokeId, uint8_t serviceChoice, uint8_t errorClass, uint8_t errorCode, IPAddress remoteIP, uint16_t remotePort) {
    uint16_t tx = 0;

    _txBuffer[tx++] = BVLL_TYPE_BACNET_IP;
    _txBuffer[tx++] = BVLL_FUNC_ORIGINAL_UNICAST_NPDU;
    _txBuffer[tx++] = 0x00;
    _txBuffer[tx++] = 0x00;

    _txBuffer[tx++] = 0x01;
    _txBuffer[tx++] = 0x00;

    _txBuffer[tx++] = PDU_TYPE_ERROR;
    _txBuffer[tx++] = invokeId;
    _txBuffer[tx++] = serviceChoice;

    // Error class (context tag 0) + one byte
    _txBuffer[tx++] = 0x09; // tag 0 len 1 (context=1)
    _txBuffer[tx++] = errorClass;

    // Error code (context tag 1) + one byte
    _txBuffer[tx++] = 0x19; // tag 1 len 1 (context=1)
    _txBuffer[tx++] = errorCode;

    _txBuffer[2] = (uint8_t)((tx >> 8) & 0xFF);
    _txBuffer[3] = (uint8_t)(tx & 0xFF);

    _udp.beginPacket(remoteIP, remotePort);
    _udp.write(_txBuffer, tx);
    _udp.endPacket();
}

// -------------------- Encoding Helpers --------------------
uint16_t BACnetDriver::encodeObjectId(uint8_t* buffer, uint8_t objectType, uint32_t instance) {
    // 10 bits type, 22 bits instance
    uint32_t oid = (((uint32_t)objectType & 0x3FFUL) << 22) | (instance & 0x3FFFFFUL);
    buffer[0] = (uint8_t)((oid >> 24) & 0xFF);
    buffer[1] = (uint8_t)((oid >> 16) & 0xFF);
    buffer[2] = (uint8_t)((oid >> 8) & 0xFF);
    buffer[3] = (uint8_t)(oid & 0xFF);
    return 4;
}

uint16_t BACnetDriver::encodeAppEnumerated(uint8_t* buffer, uint32_t value) {
    // Application tag 9 (Enumerated)
    // Only encode up to 4 bytes
    uint8_t tmp[4];
    uint8_t len = 1;
    if (value <= 0xFF) len = 1;
    else if (value <= 0xFFFF) len = 2;
    else if (value <= 0xFFFFFF) len = 3;
    else len = 4;

    for (uint8_t i = 0; i < len; i++) {
        tmp[len - 1 - i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    }

    buffer[0] = (uint8_t)(0x90 | (len & 0x0F));
    memcpy(&buffer[1], tmp, len);
    return (uint16_t)(1 + len);
}

uint16_t BACnetDriver::encodeAppUnsigned(uint8_t* buffer, uint32_t value) {
    // Application tag 2 (Unsigned Integer)
    uint8_t tmp[4];
    uint8_t len = 1;
    if (value <= 0xFF) len = 1;
    else if (value <= 0xFFFF) len = 2;
    else if (value <= 0xFFFFFF) len = 3;
    else len = 4;

    for (uint8_t i = 0; i < len; i++) {
        tmp[len - 1 - i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    }

    buffer[0] = (uint8_t)(0x20 | (len & 0x0F));
    memcpy(&buffer[1], tmp, len);
    return (uint16_t)(1 + len);
}

uint16_t BACnetDriver::encodeAppBoolean(uint8_t* buffer, bool value) {
    // Application tag 1 (Boolean), len 1
    buffer[0] = 0x11;
    buffer[1] = value ? 1 : 0;
    return 2;
}

uint16_t BACnetDriver::encodeAppReal(uint8_t* buffer, float value) {
    // Application tag 4 (Real), len 4
    buffer[0] = 0x44;
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    buffer[1] = (uint8_t)((raw >> 24) & 0xFF);
    buffer[2] = (uint8_t)((raw >> 16) & 0xFF);
    buffer[3] = (uint8_t)((raw >> 8) & 0xFF);
    buffer[4] = (uint8_t)(raw & 0xFF);
    return 5;
}

uint16_t BACnetDriver::encodeAppCharacterString(uint8_t* buffer, const char* str) {
    if (!str) str = "";
    const uint8_t slen = (uint8_t)strlen(str);
    const uint16_t payloadLen = (uint16_t)(1 + slen); // charset + chars

    uint16_t idx = 0;
    if (payloadLen <= 4) {
        buffer[idx++] = (uint8_t)(0x70 | (payloadLen & 0x0F));
    } else {
        buffer[idx++] = 0x75; // tag 7, extended length
        buffer[idx++] = (uint8_t)payloadLen;
    }

    buffer[idx++] = 0x00; // ANSI X3.4
    memcpy(&buffer[idx], str, slen);
    idx += slen;
    return idx;
}

// -------------------- Decoding Helpers --------------------
bool BACnetDriver::decodeObjectId(uint8_t* buffer, uint16_t bufferLen, uint8_t& objectType, uint32_t& instance) {
    if (bufferLen < 4) return false;
    uint32_t oid = ((uint32_t)buffer[0] << 24) |
                   ((uint32_t)buffer[1] << 16) |
                   ((uint32_t)buffer[2] << 8) |
                   ((uint32_t)buffer[3]);

    objectType = (uint8_t)((oid >> 22) & 0x3FF);
    instance = oid & 0x3FFFFF;
    return true;
}

bool BACnetDriver::decodeContextUnsigned(uint8_t expectedTagNumber, uint8_t* buffer, uint16_t bufferLen, uint32_t& value, uint16_t& consumed) {
    if (bufferLen < 2) return false;

    uint8_t tag = buffer[0];
    uint8_t tagNum = (tag >> 4) & 0x0F;
    bool isContext = (tag & 0x08) != 0;
    uint8_t len = tag & 0x07;

    if (!isContext) return false;
    if (tagNum != expectedTagNumber) return false;

    // len up to 4 in our usage
    if (len == 0 || len > 4) return false;
    if (bufferLen < (uint16_t)(1 + len)) return false;

    value = 0;
    for (uint8_t i = 0; i < len; i++) {
        value = (value << 8) | buffer[1 + i];
    }
    consumed = (uint16_t)(1 + len);
    return true;
}

bool BACnetDriver::decodeAnyValueToBool(uint8_t* buffer, uint16_t bufferLen, bool& value, uint16_t& consumed) {
    if (bufferLen < 2) return false;

    const uint8_t tag = buffer[0];
    const uint8_t tagNum = (tag >> 4) & 0x0F;
    const bool isContext = (tag & 0x08) != 0;
    const uint8_t len = tag & 0x0F;

    (void)isContext;

    // Enumerated (tag 9)
    if (tagNum == 9) {
        uint8_t l = tag & 0x0F;
        if (l == 0x0F) return false;
        if (bufferLen < (uint16_t)(1 + l)) return false;
        uint32_t v = 0;
        for (uint8_t i = 0; i < l; i++) v = (v << 8) | buffer[1 + i];
        value = (v != 0);
        consumed = (uint16_t)(1 + l);
        return true;
    }

    // Boolean (tag 1)
    if (tagNum == 1) {
        uint8_t l = tag & 0x0F;
        if (l != 1) return false;
        value = (buffer[1] != 0);
        consumed = 2;
        return true;
    }

    // Unsigned (tag 2)
    if (tagNum == 2) {
        uint8_t l = tag & 0x0F;
        if (l < 1 || l > 4) return false;
        if (bufferLen < (uint16_t)(1 + l)) return false;
        uint32_t v = 0;
        for (uint8_t i = 0; i < l; i++) v = (v << 8) | buffer[1 + i];
        value = (v != 0);
        consumed = (uint16_t)(1 + l);
        return true;
    }

    return false;
}


bool BACnetDriver::decodeAnyValueToString(uint8_t* buffer, uint16_t bufferLen, String& value, uint16_t& consumed) {
    if (bufferLen < 2) return false;

    const uint8_t tag = buffer[0];
    const uint8_t tagNum = (tag >> 4) & 0x0F;
    const uint8_t lenNib = tag & 0x0F;

    if (tagNum != 7) return false; // Character String

    uint16_t offset = 1;
    uint16_t dataLen = 0;

    if (lenNib <= 4) {
        dataLen = lenNib;
    } else if (lenNib == 5) {
        if (bufferLen < 2) return false;
        dataLen = buffer[1];
        offset = 2;
    } else {
        return false;
    }

    if (bufferLen < (uint16_t)(offset + dataLen)) return false;
    if (dataLen < 1) return false; // must include encoding byte

    // first byte is encoding; ignore for now (assume ASCII/UTF-8)
    uint16_t strLen = dataLen - 1;
    value = "";
    if (strLen > 0) {
        value.reserve(strLen);
        for (uint16_t i = 0; i < strLen; i++) value += (char)buffer[offset + 1 + i];
    }

    consumed = offset + dataLen;
    return true;
}

