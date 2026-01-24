#pragma once
/**
 * Globals.h
 * Extern declarations for global state and shared objects (kept to preserve original behavior).
 * This is a transitional step toward fully encapsulated SystemState/DeviceConfig.
 */

#include "Types.h"

// Default WiFi credentials (can be changed via web interface)
extern const char* default_ssid;
extern const char* default_password;
extern const char* ap_ssid;
extern const char* ap_password;

// I2C expanders
extern PCF8574 inputIC1;
extern PCF8574 inputIC2;
extern PCF8574 outputIC3;
extern PCF8574 outputIC4;

// DNS server
extern DNSServer dnsServer;

// Web server + WebSocket server
extern WebServer server;
extern WebSocketsServer webSocket;
extern bool webSocketClients[WEBSOCKETS_SERVER_CLIENT_MAX];

// Web UI ports (Option A: HTTP + WebSocket)
extern int httpPort;
extern int wsPort;

// RS485 serial
extern HardwareSerial rs485;

// RF receiver/transmitter
extern RCSwitch rfReceiver;
extern RCSwitch rfTransmitter;

// RTC object
extern RTC_DS3231 rtc;

// Sensor objects (created on-demand)
extern DHT* dhtSensors[3];
extern OneWire* oneWireBuses[3];
extern DallasTemperature* ds18b20Sensors[3];

// Firmware version
extern const String firmwareVersion;
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION firmwareVersion
#endif

// System state variables
extern bool outputStates[16];
extern bool inputStates[16];
extern bool directInputStates[3];
extern int analogValues[4];
extern float analogVoltages[4];
extern bool ethConnected;
extern bool wifiConnected;
extern String deviceName;
extern bool debugMode;
extern unsigned long lastTimeCheck;
extern bool rtcInitialized;
extern String currentCommunicationProtocol;

// Connection mode
extern bool apMode;
extern bool wifiClientMode;
extern bool wiredMode;

// Interrupt handling state
extern InterruptConfig interruptConfigs[16];
extern volatile bool inputStateChanged[16];
extern portMUX_TYPE inputMux;
extern bool inputInterruptsEnabled;
extern unsigned long lastInputReadTime;
extern const unsigned long INPUT_READ_INTERVAL;

// Network configuration
extern IPAddress ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress dns1;
extern IPAddress dns2;
extern String mac;
extern String wifiSSID;
extern String wifiPassword;

// WiFi (STA) IP configuration (separate from Ethernet)
extern bool wifiDhcpMode;
extern IPAddress wifiStaIp;
extern IPAddress wifiStaGateway;
extern IPAddress wifiStaSubnet;
extern IPAddress wifiStaDns1;
extern IPAddress wifiStaDns2;

// WiFi advanced configuration
extern String wifiSecurity;
extern bool wifiHidden;
extern String wifiMacFilter;
extern bool wifiAutoUpdate;
extern String wifiRadioMode;
extern int wifiChannel;
extern int wifiChannelWidth;
extern unsigned long wifiDhcpLeaseTime;
extern bool wifiWmmEnabled;

// USB configuration
extern int usbComPort;
extern int usbBaudRate;
extern int usbDataBits;
extern int usbParity;
extern int usbStopBits;

// RS485 configuration
extern int rs485BaudRate;
extern int rs485Parity;
extern int rs485DataBits;
extern int rs485StopBits;
extern String rs485Protocol;
extern String rs485Mode;
extern int rs485DeviceAddress;
extern bool rs485FlowControl;
extern bool rs485NightMode;



// MODBUS / RS485 extended configuration (used by Modbus RTU map)
extern bool outputsMasterEnable;           // Coil 00019
extern bool modbusRtuActive;               // DI 10024 and IR30043 bit5
extern uint16_t rs485FrameGapMs;           // HR40207
extern uint16_t rs485TimeoutMs;            // HR40208

// Modbus identity/config strings (HR40011..)
extern String boardName;                   // HR40011..40026
extern String serialNumber;                // HR40027..40042

// Optional hardware version string (HR40067..40074)
extern String hardwareVersionStr;

// Analog calibration values (HR40311..40326). Applied to MODBUS-reported mV only.
extern float analogScaleFactors[4];        // FLOAT32 x4
extern float analogOffsetValues[4];        // FLOAT32 x4

// Optional configured MAC addresses (HR40107..40124)
extern String ethMacConfig;
extern String wifiStaMacConfig;
extern String wifiApMacConfig;

// DHCP or static IP mode
extern bool dhcpMode;

// File upload
extern File fsUploadFile;

// Flag for device restart
extern bool restartRequired;

// HT sensor config + schedules + triggers
extern HTSensorConfig htSensorConfig[3];
extern TimeSchedule schedules[MAX_SCHEDULES];
extern AnalogTrigger analogTriggers[MAX_ANALOG_TRIGGERS];

// Diagnostics
extern unsigned long i2cErrorCount;
extern unsigned long lastSystemUptime;
extern String lastErrorMessage;
