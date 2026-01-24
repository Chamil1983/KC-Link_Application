// NetworkManager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

#include "Network.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"


// Keep this MAC identical to the proven LAN8720.ino test sketch.
// NOTE: If your network uses MAC filtering / whitelisting, this is critical.
//#define ETHERNET_MAC "C8:2E:A3:F5:7D:DA"


// State for DHCP recovery (copied from LAN8720.ino test logic)
static volatile bool g_linkUp = false;
static volatile bool g_gotDhcpIp = false;

static bool     g_usingLinkLocal = false;
static uint32_t g_linkUpAtMs = 0;
static uint8_t  g_dhcpKicks = 0;

static bool g_networkEventsReady = false;

static bool parseMac(const char *s, uint8_t mac[6]) {
    auto hexVal = [](char c)->int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    int idx = 0, hi = -1;
    for (const char *p = s; *p && idx < 6; ++p) {
        if (*p == ':' || *p == '-') continue;
        int v = hexVal(*p);
        if (v < 0) return false;
        if (hi < 0) hi = v;
        else {
            mac[idx++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }
    return (idx == 6 && hi < 0);
}

static void printEthInfo(const char *tag) {
    Serial.println();
    Serial.printf("========== Ethernet Info (%s) ==========\n", tag);
    Serial.print ("MAC      : "); Serial.println(ETH.macAddress());
    Serial.print ("Link     : "); Serial.println(ETH.linkUp() ? "UP" : "DOWN");
    Serial.print ("IPv4     : "); Serial.println(ETH.localIP());
    Serial.print ("Mask     : "); Serial.println(ETH.subnetMask());
    Serial.print ("Gateway  : "); Serial.println(ETH.gatewayIP());
    Serial.print ("DNS1     : "); Serial.println(ETH.dnsIP(0));
    Serial.println("========================================");
    Serial.println();
}

static void setCustomEthMacAndHostname() {
    // Preserve the existing project hostname semantics
    ETH.setHostname(deviceName.c_str());

    uint8_t macBytes[6];
    if (!parseMac((ethMacConfig.length() ? ethMacConfig.c_str() : ETHERNET_MAC), macBytes)) {
        Serial.println("[MAC] Invalid ETHERNET_MAC format!");
        return;
    }

    esp_err_t err = esp_iface_mac_addr_set(macBytes, ESP_MAC_ETH);
    if (err == ESP_OK) {
        Serial.printf("[MAC] Ethernet MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      macBytes[0], macBytes[1], macBytes[2], macBytes[3], macBytes[4], macBytes[5]);
    } else {
        Serial.printf("[MAC] Failed to set Ethernet MAC (err=%d)\n", (int)err);
    }
}

static void setCustomWiFiStaMacAndHostname() {

// Hostname for STA
WiFi.setHostname(deviceName.c_str());

uint8_t macBytes[6];
if (!parseMac((wifiStaMacConfig.length() ? wifiStaMacConfig.c_str() : WIFI_STA_MAC), macBytes)) {
    Serial.println("[MAC] Invalid WIFI_STA_MAC format!");
    return;
}

// Set at netif layer
esp_err_t err1 = esp_iface_mac_addr_set(macBytes, ESP_MAC_WIFI_STA);

// Also set at WiFi driver layer (more reliable)
esp_err_t err2 = esp_wifi_set_mac(WIFI_IF_STA, macBytes);

if (err1 == ESP_OK && err2 == ESP_OK) {
    Serial.printf("[MAC] WiFi STA MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  macBytes[0], macBytes[1], macBytes[2], macBytes[3], macBytes[4], macBytes[5]);
} else {
    Serial.printf("[MAC] Failed to set WiFi STA MAC (netif=%d, wifi=%d)\n", (int)err1, (int)err2);
}
}


static void setCustomWiFiApMacNetifOnly() {
    uint8_t macBytes[6];
    if (!parseMac((wifiApMacConfig.length() ? wifiApMacConfig.c_str() : WIFI_AP_MAC), macBytes)) return;
    esp_iface_mac_addr_set(macBytes, ESP_MAC_WIFI_SOFTAP);
}


static void presetWiFiMacsNetifOnly() {
    uint8_t sta[6];
    if (parseMac((wifiStaMacConfig.length()? wifiStaMacConfig.c_str(): WIFI_STA_MAC), sta)) {
        esp_iface_mac_addr_set(sta, ESP_MAC_WIFI_STA);
    }
    uint8_t ap[6];
    if (parseMac((wifiApMacConfig.length()? wifiApMacConfig.c_str(): WIFI_AP_MAC), ap)) {
        esp_iface_mac_addr_set(ap, ESP_MAC_WIFI_SOFTAP);
    }
}


static void setCustomWiFiApMac() {

uint8_t macBytes[6];
if (!parseMac((wifiApMacConfig.length() ? wifiApMacConfig.c_str() : WIFI_AP_MAC), macBytes)) {
    Serial.println("[MAC] Invalid WIFI_AP_MAC format!");
    return;
}

// Set at netif layer
esp_err_t err1 = esp_iface_mac_addr_set(macBytes, ESP_MAC_WIFI_SOFTAP);

// Also set at WiFi driver layer (requires AP-capable mode)
esp_err_t err2 = esp_wifi_set_mac(WIFI_IF_AP, macBytes);

if (err1 == ESP_OK && err2 == ESP_OK) {
    Serial.printf("[MAC] WiFi AP MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  macBytes[0], macBytes[1], macBytes[2], macBytes[3], macBytes[4], macBytes[5]);
} else {
    Serial.printf("[MAC] Failed to set WiFi AP MAC (netif=%d, wifi=%d)\n", (int)err1, (int)err2);
}
}

static void dhcpKick(const char *why) {
    esp_netif_t *netif = ETH.netif();
    if (!netif) {
        Serial.println("[DHCP] ETH.netif() is null (too early?)");
        return;
    }

    Serial.printf("[DHCP] Kick DHCP (%s), kick=%u\n", why, (unsigned)(g_dhcpKicks + 1));

    // Stop DHCP (ignore error if not started)
    esp_netif_dhcpc_stop(netif);

    // Clear IP info to 0.0.0.0
    esp_netif_ip_info_t ipinfo{};
    esp_netif_set_ip_info(netif, &ipinfo);

    // Start DHCP
    esp_err_t err = esp_netif_dhcpc_start(netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        Serial.printf("[DHCP] dhcpc_start failed, err=%d\n", (int)err);
    }

    g_dhcpKicks++;
}

static void configureLinkLocal() {
    esp_netif_t *netif = ETH.netif();
    if (!netif) return;

    uint8_t macBytes[6];
    if (!parseMac((ethMacConfig.length() ? ethMacConfig.c_str() : ETHERNET_MAC), macBytes)) return;

    // Deterministic 169.254.X.Y from MAC (valid 1..254)
    uint8_t x = (uint8_t)(1 + (macBytes[4] % 254));
    uint8_t y = (uint8_t)(1 + (macBytes[5] % 254));

    IPAddress ip(169, 254, x, y);
    IPAddress mask(255, 255, 0, 0);
    IPAddress zero(0, 0, 0, 0);

    // Stop DHCP so it doesn't overwrite our static address
    esp_netif_dhcpc_stop(netif);

    // Set static IP into netif
    esp_netif_ip_info_t ipinfo{};
    ipinfo.ip.addr      = (uint32_t)ip;
    ipinfo.netmask.addr = (uint32_t)mask;
    ipinfo.gw.addr      = (uint32_t)zero;

    esp_netif_set_ip_info(netif, &ipinfo);

    // Also reflect into Arduino ETH helpers
    ETH.config(ip, zero, mask, zero, zero);

    g_usingLinkLocal = true;
    g_gotDhcpIp = false;

    // Treat link-local as "connected enough" for local management
    ethConnected = true;
    wiredMode = true;
    mac = ETH.macAddress();

    Serial.println("[LL] DHCP not available. Using Link-Local IPv4:");
    printEthInfo("LINK-LOCAL");

    broadcastUpdate();
}

// WARNING: called from another task
void onEthEvent(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("[ETH] Started");
            setCustomEthMacAndHostname();

    // Pre-set WiFi MACs at netif layer so UI displays assigned values even if WiFi is not started
    presetWiFiMacsNetifOnly();
            break;

        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("[ETH] Link UP");
            g_linkUp = true;
            g_linkUpAtMs = millis();
            g_dhcpKicks = 0;
            g_usingLinkLocal = false;
            g_gotDhcpIp = false;
            wiredMode = true;

            if (dhcpMode) {
                // Trigger initial DHCP start (some networks need it)
                dhcpKick("link-up");
            } else {
                // Static mode: apply configured IP now that link is up
                ETH.config(ip, gateway, subnet, dns1, dns2);
                ethConnected = true;
                mac = ETH.macAddress();
                broadcastUpdate();
                printEthInfo("STATIC");
            }
            break;

        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.println("[ETH] Got IP (DHCP)");
            g_gotDhcpIp = true;
            g_usingLinkLocal = false;

            ethConnected = true;
            wiredMode = true;

            // If Ethernet connected, disable WiFi client mode (but keep AP if active)
            if (wifiClientMode && !apMode) {
                WiFi.disconnect();
                wifiClientMode = false;
                wifiConnected = false;
            }

            mac = ETH.macAddress();
            broadcastUpdate();
            printEthInfo("DHCP");
            break;

        case ARDUINO_EVENT_ETH_LOST_IP:
            Serial.println("[ETH] Lost IP");
            g_gotDhcpIp = false;
            ethConnected = false; // no usable IP
            broadcastUpdate();
            break;

        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("[ETH] Link DOWN");
            g_linkUp = false;
            g_gotDhcpIp = false;
            g_usingLinkLocal = false;
            ethConnected = false;
            wiredMode = false;
            broadcastUpdate();
            break;

        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("[ETH] Stopped");
            g_linkUp = false;
            g_gotDhcpIp = false;
            g_usingLinkLocal = false;
            ethConnected = false;
            wiredMode = false;
            broadcastUpdate();
            break;

        default:
            break;
    }
}

void serviceEthernetDhcp() {
    // Periodic DHCP recovery + link-local fallback (same proven logic as LAN8720.ino)
    if (!dhcpMode) return;
    if (!g_linkUp) return;

    bool hasAnyIp = (ETH.localIP() != IPAddress(0, 0, 0, 0));
    const uint32_t DHCP_TIMEOUT_MS = 12000;
    const uint8_t  MAX_DHCP_KICKS  = 3;

    if (!hasAnyIp && !g_usingLinkLocal) {
        if (millis() - g_linkUpAtMs > DHCP_TIMEOUT_MS) {
            if (g_dhcpKicks < MAX_DHCP_KICKS) {
                dhcpKick("timeout");
                g_linkUpAtMs = millis(); // extend window after each kick
            } else {
                configureLinkLocal();
            }
        }
    } else if (hasAnyIp && !ethConnected) {
        // If we got an IP but the event was missed, mark connected
        ethConnected = true;
        wiredMode = true;
        mac = ETH.macAddress();
        broadcastUpdate();
    }
}

void initWiFi() {
    // Register event handler
    WiFi.onEvent(WiFiEvent);

    // Configure WiFi in STA mode
    WiFi.mode(WIFI_STA);
    delay(50);

    // Set custom MAC/hostname for STA (requested)
    setCustomWiFiStaMacAndHostname();

    // Pre-set AP MAC at netif layer so UI can display it even before AP starts
    setCustomWiFiApMacNetifOnly();

    // Configure WiFi in DHCP or static mode (separate from Ethernet)
    if (wifiDhcpMode) {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    } else {
        WiFi.config(wifiStaIp, wifiStaGateway, wifiStaSubnet, wifiStaDns1, wifiStaDns2);
    }

    // Attempt to connect if credentials exist
    if (wifiSSID.length() > 0) {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        debugPrintln("Connecting to WiFi SSID: " + wifiSSID);

        // Wait for connection with timeout
        int connectionAttempts = 0;
        while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
            delay(500);
            Serial.print(".");
            connectionAttempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            wifiClientMode = true;
            apMode = false;

            Serial.println("");
            Serial.print("Connected to WiFi. IP address: ");
            Serial.println(WiFi.localIP());
            mac = WiFi.macAddress();

            // After successful connection, update saved credentials
            saveWiFiCredentials(wifiSSID, wifiPassword);
            return;
        }
    }

    // If we reach here, connection failed or no credentials - start AP
    startAPMode();
}

void startAPMode() {
    WiFi.disconnect();
    delay(100);

    // Start AP mode
    WiFi.mode(WIFI_AP);
    delay(50);
    // Set custom AP MAC (requested)
    setCustomWiFiApMac();
    WiFi.softAP(ap_ssid, ap_password);

    apMode = true;
    wifiClientMode = false;
    wifiConnected = true; // Device is "connected" in AP mode

    Serial.println("Failed to connect as client. Starting AP Mode");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Update displayed MAC
    mac = WiFi.softAPmacAddress();

    // Start DNS server for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());
}



void resetEthernet() {
    debugPrintln("Performing software reset of Ethernet module...");

    // IMPORTANT:
    // In the modular refactor we must NOT call ETH.begin() here.
    // Calling ETH.begin() twice (resetEthernet() + initEthernet()) can leave the
    // ESP32 Ethernet netif/DHCP client in a bad state where the link is UP but
    // DHCP never obtains an IP address (exactly what the log shows).
    //
    // KC868-A16 has ETH_PHY_POWER = -1, so we can't power-cycle the PHY.
    // The safest "reset" is to briefly yield and let initEthernet() do a single
    // clean ETH.begin() call.
    delay(200);

    debugPrintln("Ethernet software reset complete");
}

// Start Ethernet with a specific PHY address.
// Some LAN8720 boards use address 0, others use address 1.
static bool beginEthernetWithPhyAddr(int phyAddr) {
    debugPrintln("Starting Ethernet (LAN8720) with PHY addr=" + String(phyAddr));
    // Use the board defines from Definitions.h to avoid accidental mismatches
    // across different KinCony variants / project configurations.
    bool ok = ETH.begin(ETH_PHY_TYPE, phyAddr, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
    if (!ok) {
        debugPrintln("ETH.begin() returned false");
    }
    return ok;
}


void initEthernet() {
    // Ensure WiFi radio is off while we bring up Ethernet (avoids edge-case
    // resource conflicts on some Arduino-ESP32 builds).
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    // Start the unified Network event system (required/recommended on ESP32 core v3.x).
    if (!g_networkEventsReady) {
        Network.begin();
        Network.onEvent(onEthEvent);
        g_networkEventsReady = true;
    }

    // Ensure flags are clean
    ethConnected = false;
    wiredMode = false;
    g_linkUp = false;
    g_gotDhcpIp = false;
    g_usingLinkLocal = false;
    g_linkUpAtMs = millis();
    g_dhcpKicks = 0;

    debugPrintln("Starting Ethernet (LAN8720)...");
    bool ok = ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
    if (!ok) {
        debugPrintln("ETH.begin() returned false");
        ethConnected = false;
        wiredMode = false;
        return;
    }

    // Configure in DHCP or static mode
    if (!dhcpMode) {
        ETH.config(ip, gateway, subnet, dns1, dns2);
    }

    // Wait for Ethernet link (short timeout)
    const uint32_t LINK_TIMEOUT_MS = 12000;
    uint32_t linkStart = millis();
    while (!ETH.linkUp() && (millis() - linkStart) < LINK_TIMEOUT_MS) {
        delay(200);
    }

    if (!ETH.linkUp()) {
        wiredMode = false;
        ethConnected = false;
        debugPrintln("Ethernet link is DOWN. Check cable connection.");
        return;
    }

    wiredMode = true;
    debugPrintln("Ethernet link is UP!");

    // Ensure DHCP client is started (some builds require an explicit kick)
    if (dhcpMode && g_dhcpKicks == 0) {
        dhcpKick("post-link");
        g_linkUp = true;
        g_linkUpAtMs = millis();
    }


    // Wait for IP address (DHCP) using the proven LAN8720.ino recovery logic
    if (dhcpMode) {
        const uint32_t OVERALL_TIMEOUT_MS = 45000; // includes DHCP kicks + link-local fallback window
        uint32_t startMs = millis();
        while (!ethConnected && (millis() - startMs) < OVERALL_TIMEOUT_MS) {
            serviceEthernetDhcp();
            if (ETH.localIP() != IPAddress(0, 0, 0, 0)) {
                // We have an IP now; event may have already set ethConnected
                ethConnected = true;
                break;
            }
            if (g_usingLinkLocal) {
                // Link-local already configured inside configureLinkLocal()
                break;
            }
            delay(200);
        }

        if (!ethConnected && !g_usingLinkLocal) {
            debugPrintln("Failed to get IP address via Ethernet (DHCP)");
        }
    } else {
        // Static: consider connected if IP is non-zero
        if (ETH.localIP() != IPAddress(0, 0, 0, 0)) {
            ethConnected = true;
        }
    }

    // Update system data and display
    mac = ETH.macAddress();
    broadcastUpdate();

    debugPrintln("Ethernet init complete. IP=" + ETH.localIP().toString());
}



void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        wifiConnected = true;
        wifiClientMode = true;
        apMode = false;
        debugPrintln("WiFi connected. IP: " + WiFi.localIP().toString());
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (wifiClientMode) {
            wifiConnected = false;
            wifiClientMode = false;
            debugPrintln("WiFi connection lost");

            // If not in AP mode and Ethernet not available, start AP
            if (!apMode && !wiredMode) {
                // Try reconnecting to WiFi a few times
                int reconnectAttempts = 0;
                while (reconnectAttempts < 3) {
                    WiFi.reconnect();
                    delay(3000);
                    if (WiFi.status() == WL_CONNECTED) {
                        wifiConnected = true;
                        wifiClientMode = true;
                        debugPrintln("WiFi reconnected");
                        return;
                    }
                    reconnectAttempts++;
                }

                // If reconnection fails, start AP mode
                startAPMode();
            }
        }
        break;
    default:
        break;
    }
}


// -----------------------------
// MAC Summary (Test Snippet)
// -----------------------------
static String bytesToMacString(const uint8_t m[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(buf);
}

void printMacSummary() {
    uint64_t chipid = ESP.getEfuseMac();
    uint8_t bmac[6];
    bmac[0] = (chipid >> 40) & 0xFF;
    bmac[1] = (chipid >> 32) & 0xFF;
    bmac[2] = (chipid >> 24) & 0xFF;
    bmac[3] = (chipid >> 16) & 0xFF;
    bmac[4] = (chipid >> 8) & 0xFF;
    bmac[5] = (chipid >> 0) & 0xFF;

    uint8_t ethMac[6]{0}, staMac[6]{0}, apMac[6]{0};
    esp_read_mac(ethMac, ESP_MAC_ETH);
    esp_read_mac(staMac, ESP_MAC_WIFI_STA);
    esp_read_mac(apMac, ESP_MAC_WIFI_SOFTAP);

    Serial.println("=== MAC Summary (Test) ===");
    Serial.print("Board MAC (efuse): "); Serial.println(bytesToMacString(bmac));
    Serial.print("ETH MAC   (esp_read_mac): "); Serial.println(bytesToMacString(ethMac));
    Serial.print("STA MAC   (esp_read_mac): "); Serial.println(bytesToMacString(staMac));
    Serial.print("AP  MAC   (esp_read_mac): "); Serial.println(bytesToMacString(apMac));
    Serial.println("==========================");
}


