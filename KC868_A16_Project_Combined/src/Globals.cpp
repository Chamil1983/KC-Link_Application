#include "Globals.h"

// Default WiFi credentials (can be changed via web interface)
const char* default_ssid = "KC868-A16";
const char* default_password = "12345678";

const char* ap_ssid = "KC868-A16";      // AP SSID
const char* ap_password = "admin";      // AP Password

// Create PCF8574 objects using the Mischianti library
PCF8574 inputIC1(PCF8574_INPUTS_1_8);    // Digital Inputs 1-8
PCF8574 inputIC2(PCF8574_INPUTS_9_16);   // Digital Inputs 9-16
PCF8574 outputIC3(PCF8574_OUTPUTS_9_16); // Digital Outputs 9-16
PCF8574 outputIC4(PCF8574_OUTPUTS_1_8);  // Digital Outputs 1-8

// DNS server
DNSServer dnsServer;

// Web server port
WebServer server(80);

// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(81);
bool webSocketClients[WEBSOCKETS_SERVER_CLIENT_MAX] = { false };

// Web UI ports (Option A: HTTP + WebSocket)
int httpPort = 80;
int wsPort = 81;

// RS485 serial
// RS485 / Modbus RTU must run on ESP32 Serial2 (UART2) as per project requirements.
// Pins are remapped in initRS485() to GPIO16 (RX) and GPIO13 (TX).
HardwareSerial rs485(2);

// RF receiver/transmitter
RCSwitch rfReceiver = RCSwitch();
RCSwitch rfTransmitter = RCSwitch();

// RTC object
RTC_DS3231 rtc;

// Objects for sensors
DHT* dhtSensors[3] = { NULL, NULL, NULL };
OneWire* oneWireBuses[3] = { NULL, NULL, NULL };
DallasTemperature* ds18b20Sensors[3] = { NULL, NULL, NULL };

// Firmware version
const String firmwareVersion = "1.2.8";

// -------- BACnet / Device Identity (editable in Globals.cpp) --------
const String deviceNameStr = "KC868-A16 Controller";
const String deviceDescriptionStr = "KC868-A16 Automation Controller";
const String deviceLocationStr = "Factory Floor";
const String manufacturerStr = "Microcode Engineering Pvt Ltd";
const String yearOfDevelopmentStr = "2026";


// System state variables
bool outputStates[16] = { false };
bool inputStates[16] = { false };
bool directInputStates[3] = { false };
int analogValues[4] = { 0 };
float analogVoltages[4] = { 0.0 };
bool ethConnected = false;
bool wifiConnected = false;
String deviceName = "KC868-A16";
bool debugMode = true;
unsigned long lastTimeCheck = 0;
bool rtcInitialized = false;
String currentCommunicationProtocol = "wifi";

// Global variable to track connection mode
bool apMode = false;
bool wifiClientMode = true;
bool wiredMode = false;

// Variables for interrupt handling
InterruptConfig interruptConfigs[16];
volatile bool inputStateChanged[16] = { false };
portMUX_TYPE inputMux = portMUX_INITIALIZER_UNLOCKED;
bool inputInterruptsEnabled = false;

// Input read interval for non-interrupt inputs (if any are set to NONE priority)
unsigned long lastInputReadTime = 0;
const unsigned long INPUT_READ_INTERVAL = 20; // ms

// Network configuration
IPAddress ip;
IPAddress gateway;
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);
String mac = "";
String wifiSSID = "";
String wifiPassword = "";

// WiFi (STA) IP configuration (separate from Ethernet)
bool wifiDhcpMode = true;
IPAddress wifiStaIp;
IPAddress wifiStaGateway;
IPAddress wifiStaSubnet(255, 255, 255, 0);
IPAddress wifiStaDns1(8, 8, 8, 8);
IPAddress wifiStaDns2(8, 8, 4, 4);

// WiFi configuration
String wifiSecurity = "WPA2";          // WPA, WPA2, WEP, OPEN
bool wifiHidden = false;               // Whether SSID is hidden
String wifiMacFilter = "";             // MAC filtering
bool wifiAutoUpdate = true;            // Auto firmware updates
String wifiRadioMode = "802.11n";      // 802.11b, 802.11g, 802.11n
int wifiChannel = 6;                   // WiFi channel (1-13)
int wifiChannelWidth = 20;             // Channel width (20 or 40 MHz)
unsigned long wifiDhcpLeaseTime = 86400; // DHCP lease time (s)
bool wifiWmmEnabled = true;            // WiFi Multimedia (WMM)

// USB configuration
int usbComPort = 0;
int usbBaudRate = 115200;
int usbDataBits = 8;
int usbParity = 0;
int usbStopBits = 1;

// RS485 configuration
int rs485BaudRate = 38400;
int rs485Parity = 0;
int rs485DataBits = 8;
int rs485StopBits = 1;
String rs485Protocol = "Modbus RTU";
String rs485Mode = "Half-duplex";
int rs485DeviceAddress = 1;
bool rs485FlowControl = false;
bool rs485NightMode = false;


bool outputsMasterEnable = true;
bool modbusRtuActive = false;
uint16_t rs485FrameGapMs = 6;
uint16_t rs485TimeoutMs = 200;

String boardName = "KC868-A16";
String serialNumber = ""; // populated at runtime (derived from efuse MAC if empty)
String hardwareVersionStr = "1.1";

float analogScaleFactors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float analogOffsetValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

String ethMacConfig = ETHERNET_MAC;
String wifiStaMacConfig = WIFI_STA_MAC;
String wifiApMacConfig = WIFI_AP_MAC;



// DHCP or static IP mode
bool dhcpMode = true;

// File upload
File fsUploadFile;

// Flag for device restart
bool restartRequired = false;

// Initialize sensor configuration for HT1-HT3 pins
HTSensorConfig htSensorConfig[3] = {
  {SENSOR_TYPE_DIGITAL, 0, 0, false, 0},
  {SENSOR_TYPE_DIGITAL, 0, 0, false, 0},
  {SENSOR_TYPE_DIGITAL, 0, 0, false, 0}
};

TimeSchedule schedules[MAX_SCHEDULES];
AnalogTrigger analogTriggers[MAX_ANALOG_TRIGGERS];

// Diagnostics
unsigned long i2cErrorCount = 0;
unsigned long lastSystemUptime = 0;
String lastErrorMessage = "";


#include <WiFi.h>
#include <esp_system.h>

String getBoardMacString() {
    uint64_t macAddress = ESP.getEfuseMac();
    uint8_t macBytes[6];
    macBytes[0] = (macAddress >> 40) & 0xFF;
    macBytes[1] = (macAddress >> 32) & 0xFF;
    macBytes[2] = (macAddress >> 24) & 0xFF;
    macBytes[3] = (macAddress >> 16) & 0xFF;
    macBytes[4] = (macAddress >> 8) & 0xFF;
    macBytes[5] = (macAddress) & 0xFF;

    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macBytes[0], macBytes[1], macBytes[2], macBytes[3], macBytes[4], macBytes[5]);
    return String(buf);
}

String getDeviceSerialNumber() {
    const String prefix = "KC868-A8H73CR36SMIA8";
    uint64_t macAddress = ESP.getEfuseMac();
    char macHex[13];
    snprintf(macHex, sizeof(macHex), "%012llX",
             (unsigned long long)(macAddress & 0xFFFFFFFFFFFFULL));
    return prefix + String(macHex);
}

