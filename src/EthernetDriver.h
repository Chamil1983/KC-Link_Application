/**
 * EthernetDriver.h - Driver for W5500 Ethernet module with MCP23017 reset control
 * For Cortex Link A8R-M ESP32 IoT Smart Home Controller
 *
 * This driver manages the USR-ES1 W5500 Ethernet module with hardware reset
 * capability through MCP23017 GPIO expander (GPA5)
 *
 * Libraries:
 * - Ethernet2: https://github.com/adafruit/Ethernet2.git
 * - MCP23017: https://github.com/blemasle/arduino-mcp23017.git
 */

#ifndef ETHERNET_DRIVER_H
#define ETHERNET_DRIVER_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet2.h>
#include "Config.h"
#include "DigitalInputDriver.h"
#include <utility/w5500.h> // For low-level W5500 access

 // Ethernet state machine states
enum EthernetState {
    ETH_STATE_NOT_INITIALIZED,
    ETH_STATE_HARDWARE_RESET,
    ETH_STATE_INITIALIZING,
    ETH_STATE_CHECKING_HARDWARE,
    ETH_STATE_CHECKING_LINK,
    ETH_STATE_READY,
    ETH_STATE_ERROR
};

// Ethernet hardware types
enum EthernetHardwareType {
    ETH_HW_NONE,
    ETH_HW_W5100,
    ETH_HW_W5200,
    ETH_HW_W5500,
    ETH_HW_UNKNOWN
};

class EthernetDriver {
public:
    // Constructor
    EthernetDriver();

    // Destructor - ensure server is cleaned up
    ~EthernetDriver();

    // Initialize the Ethernet module with static IP
    bool begin(
        const byte* macAddress,
        IPAddress ip,
        IPAddress dnsServer,
        IPAddress gateway,
        IPAddress subnet
    );

    // Initialize with MAC string (format: "AA:BB:CC:DD:EE:FF") and IP address strings
    bool begin(
        const String& macStr,
        const String& ipStr,
        const String& dnsStr = "8.8.8.8",
        const String& gatewayStr = "",  // If empty, will use IP with last octet = 1
        const String& subnetStr = "255.255.255.0"
    );

    // Initialize the Ethernet module with DHCP
    bool beginDHCP(const byte* macAddress);
    bool beginDHCP(const String& macStr);

    // Check if Ethernet connection is ready
    bool isReady() const;

    // Get link status (true = connected, false = disconnected)
    bool getLinkStatus();

    // Get hardware type as string
    const String& getHardwareName() const;

    // Get hardware type as enum
    EthernetHardwareType getHardwareType() const;

    // Web server methods
    EthernetServer* getServer();
    void startWebServer(uint16_t port = 80);
    void stopWebServer();
    void handleWebClient();
    uint16_t getServerPort() const;
    bool isServerActive() const;

    // TCP client methods - public interface for client operations
    bool connectToServer(const IPAddress& serverIP, uint16_t port);
    bool connectToServer(const String& serverIP, uint16_t port);
    bool isConnected();
    bool stopConnection();

    // Client operations - safer access to client functionality
    size_t clientWrite(const char* data);
    size_t clientWrite(const uint8_t* buffer, size_t size);
    size_t clientPrint(const String& data);
    size_t clientPrintln(const String& data);
    int clientAvailable();
    int clientRead();
    int clientRead(uint8_t* buffer, size_t size);
    String clientReadString();

    // UDP methods
    bool beginUDP(uint16_t localPort);
    bool sendUDP(const IPAddress& remoteIP, uint16_t remotePort, const uint8_t* buffer, size_t size);
    bool sendUDP(const String& remoteIP, uint16_t remotePort, const String& data);
    int parsePacket();
    int readUDP(uint8_t* buffer, size_t size);
    void stopUDP();

    // Network configuration getters
    IPAddress getIP() const;
    IPAddress getSubnetMask() const;
    IPAddress getGateway() const;
    IPAddress getDNSServer() const;

    // Network configuration setters (works only when DHCP is not active)
    bool setIP(const IPAddress& ip);
    bool setIP(const String& ipStr);
    bool setDNSServer(const IPAddress& dns);
    bool setDNSServer(const String& dnsStr);
    bool setGateway(const IPAddress& gateway);
    bool setGateway(const String& gatewayStr);
    bool setSubnetMask(const IPAddress& subnet);
    bool setSubnetMask(const String& subnetStr);

    // MAC address methods
    void getMACAddress(byte* mac) const;
    String getMACAddressString() const;
    bool setMACAddress(const byte* mac);
    bool setMACAddress(const String& macStr);

    // IP address string conversion utilities
    static bool stringToIPAddress(const String& ipStr, IPAddress& result);
    static String ipAddressToString(const IPAddress& ip);

    // MAC address string conversion utilities
    static bool stringToMACAddress(const String& macStr, byte* mac);
    static String macAddressToString(const byte* mac);

    // Generate random MAC address with locally administered bit set
    static void generateRandomMACAddress(byte* mac);

    // Print network settings to Serial
    void printNetworkSettings() const;

    // Manual hardware reset
    bool resetHardware();

    // Handle Ethernet tasks - call this regularly from loop()
    void update();

    // Get number of reset attempts
    uint8_t getResetAttempts() const;

    // Get current state
    EthernetState getState() const;

private:
    // State machine variables
    EthernetState state;
    unsigned long stateTimer;
    unsigned long lastLinkCheck;
    uint8_t resetAttempts;
    uint8_t detectAttempts;

    // Hardware information
    EthernetHardwareType hardwareType;
    String hardwareName;
    byte macAddress[6];

    // Network configuration
    IPAddress ipAddress;
    IPAddress dnsServerAddress;
    IPAddress gatewayAddress;
    IPAddress subnetMaskAddress;
    bool isDHCPEnabled;

    // DHCP retry/fallback trackers
    bool          dhcpConfigured = false;     // true once DHCP or fallback config applied
    uint8_t       dhcpAttempts = 0;           // attempts made in current session
    unsigned long lastDhcpAttempt = 0;        // ms timestamp of last attempt

    // Web server
    EthernetServer* server;
    uint16_t serverPort;
    bool serverActive;

    // TCP client
    EthernetClient client;

    // UDP
    EthernetUDP udp;
    bool udpActive;
    uint16_t udpLocalPort;

    // Link status
    int8_t linkStatus;  // -1=unknown, 0=down, 1=up

    // SPI settings for W5500
    SPISettings w5500SpiSettings;

    // State machine methods
    void changeState(EthernetState newState);
    bool detectHardware();
    bool configureStaticIP();
    void checkEthernetLink();

    // Low-level hardware methods
    uint8_t readVersionRegister();
    uint8_t readPHYCFGR();
    void primeSPI();

    // Helper to apply network configuration after changes
    bool applyNetworkConfig();

    // DHCP helpers
    bool attemptDHCPOnce();
    bool configureAutoIP();  // 169.254.x.y fallback when DHCP fails
};

extern EthernetDriver ethernet;

#endif // ETHERNET_DRIVER_H