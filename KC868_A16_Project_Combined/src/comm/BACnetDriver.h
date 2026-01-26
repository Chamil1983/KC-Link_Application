#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ETH.h>
#include <IPAddress.h>

/*
 * KC868-A16 BACnet/IP Driver (Lightweight)
 * ------------------------------------------------------------
 * This driver implements a minimal BACnet/IP stack (BVLL + NPDU + APDU)
 * to support:
 *   - Device discovery (Who-Is / I-Am)
 *   - ReadProperty (BI/BO/AI/Device)
 *   - WriteProperty (BO Present_Value)
 *
 * Designed for System.IO.BACnet (.NET) client compatibility.
 * No changes required to existing MODBUS code paths.
 */

// --------------------------- Configuration ---------------------------
#define BACNET_DEVICE_ID                 88160     // Device instance
#define BACNET_UDP_PORT                  47808     // BAC0 (0xBAC0)
#define BACNET_MAX_APDU                  480

// Object counts
#define BACNET_MAX_AI_MAIN               4         // Analog Inputs A1..A4
#define BACNET_MAX_AI_SENSORS            5         // AI101..AI105 (Sensors)
#define BACNET_MAX_AI                    (BACNET_MAX_AI_MAIN + BACNET_MAX_AI_SENSORS)

#define BACNET_MAX_BI                    16        // Digital Inputs 1..16
#define BACNET_MAX_BO                    16        // MOSFET Outputs 1..16

// Buffer sizes
#define BACNET_RX_BUFFER_SIZE            1500
#define BACNET_TX_BUFFER_SIZE            1500

// --------------------------- BACnet/IP (BVLL) ------------------------
#define BVLL_TYPE_BACNET_IP              0x81
#define BVLL_FUNC_ORIGINAL_UNICAST_NPDU  0x0A
#define BVLL_FUNC_ORIGINAL_BROADCAST_NPDU 0x0B

// --------------------------- APDU Types ------------------------------
#define PDU_TYPE_CONFIRMED_SERVICE_REQUEST   0x00
#define PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST 0x10
#define PDU_TYPE_SIMPLE_ACK                  0x20
#define PDU_TYPE_COMPLEX_ACK                 0x30
#define PDU_TYPE_ERROR                       0x50

// --------------------------- Services --------------------------------
// Unconfirmed services
#define SERVICE_UNCONFIRMED_I_AM             0x00
#define SERVICE_UNCONFIRMED_WHO_IS           0x08

// Confirmed services
#define SERVICE_CONFIRMED_READ_PROPERTY      0x0C
#define SERVICE_CONFIRMED_WRITE_PROPERTY     0x0F

// --------------------------- Object Types ----------------------------
#define OBJECT_ANALOG_INPUT                  0
#define OBJECT_BINARY_INPUT                  3
#define OBJECT_BINARY_OUTPUT                 4
#define OBJECT_DEVICE                        8

// --------------------------- Property IDs ----------------------------
#define PROP_OBJECT_IDENTIFIER               75
#define PROP_OBJECT_NAME                     77
#define PROP_OBJECT_TYPE                     79
#define PROP_PRESENT_VALUE                   85
#define PROP_DESCRIPTION                     28
#define PROP_UNITS                           117
#define PROP_OUT_OF_SERVICE                  81
#define PROP_LOCATION                        58
#define PROP_MODEL_NAME                      70
#define PROP_VENDOR_NAME                     121
#define PROP_VENDOR_IDENTIFIER               120
#define PROP_FIRMWARE_REVISION               44
#define PROP_APPLICATION_SOFTWARE            12
#define PROP_PROTOCOL_VERSION                98
#define PROP_PROTOCOL_REVISION               139
#define PROP_SYSTEM_STATUS                   112
#define PROP_OBJECT_LIST                     76
#define PROP_MAX_APDU_LENGTH_ACCEPTED        62
#define PROP_SEGMENTATION_SUPPORTED          107

#define PROP_SERIAL_NUMBER                 372

// ---- Vendor properties (Microcode / KC868) ----
#define PROP_MESA_MAC_ADDRESS              512
#define PROP_MESA_HARDWARE_VER             513
#define PROP_MESA_YEAR_DEV                 514
#define PROP_MESA_SUBNET_MASK              515
#define PROP_MESA_GATEWAY                  516
#define PROP_MESA_DNS1                     517
#define PROP_MESA_AP_SSID                  518
#define PROP_MESA_AP_PASSWORD              519
#define PROP_MESA_AP_IP                    520
#define PROP_MESA_DEVICE_DATETIME           521


// --------------------------- Units (subset) --------------------------
#define UNITS_NO_UNITS                       95
#define UNITS_VOLTS                          5
#define UNITS_DEGREES_CELSIUS                62
#define UNITS_PERCENT                        98

// --------------------------- Small Object Structure ------------------
struct BACnetObject {
    uint32_t instance;
    uint8_t  type;
    char     name[32];
    char     description[64];
    float    presentValue;
    uint16_t units;          // enumerated units
    bool     outOfService;
    uint32_t lastUpdateMs;
};

// --------------------------- Driver ----------------------------------
class BACnetDriver {
public:
    BACnetDriver();
    ~BACnetDriver();

    bool begin(IPAddress localIP, IPAddress gateway, IPAddress subnet);
    void end();

    // Main processing (call frequently)
    void task();

    // Configuration (Device properties)
    void setDeviceID(uint32_t deviceID);
    void setDeviceName(const char* name);
    void setDescription(const char* desc);
    void setLocation(const char* loc);

    // ---- Updates from hardware to BACnet objects ----
    void updateAnalogInput(uint8_t channel, float volts);               // AI1..AI4
    void updateBinaryInput(uint8_t channel, bool active);               // BI1..BI16
    void updateBinaryOutput(uint8_t channel, bool active);              // BO1..BO16

    // Sensor Analog Inputs (AI101..AI105)
    void updateSensorAnalog(uint16_t instance, float value, uint16_t units, const char* desc);

    // ---- Commands from BACnet to hardware ----
    bool getBinaryOutputCommand(uint8_t channel, bool& active);         // returns true only when NEW command exists

    // Status
    bool isRunning() const { return _initialized; }
    uint32_t getDeviceID() const { return _deviceID; }

private:
    // Networking
    WiFiUDP  _udp;
    bool     _initialized;
    IPAddress _localIP;
    IPAddress _gateway;
    IPAddress _subnet;

    // Device properties
    uint32_t _deviceID;
    char     _deviceName[32];
    char     _deviceDescription[64];
    char     _deviceLocation[64];

    // Object database
    BACnetObject _aiMain[BACNET_MAX_AI_MAIN];
    BACnetObject _aiSensors[BACNET_MAX_AI_SENSORS];
    BACnetObject _bi[BACNET_MAX_BI];
    BACnetObject _bo[BACNET_MAX_BO];

    // Output commands (pending writes)
    bool _boCmdPending[BACNET_MAX_BO];
    bool _boCmdValue[BACNET_MAX_BO];

    // RX/TX buffers
    uint8_t  _rxBuffer[BACNET_RX_BUFFER_SIZE];
    uint8_t  _txBuffer[BACNET_TX_BUFFER_SIZE];

    // Stats / timing
    uint32_t _lastIAmMs;

private:
    // Packet processing
    void processIncomingPacket();
    void handleWhoIs(IPAddress remoteIP, uint16_t remotePort);
    void handleReadProperty(uint8_t invokeId, uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort);
    void handleWriteProperty(uint8_t invokeId, uint8_t* pdu, uint16_t pduLen, IPAddress remoteIP, uint16_t remotePort);

    // I-Am
    void sendIAmBroadcast();
    void sendIAmUnicast(IPAddress remoteIP, uint16_t remotePort);

    // Object lookup
    BACnetObject* findObject(uint8_t objectType, uint32_t instance);

    // Response helpers
    void sendSimpleAck(uint8_t invokeId, uint8_t serviceChoice, IPAddress remoteIP, uint16_t remotePort);
    void sendError(uint8_t invokeId, uint8_t serviceChoice, uint8_t errorClass, uint8_t errorCode, IPAddress remoteIP, uint16_t remotePort);

    // Encoding helpers
    uint16_t encodeObjectId(uint8_t* buffer, uint8_t objectType, uint32_t instance);
    uint16_t encodeAppEnumerated(uint8_t* buffer, uint32_t value);
    uint16_t encodeAppUnsigned(uint8_t* buffer, uint32_t value);
    uint16_t encodeAppBoolean(uint8_t* buffer, bool value);
    uint16_t encodeAppReal(uint8_t* buffer, float value);
    uint16_t encodeAppCharacterString(uint8_t* buffer, const char* str);

    // Decoding helpers (very small subset)
    bool decodeObjectId(uint8_t* buffer, uint16_t bufferLen, uint8_t& objectType, uint32_t& instance);
    bool decodeContextUnsigned(uint8_t expectedTagNumber, uint8_t* buffer, uint16_t bufferLen, uint32_t& value, uint16_t& consumed);
    bool decodeAnyValueToBool(uint8_t* buffer, uint16_t bufferLen, bool& value, uint16_t& consumed);
    bool decodeAnyValueToString(uint8_t* buffer, uint16_t bufferLen, String& value, uint16_t& consumed);
};

// Global instance
extern BACnetDriver bacnetDriver;

#endif // BACNET_DRIVER_H
