// ApiNetwork.cpp
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
    // If still zero, fall back
    bool allZero = true;
    for (int i=0;i<6;i++) if (m[i] != 0) { allZero=false; break; }
    return allZero ? String(fallbackStr) : macBytesToString(m);
}

void handleNetworkSettings() {
    DynamicJsonDocument doc(2048);

    // -----------------------------
    // Persisted configuration
    // -----------------------------
    doc["eth_dhcp_mode"] = dhcpMode;
    doc["eth_static_ip"] = ip.toString();
    doc["eth_static_gateway"] = gateway.toString();
    doc["eth_static_subnet"] = subnet.toString();
    doc["eth_static_dns1"] = dns1.toString();
    doc["eth_static_dns2"] = dns2.toString();

    doc["wifi_dhcp_mode"] = wifiDhcpMode;
    doc["wifi_ssid"] = wifiSSID;
    doc["wifi_password"] = wifiPassword; // requested: visible in UI
    doc["wifi_static_ip"] = wifiStaIp.toString();
    doc["wifi_static_gateway"] = wifiStaGateway.toString();
    doc["wifi_static_subnet"] = wifiStaSubnet.toString();
    doc["wifi_static_dns1"] = wifiStaDns1.toString();
    doc["wifi_static_dns2"] = wifiStaDns2.toString();

    doc["http_port"] = httpPort;
    doc["ws_port"] = wsPort;

    // -----------------------------
    // Live status
    // -----------------------------
    doc["eth_connected"] = ethConnected;
    doc["eth_link_up"] = ETH.linkUp();
    doc["eth_mac"] = readMacString(ESP_MAC_ETH, ETHERNET_MAC);
if (ETH.linkUp()) {
        int spd = ETH.linkSpeed();
        doc["eth_speed_mbps"] = spd;
        doc["eth_speed"] = (spd > 0) ? (String(spd) + " Mbps") : "Unknown";
        doc["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
    } else {
        doc["eth_speed_mbps"] = 0;
        doc["eth_speed"] = "Unknown";
        doc["eth_duplex"] = "Unknown";
    }

    if (ethConnected) {
        doc["eth_ip"] = ETH.localIP().toString();
        doc["eth_gateway"] = ETH.gatewayIP().toString();
        doc["eth_subnet"] = ETH.subnetMask().toString();
        doc["eth_dns1"] = ETH.dnsIP(0).toString();
        doc["eth_dns2"] = ETH.dnsIP(1).toString();
    } else {
        doc["eth_ip"] = "0.0.0.0";
        doc["eth_gateway"] = "0.0.0.0";
        doc["eth_subnet"] = "0.0.0.0";
        doc["eth_dns1"] = "0.0.0.0";
        doc["eth_dns2"] = "0.0.0.0";
    }

    doc["wifi_connected"] = wifiConnected;
    doc["wifi_client_mode"] = wifiClientMode;
    doc["wifi_ap_mode"] = apMode;
    doc["wifi_rssi"] = wifiClientMode ? WiFi.RSSI() : 0;

    doc["wifi_sta_mac"] = readMacString(ESP_MAC_WIFI_STA, WIFI_STA_MAC);
doc["wifi_ap_mac"] = readMacString(ESP_MAC_WIFI_SOFTAP, WIFI_AP_MAC);
if (wifiClientMode) {
        doc["wifi_ip"] = WiFi.localIP().toString();
        doc["wifi_gateway"] = WiFi.gatewayIP().toString();
        doc["wifi_subnet"] = WiFi.subnetMask().toString();
        doc["wifi_dns1"] = WiFi.dnsIP(0).toString();
        doc["wifi_dns2"] = WiFi.dnsIP(1).toString();
    } else if (apMode) {
        doc["wifi_ip"] = WiFi.softAPIP().toString();
        doc["wifi_gateway"] = "0.0.0.0";
        doc["wifi_subnet"] = "0.0.0.0";
        doc["wifi_dns1"] = "0.0.0.0";
        doc["wifi_dns2"] = "0.0.0.0";
    } else {
        doc["wifi_ip"] = "0.0.0.0";
        doc["wifi_gateway"] = "0.0.0.0";
        doc["wifi_subnet"] = "0.0.0.0";
        doc["wifi_dns1"] = "0.0.0.0";
        doc["wifi_dns2"] = "0.0.0.0";
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateNetworkSettings() {
    DynamicJsonDocument resp(512);
    resp["status"] = "error";
    resp["message"] = "Invalid request";

    bool changed = false;
    bool needRestart = false; // Option 1: restart on save
    bool reconnectWifiOnly = false;

    if (!server.hasArg("plain")) {
        String out;
        serializeJson(resp, out);
        server.send(200, "application/json", out);
        return;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        resp["message"] = "JSON parse error";
        String out;
        serializeJson(resp, out);
        server.send(200, "application/json", out);
        return;
    }

    // -----------------------------
    // Ethernet config
    // -----------------------------
    if (doc.containsKey("eth_dhcp_mode")) {
        bool newMode = doc["eth_dhcp_mode"].as<bool>();
        if (newMode != dhcpMode) {
            dhcpMode = newMode;
            changed = true;
            needRestart = true;
        }
    }

    if (doc.containsKey("eth_static_ip")) {
        String s = doc["eth_static_ip"].as<String>();
        if (s.length() > 0 && s != ip.toString()) {
            ip.fromString(s);
            changed = true;
            needRestart = true;
        }
    }
    if (doc.containsKey("eth_static_gateway")) {
        String s = doc["eth_static_gateway"].as<String>();
        if (s.length() > 0 && s != gateway.toString()) {
            gateway.fromString(s);
            changed = true;
            needRestart = true;
        }
    }
    if (doc.containsKey("eth_static_subnet")) {
        String s = doc["eth_static_subnet"].as<String>();
        if (s.length() > 0 && s != subnet.toString()) {
            subnet.fromString(s);
            changed = true;
            needRestart = true;
        }
    }
    if (doc.containsKey("eth_static_dns1")) {
        String s = doc["eth_static_dns1"].as<String>();
        if (s.length() > 0 && s != dns1.toString()) {
            dns1.fromString(s);
            changed = true;
            needRestart = true;
        }
    }
    if (doc.containsKey("eth_static_dns2")) {
        String s = doc["eth_static_dns2"].as<String>();
        if (s.length() > 0 && s != dns2.toString()) {
            dns2.fromString(s);
            changed = true;
            needRestart = true;
        }
    }

    // -----------------------------
    // WiFi config
    // -----------------------------
    if (doc.containsKey("wifi_dhcp_mode")) {
        bool newMode = doc["wifi_dhcp_mode"].as<bool>();
        if (newMode != wifiDhcpMode) {
            wifiDhcpMode = newMode;
            changed = true;
            needRestart = true;
        }
    }

    auto updateWiFiIp = [&](const char* key, IPAddress &dst, bool &flag) {
        if (!doc.containsKey(key)) return;
        String s = doc[key].as<String>();
        if (s.length() == 0) return;
        if (s != dst.toString()) {
            dst.fromString(s);
            flag = true;
        }
    };

    bool wifiStaticChanged = false;
    updateWiFiIp("wifi_static_ip", wifiStaIp, wifiStaticChanged);
    updateWiFiIp("wifi_static_gateway", wifiStaGateway, wifiStaticChanged);
    updateWiFiIp("wifi_static_subnet", wifiStaSubnet, wifiStaticChanged);
    updateWiFiIp("wifi_static_dns1", wifiStaDns1, wifiStaticChanged);
    updateWiFiIp("wifi_static_dns2", wifiStaDns2, wifiStaticChanged);
    if (wifiStaticChanged) {
        changed = true;
        needRestart = true;
    }

    if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_password")) {
        String newSSID = doc["wifi_ssid"].as<String>();
        String newPass = doc["wifi_password"].as<String>();
        if (newSSID.length() > 0 && (newSSID != wifiSSID || newPass != wifiPassword)) {
            saveWiFiCredentials(newSSID, newPass);
            changed = true;
            // If only credentials were changed, we can reconnect without reboot.
            reconnectWifiOnly = !needRestart;
        }
    }

    // -----------------------------
    // Ports (Option A)
    // -----------------------------
    if (doc.containsKey("http_port")) {
        int p = doc["http_port"].as<int>();
        if (p >= 1 && p <= 65535 && p != httpPort) {
            httpPort = p;
            changed = true;
            needRestart = true;
        }
    }
    if (doc.containsKey("ws_port")) {
        int p = doc["ws_port"].as<int>();
        if (p >= 1 && p <= 65535 && p != wsPort) {
            wsPort = p;
            changed = true;
            needRestart = true;
        }
    }

    if (changed) {
        saveNetworkSettings();
        resp["status"] = "success";
        resp["restart_required"] = needRestart;
        resp["http_port"] = httpPort;
        resp["ws_port"] = wsPort;
        if (needRestart) {
            resp["message"] = "Network settings saved. Device will restart.";
            restartRequired = true;
        } else if (reconnectWifiOnly) {
            resp["message"] = "WiFi credentials saved. Reconnecting...";
        } else {
            resp["message"] = "Network settings saved.";
        }
    } else {
        resp["status"] = "success";
        resp["restart_required"] = false;
        resp["message"] = "No changes to network settings";
        resp["http_port"] = httpPort;
        resp["ws_port"] = wsPort;
    }

    String out;
    serializeJson(resp, out);
    server.send(200, "application/json", out);

    if (restartRequired) {
        delay(1000);
        ESP.restart();
    }
}

