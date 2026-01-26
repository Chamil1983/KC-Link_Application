// ApiStatus.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"
#include "esp_mac.h"


static String macBytesToString(const uint8_t m[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(buf);
}

static String readMacString(esp_mac_type_t type, const char* fallbackStr) {
    uint8_t m[6]{0};
    esp_read_mac(m, type);
    bool allZero = true;
    for (int i=0;i<6;i++) if (m[i] != 0) { allZero=false; break; }
    return allZero ? String(fallbackStr) : macBytesToString(m);
}

static String boardEfuseMacString() {
    uint64_t chipid = ESP.getEfuseMac();
    uint8_t bmac[6];
    bmac[0] = (chipid >> 40) & 0xFF;
    bmac[1] = (chipid >> 32) & 0xFF;
    bmac[2] = (chipid >> 24) & 0xFF;
    bmac[3] = (chipid >> 16) & 0xFF;
    bmac[4] = (chipid >> 8) & 0xFF;
    bmac[5] = (chipid >> 0) & 0xFF;
    return macBytesToString(bmac);
}

void handleSystemStatus() {
    DynamicJsonDocument doc(4096);

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
    JsonArray htSensorsData = doc.createNestedArray("htSensors");
    for (int i = 0; i < 3; i++) {
        JsonObject sensor = htSensorsData.createNestedObject();
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
    doc["dhcp_mode"] = dhcpMode;

    // Add detailed network information
    doc["wifi_connected"] = wifiConnected;
    doc["wifi_client_mode"] = wifiClientMode;
    doc["wifi_ap_mode"] = apMode;
    doc["eth_connected"] = ethConnected;
    doc["wifi_ssid"] = wifiSSID;
    doc["wifi_rssi"] = WiFi.RSSI();

    // Ethernet status split into "link" vs "has IP"
    bool ethLinkUp = ETH.linkUp();
    doc["eth_link_up"] = ethLinkUp;

    // Make sure to correctly set the WiFi IP address
    String wifiIpAddress = wifiClientMode ? WiFi.localIP().toString() : (apMode ? WiFi.softAPIP().toString() : "Not connected");
    doc["wifi_ip"] = wifiIpAddress;
    doc["eth_ip"] = ethConnected ? ETH.localIP().toString() : (ethLinkUp ? "Link up (no IP yet)" : "Not connected");

    // Set MAC address from appropriate source
    String macAddress = "";
    if (ethConnected) {
        macAddress = ETH.macAddress();
    }
    else if (wifiConnected) {
        macAddress = wifiClientMode ? WiFi.macAddress() : WiFi.softAPmacAddress();
    }
        doc["board_mac"] = boardEfuseMacString();
doc["mac"] = macAddress;

    // Generate device ID from MAC (like in the JavaScript function)
    String deviceId = "unknown";
    if (macAddress.length() > 0 && macAddress != "00:00:00:00:00:00") {
        // Remove colons and take last 6 characters
        deviceId = macAddress;
        deviceId.replace(":", "");
        deviceId = deviceId.substring(deviceId.length() - 6);
        deviceId.toUpperCase();
    }
    doc["device_id"] = deviceId;

    // Generate serial number
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char dateStr[9];
    sprintf(dateStr, "%02d%02d%02d", (timeinfo.tm_year + 1900) % 100, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    String serialNumber = /*String("KC868-A16-") +*/ String(dateStr) + "-" + deviceId;
    doc["serial_number"] = serialNumber;

    doc["uptime"] = getUptimeString();
    doc["active_protocol"] = getActiveProtocolName();
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["i2c_errors"] = i2cErrorCount;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["last_error"] = lastErrorMessage;

    // Network details in a nested object
    JsonObject networkDetails = doc.createNestedObject("network");
    networkDetails["dhcp_mode"] = dhcpMode;
    networkDetails["http_port"] = httpPort;
    networkDetails["ws_port"] = wsPort;

    // WiFi details
    if (wifiConnected) {
        if (wifiClientMode) {
            networkDetails["wifi_ip"] = WiFi.localIP().toString();
            networkDetails["wifi_gateway"] = WiFi.gatewayIP().toString();
            networkDetails["wifi_subnet"] = WiFi.subnetMask().toString();
            networkDetails["wifi_dns1"] = WiFi.dnsIP(0).toString();
            networkDetails["wifi_dns2"] = WiFi.dnsIP(1).toString();
            networkDetails["wifi_rssi"] = WiFi.RSSI();
            networkDetails["wifi_mac"] = readMacString(ESP_MAC_WIFI_STA, WIFI_STA_MAC);
            networkDetails["wifi_ssid"] = wifiSSID;
        }
        else if (apMode) {
            networkDetails["wifi_mode"] = "Access Point";
            networkDetails["wifi_ap_ip"] = WiFi.softAPIP().toString();
            networkDetails["wifi_ap_mac"] = readMacString(ESP_MAC_WIFI_SOFTAP, WIFI_AP_MAC);
            networkDetails["wifi_ap_ssid"] = ap_ssid;
        }
    }

    // Ethernet details
    if (ethConnected) {
        networkDetails["eth_ip"] = ETH.localIP().toString();
        networkDetails["eth_gateway"] = ETH.gatewayIP().toString();
        networkDetails["eth_subnet"] = ETH.subnetMask().toString();
        networkDetails["eth_dns1"] = ETH.dnsIP(0).toString();
        networkDetails["eth_dns2"] = ETH.dnsIP(1).toString();
        networkDetails["eth_mac"] = readMacString(ESP_MAC_ETH, ETHERNET_MAC);
        networkDetails["eth_speed"] = String(ETH.linkSpeed()) + " Mbps";
        networkDetails["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
    }

    // Add flag indicating network info is available
    doc["rtc_initialized"] = rtcInitialized;
    doc["network_available"] = true;

    // Convenience top-level MACs for UI
    doc["eth_mac"] = readMacString(ESP_MAC_ETH, ETHERNET_MAC);
doc["wifi_sta_mac"] = readMacString(ESP_MAC_WIFI_STA, WIFI_STA_MAC);
doc["wifi_ap_mac"] = readMacString(ESP_MAC_WIFI_SOFTAP, WIFI_AP_MAC);
String jsonResponse;
    serializeJson(doc, jsonResponse);

    server.send(200, "application/json", jsonResponse);
}

