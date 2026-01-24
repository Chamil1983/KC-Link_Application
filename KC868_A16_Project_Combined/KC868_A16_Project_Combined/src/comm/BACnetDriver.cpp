#include "BACnetDriver.h"
#include "../Globals.h"
#include "../Definitions.h"
#include <WiFiUdp.h>
// Global instance
BACnetDriver bacnetDriver;

BACnetDriver::BACnetDriver()
    : _initialized(false),
    _deviceID(BACNET_DEVICE_ID),
    _requestCount(0),
    _responseCount(0),
    _errorCount(0),
    _lastIAmTime(0) {

    strcpy(_deviceName, "KC868-A16");
    strcpy(_deviceDescription, "KC868-A16 Smart Controller");
    strcpy(_deviceLocation, "Building Automation");
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
    _subnet = subnet;

    Serial.println(F("[BACnet] Initializing BACnet/IP stack..."));

    // Initialize UDP socket
    if (!_udp.begin(BACNET_UDP_PORT)) {
        Serial.println(F("[BACnet] Failed to start UDP socket"));
        return false;
    }

    // Initialize objects
    initializeObjects();

    // Send initial I-Am broadcast
    sendIAm();

    _initialized = true;
    _requestCount = 0;
    _responseCount = 0;
    _errorCount = 0;
    _lastIAmTime = millis();

    Serial.printf("[BACnet] Initialized. Device ID: %u, IP: %s, Port: %u\n",
        _deviceID, _localIP.toString().c_str(), BACNET_UDP_PORT);

    return true;
}

void BACnetDriver::end() {
    if (!_initialized) {
        return;
    }

    _udp.stop();
    _initialized = false;

    Serial.println(F("[BACnet] Stopped"));
}

void BACnetDriver::task() {
    if (!_initialized) {
        return;
    }

    // Process incoming packets
    processIncomingPacket();

    // Periodic I-Am broadcast (every 30 seconds)
    uint32_t currentTime = millis();
    if (currentTime - _lastIAmTime > 30000) {
        sendIAm();
        _lastIAmTime = currentTime;
    }
}

void BACnetDriver::initializeObjects() {
    Serial.println(F("[BACnet] Creating objects..."));

    // Initialize Analog Inputs (4 channels)
    const char* aiNames[] = {
        "Analog Input 1 (0-5V)",
        "Analog Input 2 (0-5V)",
        "Analog Input 3 (0-5V)",
        "Analog Input 4 (0-5V)"
    };

    for (uint8_t i = 0; i < BACNET_MAX_AI; i++) {
        _analogInputs[i].instance = i + 1;
        _analogInputs[i].type = OBJECT_ANALOG_INPUT;
        strncpy(_analogInputs[i].name, aiNames[i], sizeof(_analogInputs[i].name) - 1);
        strncpy(_analogInputs[i].description, "0-5000 mV range", sizeof(_analogInputs[i].description) - 1);
        _analogInputs[i].units = UNITS_MILLIVOLTS;
        _analogInputs[i].presentValue = 0.0f;
        _analogInputs[i].outOfService = false;
        _analogInputs[i].lastUpdate = 0;
    }

    // Initialize Binary Inputs (16 main + 3 direct)
    for (uint8_t i = 0; i < 16; i++) {
        _binaryInputs[i].instance = i + 1;
        _binaryInputs[i].type = OBJECT_BINARY_INPUT;
        snprintf(_binaryInputs[i].name, sizeof(_binaryInputs[i].name), "Digital Input %d", i + 1);
        strncpy(_binaryInputs[i].description, "Main digital input", sizeof(_binaryInputs[i].description) - 1);
        _binaryInputs[i].units = UNITS_NO_UNITS;
        _binaryInputs[i].presentValue = 0.0f;
        _binaryInputs[i].outOfService = false;
    }

    // Direct inputs (HT1, HT2, HT3)
    const char* directNames[] = { "Direct Input HT1", "Direct Input HT2", "Direct Input HT3" };
    for (uint8_t i = 0; i < 3; i++) {
        _binaryInputs[16 + i].instance = 17 + i;
        _binaryInputs[16 + i].type = OBJECT_BINARY_INPUT;
        strncpy(_binaryInputs[16 + i].name, directNames[i], sizeof(_binaryInputs[16 + i].name) - 1);
        strncpy(_binaryInputs[16 + i].description, "Direct GPIO input", sizeof(_binaryInputs[16 + i].description) - 1);
        _binaryInputs[16 + i].units = UNITS_NO_UNITS;
        _binaryInputs[16 + i].presentValue = 0.0f;
        _binaryInputs[16 + i].outOfService = false;
    }

    // Initialize Binary Outputs (16 MOSFET outputs)
    for (uint8_t i = 0; i < BACNET_MAX_BO; i++) {
        _binaryOutputs[i].instance = i + 1;
        _binaryOutputs[i].type = OBJECT_BINARY_OUTPUT;
        snprintf(_binaryOutputs[i].name, sizeof(_binaryOutputs[i].name), "MOSFET Output %d", i + 1);
        strncpy(_binaryOutputs[i].description, "Digital MOSFET output", sizeof(_binaryOutputs[i].description) - 1);
        _binaryOutputs[i].units = UNITS_NO_UNITS;
        _binaryOutputs[i].presentValue = 0.0f;
        _binaryOutputs[i].outOfService = false;
    }

    // Initialize Multi-State Values (sensors)
    const char* sensorNames[] = {
        "DHT1 (Temp/Humidity)",
        "DHT2 (Temp/Humidity)",
        "DS18B20 (Temperature)"
    };

    for (uint8_t i = 0; i < BACNET_MAX_MSV; i++) {
        _multiStateValues[i].instance = i + 1;
        _multiStateValues[i].type = OBJECT_MULTI_STATE_VALUE;
        strncpy(_multiStateValues[i].name, sensorNames[i], sizeof(_multiStateValues[i].name) - 1);
        strncpy(_multiStateValues[i].description, "Environmental sensor", sizeof(_multiStateValues[i].description) - 1);
        _multiStateValues[i].units = UNITS_DEGREES_CELSIUS;
        _multiStateValues[i].presentValue = 0.0f;
        _multiStateValues[i].outOfService = false;
    }

    Serial.printf("[BACnet] Created %d AI, %d BI, %d BO, %d MSV objects\n",
        BACNET_MAX_AI, BACNET_MAX_BI, BACNET_MAX_BO, BACNET_MAX_MSV);
}

void BACnetDriver::processIncomingPacket() {
    int packetSize = _udp.parsePacket();
    if (packetSize <= 0) {
        return;
    }

    _requestCount++;

    // Read packet
    IPAddress remoteIP = _udp.remoteIP();
    uint16_t remotePort = _udp.remotePort();

    int len = _udp.read(_rxBuffer, BACNET_RX_BUFFER_SIZE);
    if (len <= 0) {
        return;
    }

    // Parse BVLL (BACnet Virtual Link Layer) header
    // BVLL Type: 0x81 (BACnet/IP)
    if (_rxBuffer[0] != 0x81) {
        _errorCount++;
        return;
    }

    uint8_t bvllFunction = _rxBuffer[1];
    uint16_t bvllLength = (_rxBuffer[2] << 8) | _rxBuffer[3];

    // BVLL functions:
    // 0x0A = Original-Unicast-NPDU
    // 0x0B = Original-Broadcast-NPDU
    if (bvllFunction != 0x0A && bvllFunction != 0x0B) {
        return; // Ignore other BVLL functions for now
    }

    // Parse NPDU (Network Protocol Data Unit) header
    uint8_t* npdu = &_rxBuffer[4];
    uint16_t npduLen = len - 4;

    if (npduLen < 2) {
        _errorCount++;
        return;
    }

    uint8_t npduVersion = npdu[0];
    uint8_t npduControl = npdu[1];

    if (npduVersion != BACNET_PROTOCOL_VERSION) {
        _errorCount++;
        return;
    }

    // Skip NPDU header (simple case: no destination, no source, no hop count)
    uint8_t* apdu = &npdu[2];
    uint16_t apduLen = npduLen - 2;

    if (apduLen < 1) {
        _errorCount++;
        return;
    }

    // Parse APDU (Application Protocol Data Unit)
    uint8_t apduType = apdu[0] & 0xF0;

    switch (apduType) {
    case PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST:
    {
        uint8_t serviceChoice = apdu[1];
        if (serviceChoice == SERVICE_UNCONFIRMED_WHO_IS) {
            handleWhoIs(&apdu[2], apduLen - 2);
        }
    }
    break;

    case PDU_TYPE_CONFIRMED_SERVICE_REQUEST:
    {
        uint8_t segmentation = apdu[0];
        uint8_t invokeId = apdu[1];
        uint8_t serviceChoice = apdu[3];

        if (serviceChoice == SERVICE_CONFIRMED_READ_PROPERTY) {
            handleReadProperty(&apdu[4], apduLen - 4, remoteIP, remotePort);
        }
        else if (serviceChoice == SERVICE_CONFIRMED_WRITE_PROPERTY) {
            handleWriteProperty(&apdu[4], apduLen - 4, remoteIP, remotePort);
        }
    }
    break;

    default:
        // Unsupported PDU type
        _errorCount++;
        break;
    }
}

void BACnetDriver::sendIAm() {
    // Build I-Am message
    uint16_t offset = 0;

    // BVLL Header
    _txBuffer[offset++] = 0x81;  // BACnet/IP
    _txBuffer[offset++] = 0x0B;  // Original-Broadcast-NPDU
    _txBuffer[offset++] = 0x00;  // Length (high byte) - filled later
    _txBuffer[offset++] = 0x00;  // Length (low byte) - filled later

    // NPDU Header
    _txBuffer[offset++] = BACNET_PROTOCOL_VERSION;
    _txBuffer[offset++] = 0x20;  // Control: no destination, priority normal

    // APDU - Unconfirmed Service Request (I-Am)
    _txBuffer[offset++] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST;
    _txBuffer[offset++] = SERVICE_UNCONFIRMED_I_AM;

    // I-Am Device Instance (context tag 0, object identifier)
    offset += encodeContextTag(&_txBuffer[offset], 0, NULL, 4);
    offset += encodeObjectId(&_txBuffer[offset], OBJECT_DEVICE, _deviceID);

    // Max APDU Length Accepted (context tag 1, unsigned)
    _txBuffer[offset++] = 0x19;  // Context tag 1
    offset += encodeUnsigned(&_txBuffer[offset], BACNET_MAX_APDU);

    // Segmentation Supported (context tag 2, enumerated)
    _txBuffer[offset++] = 0x29;  // Context tag 2
    _txBuffer[offset++] = 0x03;  // No segmentation

    // Vendor ID (context tag 3, unsigned)
    _txBuffer[offset++] = 0x39;  // Context tag 3
    offset += encodeUnsigned(&_txBuffer[offset], 888);  // Custom vendor ID

    // Fill in BVLL length
    _txBuffer[2] = (offset >> 8) & 0xFF;
    _txBuffer[3] = offset & 0xFF;

    // Send broadcast
    IPAddress broadcastIP(_localIP[0], _localIP[1], _localIP[2], 255);
    _udp.beginPacket(broadcastIP, BACNET_UDP_PORT);
    _udp.write(_txBuffer, offset);
    _udp.endPacket();

    Serial.printf("[BACnet] Sent I-Am broadcast (Device ID: %u)\n", _deviceID);
}

void BACnetDriver::handleWhoIs(uint8_t* pdu, uint16_t pduLen) {
    // WHO-IS can optionally specify device instance range
    // For simplicity, we always respond with I-Am
    Serial.println(F("[BACnet] Received Who-Is, sending I-Am"));
    sendIAm();
}

void BACnetDriver::handleReadProperty(uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort) {
    if (pduLen < 10) {
        _errorCount++;
        return;
    }

    uint16_t offset = 0;

    // Decode Object ID (context tag 0)
    if (pdu[offset] != 0x0C) {  // Context tag 0, object identifier
        _errorCount++;
        return;
    }
    offset++;

    uint8_t objectType = 0;
    uint32_t instance = 0;
    if (!decodeObjectId(&pdu[offset], pduLen - offset, objectType, instance)) {
        _errorCount++;
        return;
    }
    offset += 4;

    // Decode Property ID (context tag 1)
    if (pdu[offset] != 0x19) {  // Context tag 1, enumerated
        _errorCount++;
        return;
    }
    offset++;

    uint32_t propertyId = 0;
    if (!decodeEnumerated(&pdu[offset], pduLen - offset, propertyId)) {
        _errorCount++;
        return;
    }

    Serial.printf("[BACnet] Read Property: Object=%d:%u, Property=%u\n", objectType, instance, propertyId);

    // Find object
    BACnetObject* obj = findObject(objectType, instance);

    // Build response (simplified - complete implementation would handle all properties)
    uint16_t txOffset = 0;

    // BVLL Header
    _txBuffer[txOffset++] = 0x81;
    _txBuffer[txOffset++] = 0x0A;  // Original-Unicast-NPDU
    _txBuffer[txOffset++] = 0x00;  // Length - filled later
    _txBuffer[txOffset++] = 0x00;

    // NPDU
    _txBuffer[txOffset++] = BACNET_PROTOCOL_VERSION;
    _txBuffer[txOffset++] = 0x00;  // No network layer message

    // APDU - Complex ACK
    _txBuffer[txOffset++] = PDU_TYPE_COMPLEX_ACK;
    _txBuffer[txOffset++] = 0x00;  // Invoke ID (should match request)
    _txBuffer[txOffset++] = SERVICE_CONFIRMED_READ_PROPERTY;

    // Object ID (context tag 0)
    _txBuffer[txOffset++] = 0x0C;
    txOffset += encodeObjectId(&_txBuffer[txOffset], objectType, instance);

    // Property ID (context tag 1)
    _txBuffer[txOffset++] = 0x19;
    txOffset += encodeEnumerated(&_txBuffer[txOffset], propertyId);

    // Property Value (opening tag 3)
    _txBuffer[txOffset++] = 0x3E;

    // Encode property value based on property ID
    if (obj) {
        if (propertyId == PROP_PRESENT_VALUE) {
            if (objectType == OBJECT_ANALOG_INPUT || objectType == OBJECT_ANALOG_VALUE) {
                _txBuffer[txOffset++] = 0x44;  // Application tag: REAL
                txOffset += encodeReal(&_txBuffer[txOffset], obj->presentValue);
            }
            else if (objectType == OBJECT_BINARY_INPUT || objectType == OBJECT_BINARY_OUTPUT) {
                _txBuffer[txOffset++] = 0x91;  // Application tag: ENUMERATED
                _txBuffer[txOffset++] = (obj->presentValue > 0.5f) ? 0x01 : 0x00;
            }
        }
        else if (propertyId == PROP_OBJECT_NAME) {
            _txBuffer[txOffset++] = 0x75;  // Application tag: CHARACTER STRING
            txOffset += encodeCharacterString(&_txBuffer[txOffset], obj->name);
        }
        else if (propertyId == PROP_DESCRIPTION) {
            _txBuffer[txOffset++] = 0x75;
            txOffset += encodeCharacterString(&_txBuffer[txOffset], obj->description);
        }
        else if (propertyId == PROP_UNITS && objectType == OBJECT_ANALOG_INPUT) {
            _txBuffer[txOffset++] = 0x91;  // ENUMERATED
            _txBuffer[txOffset++] = obj->units;
        }
    }
    else if (objectType == OBJECT_DEVICE && instance == _deviceID) {
        // Handle device properties
        if (propertyId == PROP_OBJECT_NAME) {
            _txBuffer[txOffset++] = 0x75;
            txOffset += encodeCharacterString(&_txBuffer[txOffset], _deviceName);
        }
        else if (propertyId == PROP_MODEL_NAME) {
            _txBuffer[txOffset++] = 0x75;
            txOffset += encodeCharacterString(&_txBuffer[txOffset], "KC868-A16");
        }
        else if (propertyId == PROP_VENDOR_NAME) {
            _txBuffer[txOffset++] = 0x75;
            txOffset += encodeCharacterString(&_txBuffer[txOffset], "Microcode Engineering");
        }
        else if (propertyId == PROP_FIRMWARE_REVISION) {
            _txBuffer[txOffset++] = 0x75;
            txOffset += encodeCharacterString(&_txBuffer[txOffset], "1.0.0");
        }
    }

    // Property Value (closing tag 3)
    _txBuffer[txOffset++] = 0x3F;

    // Fill BVLL length
    _txBuffer[2] = (txOffset >> 8) & 0xFF;
    _txBuffer[3] = txOffset & 0xFF;

    sendResponse(_txBuffer, txOffset, remoteIP, remotePort);
    _responseCount++;
}

void BACnetDriver::handleWriteProperty(uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort) {
    // Simplified Write Property handler for Binary Outputs
    uint16_t offset = 0;

    // Decode Object ID (context tag 0)
    if (pdu[offset] != 0x0C) {
        _errorCount++;
        return;
    }
    offset++;

    uint8_t objectType = 0;
    uint32_t instance = 0;
    if (!decodeObjectId(&pdu[offset], pduLen - offset, objectType, instance)) {
        _errorCount++;
        return;
    }
    offset += 4;

    // Only handle Binary Output writes
    if (objectType != OBJECT_BINARY_OUTPUT) {
        sendErrorResponse(0, SERVICE_CONFIRMED_WRITE_PROPERTY, 2, 9, remoteIP, remotePort);
        return;
    }

    BACnetObject* obj = findObject(objectType, instance);
    if (!obj) {
        sendErrorResponse(0, SERVICE_CONFIRMED_WRITE_PROPERTY, 1, 31, remoteIP, remotePort);
        return;
    }

    // Skip property ID and array index
    // Parse property value (simplified)
    // Look for enumerated value in opening/closing tag 3
    bool foundValue = false;
    for (uint16_t i = offset; i < pduLen - 1; i++) {
        if (pdu[i] == 0x91) {  // ENUMERATED tag
            bool newValue = (pdu[i + 1] != 0);
            obj->presentValue = newValue ? 1.0f : 0.0f;
            obj->lastUpdate = millis();
            foundValue = true;
            Serial.printf("[BACnet] Write BO%u = %d\n", instance, newValue);
            break;
        }
    }

    if (!foundValue) {
        sendErrorResponse(0, SERVICE_CONFIRMED_WRITE_PROPERTY, 2, 2, remoteIP, remotePort);
        return;
    }

    // Send Simple ACK
    uint16_t txOffset = 0;
    _txBuffer[txOffset++] = 0x81;
    _txBuffer[txOffset++] = 0x0A;
    _txBuffer[txOffset++] = 0x00;
    _txBuffer[txOffset++] = 0x08;  // Length = 8
    _txBuffer[txOffset++] = BACNET_PROTOCOL_VERSION;
    _txBuffer[txOffset++] = 0x00;
    _txBuffer[txOffset++] = PDU_TYPE_SIMPLE_ACK;
    _txBuffer[txOffset++] = 0x00;  // Invoke ID
    _txBuffer[txOffset++] = SERVICE_CONFIRMED_WRITE_PROPERTY;

    sendResponse(_txBuffer, txOffset, remoteIP, remotePort);
    _responseCount++;
}

BACnetObject* BACnetDriver::findObject(uint8_t objectType, uint32_t instance) {
    switch (objectType) {
    case OBJECT_ANALOG_INPUT:
        for (uint8_t i = 0; i < BACNET_MAX_AI; i++) {
            if (_analogInputs[i].instance == instance) {
                return &_analogInputs[i];
            }
        }
        break;

    case OBJECT_BINARY_INPUT:
        for (uint8_t i = 0; i < BACNET_MAX_BI; i++) {
            if (_binaryInputs[i].instance == instance) {
                return &_binaryInputs[i];
            }
        }
        break;

    case OBJECT_BINARY_OUTPUT:
        for (uint8_t i = 0; i < BACNET_MAX_BO; i++) {
            if (_binaryOutputs[i].instance == instance) {
                return &_binaryOutputs[i];
            }
        }
        break;

    case OBJECT_MULTI_STATE_VALUE:
        for (uint8_t i = 0; i < BACNET_MAX_MSV; i++) {
            if (_multiStateValues[i].instance == instance) {
                return &_multiStateValues[i];
            }
        }
        break;
    }

    return nullptr;
}

void BACnetDriver::sendResponse(uint8_t* response, uint16_t length, IPAddress remoteIP, uint16_t remotePort) {
    _udp.beginPacket(remoteIP, remotePort);
    _udp.write(response, length);
    _udp.endPacket();
}

void BACnetDriver::sendErrorResponse(uint8_t invokeId, uint8_t serviceChoice, uint8_t errorClass, uint8_t errorCode, IPAddress remoteIP, uint16_t remotePort) {
    uint16_t offset = 0;

    _txBuffer[offset++] = 0x81;
    _txBuffer[offset++] = 0x0A;
    _txBuffer[offset++] = 0x00;
    _txBuffer[offset++] = 0x0B;  // Length = 11
    _txBuffer[offset++] = BACNET_PROTOCOL_VERSION;
    _txBuffer[offset++] = 0x00;
    _txBuffer[offset++] = PDU_TYPE_ERROR;
    _txBuffer[offset++] = invokeId;
    _txBuffer[offset++] = serviceChoice;
    _txBuffer[offset++] = 0x91;  // Error class
    _txBuffer[offset++] = errorClass;
    _txBuffer[offset++] = 0x91;  // Error code
    _txBuffer[offset++] = errorCode;

    sendResponse(_txBuffer, offset, remoteIP, remotePort);
    _errorCount++;
}

// Encoding helpers
uint16_t BACnetDriver::encodeObjectId(uint8_t* buffer, uint8_t objectType, uint32_t instance) {
    uint32_t value = ((uint32_t)objectType << 22) | (instance & 0x3FFFFF);
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
    return 4;
}

uint16_t BACnetDriver::encodeUnsigned(uint8_t* buffer, uint32_t value) {
    if (value < 256) {
        buffer[0] = value;
        return 1;
    }
    else if (value < 65536) {
        buffer[0] = (value >> 8) & 0xFF;
        buffer[1] = value & 0xFF;
        return 2;
    }
    else {
        buffer[0] = (value >> 24) & 0xFF;
        buffer[1] = (value >> 16) & 0xFF;
        buffer[2] = (value >> 8) & 0xFF;
        buffer[3] = value & 0xFF;
        return 4;
    }
}

uint16_t BACnetDriver::encodeReal(uint8_t* buffer, float value) {
    union {
        float f;
        uint32_t u;
    } data;
    data.f = value;

    buffer[0] = (data.u >> 24) & 0xFF;
    buffer[1] = (data.u >> 16) & 0xFF;
    buffer[2] = (data.u >> 8) & 0xFF;
    buffer[3] = data.u & 0xFF;
    return 4;
}

uint16_t BACnetDriver::encodeCharacterString(uint8_t* buffer, const char* str) {
    uint8_t len = strlen(str);
    buffer[0] = 0;  // Character set: ANSI X3.4
    buffer[1] = len;
    memcpy(&buffer[2], str, len);
    return len + 2;
}

uint16_t BACnetDriver::encodeEnumerated(uint8_t* buffer, uint32_t value) {
    return encodeUnsigned(buffer, value);
}

uint16_t BACnetDriver::encodeContextTag(uint8_t* buffer, uint8_t tagNumber, const void* value, uint16_t valueLen) {
    buffer[0] = (tagNumber << 4) | 0x08 | (valueLen & 0x07);
    return 1;
}

// Decoding helpers
bool BACnetDriver::decodeObjectId(uint8_t* buffer, uint16_t bufferLen, uint8_t& objectType, uint32_t& instance) {
    if (bufferLen < 4) return false;

    uint32_t value = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) | ((uint32_t)buffer[2] << 8) | buffer[3];
    objectType = (value >> 22) & 0x3FF;
    instance = value & 0x3FFFFF;
    return true;
}

bool BACnetDriver::decodeUnsigned(uint8_t* buffer, uint16_t bufferLen, uint32_t& value) {
    if (bufferLen < 1) return false;

    if (bufferLen == 1) {
        value = buffer[0];
    }
    else if (bufferLen == 2) {
        value = ((uint32_t)buffer[0] << 8) | buffer[1];
    }
    else if (bufferLen >= 4) {
        value = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) | ((uint32_t)buffer[2] << 8) | buffer[3];
    }
    return true;
}

bool BACnetDriver::decodeEnumerated(uint8_t* buffer, uint16_t bufferLen, uint32_t& value) {
    return decodeUnsigned(buffer, bufferLen, value);
}

bool BACnetDriver::decodeReal(uint8_t* buffer, uint16_t bufferLen, float& value) {
    if (bufferLen < 4) return false;

    union {
        float f;
        uint32_t u;
    } data;

    data.u = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) | ((uint32_t)buffer[2] << 8) | buffer[3];
    value = data.f;
    return true;
}

// Public API methods
void BACnetDriver::setDeviceID(uint32_t deviceID) {
    _deviceID = deviceID;
}

void BACnetDriver::setDeviceName(const char* name) {
    if (name) {
        strncpy(_deviceName, name, sizeof(_deviceName) - 1);
        _deviceName[sizeof(_deviceName) - 1] = '\0';
    }
}

void BACnetDriver::setDescription(const char* desc) {
    if (desc) {
        strncpy(_deviceDescription, desc, sizeof(_deviceDescription) - 1);
        _deviceDescription[sizeof(_deviceDescription) - 1] = '\0';
    }
}

void BACnetDriver::setLocation(const char* loc) {
    if (loc) {
        strncpy(_deviceLocation, loc, sizeof(_deviceLocation) - 1);
        _deviceLocation[sizeof(_deviceLocation) - 1] = '\0';
    }
}

void BACnetDriver::updateAnalogInput(uint8_t channel, float value) {
    if (channel < BACNET_MAX_AI) {
        _analogInputs[channel].presentValue = value;
        _analogInputs[channel].lastUpdate = millis();
    }
}

void BACnetDriver::updateBinaryInput(uint8_t channel, bool value) {
    if (channel < BACNET_MAX_BI) {
        _binaryInputs[channel].presentValue = value ? 1.0f : 0.0f;
        _binaryInputs[channel].lastUpdate = millis();
    }
}

void BACnetDriver::updateBinaryOutput(uint8_t channel, bool value) {
    if (channel < BACNET_MAX_BO) {
        _binaryOutputs[channel].presentValue = value ? 1.0f : 0.0f;
        _binaryOutputs[channel].lastUpdate = millis();
    }
}

void BACnetDriver::updateSensorValue(uint8_t sensorIndex, float value, const char* desc) {
    if (sensorIndex < BACNET_MAX_MSV) {
        _multiStateValues[sensorIndex].presentValue = value;
        _multiStateValues[sensorIndex].lastUpdate = millis();
        if (desc) {
            strncpy(_multiStateValues[sensorIndex].description, desc, sizeof(_multiStateValues[sensorIndex].description) - 1);
        }
    }
}

bool BACnetDriver::getBinaryOutputCommand(uint8_t channel, bool& value) {
    if (channel < BACNET_MAX_BO) {
        value = (_binaryOutputs[channel].presentValue > 0.5f);
        return true;
    }
    return false;
}