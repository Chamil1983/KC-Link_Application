// ApiConfig.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleConfig() {
    DynamicJsonDocument doc(1024);

    doc["device_name"] = deviceName;
    doc["dhcp_mode"] = dhcpMode;
    doc["debug_mode"] = debugMode;
    doc["wifi_ssid"] = wifiSSID;  // Only send SSID, not password
    doc["firmware_version"] = firmwareVersion;

    if (!dhcpMode) {
        doc["ip"] = ip.toString();
        doc["gateway"] = gateway.toString();
        doc["subnet"] = subnet.toString();
        doc["dns1"] = dns1.toString();
        doc["dns2"] = dns2.toString();
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            bool changed = false;

            if (doc.containsKey("device_name")) {
                deviceName = doc["device_name"].as<String>();
                changed = true;
            }

            if (doc.containsKey("debug_mode")) {
                debugMode = doc["debug_mode"];
                changed = true;
            }

            if (doc.containsKey("dhcp_mode")) {
                dhcpMode = doc["dhcp_mode"];

                // Static IP settings
                if (!dhcpMode) {
                    if (doc.containsKey("ip") && doc.containsKey("gateway") &&
                        doc.containsKey("subnet") && doc.containsKey("dns1")) {

                        ip.fromString(doc["ip"].as<String>());
                        gateway.fromString(doc["gateway"].as<String>());
                        subnet.fromString(doc["subnet"].as<String>());
                        dns1.fromString(doc["dns1"].as<String>());

                        if (doc.containsKey("dns2")) {
                            dns2.fromString(doc["dns2"].as<String>());
                        }
                    }
                }

                changed = true;
            }

            if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_password")) {
                String newSSID = doc["wifi_ssid"].as<String>();
                String newPass = doc["wifi_password"].as<String>();

                // Only update if values provided
                if (newSSID.length() > 0) {
                    // Store WiFi credentials in EEPROM
                    saveWiFiCredentials(newSSID, newPass);
                    changed = true;
                }
            }

            if (changed) {
                saveConfiguration();
                response = "{\"status\":\"success\",\"msg\":\"Configuration updated. Device will restart.\"}";

                // Set flag to restart device after response is sent
                restartRequired = true;
            }
        }
    }

    server.send(200, "application/json", response);

    // Restart if needed
    if (restartRequired) {
        delay(1000);
        ESP.restart();
    }
}

