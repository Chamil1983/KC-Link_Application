/**
 * EthernetDriver.cpp - Implementation of W5500 Ethernet driver
 * For Cortex Link A8R-M ESP32 IoT Smart Home Controller
 */

#include "EthernetDriver.h"

 // Create global instance
EthernetDriver ethernet;

EthernetDriver::EthernetDriver()
    : state(ETH_STATE_NOT_INITIALIZED),
    stateTimer(0),
    lastLinkCheck(0),
    resetAttempts(0),
    detectAttempts(0),
    hardwareType(ETH_HW_NONE),
    hardwareName("None"),
    server(nullptr),
    serverPort(80),
    serverActive(false),
    udpActive(false),
    udpLocalPort(0),
    linkStatus(-1),
    isDHCPEnabled(false),
    w5500SpiSettings(ETH_SPI_FREQ, MSBFIRST, SPI_MODE0)
{
    memset(macAddress, 0, sizeof(macAddress));
}

EthernetDriver::~EthernetDriver() {
    if (server != nullptr) {
        delete server;
        server = nullptr;
    }
    if (udpActive) {
        udp.stop();
        udpActive = false;
    }
}

bool EthernetDriver::begin(
    const byte* mac,
    IPAddress ip,
    IPAddress dnsServer,
    IPAddress gateway,
    IPAddress subnet
) {
    memcpy(macAddress, mac, 6);
    ipAddress = ip;
    dnsServerAddress = dnsServer;
    gatewayAddress = gateway;
    subnetMaskAddress = subnet;
    isDHCPEnabled = false;
    dhcpConfigured = true;   // we will apply static right away

    SPI.begin(PIN_ETH_SCLK, PIN_ETH_MISO, PIN_ETH_MOSI);
    pinMode(PIN_ETH_CS, OUTPUT);
    digitalWrite(PIN_ETH_CS, HIGH);
    delay(50);

    Ethernet.init(PIN_ETH_CS);

    if (!digitalInputs.isConnected()) {
        digitalInputs.begin();
    }

    changeState(ETH_STATE_HARDWARE_RESET);

    unsigned long startTime = millis();
    while (state != ETH_STATE_READY && state != ETH_STATE_ERROR) {
        update();
        if (millis() - startTime > 10000) {
            Serial.println(F("[ETH] Initialization timeout"));
            return false;
        }
        delay(10);
    }

    return (state == ETH_STATE_READY);
}

bool EthernetDriver::begin(
    const String& macStr,
    const String& ipStr,
    const String& dnsStr,
    const String& gatewayStr,
    const String& subnetStr
) {
    byte mac[6];
    if (!stringToMACAddress(macStr, mac)) {
        Serial.println(F("[ETH] Invalid MAC address format"));
        return false;
    }

    IPAddress ip, dns, gw, subnet;
    if (!stringToIPAddress(ipStr, ip)) {
        Serial.println(F("[ETH] Invalid IP address format"));
        return false;
    }
    if (!stringToIPAddress(dnsStr, dns)) {
        dns = IPAddress(8, 8, 8, 8);
    }
    if (!stringToIPAddress(subnetStr, subnet)) {
        subnet = IPAddress(255, 255, 255, 0);
    }
    if (gatewayStr.length() == 0 || !stringToIPAddress(gatewayStr, gw)) {
        gw = ip; gw[3] = 1;
    }

    return begin(mac, ip, dns, gw, subnet);
}

bool EthernetDriver::beginDHCP(const byte* macAddr) {
    memcpy(this->macAddress, macAddr, 6);
    isDHCPEnabled = true;
    dhcpConfigured = false;
    dhcpAttempts = 0;
    lastDhcpAttempt = 0;

    SPI.begin(PIN_ETH_SCLK, PIN_ETH_MISO, PIN_ETH_MOSI);
    pinMode(PIN_ETH_CS, OUTPUT);
    digitalWrite(PIN_ETH_CS, HIGH);
    delay(50);

    Ethernet.init(PIN_ETH_CS);

    if (!digitalInputs.isConnected()) {
        digitalInputs.begin();
    }

    changeState(ETH_STATE_HARDWARE_RESET);

    // Wait hardware bring-up to CHECKING_LINK
    unsigned long hwStart = millis();
    while (state != ETH_STATE_CHECKING_LINK && state != ETH_STATE_ERROR) {
        update();
        if (millis() - hwStart > 5000) {
            Serial.println(F("[ETH] Hardware initialization timeout"));
            return false;
        }
        delay(10);
    }
    if (state == ETH_STATE_ERROR) return false;

    // Wait for link (avoid blocking DHCP when link DOWN)
    Serial.println(F("[ETH] Waiting for PHY link to come UP before DHCP..."));
    unsigned long linkStart = millis();
    while (millis() - linkStart <= 15000) {
        checkEthernetLink();
        if (linkStatus == 1) break;
        delay(100);
    }
    if (linkStatus != 1) {
        Serial.println(F("[ETH] Link is DOWN after waiting, cannot start DHCP"));
        changeState(ETH_STATE_ERROR);
        return false;
    }

    // Try DHCP a few times
    Serial.println(F("[ETH] Starting DHCP..."));
    const uint8_t MAX_ATTEMPTS = 1;
    //for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
    //    Serial.print(F("[ETH] DHCP attempt "));
    //    Serial.println(attempt);
    //    if (attemptDHCPOnce()) {
    //        changeState(ETH_STATE_READY);
    //        printNetworkSettings();
    //        return true;
    //    }
    //    Serial.println(F("[ETH] DHCP attempt failed"));
    //    delay(1500);
    //}

    // DHCP failed -> AutoIP fallback (169.254.x.y)
    if (configureAutoIP()) {
        Serial.println(F("[ETH] DHCP failed; AutoIP fallback applied"));
        changeState(ETH_STATE_READY);
        printNetworkSettings();
        return true;
    }

    Serial.println(F("[ETH] Failed to configure Ethernet using DHCP after retries"));
    changeState(ETH_STATE_ERROR);
    return false;
}

bool EthernetDriver::beginDHCP(const String& macStr) {
    byte mac[6];
    if (!stringToMACAddress(macStr, mac)) {
        Serial.println(F("[ETH] Invalid MAC address format"));
        return false;
    }
    return beginDHCP(mac);
}

bool EthernetDriver::isReady() const {
    return (state == ETH_STATE_READY);
}

bool EthernetDriver::getLinkStatus() {
    checkEthernetLink();
    return (linkStatus == 1);
}

const String& EthernetDriver::getHardwareName() const { return hardwareName; }
EthernetHardwareType EthernetDriver::getHardwareType() const { return hardwareType; }
EthernetServer* EthernetDriver::getServer() { return server; }

void EthernetDriver::startWebServer(uint16_t port) {
    if (server != nullptr) delete server;
    serverPort = port;
    server = new EthernetServer(port);
    server->begin();
    serverActive = true;

    Serial.print(F("[ETH] Web server started at http://"));
    Serial.print(Ethernet.localIP());
    Serial.print(F(":"));
    Serial.println(port);
}

void EthernetDriver::stopWebServer() {
    if (server != nullptr) { delete server; server = nullptr; }
    serverActive = false;
    Serial.println(F("[ETH] Web server stopped"));
}

uint16_t EthernetDriver::getServerPort() const { return serverPort; }
bool EthernetDriver::isServerActive() const { return serverActive; }

void EthernetDriver::handleWebClient() {
    if (!serverActive || server == nullptr) return;
    EthernetClient c = server->available();
    if (!c) return;

    Serial.println(F("[ETH] New client connected"));
    bool blank = true;
    while (c.connected()) {
        if (c.available()) {
            char ch = c.read();
            if (ch == '\n' && blank) {
                c.println(F("HTTP/1.1 200 OK"));
                c.println(F("Content-Type: text/html"));
                c.println(F("Connection: close"));
                c.println();
                c.println(F("<!DOCTYPE HTML><html><head><title>W5500 Status</title>"));
                c.println(F("<meta name='viewport' content='width=device-width, initial-scale=1'>"));
                c.println(F("<style>body{font-family:Arial;margin:20px;}table{width:100%;border-collapse:collapse;}th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd;}th{background:#f2f2f2}.status-ok{color:green}.status-error{color:red}</style></head><body>"));
                c.println(F("<h1>W5500 Ethernet Status</h1><table>"));
                c.println(F("<tr><th>Parameter</th><th>Value</th></tr>"));
                c.print(F("<tr><td>Hardware</td><td>")); c.print(hardwareName); c.println(F("</td></tr>"));
                c.print(F("<tr><td>Link Status</td><td>"));
                if (linkStatus == 1) c.print(F("<span class='status-ok'>Connected</span>"));
                else if (linkStatus == 0) c.print(F("<span class='status-error'>Disconnected</span>"));
                else c.print(F("<span class='status-error'>Unknown</span>"));
                c.println(F("</td></tr>"));
                c.print(F("<tr><td>Network Type</td><td>")); c.print(isDHCPEnabled ? F("DHCP") : F("Static IP")); c.println(F("</td></tr>"));
                c.print(F("<tr><td>IP Address</td><td>")); c.print(Ethernet.localIP()); c.println(F("</td></tr>"));
                c.print(F("<tr><td>Subnet Mask</td><td>")); c.print(Ethernet.subnetMask()); c.println(F("</td></tr>"));
                c.print(F("<tr><td>Gateway</td><td>")); c.print(Ethernet.gatewayIP()); c.println(F("</td></tr>"));
                c.print(F("<tr><td>DNS Server</td><td>")); c.print(Ethernet.dnsServerIP()); c.println(F("</td></tr>"));
                c.print(F("<tr><td>MAC Address</td><td>")); c.print(getMACAddressString()); c.println(F("</td></tr>"));
                c.print(F("<tr><td>Reset Attempts</td><td>")); c.print(resetAttempts); c.println(F("</td></tr>"));
                c.print(F("<tr><td>Web Server Port</td><td>")); c.print(serverPort); c.println(F("</td></tr>"));
                c.println(F("</table><p><small>Refresh page to update values</small></p></body></html>"));
                break;
            }
            if (ch == '\n') blank = true;
            else if (ch != '\r') blank = false;
        }
    }
    delay(10);
    c.stop();
    Serial.println(F("[ETH] Client disconnected"));
}

bool EthernetDriver::connectToServer(const IPAddress& serverIP, uint16_t port) {
    if (state != ETH_STATE_READY) {
        Serial.println(F("[ETH] Ethernet not ready to connect to server"));
        return false;
    }
    client.stop();
    if (client.connect(serverIP, port)) {
        Serial.println(F("[ETH] Connected to server"));
        return true;
    }
    Serial.println(F("[ETH] Connection to server failed"));
    return false;
}

bool EthernetDriver::connectToServer(const String& serverIP, uint16_t port) {
    IPAddress ip;
    if (!stringToIPAddress(serverIP, ip)) {
        Serial.println(F("[ETH] Invalid server IP format"));
        return false;
    }
    return connectToServer(ip, port);
}

bool EthernetDriver::isConnected() { return client.connected(); }
bool EthernetDriver::stopConnection() { client.stop(); return true; }

bool EthernetDriver::beginUDP(uint16_t localPort) {
    if (state != ETH_STATE_READY) {
        Serial.println(F("[ETH] Ethernet not ready for UDP"));
        return false;
    }
    if (udpActive) udp.stop();
    udpLocalPort = localPort;
    if (udp.begin(localPort) == 1) {
        Serial.print(F("[ETH] UDP started on port ")); Serial.println(localPort);
        udpActive = true;
        return true;
    }
    Serial.println(F("[ETH] Failed to start UDP"));
    udpActive = false;
    return false;
}

bool EthernetDriver::sendUDP(const IPAddress& remoteIP, uint16_t remotePort, const uint8_t* buffer, size_t size) {
    if (!udpActive) {
        Serial.println(F("[ETH] UDP not active"));
        return false;
    }
    udp.beginPacket(remoteIP, remotePort);
    size_t written = udp.write(buffer, size);
    bool ok = (udp.endPacket() == 1) && (written == size);
    if (!ok) Serial.println(F("[ETH] Failed to send UDP packet"));
    return ok;
}

bool EthernetDriver::sendUDP(const String& remoteIP, uint16_t remotePort, const String& data) {
    IPAddress ip;
    if (!stringToIPAddress(remoteIP, ip)) {
        Serial.println(F("[ETH] Invalid remote IP format"));
        return false;
    }
    return sendUDP(ip, remotePort, (const uint8_t*)data.c_str(), data.length());
}

int EthernetDriver::parsePacket() { return udpActive ? udp.parsePacket() : 0; }
int EthernetDriver::readUDP(uint8_t* buffer, size_t size) { return udpActive ? udp.read(buffer, size) : 0; }

void EthernetDriver::stopUDP() {
    if (udpActive) { udp.stop(); udpActive = false; Serial.println(F("[ETH] UDP stopped")); }
}

IPAddress EthernetDriver::getIP() const { return Ethernet.localIP(); }
IPAddress EthernetDriver::getSubnetMask() const { return Ethernet.subnetMask(); }
IPAddress EthernetDriver::getGateway() const { return Ethernet.gatewayIP(); }
IPAddress EthernetDriver::getDNSServer() const { return Ethernet.dnsServerIP(); }

bool EthernetDriver::setIP(const IPAddress& ip) {
    if (isDHCPEnabled) { Serial.println(F("[ETH] Cannot set IP when DHCP is active")); return false; }
    ipAddress = ip;
    return applyNetworkConfig();
}
bool EthernetDriver::setIP(const String& ipStr) {
    IPAddress ip; if (!stringToIPAddress(ipStr, ip)) { Serial.println(F("[ETH] Invalid IP address format")); return false; }
    return setIP(ip);
}

bool EthernetDriver::setDNSServer(const IPAddress& dns) {
    if (isDHCPEnabled) { Serial.println(F("[ETH] Cannot set DNS when DHCP is active")); return false; }
    dnsServerAddress = dns; return applyNetworkConfig();
}
bool EthernetDriver::setDNSServer(const String& dnsStr) {
    IPAddress dns; if (!stringToIPAddress(dnsStr, dns)) { Serial.println(F("[ETH] Invalid DNS address format")); return false; }
    return setDNSServer(dns);
}

bool EthernetDriver::setGateway(const IPAddress& gateway) {
    if (isDHCPEnabled) { Serial.println(F("[ETH] Cannot set Gateway when DHCP is active")); return false; }
    gatewayAddress = gateway; return applyNetworkConfig();
}
bool EthernetDriver::setGateway(const String& gatewayStr) {
    IPAddress gw; if (!stringToIPAddress(gatewayStr, gw)) { Serial.println(F("[ETH] Invalid Gateway address format")); return false; }
    return setGateway(gw);
}

bool EthernetDriver::setSubnetMask(const IPAddress& subnet) {
    if (isDHCPEnabled) { Serial.println(F("[ETH] Cannot set Subnet when DHCP is active")); return false; }
    subnetMaskAddress = subnet; return applyNetworkConfig();
}
bool EthernetDriver::setSubnetMask(const String& subnetStr) {
    IPAddress sn; if (!stringToIPAddress(subnetStr, sn)) { Serial.println(F("[ETH] Invalid Subnet mask format")); return false; }
    return setSubnetMask(sn);
}

void EthernetDriver::getMACAddress(byte* mac) const { if (mac) memcpy(mac, macAddress, 6); }
String EthernetDriver::getMACAddressString() const { return macAddressToString(macAddress); }

bool EthernetDriver::setMACAddress(const byte* mac) {
    if (state != ETH_STATE_NOT_INITIALIZED && state != ETH_STATE_ERROR) {
        Serial.println(F("[ETH] Cannot change MAC when Ethernet is active"));
        return false;
    }
    memcpy(macAddress, mac, 6);
    return true;
}
bool EthernetDriver::setMACAddress(const String& macStr) {
    byte mac[6];
    if (!stringToMACAddress(macStr, mac)) { Serial.println(F("[ETH] Invalid MAC address format")); return false; }
    return setMACAddress(mac);
}

bool EthernetDriver::stringToIPAddress(const String& ipStr, IPAddress& result) {
    int index = 0; int values[4] = { 0 }; int startPos = 0;
    for (int i = 0; i < ipStr.length(); i++) {
        if (ipStr[i] == '.' || i == ipStr.length() - 1) {
            String octet = (i == ipStr.length() - 1) ? ipStr.substring(startPos) : ipStr.substring(startPos, i);
            int value = octet.toInt(); if (value < 0 || value > 255) return false;
            values[index++] = value; startPos = i + 1; if (index > 4) return false;
        }
    }
    if (index != 4) return false;
    result = IPAddress(values[0], values[1], values[2], values[3]); return true;
}

String EthernetDriver::ipAddressToString(const IPAddress& ip) {
    String s; for (int i = 0; i < 4; i++) { s += String(ip[i]); if (i < 3) s += '.'; } return s;
}

bool EthernetDriver::stringToMACAddress(const String& macStr, byte* mac) {
    String cleanMac = "";
    for (unsigned int i = 0; i < macStr.length(); i++) {
        if (macStr[i] != ':' && macStr[i] != '-' && macStr[i] != ' ') cleanMac += macStr[i];
    }
    if (cleanMac.length() != 12) return false;
    for (int i = 0; i < 6; i++) {
        String byteStr = cleanMac.substring(i * 2, i * 2 + 2);
        char* endPtr; mac[i] = strtol(byteStr.c_str(), &endPtr, 16);
        if (*endPtr != '\0') return false;
    }
    return true;
}

String EthernetDriver::macAddressToString(const byte* mac) {
    String s; for (int i = 0; i < 6; i++) { if (mac[i] < 0x10) s += '0'; s += String(mac[i], HEX); if (i < 5) s += ':'; } return s;
}

void EthernetDriver::generateRandomMACAddress(byte* mac) {
    if (!mac) return;
    randomSeed(millis());
    for (int i = 0; i < 6; i++) mac[i] = random(256);
    mac[0] = (mac[0] & 0xFC) | 0x02; // LAA + unicast
}

void EthernetDriver::printNetworkSettings() const {
    Serial.println(F("[ETH] Network Configuration:"));
    Serial.print(F("  MAC:     ")); Serial.println(getMACAddressString());
    Serial.print(F("  IP:      ")); Serial.println(Ethernet.localIP());
    Serial.print(F("  Subnet:  ")); Serial.println(Ethernet.subnetMask());
    Serial.print(F("  Gateway: ")); Serial.println(Ethernet.gatewayIP());
    Serial.print(F("  DNS:     ")); Serial.println(Ethernet.dnsServerIP());
    Serial.print(F("  Type:    ")); Serial.println(isDHCPEnabled ? F("DHCP") : F("Static IP"));
}

bool EthernetDriver::resetHardware() {
    resetAttempts++;
    Serial.print(F("[ETH] Hardware reset #")); Serial.println(resetAttempts);

    SPI.begin(PIN_ETH_SCLK, PIN_ETH_MISO, PIN_ETH_MOSI);
    pinMode(PIN_ETH_CS, OUTPUT);
    digitalWrite(PIN_ETH_CS, HIGH);
    delay(10);

    if (!digitalInputs.isConnected()) {
        digitalInputs.begin();
    }

    Serial.println(F("[ETH] Asserting W5500 reset signal"));
    digitalInputs.ethernetReset(LOW);
    delay(ETH_RESET_DURATION);

    Serial.println(F("[ETH] Releasing W5500 reset signal"));
    digitalInputs.ethernetReset(HIGH);

    delay(ETH_RECOVERY_DELAY * 3);

    Serial.println(F("[ETH] Re-initializing SPI CS pin"));
    digitalWrite(PIN_ETH_CS, HIGH);
    delay(50);

    Ethernet.init(PIN_ETH_CS);
    delay(50);

    // Reset DHCP trackers on HW reset
    dhcpConfigured = false;
    dhcpAttempts = 0;
    lastDhcpAttempt = 0;

    Serial.println(F("[ETH] Hardware reset complete"));
    return true;
}

void EthernetDriver::update() {
    unsigned long now = millis();

    if (isDHCPEnabled && state == ETH_STATE_READY) {
        Ethernet.maintain();
    }

    switch (state) {
    case ETH_STATE_HARDWARE_RESET:
        resetHardware();
        changeState(ETH_STATE_INITIALIZING);
        break;

    case ETH_STATE_INITIALIZING:
        primeSPI();
        changeState(ETH_STATE_CHECKING_HARDWARE);
        break;

    case ETH_STATE_CHECKING_HARDWARE:
        if (detectHardware()) {
            changeState(ETH_STATE_CHECKING_LINK);
        }
        else {
            detectAttempts++;
            if (detectAttempts >= 3) {
                if (resetAttempts < 5) {
                    Serial.println(F("[ETH] Detection failed, retrying reset"));
                    changeState(ETH_STATE_HARDWARE_RESET);
                }
                else {
                    Serial.println(F("[ETH] Hardware detection failed after multiple attempts"));
                    changeState(ETH_STATE_ERROR);
                }
            }
        }
        break;

    case ETH_STATE_CHECKING_LINK:
        checkEthernetLink();

        if (isDHCPEnabled) {
            // Non-blocking DHCP retry path when link is UP
            if (linkStatus == 1 && !dhcpConfigured) {
                const unsigned long RETRY_GAP_MS = 3000;
                if (now - lastDhcpAttempt >= RETRY_GAP_MS) {
                    lastDhcpAttempt = now;
                    dhcpAttempts++;
                    Serial.print(F("[ETH] DHCP retry (state machine) attempt "));
                    Serial.println(dhcpAttempts);
                    if (attemptDHCPOnce()) {
                        changeState(ETH_STATE_READY);
                        dhcpConfigured = true;
                        printNetworkSettings();
                    }
                    else if (dhcpAttempts >= 3) {
                        // Apply AutoIP fallback
                        if (configureAutoIP()) {
                            Serial.println(F("[ETH] DHCP failed; AutoIP fallback (state machine) applied"));
                            changeState(ETH_STATE_READY);
                            dhcpConfigured = true;
                            printNetworkSettings();
                        }
                        else {
                            changeState(ETH_STATE_ERROR);
                        }
                    }
                }
            }
            return; // stay in link check / DHCP handling
        }

        // Static IP path: apply config after a short delay even if link is down
        if (now - stateTimer > 2000) {
            if (configureStaticIP()) changeState(ETH_STATE_READY);
            else changeState(ETH_STATE_ERROR);
        }
        break;

    case ETH_STATE_READY:
        if (now - lastLinkCheck >= 2000) {
            lastLinkCheck = now;
            checkEthernetLink();
        }
        break;

    case ETH_STATE_ERROR:
        if (now - stateTimer > 5000) {
            Serial.println(F("[ETH] Attempting recovery from error state"));
            resetAttempts = 0;
            detectAttempts = 0;
            changeState(ETH_STATE_HARDWARE_RESET);
        }
        break;

    default:
        changeState(ETH_STATE_HARDWARE_RESET);
        break;
    }
}

uint8_t EthernetDriver::getResetAttempts() const { return resetAttempts; }
EthernetState EthernetDriver::getState() const { return state; }

void EthernetDriver::changeState(EthernetState newState) {
    state = newState;
    stateTimer = millis();
    Serial.print(F("[ETH] State: "));
    switch (newState) {
    case ETH_STATE_NOT_INITIALIZED:   Serial.println(F("NOT_INITIALIZED")); break;
    case ETH_STATE_HARDWARE_RESET:    Serial.println(F("HARDWARE_RESET")); break;
    case ETH_STATE_INITIALIZING:      Serial.println(F("INITIALIZING")); break;
    case ETH_STATE_CHECKING_HARDWARE: Serial.println(F("CHECKING_HARDWARE")); break;
    case ETH_STATE_CHECKING_LINK:     Serial.println(F("CHECKING_LINK")); break;
    case ETH_STATE_READY:             Serial.println(F("READY")); break;
    case ETH_STATE_ERROR:             Serial.println(F("ERROR")); break;
    default:                          Serial.println(F("UNKNOWN")); break;
    }
}

bool EthernetDriver::detectHardware() {
    Serial.println(F("[ETH] Detecting Ethernet hardware..."));

    SPI.beginTransaction(w5500SpiSettings);
    digitalWrite(PIN_ETH_CS, HIGH);
    delayMicroseconds(100);
    digitalWrite(PIN_ETH_CS, LOW);

    // Common Mode Register reset
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x80);
    SPI.transfer(0x80);
    digitalWrite(PIN_ETH_CS, HIGH);
    SPI.endTransaction();

    delay(100);

    uint8_t version = readVersionRegister();

    Serial.print(F("[ETH] VERSION register: 0x"));
    Serial.println(version, HEX);

    if (version == 0x04) {
        hardwareType = ETH_HW_W5500;
        hardwareName = "W5500";
        Serial.println(F("[ETH] W5500 detected successfully"));

        uint8_t phycfgr = readPHYCFGR();
        Serial.print(F("[ETH] PHYCFGR register: 0x")); Serial.println(phycfgr, HEX);
        Serial.print(F("[ETH] PHY Link: "));   Serial.println((phycfgr & 0x01) ? "UP" : "DOWN");
        Serial.print(F("[ETH] PHY Speed: "));  Serial.println((phycfgr & 0x02) ? "10Mbps" : "100Mbps");
        Serial.print(F("[ETH] PHY Duplex: ")); Serial.println((phycfgr & 0x04) ? "Full" : "Half");
        return true;
    }
    else if (version == 0x00 || version == 0xFF) {
        hardwareType = ETH_HW_NONE;
        hardwareName = "None";
        Serial.println(F("[ETH] No Ethernet hardware detected - check connections"));
        Serial.println(F("[ETH] Verifying MCP23017 reset pin state: "));
        Serial.print(F("Reset Pin State: "));
        Serial.println(digitalInputs.getEthernetResetState() ? "HIGH (inactive)" : "LOW (active)");
        return false;
    }
    else {
        hardwareType = ETH_HW_UNKNOWN;
        hardwareName = "Unknown";
        Serial.print(F("[ETH] Unknown Ethernet hardware (0x")); Serial.print(version, HEX); Serial.println(F(")"));
        return false;
    }
}

bool EthernetDriver::configureStaticIP() {
    Serial.println(F("[ETH] Configuring static IP"));
    byte macCopy[6]; memcpy(macCopy, macAddress, 6);

    Ethernet.init(PIN_ETH_CS);
    Ethernet.begin(macCopy, ipAddress, dnsServerAddress, gatewayAddress, subnetMaskAddress);
    delay(100);

    IPAddress actualIP = Ethernet.localIP();
    Serial.print(F("[ETH] Assigned IP: ")); Serial.println(actualIP);

    if (actualIP == IPAddress(0, 0, 0, 0)) {
        Serial.println(F("[ETH] Static IP configuration failed - got 0.0.0.0"));
        return false;
    }
    Serial.println(F("[ETH] Static IP configuration successful"));
    printNetworkSettings();
    return true;
}

bool EthernetDriver::applyNetworkConfig() {
    if (!isReady()) {
        Serial.println(F("[ETH] Cannot apply network config when Ethernet is not ready"));
        return false;
    }
    byte macCopy[6]; memcpy(macCopy, macAddress, 6);
    Ethernet.begin(macCopy, ipAddress, dnsServerAddress, gatewayAddress, subnetMaskAddress);

    IPAddress actualIP = Ethernet.localIP();
    if (actualIP == IPAddress(0, 0, 0, 0)) {
        Serial.println(F("[ETH] Failed to apply new network configuration"));
        return false;
    }
    Serial.println(F("[ETH] Network configuration updated"));
    printNetworkSettings();
    return true;
}

void EthernetDriver::checkEthernetLink() {
    uint8_t phycfgr = readPHYCFGR();
    int8_t currentLink = (phycfgr & 0x01) ? 1 : 0;
    if (currentLink != linkStatus) {
        linkStatus = currentLink;
        Serial.print(F("[ETH] Link status: "));
        Serial.println(linkStatus ? F("UP") : F("DOWN"));
    }
}

uint8_t EthernetDriver::readVersionRegister() {
    SPI.beginTransaction(w5500SpiSettings);
    digitalWrite(PIN_ETH_CS, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ETH_CS, LOW);
    SPI.transfer(0x00);
    SPI.transfer(0x39);
    SPI.transfer(0x00);
    uint8_t version = SPI.transfer(0x00);
    digitalWrite(PIN_ETH_CS, HIGH);
    SPI.endTransaction();
    return version;
}

uint8_t EthernetDriver::readPHYCFGR() {
    SPI.beginTransaction(w5500SpiSettings);
    digitalWrite(PIN_ETH_CS, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ETH_CS, LOW);
    SPI.transfer(0x00);
    SPI.transfer(0x2E);
    SPI.transfer(0x00);
    uint8_t phycfgr = SPI.transfer(0x00);
    digitalWrite(PIN_ETH_CS, HIGH);
    SPI.endTransaction();
    return phycfgr;
}

void EthernetDriver::primeSPI() {
    Serial.println(F("[ETH] Priming SPI bus..."));
    SPI.beginTransaction(w5500SpiSettings);
    digitalWrite(PIN_ETH_CS, HIGH);
    delayMicroseconds(250);
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_ETH_CS, LOW);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        digitalWrite(PIN_ETH_CS, HIGH);
        delay(1);
    }
    SPI.endTransaction();

    SPI.begin(PIN_ETH_SCLK, PIN_ETH_MISO, PIN_ETH_MOSI);
    SPI.setFrequency(ETH_SPI_FREQ);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    pinMode(PIN_ETH_CS, OUTPUT);
    digitalWrite(PIN_ETH_CS, HIGH);
    delay(10);

    Ethernet.init(PIN_ETH_CS);
    Serial.println(F("[ETH] SPI bus primed"));
}

size_t EthernetDriver::clientWrite(const char* data) { if (!isConnected()) return 0; return client.write(data); }
size_t EthernetDriver::clientWrite(const uint8_t* buffer, size_t size) { if (!isConnected()) return 0; return client.write(buffer, size); }
size_t EthernetDriver::clientPrint(const String& data) { if (!isConnected()) return 0; return client.print(data); }
size_t EthernetDriver::clientPrintln(const String& data) { if (!isConnected()) return 0; return client.println(data); }
int EthernetDriver::clientAvailable() { return isConnected() ? client.available() : 0; }
int EthernetDriver::clientRead() { if (!isConnected() || !client.available()) return -1; return client.read(); }
int EthernetDriver::clientRead(uint8_t* buffer, size_t size) { if (!isConnected() || !client.available()) return 0; return client.read(buffer, size); }
String EthernetDriver::clientReadString() { if (!isConnected() || !client.available()) return ""; return client.readString(); }

// ===== DHCP helpers =====
bool EthernetDriver::attemptDHCPOnce() {
    byte macCopy[6]; memcpy(macCopy, macAddress, 6);
    int res = Ethernet.begin(macCopy); // 0 on failure
    if (res != 0) {
        ipAddress = Ethernet.localIP();
        dnsServerAddress = Ethernet.dnsServerIP();
        gatewayAddress = Ethernet.gatewayIP();
        subnetMaskAddress = Ethernet.subnetMask();
        dhcpConfigured = true;
        return true;
    }
    return false;
}

bool EthernetDriver::configureAutoIP() {
    // Derive a stable link-local IP from the ESP32 chip MAC to minimize collision risk.
    uint64_t chipMac = ESP.getEfuseMac();
    uint8_t a = (uint8_t)((chipMac >> 8) & 0xFF);
    uint8_t b = (uint8_t)(chipMac & 0xFF);
    a = (a % 254) + 1; // 1..254
    b = (b % 254) + 1; // 1..254

    IPAddress ip(169, 254, a, b);
    IPAddress mask(255, 255, 0, 0);
    IPAddress gw(0, 0, 0, 0);
    IPAddress dns(0, 0, 0, 0);

    byte macCopy[6]; memcpy(macCopy, macAddress, 6);
    Ethernet.init(PIN_ETH_CS);
    Ethernet.begin(macCopy, ip, dns, gw, mask);
    delay(100);

    if (Ethernet.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println(F("[ETH] AutoIP assignment failed (0.0.0.0)"));
        return false;
    }

    ipAddress = ip;
    dnsServerAddress = dns;
    gatewayAddress = gw;
    subnetMaskAddress = mask;
    dhcpConfigured = true;
    return true;
}