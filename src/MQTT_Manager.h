#pragma once
/*
  MQTT_Manager.h (Refined)
  Added:
    - sys/heap_free, sys/failed_conn_count, sys/reconnect_backoff_ms publishing.
    - rtc/alarm1_active, rtc/alarm1_time topics.
    - Public getters for failed connection count & backoff for status display.
    - Extended snapshot JSON with new health + RTC alarm fields.
*/

#include <Arduino.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include "EthernetDriver.h"

#include "Config.h"
#include "Debug.h"
#include "DigitalInputDriver.h"
#include "RelayDriver.h"
#include "DACControl.h"
#include "AnalogInputs.h"
#include "DHT_Sensors.h"
#include "RTCManager.h"

#ifndef MQTT_RETAIN_SNAPSHOT
#define MQTT_RETAIN_SNAPSHOT 1
#endif
#ifndef MQTT_RETAIN_STATES
#define MQTT_RETAIN_STATES 0
#endif

struct MQTTConfig {
    String   broker;
    uint16_t port = 1883;
    String   username;
    String   password;
    String   baseTopic = "cl/a8rm";
    String   clientId;
    bool     useDHCP = true;
    bool     ethUseDHCP = true;
    String   ethMAC = "DE:AD:BE:EF:FE:ED";
    String   ethIP = "";
    String   ethDNS = "8.8.8.8";
    String   ethGW = "";
    String   ethMASK = "255.255.255.0";
};

class MQTTManager {
public:
    enum NetMode : uint8_t { NET_WIFI = 0, NET_ETH = 1 };
    explicit MQTTManager();

    bool beginWifi(DigitalInputDriver* di,
        RelayDriver* rel,
        DACControl* dac,
        AnalogInputs* ai,
        DHTSensors* dht,
        RTCManager* rtc,
        uint32_t publishIntervalMs = 1000);

    bool beginEthernet(EthernetDriver* eth,
        DigitalInputDriver* di,
        RelayDriver* rel,
        DACControl* dac,
        AnalogInputs* ai,
        DHTSensors* dht,
        RTCManager* rtc,
        uint32_t publishIntervalMs = 1000);

    bool begin(EthernetDriver* eth,
        DigitalInputDriver* di,
        RelayDriver* rel,
        DACControl* dac,
        AnalogInputs* ai,
        DHTSensors* dht,
        RTCManager* rtc,
        uint32_t publishIntervalMs = 1000) {
        return beginEthernet(eth, di, rel, dac, ai, dht, rtc, publishIntervalMs);
    }

    bool setupEthernetNetwork(EthernetDriver* eth);

    void setConfig(const MQTTConfig& cfg);
    MQTTConfig getConfig() const;

    bool saveConfig();
    bool loadConfig();
    void clearConfig();

    void loop();
    void forceFullPublish();
    bool isConnected() { return _mqtt.connected(); }

    unsigned long lastConnectMs() const { return _lastConnOkMs; }
    NetMode mode() const { return _mode; }

    void printStatus(Stream& s);

    bool subscribe(const String& topic);
    bool publish(const String& topic, const String& payload, bool retained = false);

    String topic(const String& leaf);
    void publishDAC(bool force);
    void publishRTC(bool force);
    void publishRelayStates(bool force);

    // Health accessors
    uint8_t getFailedConnCount() const { return _failedConnCount; }
    uint32_t getReconnectBackoffMs() const { return _reconnectBackoffMs; }

private:
    NetMode _mode = NET_WIFI;

    WiFiClient     _wifiClient;
    EthernetClient _ethClient;
    EthernetDriver* _eth = nullptr;

    DigitalInputDriver* _di = nullptr;
    RelayDriver* _rel = nullptr;
    DACControl* _dac = nullptr;
    AnalogInputs* _ai = nullptr;
    DHTSensors* _dht = nullptr;
    RTCManager* _rtc = nullptr;

    PubSubClient _mqtt;
    MQTTConfig   _cfg;

    Preferences _prefs;
    static constexpr const char* PREF_NS = "mqtt";
    static constexpr const char* KEY_BROKER = "broker";
    static constexpr const char* KEY_PORT = "port";
    static constexpr const char* KEY_USER = "user";
    static constexpr const char* KEY_PASS = "pass";
    static constexpr const char* KEY_BASE = "base";
    static constexpr const char* KEY_CID = "cid";
    static constexpr const char* KEY_ETH_DHCP = "eth_dhcp";
    static constexpr const char* KEY_ETH_MAC = "eth_mac";
    static constexpr const char* KEY_ETH_IP = "eth_ip";
    static constexpr const char* KEY_ETH_DNS = "eth_dns";
    static constexpr const char* KEY_ETH_GW = "eth_gw";
    static constexpr const char* KEY_ETH_MASK = "eth_mask";

    uint32_t      _publishInterval = 1000;
    unsigned long _lastPub = 0;
    unsigned long _lastConnAttempt = 0;
    unsigned long _lastConnOkMs = 0;

    uint32_t _reconnectBackoffMs = 500;
    uint8_t  _failedConnCount = 0;

    uint8_t  _lastDI = 0xFF;
    int16_t  _lastAIVmv[2] = { -32768, -32768 };
    int16_t  _lastAImA[2] = { -32768, -32768 };
    int16_t  _lastDHTtC[2] = { -32768, -32768 };
    int16_t  _lastDHTrh[2] = { -32768, -32768 };
    uint8_t  _lastDSCount = 0;
    int16_t  _lastDSmC[8] = { 0 };
    size_t   _lastJsonLen = 0;
    uint8_t  _lastRelayBits = 0xFF;

    String id();
    String base();
    bool ensureConnected();
    void announceBirth(bool online);
    void subscribeCommandTopics();

    void publishSnapshot(bool forceAll = false);
    void publishSys(bool force);
    void publishDI(bool force);
    void publishRelays(bool force);
    void publishAI(bool force);
    void publishDHT(bool force);
    void publishDS18(bool force);
    void publishSnapshotJson(bool forceAll, bool retainedOverride = false);

    static void mqttCallbackThunk(char* topic, byte* payload, unsigned int length);
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleCommand(const String& leaf, const String& payload);
    void cmdPing();
    void cmdRelaySet(uint8_t idx1, const String& v);
    void cmdDacMvSet(uint8_t ch1, int mv);
    void cmdDacRawSet(uint8_t ch1, uint16_t raw);
    void cmdRequestFull();

    static String trimCopy(const String& s);
    static int toIntSafe(const String& s, int def = 0);
    static uint16_t toUInt16Safe(const String& s, uint16_t def = 0);
    static String ipToString(IPAddress ip);

    bool probeBrokerTCP();
};

extern MQTTManager* __mqtt_manager_singleton;