// ApiCommunication.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleCommunicationStatus() {
    DynamicJsonDocument doc(1024);

    doc["usb_available"] = true;  // Serial is always available on ESP32
    doc["wifi_connected"] = wifiConnected;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["eth_connected"] = ethConnected;
    doc["rs485_available"] = true;

    doc["active_protocol"] = currentCommunicationProtocol;

    // I2C status
    doc["i2c_status"] = (i2cErrorCount == 0) ? "OK" : "Issues detected";
    doc["i2c_error_count"] = i2cErrorCount;

    // Protocol-specific details
    if (currentCommunicationProtocol == "wifi") {
        doc["wifi_security"] = wifiSecurity;
        doc["wifi_ssid"] = wifiSSID;
        doc["wifi_channel"] = wifiChannel;
    }
    else if (currentCommunicationProtocol == "ethernet") {
        doc["eth_mac"] = ethConnected ? ETH.macAddress() : "Disconnected";
        doc["eth_ip"] = ethConnected ? ETH.localIP().toString() : "Unknown";
        doc["eth_speed"] = ethConnected ? String(ETH.linkSpeed()) + " Mbps" : "Unknown";
    }
    else if (currentCommunicationProtocol == "rs485") {
        doc["rs485_baud"] = rs485BaudRate;
        doc["rs485_protocol"] = rs485Protocol;
        doc["rs485_address"] = rs485DeviceAddress;
    }
    else if (currentCommunicationProtocol == "usb") {
        doc["usb_baud"] = usbBaudRate;
        doc["usb_com"] = usbComPort;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetCommunication() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("protocol")) {
                String protocol = doc["protocol"];

                // Update the communication protocol
                if (protocol == "usb" || protocol == "wifi" || protocol == "ethernet" || protocol == "rs485") {
                    currentCommunicationProtocol = protocol;

                    // Apply protocol-specific settings
                    if (protocol == "rs485") {
                        // Initialize RS485 with current settings
                        initRS485();
                    }

                    // Save to EEPROM
                    saveCommunicationSettings();

                    response = "{\"status\":\"success\",\"protocol\":\"" + protocol + "\"}";
                }
            }
        }
    }

    server.send(200, "application/json", response);
}

void handleCommunicationConfig() {
    DynamicJsonDocument doc(2048);

    // Get protocol from query string
    String protocol = server.hasArg("protocol") ? server.arg("protocol") : currentCommunicationProtocol;
    doc["protocol"] = protocol;

    // Return configuration based on protocol
    if (protocol == "wifi") {
        doc["security"] = wifiSecurity;
        doc["hidden"] = wifiHidden;
        doc["mac_filter"] = wifiMacFilter;
        doc["auto_update"] = wifiAutoUpdate;
        doc["radio_mode"] = wifiRadioMode;
        doc["channel"] = wifiChannel;
        doc["channel_width"] = wifiChannelWidth;
        doc["dhcp_lease_time"] = wifiDhcpLeaseTime;
        doc["wmm_enabled"] = wifiWmmEnabled;
        doc["ssid"] = wifiSSID;

        // Extra network info
        if (wifiConnected) {
            doc["ip"] = WiFi.localIP().toString();
            doc["mac"] = WiFi.macAddress();
            doc["rssi"] = WiFi.RSSI();
            doc["hostname"] = WiFi.getHostname();
        }
    }
    else if (protocol == "ethernet") {
        doc["dhcp_mode"] = dhcpMode;

        if (!dhcpMode) {
            doc["ip"] = ip.toString();
            doc["gateway"] = gateway.toString();
            doc["subnet"] = subnet.toString();
            doc["dns1"] = dns1.toString();
            doc["dns2"] = dns2.toString();
        }

        // Add current Ethernet status if connected
        if (ethConnected) {
            doc["eth_mac"] = ETH.macAddress();
            doc["eth_ip"] = ETH.localIP().toString();
            doc["eth_subnet"] = ETH.subnetMask().toString();
            doc["eth_gateway"] = ETH.gatewayIP().toString();
            doc["eth_dns"] = ETH.dnsIP().toString();
            doc["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
            doc["eth_speed"] = String(ETH.linkSpeed()) + " Mbps";
            doc["eth_hostname"] = ETH.getHostname();
        }
    }
    else if (protocol == "usb") {
        doc["com_port"] = usbComPort;
        doc["baud_rate"] = usbBaudRate;
        doc["data_bits"] = usbDataBits;
        doc["parity"] = usbParity;
        doc["stop_bits"] = usbStopBits;

        // Available baud rates for selection
        JsonArray baudRates = doc.createNestedArray("available_baud_rates");
        baudRates.add(9600);
        baudRates.add(19200);
        baudRates.add(38400);
        baudRates.add(57600);
        baudRates.add(115200);

        // Available data bits for selection
        JsonArray dataBits = doc.createNestedArray("available_data_bits");
        dataBits.add(7);
        dataBits.add(8);

        // Available parity options for selection
        JsonArray parityOptions = doc.createNestedArray("available_parity");
        parityOptions.add(0);  // None
        parityOptions.add(1);  // Odd
        parityOptions.add(2);  // Even

        // Available stop bits for selection
        JsonArray stopBits = doc.createNestedArray("available_stop_bits");
        stopBits.add(1);
        stopBits.add(2);
    }
    else if (protocol == "rs485") {
        doc["baud_rate"] = rs485BaudRate;
        doc["parity"] = rs485Parity;
        doc["data_bits"] = rs485DataBits;
        doc["stop_bits"] = rs485StopBits;
        doc["protocol_type"] = rs485Protocol;
        doc["comm_mode"] = rs485Mode;
        doc["device_address"] = rs485DeviceAddress;
        doc["flow_control"] = rs485FlowControl;
        doc["night_mode"] = rs485NightMode;

        // Available protocol types for selection
        JsonArray protocolTypes = doc.createNestedArray("available_protocols");
        protocolTypes.add("Modbus RTU");
        protocolTypes.add("BACnet");
        protocolTypes.add("Custom ASCII");
        protocolTypes.add("Custom Binary");

        // Available communication modes for selection
        JsonArray commModes = doc.createNestedArray("available_modes");
        commModes.add("Half-duplex");
        commModes.add("Full-duplex");
        commModes.add("Log Mode");
        commModes.add("NMEA Mode");
        commModes.add("TCP ASCII");
        commModes.add("TCP Binary");

        // Available baud rates for selection
        JsonArray baudRates = doc.createNestedArray("available_baud_rates");
        baudRates.add(1200);
        baudRates.add(2400);
        baudRates.add(4800);
        baudRates.add(9600);
        baudRates.add(19200);
        baudRates.add(38400);
        baudRates.add(57600);
        baudRates.add(115200);
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateCommunicationConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("protocol")) {
            String protocol = doc["protocol"];
            bool changed = false;

            // Process protocol-specific settings
            if (protocol == "wifi") {
                if (doc.containsKey("security")) {
                    wifiSecurity = doc["security"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("hidden")) {
                    wifiHidden = doc["hidden"].as<bool>();
                    changed = true;
                }
                if (doc.containsKey("mac_filter")) {
                    wifiMacFilter = doc["mac_filter"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("auto_update")) {
                    wifiAutoUpdate = doc["auto_update"].as<bool>();
                    changed = true;
                }
                if (doc.containsKey("radio_mode")) {
                    wifiRadioMode = doc["radio_mode"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("channel")) {
                    wifiChannel = doc["channel"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("channel_width")) {
                    wifiChannelWidth = doc["channel_width"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("dhcp_lease_time")) {
                    wifiDhcpLeaseTime = doc["dhcp_lease_time"].as<long>();
                    changed = true;
                }
                if (doc.containsKey("wmm_enabled")) {
                    wifiWmmEnabled = doc["wmm_enabled"].as<bool>();
                    changed = true;
                }
                // WiFi credentials are handled separately in handleUpdateConfig
            }
            else if (protocol == "usb") {
                if (doc.containsKey("com_port")) {
                    usbComPort = doc["com_port"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("baud_rate")) {
                    usbBaudRate = doc["baud_rate"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("data_bits")) {
                    usbDataBits = doc["data_bits"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("parity")) {
                    usbParity = doc["parity"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("stop_bits")) {
                    usbStopBits = doc["stop_bits"].as<int>();
                    changed = true;
                }
            }
            else if (protocol == "rs485") {
                if (doc.containsKey("baud_rate")) {
                    rs485BaudRate = doc["baud_rate"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("parity")) {
                    rs485Parity = doc["parity"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("data_bits")) {
                    rs485DataBits = doc["data_bits"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("stop_bits")) {
                    rs485StopBits = doc["stop_bits"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("protocol_type")) {
                    rs485Protocol = doc["protocol_type"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("comm_mode")) {
                    rs485Mode = doc["comm_mode"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("device_address")) {
                    rs485DeviceAddress = doc["device_address"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("flow_control")) {
                    rs485FlowControl = doc["flow_control"].as<bool>();
                    changed = true;
                }
                if (doc.containsKey("night_mode")) {
                    rs485NightMode = doc["night_mode"].as<bool>();
                    changed = true;
                }
            }

            // If any settings changed, save and return success
            if (changed) {
                saveCommunicationConfig();

                // Re-initialize interface if necessary
                if (protocol == "rs485" && currentCommunicationProtocol == "rs485") {
                    initRS485();
                }

                response = "{\"status\":\"success\",\"message\":\"Communication settings updated\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

