// ConfigStore.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void saveInterruptConfigs() {
    DynamicJsonDocument doc(2048);
    JsonArray configArray = doc.createNestedArray("interrupts");

    for (int i = 0; i < 16; i++) {
        JsonObject config = configArray.createNestedObject();
        config["enabled"] = interruptConfigs[i].enabled;
        config["priority"] = interruptConfigs[i].priority;
        config["inputIndex"] = interruptConfigs[i].inputIndex;
        config["triggerType"] = interruptConfigs[i].triggerType;
        config["name"] = interruptConfigs[i].name;
    }

    // Serialize to buffer
    char jsonBuffer[2048];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 1024; i++) {
        EEPROM.write(EEPROM_INTERRUPT_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(EEPROM_INTERRUPT_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    debugPrintln("Interrupt configurations saved");
}

void loadInterruptConfigs() {
    // Create a buffer to read JSON data
    char jsonBuffer[2048];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 2047) {
        jsonBuffer[i] = EEPROM.read(EEPROM_INTERRUPT_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error && doc.containsKey("interrupts")) {
            JsonArray configArray = doc["interrupts"];

            int index = 0;
            for (JsonObject config : configArray) {
                if (index >= 16) break;

                interruptConfigs[index].enabled = config["enabled"] | false;
                interruptConfigs[index].priority = config["priority"] | INPUT_PRIORITY_MEDIUM;
                interruptConfigs[index].inputIndex = config["inputIndex"] | index;
                interruptConfigs[index].triggerType = config["triggerType"] | INTERRUPT_TRIGGER_CHANGE;

                const char* name = config["name"];
                if (name) {
                    strlcpy(interruptConfigs[index].name, name, 32);
                }

                index++;
            }

            debugPrintln("Interrupt configurations loaded");
        }
        else {
            debugPrintln("No valid interrupt configurations found, using defaults");
        }
    }
    else {
        debugPrintln("No interrupt configurations found, using defaults");
    }
}


// EEPROM backup of network settings (separate from main config JSON)
struct NetCfgEeprom {
    uint32_t magic;
    uint8_t version;
    uint8_t eth_dhcp;
    uint8_t wifi_dhcp;
    uint8_t reserved;
    uint32_t eth_ip;
    uint32_t eth_gw;
    uint32_t eth_sn;
    uint32_t eth_d1;
    uint32_t eth_d2;
    uint32_t wifi_ip;
    uint32_t wifi_gw;
    uint32_t wifi_sn;
    uint32_t wifi_d1;
    uint32_t wifi_d2;
    uint16_t http;
    uint16_t ws;
    uint16_t crc;
};

static uint16_t crc16_simple(const uint8_t* data, size_t len) {
    uint16_t c = 0;
    for (size_t i = 0; i < len; i++) c = (uint16_t)(c + data[i]);
    return c;
}

static void writeNetCfgToEEPROM() {
    NetCfgEeprom cfg{};
    cfg.magic = EEPROM_NETCFG_MAGIC;
    cfg.version = 1;
    cfg.eth_dhcp = dhcpMode ? 1 : 0;
    cfg.wifi_dhcp = wifiDhcpMode ? 1 : 0;
    cfg.eth_ip = (uint32_t)ip;
    cfg.eth_gw = (uint32_t)gateway;
    cfg.eth_sn = (uint32_t)subnet;
    cfg.eth_d1 = (uint32_t)dns1;
    cfg.eth_d2 = (uint32_t)dns2;
    cfg.wifi_ip = (uint32_t)wifiStaIp;
    cfg.wifi_gw = (uint32_t)wifiStaGateway;
    cfg.wifi_sn = (uint32_t)wifiStaSubnet;
    cfg.wifi_d1 = (uint32_t)wifiStaDns1;
    cfg.wifi_d2 = (uint32_t)wifiStaDns2;
    cfg.http = (uint16_t)httpPort;
    cfg.ws = (uint16_t)wsPort;

    cfg.crc = crc16_simple((const uint8_t*)&cfg, sizeof(NetCfgEeprom) - sizeof(cfg.crc));

    EEPROM.put(EEPROM_NETCFG_ADDR, cfg);
    EEPROM.commit();
}

static bool readNetCfgFromEEPROM() {
    NetCfgEeprom cfg{};
    EEPROM.get(EEPROM_NETCFG_ADDR, cfg);

    if (cfg.magic != EEPROM_NETCFG_MAGIC) return false;
    const uint16_t want = crc16_simple((const uint8_t*)&cfg, sizeof(NetCfgEeprom) - sizeof(cfg.crc));
    if (cfg.crc != want) return false;

    dhcpMode = (cfg.eth_dhcp != 0);
    ip = IPAddress(cfg.eth_ip);
    gateway = IPAddress(cfg.eth_gw);
    subnet = IPAddress(cfg.eth_sn);
    dns1 = IPAddress(cfg.eth_d1);
    dns2 = IPAddress(cfg.eth_d2);

    wifiDhcpMode = (cfg.wifi_dhcp != 0);
    wifiStaIp = IPAddress(cfg.wifi_ip);
    wifiStaGateway = IPAddress(cfg.wifi_gw);
    wifiStaSubnet = IPAddress(cfg.wifi_sn);
    wifiStaDns1 = IPAddress(cfg.wifi_d1);
    wifiStaDns2 = IPAddress(cfg.wifi_d2);

    httpPort = (int)cfg.http;
    wsPort = (int)cfg.ws;
    return true;
}

void saveNetworkSettings() {
    // Persist network settings in Flash (NVS) using Preferences.
    // NOTE: We intentionally avoid storing network JSON in EEPROM because
    // the EEPROM address space is already heavily used and previous builds
    // overlapped with other structures (causing corrupted reads).
    Preferences prefs;
    if (!prefs.begin("netcfg", false)) {
        debugPrintln("Failed to open Preferences namespace netcfg");
        return;
    }

    // Ethernet (uses existing globals: dhcpMode/ip/gateway/subnet/dns1/dns2)
    prefs.putBool("eth_dhcp", dhcpMode);
    prefs.putUInt("eth_ip", (uint32_t)ip);
    prefs.putUInt("eth_gw", (uint32_t)gateway);
    prefs.putUInt("eth_sn", (uint32_t)subnet);
    prefs.putUInt("eth_d1", (uint32_t)dns1);
    prefs.putUInt("eth_d2", (uint32_t)dns2);

    // WiFi STA config (separate)
    prefs.putBool("wifi_dhcp", wifiDhcpMode);
    prefs.putUInt("wifi_ip", (uint32_t)wifiStaIp);
    prefs.putUInt("wifi_gw", (uint32_t)wifiStaGateway);
    prefs.putUInt("wifi_sn", (uint32_t)wifiStaSubnet);
    prefs.putUInt("wifi_d1", (uint32_t)wifiStaDns1);
    prefs.putUInt("wifi_d2", (uint32_t)wifiStaDns2);

    // Web UI ports (Option A)
    prefs.putUShort("http", (uint16_t)httpPort);
    prefs.putUShort("ws", (uint16_t)wsPort);

    // Also persist WiFi credentials in flash as a backup to EEPROM
    prefs.putString("ssid", wifiSSID);
    prefs.putString("pass", wifiPassword);

    prefs.end();

    // Also keep an EEPROM backup (reserved range) for recovery
    writeNetCfgToEEPROM();

    // Keep legacy EEPROM config JSON in sync for Ethernet globals
    saveConfiguration();

    debugPrintln("Network settings saved to Flash (Preferences)");
}

void loadNetworkSettings() {
    // Load network settings from Flash (Preferences)
    Preferences prefs;
    if (!prefs.begin("netcfg", true)) {
        debugPrintln("Failed to open Preferences namespace netcfg (read)");
        return;
    }

    const bool hasEth = prefs.isKey("eth_dhcp");
    const bool hasWifi = prefs.isKey("wifi_dhcp");

        // If flash preferences don't have network settings, try EEPROM backup
    if (!hasEth && !hasWifi) {
        if (readNetCfgFromEEPROM()) {
            debugPrintln("Network settings loaded from EEPROM backup");
        }
    }

if (hasEth) {
        dhcpMode = prefs.getBool("eth_dhcp", true);
        ip = IPAddress(prefs.getUInt("eth_ip", (uint32_t)IPAddress()));
        gateway = IPAddress(prefs.getUInt("eth_gw", (uint32_t)IPAddress()));
        subnet = IPAddress(prefs.getUInt("eth_sn", (uint32_t)IPAddress(255,255,255,0)));
        dns1 = IPAddress(prefs.getUInt("eth_d1", (uint32_t)IPAddress(8,8,8,8)));
        dns2 = IPAddress(prefs.getUInt("eth_d2", (uint32_t)IPAddress(8,8,4,4)));
    }

    if (hasWifi) {
        wifiDhcpMode = prefs.getBool("wifi_dhcp", true);
        wifiStaIp = IPAddress(prefs.getUInt("wifi_ip", (uint32_t)IPAddress()));
        wifiStaGateway = IPAddress(prefs.getUInt("wifi_gw", (uint32_t)IPAddress()));
        wifiStaSubnet = IPAddress(prefs.getUInt("wifi_sn", (uint32_t)IPAddress(255,255,255,0)));
        wifiStaDns1 = IPAddress(prefs.getUInt("wifi_d1", (uint32_t)IPAddress(8,8,8,8)));
        wifiStaDns2 = IPAddress(prefs.getUInt("wifi_d2", (uint32_t)IPAddress(8,8,4,4)));
    }

    httpPort = (int)prefs.getUShort("http", 80);
    wsPort = (int)prefs.getUShort("ws", 81);

    // Restore WiFi credentials backup (if EEPROM is empty/corrupt)
    if (wifiSSID.length() == 0) {
        String ssid = prefs.getString("ssid", "");
        String pass = prefs.getString("pass", "");
        if (ssid.length() > 0) {
            wifiSSID = ssid;
            wifiPassword = pass;
        }
    }

    prefs.end();
    debugPrintln("Network settings loaded from Flash (Preferences)");
}

void saveWiFiCredentials(String ssid, String password) {
    // Store SSID
    for (int i = 0; i < 64; i++) {
        if (i < ssid.length()) {
            EEPROM.write(EEPROM_WIFI_SSID_ADDR + i, ssid[i]);
        }

        else {
            EEPROM.write(EEPROM_WIFI_SSID_ADDR + i, 0);
        }
    }

    // Store password
    for (int i = 0; i < 64; i++) {
        if (i < password.length()) {
            EEPROM.write(EEPROM_WIFI_PASS_ADDR + i, password[i]);
        }
        else {
            EEPROM.write(EEPROM_WIFI_PASS_ADDR + i, 0);
        }
    }

    EEPROM.commit();

    // Update global variables
    wifiSSID = ssid;
    wifiPassword = password;

    // Also store in Flash (Preferences) as a backup
    Preferences prefs;
    if (prefs.begin("netcfg", false)) {
        prefs.putString("ssid", wifiSSID);
        prefs.putString("pass", wifiPassword);
        prefs.end();
    }
}

void loadWiFiCredentials() {
    wifiSSID = "";
    wifiPassword = "";

    // Read SSID (stop on 0x00 or 0xFF)
    for (int i = 0; i < 64; i++) {
        uint8_t v = EEPROM.read(EEPROM_WIFI_SSID_ADDR + i);
        if (v == 0x00 || v == 0xFF) break;
        wifiSSID += (char)v;
    }

    // Read password (stop on 0x00 or 0xFF)
    for (int i = 0; i < 64; i++) {
        uint8_t v = EEPROM.read(EEPROM_WIFI_PASS_ADDR + i);
        if (v == 0x00 || v == 0xFF) break;
        wifiPassword += (char)v;
    }

    // If EEPROM is empty, try flash backup
    if (wifiSSID.length() == 0) {
        Preferences prefs;
        if (prefs.begin("netcfg", true)) {
            String ssid = prefs.getString("ssid", "");
            String pass = prefs.getString("pass", "");
            if (ssid.length() > 0) {
                wifiSSID = ssid;
                wifiPassword = pass;
            }
            prefs.end();
        }
    }

    debugPrintln("Loaded WiFi SSID: " + wifiSSID);
}

void saveCommunicationSettings() {
    // Store protocol in EEPROM
    for (int i = 0; i < 10; i++) {
        if (i < currentCommunicationProtocol.length()) {
            EEPROM.write(EEPROM_COMM_ADDR + i, currentCommunicationProtocol[i]);
        }
        else {
            EEPROM.write(EEPROM_COMM_ADDR + i, 0);
        }
    }

    EEPROM.commit();
    debugPrintln("Saved communication protocol: " + currentCommunicationProtocol);
}

void loadCommunicationSettings() {
    String protocol = "";

    // Read protocol
    for (int i = 0; i < 10; i++) {
        char c = EEPROM.read(EEPROM_COMM_ADDR + i);
        if (c != 0) {
            protocol += c;
        }
        else {
            break;
        }
    }

    // Validate protocol
    if (protocol == "wifi" || protocol == "ethernet" || protocol == "usb" || protocol == "rs485") {
        currentCommunicationProtocol = protocol;
    }
    else {
        // Default to WiFi if invalid
        currentCommunicationProtocol = "wifi";
    }

    debugPrintln("Loaded communication protocol: " + currentCommunicationProtocol);
}

void saveCommunicationConfig() {
    DynamicJsonDocument doc(2048);

    // WiFi config
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["security"] = wifiSecurity;
    wifi["hidden"] = wifiHidden;
    wifi["mac_filter"] = wifiMacFilter;
    wifi["auto_update"] = wifiAutoUpdate;
    wifi["radio_mode"] = wifiRadioMode;
    wifi["channel"] = wifiChannel;
    wifi["channel_width"] = wifiChannelWidth;
    wifi["dhcp_lease_time"] = wifiDhcpLeaseTime;
    wifi["wmm_enabled"] = wifiWmmEnabled;

    // USB config
    JsonObject usb = doc.createNestedObject("usb");
    usb["com_port"] = usbComPort;
    usb["baud_rate"] = usbBaudRate;
    usb["data_bits"] = usbDataBits;
    usb["parity"] = usbParity;
    usb["stop_bits"] = usbStopBits;

    // RS485 config
    JsonObject rs485Config = doc.createNestedObject("rs485");
    rs485Config["baud_rate"] = rs485BaudRate;
    rs485Config["parity"] = rs485Parity;
    rs485Config["data_bits"] = rs485DataBits;
    rs485Config["stop_bits"] = rs485StopBits;
    rs485Config["protocol"] = rs485Protocol;
    rs485Config["mode"] = rs485Mode;
    rs485Config["device_address"] = rs485DeviceAddress;
    rs485Config["flow_control"] = rs485FlowControl;
    rs485Config["night_mode"] = rs485NightMode;

    // Serialize JSON to a buffer
    char jsonBuffer[2048];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 1024; i++) {
        EEPROM.write(EEPROM_COMM_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(EEPROM_COMM_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    debugPrintln("Saved communication protocol configuration");
}

void loadCommunicationConfig() {
    // Create a buffer to read JSON data
    char jsonBuffer[2048];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 2047) {
        jsonBuffer[i] = EEPROM.read(EEPROM_COMM_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error) {
            // WiFi config
            if (doc.containsKey("wifi")) {
                wifiSecurity = doc["wifi"]["security"] | "WPA2";
                wifiHidden = doc["wifi"]["hidden"] | false;
                wifiMacFilter = doc["wifi"]["mac_filter"] | "";
                wifiAutoUpdate = doc["wifi"]["auto_update"] | true;
                wifiRadioMode = doc["wifi"]["radio_mode"] | "802.11n";
                wifiChannel = doc["wifi"]["channel"] | 6;
                wifiChannelWidth = doc["wifi"]["channel_width"] | 20;
                wifiDhcpLeaseTime = doc["wifi"]["dhcp_lease_time"] | 86400;
                wifiWmmEnabled = doc["wifi"]["wmm_enabled"] | true;
            }

            // USB config
            if (doc.containsKey("usb")) {
                usbComPort = doc["usb"]["com_port"] | 0;
                usbBaudRate = doc["usb"]["baud_rate"] | 115200;
                usbDataBits = doc["usb"]["data_bits"] | 8;
                usbParity = doc["usb"]["parity"] | 0;
                usbStopBits = doc["usb"]["stop_bits"] | 1;
            }

            // RS485 config
            if (doc.containsKey("rs485")) {
                rs485BaudRate = doc["rs485"]["baud_rate"] | 9600;
                rs485Parity = doc["rs485"]["parity"] | 0;
                rs485DataBits = doc["rs485"]["data_bits"] | 8;
                rs485StopBits = doc["rs485"]["stop_bits"] | 1;
                rs485Protocol = doc["rs485"]["protocol"] | "Modbus RTU";
                rs485Mode = doc["rs485"]["mode"] | "Half-duplex";
                rs485DeviceAddress = doc["rs485"]["device_address"] | 1;
                rs485FlowControl = doc["rs485"]["flow_control"] | false;
                rs485NightMode = doc["rs485"]["night_mode"] | false;
            }

            debugPrintln("Communication configuration loaded from EEPROM");
        }
        else {
            debugPrintln("Failed to parse communication configuration JSON");
            // Keep defaults
        }
    }
    else {
        debugPrintln("No communication configuration found, using defaults");
    }
}

void saveConfiguration() {
    DynamicJsonDocument doc(2048);

    // Device settings
    doc["device_name"] = deviceName;
    doc["debug_mode"] = debugMode;
    doc["dhcp_mode"] = dhcpMode;

    // MODBUS identity + config (used by Modbus register map)
    doc["board_name"] = boardName;
    doc["serial_number"] = serialNumber;
    doc["hardware_version"] = hardwareVersionStr;
    doc["outputs_master_enable"] = outputsMasterEnable;
    doc["eth_mac_cfg"] = ethMacConfig;
    doc["wifi_sta_mac_cfg"] = wifiStaMacConfig;
    doc["wifi_ap_mac_cfg"] = wifiApMacConfig;

    JsonArray aScale = doc.createNestedArray("analog_scale");
    JsonArray aOffset = doc.createNestedArray("analog_offset");
    for (int i = 0; i < 4; i++) {
        aScale.add(analogScaleFactors[i]);
        aOffset.add(analogOffsetValues[i]);
    }

    // Network settings if static IP
    if (!dhcpMode) {
        doc["ip"] = ip.toString();
        doc["gateway"] = gateway.toString();
        doc["subnet"] = subnet.toString();
        doc["dns1"] = dns1.toString();
        doc["dns2"] = dns2.toString();
    }

    // Firmware version for tracking
    doc["firmware_version"] = firmwareVersion;

    // Serialize JSON to a buffer
    char jsonBuffer[2048];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 1536; i++) {
        EEPROM.write(EEPROM_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(EEPROM_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    debugPrintln("Configuration saved to EEPROM");
}

void loadConfiguration() {
    // Create a buffer to read JSON data
    char jsonBuffer[2048];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 2047) {
        jsonBuffer[i] = EEPROM.read(EEPROM_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error) {
            // Device settings
            deviceName = doc["device_name"] | "KC868-A16";
            debugMode = doc["debug_mode"] | true;
            dhcpMode = doc["dhcp_mode"] | true;


            // MODBUS identity + config
            boardName = doc["board_name"] | "KC868-A16";
            serialNumber = doc["serial_number"] | "";
            hardwareVersionStr = doc["hardware_version"] | "1.0";
            outputsMasterEnable = doc["outputs_master_enable"] | true;
            ethMacConfig = doc["eth_mac_cfg"] | String(ETHERNET_MAC);
            wifiStaMacConfig = doc["wifi_sta_mac_cfg"] | String(WIFI_STA_MAC);
            wifiApMacConfig = doc["wifi_ap_mac_cfg"] | String(WIFI_AP_MAC);

            if (doc.containsKey("analog_scale")) {
                JsonArray aScale = doc["analog_scale"].as<JsonArray>();
                int idx = 0;
                for (JsonVariant v : aScale) {
                    if (idx >= 4) break;
                    analogScaleFactors[idx++] = v.as<float>();
                }
            }
            if (doc.containsKey("analog_offset")) {
                JsonArray aOffset = doc["analog_offset"].as<JsonArray>();
                int idx = 0;
                for (JsonVariant v : aOffset) {
                    if (idx >= 4) break;
                    analogOffsetValues[idx++] = v.as<float>();
                }
            }

            // Network settings
            if (!dhcpMode && doc.containsKey("ip") && doc.containsKey("gateway")) {
                ip.fromString(doc["ip"].as<String>());
                gateway.fromString(doc["gateway"].as<String>());
                subnet.fromString(doc["subnet"] | "255.255.255.0");
                dns1.fromString(doc["dns1"] | "8.8.8.8");
                dns2.fromString(doc["dns2"] | "8.8.4.4");
            }

            debugPrintln("Configuration loaded from EEPROM");
        }
        else {
            debugPrintln("Failed to parse configuration JSON");
            // Use defaults
            initializeDefaultConfig();
        }
    }
    else {
        // No data found, use defaults
        initializeDefaultConfig();
    }

    // Initialize default schedules
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedules[i].enabled = false;
        schedules[i].triggerType = 0;     // Default to time-based
        schedules[i].days = 0;
        schedules[i].hour = 0;
        schedules[i].minute = 0;
        schedules[i].inputMask = 0;       // No inputs selected
        schedules[i].inputStates = 0;     // All LOW
        schedules[i].logic = 0;           // AND logic
        schedules[i].action = 0;
        schedules[i].targetType = 0;
        schedules[i].targetId = 0;
        snprintf(schedules[i].name, 32, "Schedule %d", i + 1);
    }

    // Initialize default analog triggers
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        analogTriggers[i].enabled = false;
        analogTriggers[i].analogInput = 0;
        analogTriggers[i].threshold = 2048;  // Middle value
        analogTriggers[i].condition = 0;
        analogTriggers[i].action = 0;
        analogTriggers[i].targetType = 0;
        analogTriggers[i].targetId = 0;
        snprintf(analogTriggers[i].name, 32, "Trigger %d", i + 1);
    }
}

void initializeDefaultConfig() {
    deviceName = "KC868-A16";
    boardName = "KC868-A16";
    serialNumber = "";
    hardwareVersionStr = "1.0";
    outputsMasterEnable = true;

    // Restore default MAC strings (can be overridden via MODBUS holding registers)
    ethMacConfig = ETHERNET_MAC;
    wifiStaMacConfig = WIFI_STA_MAC;
    wifiApMacConfig = WIFI_AP_MAC;

    for (int i = 0; i < 4; i++) {
        analogScaleFactors[i] = 1.0f;
        analogOffsetValues[i] = 0.0f;
    }

    debugMode = true;
    dhcpMode = true;

    debugPrintln("Using default configuration");
}

void saveSchedulesToEEPROM() {
    // Calculate required space based on MAX_SCHEDULES
    size_t requiredSpace = MAX_SCHEDULES * sizeof(TimeSchedule);

    // Create a buffer for serializing schedules
    char* buffer = new char[requiredSpace];
    if (!buffer) {
        debugPrintln("Failed to allocate memory for saving schedules");
        return;
    }

    // Copy schedules to buffer
    memcpy(buffer, schedules, requiredSpace);

    // Write to EEPROM
    for (size_t i = 0; i < requiredSpace; i++) {
        EEPROM.write(EEPROM_SCHEDULE_ADDR + i, buffer[i]);
    }

    // Free memory
    delete[] buffer;

    // Commit changes to EEPROM
    EEPROM.commit();

    debugPrintln("Schedules saved to EEPROM");
}

void saveHTSensorConfig() {
    DynamicJsonDocument doc(512);
    JsonArray configArray = doc.createNestedArray("htConfig");

    for (int i = 0; i < 3; i++) {
        JsonObject config = configArray.createNestedObject();
        config["sensorType"] = htSensorConfig[i].sensorType;
    }

    // Serialize to buffer
    char jsonBuffer[512];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    const int HT_CONFIG_ADDR = 3900;  // Choose an address not overlapping with other configurations

    for (size_t i = 0; i < n && i < 256; i++) {
        EEPROM.write(HT_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(HT_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    debugPrintln("HT sensor configuration saved to EEPROM");
}

void loadHTSensorConfig() {
    // Create a buffer to read JSON data
    char jsonBuffer[512];
    size_t i = 0;

    // Choose an address not overlapping with other configurations
    const int HT_CONFIG_ADDR = 3900;

    // Read data until null terminator or max buffer size
    while (i < 511) {
        jsonBuffer[i] = EEPROM.read(HT_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error && doc.containsKey("htConfig")) {
            JsonArray configArray = doc["htConfig"];

            int index = 0;
            for (JsonObject config : configArray) {
                if (index >= 3) break;

                htSensorConfig[index].sensorType = config["sensorType"] | SENSOR_TYPE_DIGITAL;
                index++;
            }

            debugPrintln("HT sensor configuration loaded");
        }
        else {
            debugPrintln("No valid HT sensor configuration found, using defaults");
        }
    }
    else {
        debugPrintln("No HT sensor configuration found, using defaults");
    }

    // Initialize all sensors based on loaded or default configuration
    for (int i = 0; i < 3; i++) {
        initializeSensor(i);
    }
}

