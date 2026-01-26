// WebSocketManager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
    case WStype_DISCONNECTED:
        debugPrintln("WebSocket client disconnected");
        webSocketClients[num] = false;
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        debugPrintln("WebSocket client connected: " + ip.toString());

        // Mark client as subscribed
        webSocketClients[num] = true;

        // Send initial status update
        DynamicJsonDocument doc(1024);
        doc["type"] = "status";
        doc["connected"] = true;

        String message;
        serializeJson(doc, message);
        webSocket.sendTXT(num, message);

        // Send current state of all relays and inputs
        broadcastUpdate();
    }
    break;
    case WStype_TEXT:
    {
        String text = String((char*)payload);
        debugPrintln("WebSocket received: " + text);

        // Process WebSocket command
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, text);

        if (!error) {
            String cmd = doc["command"];

            if (cmd == "subscribe") {
                // Subscribe to real-time updates
                webSocketClients[num] = true;
                debugPrintln("Client subscribed to updates");
            }
            else if (cmd == "unsubscribe") {
                // Unsubscribe from updates
                webSocketClients[num] = false;
                debugPrintln("Client unsubscribed from updates");
            }
            else if (cmd == "toggle_relay") {
                // Toggle relay command
                int relay = doc["relay"];
                bool state = doc["state"];

                debugPrintln("WebSocket: Toggling relay " + String(relay) + " to " + String(state ? "ON" : "OFF"));

                if (relay >= 0 && relay < 16) {
                    outputStates[relay] = state;

                    if (writeOutputs()) {
                        debugPrintln("Relay toggled successfully via WebSocket");

                        // Send response
                        DynamicJsonDocument responseDoc(512);
                        responseDoc["type"] = "relay_update";
                        responseDoc["relay"] = relay;
                        responseDoc["state"] = outputStates[relay];

                        String response;
                        serializeJson(responseDoc, response);
                        webSocket.sendTXT(num, response);

                        // Broadcast update to all subscribed clients
                        broadcastUpdate();
                    }
                    else {
                        // Send error response
                        DynamicJsonDocument errorDoc(512);
                        errorDoc["type"] = "error";
                        errorDoc["message"] = "Failed to write to relay";

                        String errorResponse;
                        serializeJson(errorDoc, errorResponse);
                        webSocket.sendTXT(num, errorResponse);

                        debugPrintln("ERROR: Failed to toggle relay via WebSocket");
                    }
                }
                else {
                    debugPrintln("ERROR: Invalid relay index: " + String(relay));
                }
            }
            else if (cmd == "get_protocol_config") {
                // Get protocol-specific configuration
                String protocol = doc["protocol"];

                DynamicJsonDocument responseDoc(1024);
                responseDoc["type"] = "protocol_config";
                responseDoc["protocol"] = protocol;

                if (protocol == "wifi") {
                    responseDoc["security"] = wifiSecurity;
                    responseDoc["hidden"] = wifiHidden;
                    responseDoc["mac_filter"] = wifiMacFilter;
                    responseDoc["auto_update"] = wifiAutoUpdate;
                    responseDoc["radio_mode"] = wifiRadioMode;
                    responseDoc["channel"] = wifiChannel;
                    responseDoc["channel_width"] = wifiChannelWidth;
                    responseDoc["dhcp_lease_time"] = wifiDhcpLeaseTime;
                    responseDoc["wmm_enabled"] = wifiWmmEnabled;
                    responseDoc["ssid"] = wifiSSID;
                }
                else if (protocol == "ethernet") {
                    responseDoc["dhcp_mode"] = dhcpMode;
                    if (!dhcpMode) {
                        responseDoc["ip"] = ip.toString();
                        responseDoc["gateway"] = gateway.toString();
                        responseDoc["subnet"] = subnet.toString();
                        responseDoc["dns1"] = dns1.toString();
                        responseDoc["dns2"] = dns2.toString();
                    }
                    if (ethConnected) {
                        responseDoc["eth_mac"] = ETH.macAddress();
                        responseDoc["eth_ip"] = ETH.localIP().toString();
                        responseDoc["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
                        responseDoc["eth_speed"] = String(ETH.linkSpeed()) + " Mbps";
                    }
                }
                else if (protocol == "usb") {
                    responseDoc["com_port"] = usbComPort;
                    responseDoc["baud_rate"] = usbBaudRate;
                    responseDoc["data_bits"] = usbDataBits;
                    responseDoc["parity"] = usbParity;
                    responseDoc["stop_bits"] = usbStopBits;
                }
                else if (protocol == "rs485") {
                    responseDoc["baud_rate"] = rs485BaudRate;
                    responseDoc["parity"] = rs485Parity;
                    responseDoc["data_bits"] = rs485DataBits;
                    responseDoc["stop_bits"] = rs485StopBits;
                    responseDoc["protocol_type"] = rs485Protocol;
                    responseDoc["comm_mode"] = rs485Mode;
                    responseDoc["device_address"] = rs485DeviceAddress;
                    responseDoc["flow_control"] = rs485FlowControl;
                    responseDoc["night_mode"] = rs485NightMode;
                }

                String response;
                serializeJson(responseDoc, response);
                webSocket.sendTXT(num, response);
            }
        }
        else {
            debugPrintln("ERROR: Invalid JSON in WebSocket message");
        }
    }
    break;
    }
}

void broadcastUpdate() {
    DynamicJsonDocument doc(4096);
    doc["type"] = "status_update";
    doc["time"] = getTimeString();
    doc["timestamp"] = millis(); // Add timestamp for freshness checking

    // Add output states
    JsonArray outputs = doc.createNestedArray("outputs");
    for (int i = 0; i < 16; i++) {
        JsonObject output = outputs.createNestedObject();
        output["id"] = i;
        output["state"] = outputStates[i];
    }

    // Add input states
    JsonArray inputs = doc.createNestedArray("inputs");
    for (int i = 0; i < 16; i++) {
        JsonObject input = inputs.createNestedObject();
        input["id"] = i;
        input["state"] = inputStates[i];
    }

    // Add direct input states (HT1-HT3)
    JsonArray directInputs = doc.createNestedArray("direct_inputs");
    for (int i = 0; i < 3; i++) {
        JsonObject input = directInputs.createNestedObject();
        input["id"] = i;
        input["state"] = directInputStates[i];
    }

    // Add HT sensors data
    JsonArray htSensors = doc.createNestedArray("htSensors");
    for (int i = 0; i < 3; i++) {
        JsonObject sensor = htSensors.createNestedObject();
        sensor["index"] = i;
        sensor["pin"] = "HT" + String(i + 1);
        sensor["sensorType"] = htSensorConfig[i].sensorType;

        const char* sensorTypeNames[] = {
            "Digital Input", "DHT11", "DHT22", "DS18B20"
        };
        sensor["sensorTypeName"] = sensorTypeNames[htSensorConfig[i].sensorType];

        switch (htSensorConfig[i].sensorType) {
        case SENSOR_TYPE_DIGITAL:
            sensor["value"] = directInputStates[i] ? "HIGH" : "LOW";
            break;

        case SENSOR_TYPE_DHT11:
        case SENSOR_TYPE_DHT22:
            sensor["temperature"] = htSensorConfig[i].temperature;
            sensor["humidity"] = htSensorConfig[i].humidity;
            break;

        case SENSOR_TYPE_DS18B20:
            sensor["temperature"] = htSensorConfig[i].temperature;
            break;
        }
    }

    // Add analog inputs
    JsonArray analog = doc.createNestedArray("analog");
    for (int i = 0; i < 4; i++) {
        JsonObject analogInput = analog.createNestedObject();
        analogInput["id"] = i;
        analogInput["value"] = analogValues[i];
        analogInput["voltage"] = analogVoltages[i];
        analogInput["percentage"] = calculatePercentage(analogVoltages[i]);
    }

    // Add system information
    doc["device"] = deviceName;
    doc["wifi_connected"] = wifiConnected;
    doc["wifi_client_mode"] = wifiClientMode;
    doc["wifi_ap_mode"] = apMode;
    doc["wifi_rssi"] = WiFi.RSSI();

    // Make sure to correctly set the WiFi IP address
    String wifiIpAddress = wifiClientMode ? WiFi.localIP().toString() : (apMode ? WiFi.softAPIP().toString() : "Not connected");
    doc["wifi_ip"] = wifiIpAddress;

    doc["eth_connected"] = ethConnected;
    doc["eth_ip"] = ethConnected ? ETH.localIP().toString() : "Not connected";

    // Set MAC address from appropriate source
    String macAddress = "";
    if (ethConnected) {
        macAddress = ETH.macAddress();
    }
    else if (wifiConnected) {
        macAddress = wifiClientMode ? WiFi.macAddress() : WiFi.softAPmacAddress();
    }
    doc["mac"] = macAddress;

    doc["uptime"] = getUptimeString();
    doc["active_protocol"] = getActiveProtocolName();
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["i2c_errors"] = i2cErrorCount;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["last_error"] = lastErrorMessage;

    // Add additional network info for better dashboard display
    doc["network"] = true;  // Flag to indicate network info is available

    String jsonString;
    serializeJson(doc, jsonString);

    // Send to all WebSocket clients
    webSocket.broadcastTXT(jsonString);
}

