#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ETH.h>

// BACnet Configuration
#define BACNET_DEVICE_ID            88160    // KC868-A16 Device ID
#define BACNET_UDP_PORT            47808    // Standard BACnet/IP port
#define BACNET_MAX_APDU            480
#define BACNET_PROTOCOL_VERSION    1
#define BACNET_PROTOCOL_REVISION   22

// BACnet PDU Types
#define PDU_TYPE_CONFIRMED_SERVICE_REQUEST  0x00
#define PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST 0x10
#define PDU_TYPE_SIMPLE_ACK         0x20
#define PDU_TYPE_COMPLEX_ACK        0x30
#define PDU_TYPE_ERROR              0x50
#define PDU_TYPE_REJECT             0x60
#define PDU_TYPE_ABORT              0x70

// BACnet Service Choices
#define SERVICE_CONFIRMED_READ_PROPERTY  0x0C
#define SERVICE_CONFIRMED_WRITE_PROPERTY 0x0F
#define SERVICE_UNCONFIRMED_WHO_IS       0x08
#define SERVICE_UNCONFIRMED_I_AM         0x00

// BACnet Object Types
#define OBJECT_ANALOG_INPUT         0
#define OBJECT_ANALOG_OUTPUT        1
#define OBJECT_ANALOG_VALUE         2
#define OBJECT_BINARY_INPUT         3
#define OBJECT_BINARY_OUTPUT        4
#define OBJECT_BINARY_VALUE         5
#define OBJECT_DEVICE               8
#define OBJECT_MULTI_STATE_INPUT    13
#define OBJECT_MULTI_STATE_OUTPUT   14
#define OBJECT_MULTI_STATE_VALUE    19

// BACnet Property IDs
#define PROP_OBJECT_IDENTIFIER      75
#define PROP_OBJECT_NAME            77
#define PROP_OBJECT_TYPE            79
#define PROP_PRESENT_VALUE          85
#define PROP_DESCRIPTION            28
#define PROP_DEVICE_TYPE            31
#define PROP_STATUS_FLAGS           111
#define PROP_OUT_OF_SERVICE         81
#define PROP_UNITS                  117
#define PROP_MODEL_NAME             70
#define PROP_VENDOR_NAME            121
#define PROP_VENDOR_IDENTIFIER      120
#define PROP_FIRMWARE_REVISION      44
#define PROP_APPLICATION_SOFTWARE   12
#define PROP_PROTOCOL_VERSION       98
#define PROP_PROTOCOL_REVISION      139
#define PROP_SYSTEM_STATUS          112
#define PROP_LOCATION               58

// BACnet Engineering Units
#define UNITS_NO_UNITS              95
#define UNITS_PERCENT               98
#define UNITS_DEGREES_CELSIUS       62
#define UNITS_VOLTS                 5
#define UNITS_MILLIVOLTS            124

// Object counts
#define BACNET_MAX_AI              4       // Analog Inputs
#define BACNET_MAX_BI              19      // Binary Inputs (16 + 3 direct)
#define BACNET_MAX_BO              16      // Binary Outputs
#define BACNET_MAX_MSV             3       // Multi-State Values (sensors)

// Buffer sizes
#define BACNET_RX_BUFFER_SIZE      1500
#define BACNET_TX_BUFFER_SIZE      1500

// BACnet Object Instance Structure
struct BACnetObject {
    uint32_t instance;
    uint8_t type;
    char name[32];
    char description[64];
    float presentValue;
    uint8_t units;
    bool outOfService;
    uint32_t lastUpdate;
};

class BACnetDriver {
public:
    BACnetDriver();
    ~BACnetDriver();

    // Initialization
    bool begin(IPAddress localIP, IPAddress gateway, IPAddress subnet);
    void end();

    // Main processing
    void task();

    // Configuration
    void setDeviceID(uint32_t deviceID);
    void setDeviceName(const char* name);
    void setDescription(const char* desc);
    void setLocation(const char* loc);

    // Object value updates (from hardware to BACnet)
    void updateAnalogInput(uint8_t channel, float value);
    void updateBinaryInput(uint8_t channel, bool value);
    void updateBinaryOutput(uint8_t channel, bool value);
    void updateSensorValue(uint8_t sensorIndex, float value, const char* desc);

    // Object value reads (from BACnet to hardware)
    bool getBinaryOutputCommand(uint8_t channel, bool& value);

    // Status
    bool isRunning() const { return _initialized; }
    uint32_t getDeviceID() const { return _deviceID; }
    IPAddress getLocalIP() const { return _localIP; }
    uint16_t getPort() const { return BACNET_UDP_PORT; }

    // Statistics
    uint32_t getRequestCount() const { return _requestCount; }
    uint32_t getResponseCount() const { return _responseCount; }
    uint32_t getErrorCount() const { return _errorCount; }

private:
    bool _initialized;
    uint32_t _deviceID;
    char _deviceName[64];
    char _deviceDescription[128];
    char _deviceLocation[64];
    IPAddress _localIP;
    IPAddress _gateway;
    IPAddress _subnet;

    // UDP communication
    WiFiUDP _udp;

    // Statistics
    uint32_t _requestCount;
    uint32_t _responseCount;
    uint32_t _errorCount;
    uint32_t _lastIAmTime;

    // BACnet buffers
    uint8_t _rxBuffer[BACNET_RX_BUFFER_SIZE];
    uint8_t _txBuffer[BACNET_TX_BUFFER_SIZE];

    // Object storage
    BACnetObject _analogInputs[BACNET_MAX_AI];
    BACnetObject _binaryInputs[BACNET_MAX_BI];
    BACnetObject _binaryOutputs[BACNET_MAX_BO];
    BACnetObject _multiStateValues[BACNET_MAX_MSV];

    // Internal methods
    void initializeObjects();
    void processIncomingPacket();
    void sendIAm();
    void handleWhoIs(uint8_t* pdu, uint16_t pduLen);
    void handleReadProperty(uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort);
    void handleWriteProperty(uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort);

    // Encoding/Decoding helpers
    uint16_t encodeObjectId(uint8_t* buffer, uint8_t objectType, uint32_t instance);
    uint16_t encodeApplicationTag(uint8_t* buffer, uint8_t tag, const void* value, uint16_t valueLen);
    uint16_t encodeContextTag(uint8_t* buffer, uint8_t tagNumber, const void* value, uint16_t valueLen);
    uint16_t encodeUnsigned(uint8_t* buffer, uint32_t value);
    uint16_t encodeReal(uint8_t* buffer, float value);
    uint16_t encodeCharacterString(uint8_t* buffer, const char* str);
    uint16_t encodeEnumerated(uint8_t* buffer, uint32_t value);
    uint16_t encodeBoolean(uint8_t* buffer, bool value);

    bool decodeObjectId(uint8_t* buffer, uint16_t bufferLen, uint8_t& objectType, uint32_t& instance);
    bool decodeUnsigned(uint8_t* buffer, uint16_t bufferLen, uint32_t& value);
    bool decodeEnumerated(uint8_t* buffer, uint16_t bufferLen, uint32_t& value);
    bool decodeBoolean(uint8_t* buffer, uint16_t bufferLen, bool& value);
    bool decodeReal(uint8_t* buffer, uint16_t bufferLen, float& value);

    BACnetObject* findObject(uint8_t objectType, uint32_t instance);
    void sendResponse(uint8_t* response, uint16_t length, IPAddress remoteIP, uint16_t remotePort);
    void sendErrorResponse(uint8_t invokeId, uint8_t serviceChoice, uint8_t errorClass, uint8_t errorCode, IPAddress remoteIP, uint16_t remotePort);
};

// Global instance
extern BACnetDriver bacnetDriver;

#endif // BACNET_DRIVER_H