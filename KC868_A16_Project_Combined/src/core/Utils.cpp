// Utils.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void generateAndDisplaySerialNumber() {
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Format date as YYMMDD
    char dateStr[9];
    sprintf(dateStr, "%02d%02d%02d", (timeinfo.tm_year + 1900) % 100, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    // Get MAC address from appropriate source
    String macAddress = "";
    if (ethConnected) {
        macAddress = ETH.macAddress();
    }
    else if (wifiConnected) {
        macAddress = wifiClientMode ? WiFi.macAddress() : WiFi.softAPmacAddress();
    }

    // Generate device ID from MAC
    String deviceId = "unknown";
    if (macAddress.length() > 0 && macAddress != "00:00:00:00:00:00") {
        // Remove colons and take last 6 characters
        deviceId = macAddress;
        deviceId.replace(":", "");
        deviceId = deviceId.substring(deviceId.length() - 6);
        deviceId.toUpperCase();
    }

    // Generate serial number format: KC868-A16-YYMMDD-XXXX where XXXX is the device ID
    String serialNumber = /*String("KC868-A16-") +*/ String(dateStr) + "-" + deviceId;

    debugPrintln("Device ID: " + deviceId);
    debugPrintln("Serial Number: " + serialNumber);
}

String getUptimeString() {
    unsigned long uptime = millis() / 1000; // Convert to seconds

    unsigned long days = uptime / 86400;
    uptime %= 86400;

    unsigned long hours = uptime / 3600;
    uptime %= 3600;

    unsigned long minutes = uptime / 60;
    unsigned long seconds = uptime % 60;

    char uptimeStr[30];
    if (days > 0) {
        sprintf(uptimeStr, "%ld days, %02ld:%02ld:%02ld", days, hours, minutes, seconds);
    }
    else {
        sprintf(uptimeStr, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    }

    return String(uptimeStr);
}

String getActiveProtocolName() {
    // Convert the protocol name to uppercase for display
    String protocolName = currentCommunicationProtocol;

    if (protocolName == "wifi") {
        return "WiFi";
    }
    else if (protocolName == "ethernet") {
        return "Ethernet";
    }
    else if (protocolName == "rs485") {
        return "RS-485";
    }
    else if (protocolName == "usb") {
        return "USB";
    }

    // Return as-is if unknown
    return protocolName;
}

