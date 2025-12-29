#include "MQTT_Manager.h"

#ifdef ESP32
#include <Preferences.h>
#include <WiFi.h>
#include <ESP.h>
#endif

#ifdef ESP32
static void mqtt_read_hwinfo(String& board, String& sn, String& mfg, String& fw, String& hw, String& year) {
    Preferences p;
    if (!p.begin("hwinfo", true)) return;
    board = p.getString("board_name", "KC-Link A8R-M");
    sn    = p.getString("board_sn", "");
    mfg   = p.getString("mfg", "Microcode Engineering");
    fw    = p.getString("fw", "1.0.0");
    hw    = p.getString("hw", "A8R-M-REV1");
    year  = p.getString("year", "2025");
    p.end();
}
static String mqtt_mac_best(const String& ethMac, const String& staMac, const String& apMac) {
    if (ethMac.length() >= 11) return ethMac;
    if (staMac.length() >= 11) return staMac;
    if (apMac.length() >= 11) return apMac;
    return String("");
}
#endif



MQTTManager* __attribute__((weak)) __mqtt_manager_singleton = nullptr;

MQTTManager::MQTTManager()
    : _mqtt(_wifiClient) {
    _mqtt.setCallback([](char*, byte*, unsigned int) {});
}

bool MQTTManager::beginWifi(DigitalInputDriver* di,
    RelayDriver* rel,
    DACControl* dac,
    AnalogInputs* ai,
    DHTSensors* dht,
    RTCManager* rtc,
    uint32_t publishIntervalMs) {
    _mode = NET_WIFI;
    _eth = nullptr;

    _di = di; _rel = rel; _dac = dac; _ai = ai; _dht = dht; _rtc = rtc;
    _publishInterval = publishIntervalMs ? publishIntervalMs : 1000;
    if (_cfg.clientId.length() == 0) _cfg.clientId = id();

    _mqtt.setClient(_wifiClient);
    _mqtt.setServer(_cfg.broker.c_str(), _cfg.port);
    _wifiClient.setNoDelay(true);
    _wifiClient.setTimeout(3000);

    _mqtt.setKeepAlive(60);
    _mqtt.setSocketTimeout(3);
    _mqtt.setBufferSize(1024);

    __mqtt_manager_singleton = this;
    _mqtt.setCallback(MQTTManager::mqttCallbackThunk);
    return true;
}

bool MQTTManager::beginEthernet(EthernetDriver* eth,
    DigitalInputDriver* di,
    RelayDriver* rel,
    DACControl* dac,
    AnalogInputs* ai,
    DHTSensors* dht,
    RTCManager* rtc,
    uint32_t publishIntervalMs) {
    _mode = NET_ETH;
    _eth = eth;

    _di = di; _rel = rel; _dac = dac; _ai = ai; _dht = dht; _rtc = rtc;
    _publishInterval = publishIntervalMs ? publishIntervalMs : 1000;
    if (_cfg.clientId.length() == 0) _cfg.clientId = id();

    _mqtt.setClient(_ethClient);
    _mqtt.setServer(_cfg.broker.c_str(), _cfg.port);
    _ethClient.setTimeout(3000);

    _mqtt.setKeepAlive(60);
    _mqtt.setSocketTimeout(3);
    _mqtt.setBufferSize(1024);

    __mqtt_manager_singleton = this;
    _mqtt.setCallback(MQTTManager::mqttCallbackThunk);
    return true;
}

bool MQTTManager::setupEthernetNetwork(EthernetDriver* eth) {
    if (!eth) return false;
    if (_cfg.ethUseDHCP) {
        return eth->beginDHCP(_cfg.ethMAC.length() ? _cfg.ethMAC : String("DE:AD:BE:EF:FE:ED"));
    }
    else {
        String mac = _cfg.ethMAC.length() ? _cfg.ethMAC : String("DE:AD:BE:EF:FE:ED");
        String ip = _cfg.ethIP.length() ? _cfg.ethIP : String("0.0.0.0");
        String dns = _cfg.ethDNS.length() ? _cfg.ethDNS : String("8.8.8.8");
        String gw = _cfg.ethGW.length() ? _cfg.ethGW : String("");
        String mask = _cfg.ethMASK.length() ? _cfg.ethMASK : String("255.255.255.0");
        return eth->begin(mac, ip, dns, gw, mask);
    }
}

void MQTTManager::setConfig(const MQTTConfig& cfg) {
    _cfg = cfg;
    if (_cfg.clientId.length() == 0) _cfg.clientId = id();
    _mqtt.setServer(_cfg.broker.c_str(), _cfg.port);
}

MQTTConfig MQTTManager::getConfig() const { return _cfg; }

bool MQTTManager::saveConfig() {
    if (!_prefs.begin(PREF_NS, false)) return false;
    _prefs.putString(KEY_BROKER, _cfg.broker);
    _prefs.putUShort(KEY_PORT, _cfg.port);
    _prefs.putString(KEY_USER, _cfg.username);
    _prefs.putString(KEY_PASS, _cfg.password);
    _prefs.putString(KEY_BASE, _cfg.baseTopic);
    _prefs.putString(KEY_CID, _cfg.clientId);
    _prefs.putBool(KEY_ETH_DHCP, _cfg.ethUseDHCP);
    _prefs.putString(KEY_ETH_MAC, _cfg.ethMAC);
    _prefs.putString(KEY_ETH_IP, _cfg.ethIP);
    _prefs.putString(KEY_ETH_DNS, _cfg.ethDNS);
    _prefs.putString(KEY_ETH_GW, _cfg.ethGW);
    _prefs.putString(KEY_ETH_MASK, _cfg.ethMASK);
    _prefs.end();
    return true;
}

bool MQTTManager::loadConfig() {
    if (!_prefs.begin(PREF_NS, true)) return false;
    String b = _prefs.getString(KEY_BROKER, "");
    uint16_t p = _prefs.getUShort(KEY_PORT, 1883);
    String u = _prefs.getString(KEY_USER, "");
    String w = _prefs.getString(KEY_PASS, "");
    String bt = _prefs.getString(KEY_BASE, "cl/a8rm");
    String cid = _prefs.getString(KEY_CID, "");
    bool   edhcp = _prefs.getBool(KEY_ETH_DHCP, true);
    String emac = _prefs.getString(KEY_ETH_MAC, "DE:AD:BE:EF:FE:ED");
    String eip = _prefs.getString(KEY_ETH_IP, "");
    String edns = _prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    String egw = _prefs.getString(KEY_ETH_GW, "");
    String emask = _prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    _prefs.end();

    if (b.length()) _cfg.broker = b;
    _cfg.port = p;
    _cfg.username = u;
    _cfg.password = w;
    _cfg.baseTopic = bt;
    _cfg.clientId = cid.length() ? cid : id();
    _cfg.ethUseDHCP = edhcp;
    _cfg.ethMAC = emac;
    _cfg.ethIP = eip;
    _cfg.ethDNS = edns;
    _cfg.ethGW = egw;
    _cfg.ethMASK = emask;

    _mqtt.setServer(_cfg.broker.c_str(), _cfg.port);
    return true;
}

void MQTTManager::clearConfig() {
    if (_prefs.begin(PREF_NS, false)) {
        _prefs.clear();
        _prefs.end();
    }
}

void MQTTManager::loop() {
    ensureConnected();

    if (_mqtt.connected() && _di && _di->inputsChanged()) {
        publishDI(true);
        publishSnapshotJson(true, false);
        _di->clearInputsChanged();
    }

    unsigned long now = millis();
    if (_mqtt.connected() && (now - _lastPub) >= _publishInterval) {
        _lastPub = now;
        publishSnapshot(false);
    }
    _mqtt.loop();
}

void MQTTManager::forceFullPublish() {
    publishSnapshot(true);
}

bool MQTTManager::subscribe(const String& topicStr) {
    if (!_mqtt.connected()) return false;
    INFO_LOG("MQTT SUB %s", topicStr.c_str());
    return _mqtt.subscribe(topicStr.c_str());
}

bool MQTTManager::publish(const String& topicStr, const String& payload, bool retained) {
    if (!_mqtt.connected()) return false;
    if (_cfg.clientId.length() == 0 || _cfg.baseTopic.length() == 0) return false;
    bool ok = _mqtt.publish(topicStr.c_str(), payload.c_str(), retained);
    TRACE_LOG("MQTT PUB %s => %s (%s)", topicStr.c_str(), payload.c_str(), ok ? "OK" : "FAIL");
    return ok;
}

void MQTTManager::printStatus(Stream& s) {
    s.println(F("\n=== MQTT Status ==="));
    s.print(F("Mode: ")); s.println(_mode == NET_WIFI ? F("WiFi") : F("Ethernet"));
    s.print(F("Broker: ")); s.print(_cfg.broker); s.print(F(":")); s.println(_cfg.port);
    s.print(F("ClientId: ")); s.println(_cfg.clientId);
    s.print(F("Base: ")); s.println(base());
    s.print(F("User: ")); s.println(_cfg.username);
    if (_mode == NET_WIFI) {
        s.print(F("WiFi RSSI: ")); s.println(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
    }
    else {
        s.print(F("ETH Link: ")); s.println((_eth && _eth->getLinkStatus()) ? F("UP") : F("DOWN"));
        if (_eth) { s.print(F("ETH IP: ")); s.println(_eth->getIP()); }
    }
    s.print(F("Connected: ")); s.println(_mqtt.connected() ? "YES" : "NO");
    s.print(F("Backoff(ms): ")); s.println(_reconnectBackoffMs);
    s.print(F("FailedConnCount: ")); s.println(_failedConnCount);
}

String MQTTManager::id() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[16];
    snprintf(buf, sizeof(buf), "A8RM-%03X", (uint32_t)(mac & 0xFFF));
    return String(buf);
}

String MQTTManager::base() {
    String b = _cfg.baseTopic;
    if (b.length() && b[b.length() - 1] == '/') b.remove(b.length() - 1);
    return b + "/" + _cfg.clientId;
}

String MQTTManager::topic(const String& leaf) {
    return base() + "/" + leaf;
}

bool MQTTManager::ensureConnected() {
    if (_mode == NET_WIFI) {
        // AP-only mode: WiFi.status() may NOT be WL_CONNECTED, but TCP works on SoftAP interface.
        // Accept either STA connected OR SoftAP running.
        bool staOk = (WiFi.status() == WL_CONNECTED);
        bool apOk = (WiFi.getMode() & WIFI_AP) && (WiFi.softAPIP()[0] != 0);
        if (!staOk && !apOk) return false;
    }
    else {
        if (!_eth || !_eth->isReady()) return false;
        IPAddress ip = _eth->getIP();
        if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) return false;
    }
    if (_mqtt.connected()) return true;

    unsigned long now = millis();
    if (now - _lastConnAttempt < _reconnectBackoffMs) return false;
    _lastConnAttempt = now;

    _mqtt.setServer(_cfg.broker.c_str(), _cfg.port);
    if (_cfg.broker.length() == 0) {
        WARNING_LOG("MQTT: broker not set");
        return false;
    }

    if (!probeBrokerTCP()) {
        ERROR_LOG("TCP connect probe failed %s:%u", _cfg.broker.c_str(), _cfg.port);
        _failedConnCount++;
        _reconnectBackoffMs = min<uint32_t>(_reconnectBackoffMs * 2, 30000);
        return false;
    }

    String willTopic = topic("birth/online");
    bool ok;
    if (_cfg.username.length()) {
        ok = _mqtt.connect(_cfg.clientId.c_str(),
            _cfg.username.c_str(), _cfg.password.c_str(),
            willTopic.c_str(), 1, true, "0");
    }
    else {
        ok = _mqtt.connect(_cfg.clientId.c_str(),
            willTopic.c_str(), 1, true, "0");
    }

    if (!ok) {
        ERROR_LOG("MQTT connect failed rc=%d host=%s port=%u", _mqtt.state(), _cfg.broker.c_str(), _cfg.port);
        _failedConnCount++;
        _reconnectBackoffMs = min<uint32_t>(_reconnectBackoffMs * 2, 30000);
        return false;
    }

    _lastConnOkMs = millis();
    INFO_LOG("MQTT connected (backoff reset) as %s -> %s:%u",
        _cfg.clientId.c_str(), _cfg.broker.c_str(), _cfg.port);
    _failedConnCount = 0;
    _reconnectBackoffMs = 500;

    announceBirth(true);
    subscribeCommandTopics();
    forceFullPublish();
    return true;
}

void MQTTManager::announceBirth(bool online) {
    publish(topic("birth/online"), online ? "1" : "0", true);
}

void MQTTManager::subscribeCommandTopics() {
    subscribe(topic("cmd/ping"));
    subscribe(topic("cmd/rel/+/set"));
    subscribe(topic("cmd/rel/+"));
    subscribe(topic("cmd/dac/+/mv_set"));
    subscribe(topic("cmd/dac/+/mv"));
    subscribe(topic("cmd/dac/+/raw_set"));
    subscribe(topic("cmd/request/full"));
    subscribe(topic("cmd/rtc/set"));
    subscribe(topic("cmd/buzzer/beep"));
    subscribe(topic("cmd/buzzer/pattern"));
    subscribe(topic("cmd/buzzer/stop"));
}

void MQTTManager::publishSnapshot(bool forceAll) {
    publishSys(forceAll);
        publishDeviceInfo(forceAll);
publishDI(forceAll);
    publishRelays(forceAll);
    publishRelayStates(forceAll);
    publishDAC(forceAll);
    publishAI(forceAll);
    publishDHT(forceAll);
    publishDS18(forceAll);
    publishRTC(forceAll);
    publishSnapshotJson(forceAll, MQTT_RETAIN_SNAPSHOT);
}

void MQTTManager::publishSys(bool) {
    if (_mode == NET_WIFI) {
        publish(topic("sys/ip"), WiFi.localIP().toString(), MQTT_RETAIN_STATES);
        publish(topic("sys/link"), (WiFi.status() == WL_CONNECTED) ? "UP" : "DOWN", MQTT_RETAIN_STATES);
    }
    else {
        publish(topic("sys/ip"), _eth ? ipToString(_eth->getIP()) : "0.0.0.0", MQTT_RETAIN_STATES);
        publish(topic("sys/link"), (_eth && _eth->getLinkStatus()) ? "UP" : "DOWN", MQTT_RETAIN_STATES);
    }
    publish(topic("sys/uptime_ms"), String(millis()), false);

#ifdef ESP32
    publish(topic("sys/heap_free"), String(ESP.getFreeHeap()), false);
#endif
    publish(topic("sys/failed_conn_count"), String(_failedConnCount), MQTT_RETAIN_STATES);
    publish(topic("sys/reconnect_backoff_ms"), String(_reconnectBackoffMs), false);
}

void MQTTManager::publishDI(bool force) {
    if (!_di || !_di->isConnected()) return;
    _di->updateAllInputs();
    uint8_t st = _di->getInputState();
    if (force || st != _lastDI) {
        for (uint8_t i = 0; i < NUM_DIGITAL_INPUTS; ++i) {
            publish(topic(String("di/") + String(i + 1)),
                (st & (1 << i)) ? "1" : "0",
                MQTT_RETAIN_STATES);
        }
        _lastDI = st;
    }
}

void MQTTManager::publishRelays(bool) {
    if (!_rel || !_rel->isConnected()) return;
    uint8_t statesBits = _rel->getAllStates();
    for (uint8_t i = 0; i < NUM_RELAY_OUTPUTS; ++i) {
        uint8_t on = (statesBits & (1 << i)) ? 0 : 1;
        publish(topic(String("rel/") + String(i + 1) + "/state"),
            on ? "1" : "0",
            MQTT_RETAIN_STATES);
    }
}

void MQTTManager::publishRelayStates(bool force) {
    if (!_rel || !_rel->isConnected()) return;
    uint8_t bits = _rel->getAllStates();
    if (!force && bits == _lastRelayBits) return;
    for (uint8_t i = 0; i < NUM_RELAY_OUTPUTS; ++i) {
        uint8_t on = (bits & (1 << i)) ? 0 : 1;
        publish(topic(String("rel/") + String(i + 1)),
            on ? "1" : "0",
            MQTT_RETAIN_STATES);
    }
    _lastRelayBits = bits;
}

void MQTTManager::publishDAC(bool) {
    if (!_dac || !_dac->isInitialized()) return;
    for (uint8_t ch = 0; ch < 2; ++ch) {
        float v = _dac->getVoltage(ch);
        int mv = (int)lroundf(v * 1000.0f);
        publish(topic(String("dac/") + String(ch + 1) + "/mv"),
            String(mv),
            MQTT_RETAIN_STATES);
    }
}

void MQTTManager::publishAI(bool force) {
    if (!_ai) return;
    for (uint8_t i = 0; i < NUM_ANALOG_CHANNELS; ++i) {
        float v = _ai->readVoltage(i);
        int mv = (int)lroundf(v * 1000.0f);
        if (force || mv != _lastAIVmv[i]) {
            publish(topic(String("ai/voltage/") + String(i + 1)), String(mv), MQTT_RETAIN_STATES);
            _lastAIVmv[i] = mv;
        }
    }
    for (uint8_t i = 0; i < NUM_CURRENT_CHANNELS; ++i) {
        float mA = _ai->readCurrent(i);
        int imA = (int)lroundf(mA);
        if (force || imA != _lastAImA[i]) {
            publish(topic(String("ai/current/") + String(i + 1)), String(imA), MQTT_RETAIN_STATES);
            _lastAImA[i] = imA;
        }
    }
}

void MQTTManager::publishDHT(bool force) {
    if (!_dht) return;
    _dht->update();
    for (uint8_t ch = 0; ch < NUM_DHT_SENSORS; ++ch) {
        float t = _dht->getTemperature(ch);
        float h = _dht->getHumidity(ch);
        int t100 = isnan(t) ? INT16_MIN : (int)lroundf(t * 100.0f);
        int h100 = isnan(h) ? INT16_MIN : (int)lroundf(h * 100.0f);

        if (force || t100 != _lastDHTtC[ch]) {
            publish(topic(String("dht/") + String(ch + 1) + "/t_c"), String(t100), MQTT_RETAIN_STATES);
            _lastDHTtC[ch] = t100;
        }
        if (_dht->getSensorType(ch) != DHT_TYPE_DS18B20) {
            if (force || h100 != _lastDHTrh[ch]) {
                publish(topic(String("dht/") + String(ch + 1) + "/rh"), String(h100), MQTT_RETAIN_STATES);
                _lastDHTrh[ch] = h100;
            }
        }
    }
}

void MQTTManager::publishDS18(bool force) {
    if (!_dht) return;
    uint8_t cnt = _dht->getDS18B20Count();
    if (force || cnt != _lastDSCount) _lastDSCount = cnt;
    for (uint8_t i = 0; i < cnt && i < 8; ++i) {
        float t = _dht->getDS18B20Temperature(i);
        int t100 = (t < -100.0f) ? INT16_MIN : (int)lroundf(t * 100.0f);
        if (force || t100 != _lastDSmC[i]) {
            publish(topic(String("ds18/") + String(i) + "/t_c"), String(t100), MQTT_RETAIN_STATES);
            _lastDSmC[i] = t100;
        }
    }
}

void MQTTManager::publishRTC(bool) {
    if (!_rtc || !_rtc->isConnected()) return;
    DateTime now = _rtc->getDateTime(false);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());
    publish(topic("rtc/iso"), String(buf), MQTT_RETAIN_STATES);

    // Alarm state publishing
    bool a = _rtc->hasAlarm1();
    publish(topic("rtc/alarm1_active"), a ? "1" : "0", MQTT_RETAIN_STATES);
    if (a) {
        DateTime at = _rtc->getAlarm1();
        char abuf[24];
        snprintf(abuf, sizeof(abuf), "%04u-%02u-%02u %02u:%02u:%02u",
            at.year(), at.month(), at.day(),
            at.hour(), at.minute(), at.second());
        publish(topic("rtc/alarm1_time"), String(abuf), MQTT_RETAIN_STATES);
    }
    else {
        publish(topic("rtc/alarm1_time"), "", MQTT_RETAIN_STATES);
    }
}


void MQTTManager::publishDeviceInfo(bool force) {
#ifdef ESP32
    if (!_mqtt.connected()) return;

    String board, sn, mfg, fw, hw, year;
    mqtt_read_hwinfo(board, sn, mfg, fw, hw, year);

    _prefs.begin(PREF_NS, true);
    String ethMac = _prefs.getString(KEY_ETH_MAC, "A8:FD:B5:E1:D4:B3");
    _prefs.end();

    _prefs.begin("wifi_cfg", true);
    String apMac = _prefs.getString("ap_mac", "00:00:00:00:00:00");
    _prefs.end();

    _prefs.begin("wifi_cfg", true);
    String staMac = _prefs.getString("sta_mac", "00:00:00:00:00:00");
    _prefs.end();

    // Enforce max length 17 (we pack into 9 regs = 18 chars space)
    if (ethMac.length() > 17) ethMac = ethMac.substring(0, 17);
    if (staMac.length() > 17) staMac = staMac.substring(0, 17);
    if (apMac.length() > 17)  apMac = apMac.substring(0, 17);


    //String staMac = WiFi.macAddress();
    //String apMac  = WiFi.softAPmacAddress();
    //String ethMac = (_eth) ? _eth->getMACAddressString() : String("");
    // EFUSE base MAC (ESP32 unique MAC) formatted like espMacToString()
    uint64_t efuse = ESP.getEfuseMac();
    char ef[18];
    snprintf(ef, sizeof(ef), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)(efuse >> 40), (uint8_t)(efuse >> 32), (uint8_t)(efuse >> 24),
             (uint8_t)(efuse >> 16), (uint8_t)(efuse >> 8), (uint8_t)(efuse));
    String efuseMac = String(ef);


    // Publish retained info topics (low frequency)
    publish(topic("info/board"), board, true);
    publish(topic("info/serial"), sn, true);
    publish(topic("info/mfg"), mfg, true);
    publish(topic("info/fw"), fw, true);
    publish(topic("info/hw"), hw, true);
    publish(topic("info/year"), year, true);
    publish(topic("info/mac/sta"), staMac, true);
    publish(topic("info/mac/ap"), apMac, true);
    publish(topic("info/mac/efuse"), efuseMac, true);
    if (ethMac.length() > 0) publish(topic("info/mac/eth"), ethMac, true);

    // Also publish a compact JSON
    String j = "{";
    j += "\"board\":\"" + board + "\"";
    j += ",\"serial\":\"" + sn + "\"";
    j += ",\"mfg\":\"" + mfg + "\"";
    j += ",\"fw\":\"" + fw + "\"";
    j += ",\"hw\":\"" + hw + "\"";
    j += ",\"year\":\"" + year + "\"";
    j += ",\"mac\":{";
    j += "\"sta\":\"" + staMac + "\",\"ap\":\"" + apMac + "\",\"efuse\":\"" + efuseMac + "\"";
    if (ethMac.length() > 0) j += ",\"eth\":\"" + ethMac + "\"";
    j += "}}";
    publish(topic("info/json"), j, true);

#else
    (void)force;
#endif
}

void MQTTManager::publishSnapshotJson(bool /*forceAll*/, bool retainedOverride) {
    String json = "{";

    // sys
    json += "\"sys\":{";
    if (_mode == NET_WIFI) {
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",\"link\":\"" +
            String(WiFi.status() == WL_CONNECTED ? "UP" : "DOWN");
    }
    else {
        json += "\"ip\":\"" + (_eth ? ipToString(_eth->getIP()) : String("0.0.0.0")) + "\",\"link\":\"" +
            String(_eth && _eth->getLinkStatus() ? "UP" : "DOWN");
    }
    json += "\",\"uptime_ms\":" + String(millis());
#ifdef ESP32
    json += ",\"heap_free\":" + String(ESP.getFreeHeap());
#endif
    json += ",\"failed_conn_count\":" + String(_failedConnCount);
    json += ",\"reconnect_backoff_ms\":" + String(_reconnectBackoffMs);
    json += "}";

    // di
    if (_di && _di->isConnected()) {
        _di->updateAllInputs();
        uint8_t st = _di->getInputState();
        json += ",\"di\":{";
        for (uint8_t i = 0; i < NUM_DIGITAL_INPUTS; i++) {
            json += "\"" + String(i + 1) + "\":" + String((st & (1 << i)) ? 1 : 0);
            if (i + 1 < NUM_DIGITAL_INPUTS) json += ",";
        }
        json += "}";
    }

    // rel
    if (_rel && _rel->isConnected()) {
        uint8_t bits = _rel->getAllStates();
        json += ",\"rel\":{";
        for (uint8_t i = 0; i < NUM_RELAY_OUTPUTS; i++) {
            uint8_t on = (bits & (1 << i)) ? 0 : 1;
            json += "\"" + String(i + 1) + "\":" + String(on);
            if (i + 1 < NUM_RELAY_OUTPUTS) json += ",";
        }
        json += "}";
    }

    // dac
    if (_dac && _dac->isInitialized()) {
        json += ",\"dac\":{";
        for (uint8_t ch = 0; ch < 2; ch++) {
            int mv = (int)lroundf(_dac->getVoltage(ch) * 1000.0f);
            json += "\"ch" + String(ch + 1) + "_mv\":" + String(mv);
            if (ch + 1 < 2) json += ",";
        }
        json += "}";
    }

    // analog + current
    if (_ai) {
        json += ",\"analog\":{";
        for (uint8_t ch = 0; ch < NUM_ANALOG_CHANNELS; ch++) {
            int mv = (int)lroundf(_ai->readVoltage(ch) * 1000.0f);
            json += "\"ch" + String(ch + 1) + "_mv\":" + String(mv);
            if (ch + 1 < NUM_ANALOG_CHANNELS) json += ",";
        }
        json += "},\"current\":{";
        for (uint8_t ch = 0; ch < NUM_CURRENT_CHANNELS; ch++) {
            int mA = (int)lroundf(_ai->readCurrent(ch));
            json += "\"ch" + String(ch + 1) + "_mA\":" + String(mA);
            if (ch + 1 < NUM_CURRENT_CHANNELS) json += ",";
        }
        json += "}";
    }

    // dht + ds18
    if (_dht) {
        _dht->update();
        json += ",\"dht\":{";
        for (uint8_t ch = 0; ch < NUM_DHT_SENSORS; ch++) {
            float t = _dht->getTemperature(ch);
            float h = _dht->getHumidity(ch);
            json += "\"ch" + String(ch + 1) + "\":{";
            if (!isnan(t)) json += "\"temp_c\":" + String(t, 2); else json += "\"temp_c\":null";
            json += ",";
            if (!isnan(h)) json += "\"rh\":" + String(h, 2); else json += "\"rh\":null";
            json += "}";
            if (ch + 1 < NUM_DHT_SENSORS) json += ",";
        }
        json += "}";

        uint8_t cnt = _dht->getDS18B20Count();
        json += ",\"ds18\":{";
        json += "\"count\":" + String(cnt);
        float firstT = (cnt > 0) ? _dht->getDS18B20Temperature(0) : NAN;
        json += ",\"first_c\":" + String(isnan(firstT) ? 0 : firstT, 2);
        json += "}";
    }

    if (_rtc && _rtc->isConnected()) {
        DateTime dt = _rtc->getDateTime(false);
        char buf[24];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
            dt.year(), dt.month(), dt.day(),
            dt.hour(), dt.minute(), dt.second());
        json += ",\"rtc\":{";
        json += "\"iso\":\"" + String(buf) + "\"";
        bool a = _rtc->hasAlarm1();
        json += ",\"alarm1_active\":" + String(a ? 1 : 0);
        if (a) {
            DateTime at = _rtc->getAlarm1();
            char abuf[24];
            snprintf(abuf, sizeof(abuf), "%04u-%02u-%02u %02u:%02u:%02u",
                at.year(), at.month(), at.day(),
                at.hour(), at.minute(), at.second());
            json += ",\"alarm1_time\":\"" + String(abuf) + "\"";
        }
        else {
            json += ",\"alarm1_time\":\"\"";
        }
        json += "}";
    }

    json += "}";

    publish(topic("snapshot/json"), json, retainedOverride);
    _lastJsonLen = json.length();
}

void MQTTManager::mqttCallbackThunk(char* topicC, byte* payload, unsigned int length) {
    if (__mqtt_manager_singleton) {
        __mqtt_manager_singleton->mqttCallback(topicC, payload, length);
    }
}

void MQTTManager::mqttCallback(char* topicC, byte* payload, unsigned int length) {
    String t(topicC);
    String p; p.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) p += (char)payload[i];
    p = trimCopy(p);

    String prefix = base() + "/cmd/";
    if (!t.startsWith(prefix)) {
        WARNING_LOG("MQTT RX unhandled topic: %s", t.c_str());
        return;
    }
    String leaf = t.substring(prefix.length());
    INFO_LOG("MQTT CMD %s => %s", leaf.c_str(), p.c_str());
    handleCommand(leaf, p);
}

void MQTTManager::handleCommand(const String& leaf, const String& payload) {
    if (leaf == "ping") { cmdPing(); return; }
    if (leaf == "request/full") { cmdRequestFull(); return; }

    if (leaf == "rtc/set") { cmdRtcSet(payload); return; }

    if (leaf == "buzzer/beep") { cmdBuzzerBeepJson(payload); return; }
    if (leaf == "buzzer/pattern") { cmdBuzzerPatternJson(payload); return; }
    if (leaf == "buzzer/stop") { cmdBuzzerStop(); return; }

    if (leaf.startsWith("rel/")) {
        String core = leaf;
        if (core.endsWith("/set")) core = core.substring(0, core.length() - 4);
        int slash = core.indexOf('/', 4);
        if (slash < 0) {
            String sidx = core.substring(4);
            uint8_t idx1 = (uint8_t)toIntSafe(sidx, 0);
            cmdRelaySet(idx1, payload);
            return;
        }
    }

    if (leaf.startsWith("dac/") && (leaf.endsWith("/mv_set") || leaf.endsWith("/mv"))) {
        String baseLeaf = leaf;
        if (baseLeaf.endsWith("_set")) baseLeaf = baseLeaf.substring(0, baseLeaf.length() - 4);
        int slash = baseLeaf.indexOf('/', 4);
        if (slash < 0) return;
        String sch = baseLeaf.substring(4, slash);
        uint8_t ch1 = (uint8_t)toIntSafe(sch, 0);
        int mv = toIntSafe(payload, 0);
        cmdDacMvSet(ch1, mv);
        return;
    }

    if (leaf.startsWith("dac/") && leaf.endsWith("/raw_set")) {
        int slash = leaf.indexOf('/', 4);
        if (slash < 0) return;
        String sch = leaf.substring(4, slash);
        uint8_t ch1 = (uint8_t)toIntSafe(sch, 0);
        uint16_t raw = toUInt16Safe(payload, 0);
        cmdDacRawSet(ch1, raw);
        return;
    }

    WARNING_LOG("Unhandled command leaf=%s", leaf.c_str());
}

void MQTTManager::cmdPing() {
    publish(topic("resp/ping"), "pong", false);
}

void MQTTManager::cmdRelaySet(uint8_t idx1, const String& v) {
    if (!_rel || !_rel->isConnected()) return;
    if (idx1 < 1 || idx1 > NUM_RELAY_OUTPUTS) return;

    bool ok = false;
    if (v == "1" || v == "on" || v == "ON")        ok = _rel->setState(idx1, RELAY_ON);
    else if (v == "0" || v == "off" || v == "OFF") ok = _rel->setState(idx1, RELAY_OFF);
    else if (v == "toggle" || v == "T" || v == "tg") ok = _rel->toggle(idx1);
    else { WARNING_LOG("Relay set invalid value: %s", v.c_str()); return; }

    INFO_LOG("Relay %u set -> %s (%s)", idx1, v.c_str(), ok ? "ok" : "fail");
    publishRelayStates(true);
    publishRelays(true);
    publishSnapshotJson(true, MQTT_RETAIN_SNAPSHOT);
}

void MQTTManager::cmdDacMvSet(uint8_t ch1, int mv) {
    if (!_dac || !_dac->isInitialized()) return;
    if (ch1 < 1 || ch1 > 2) return;
    float v = mv / 1000.0f;
    _dac->setVoltage((uint8_t)(ch1 - 1), v);
    publishDAC(true);
    publishSnapshotJson(true, MQTT_RETAIN_SNAPSHOT);
}

void MQTTManager::cmdDacRawSet(uint8_t ch1, uint16_t raw) {
    if (!_dac || !_dac->isInitialized()) return;
    if (ch1 < 1 || ch1 > 2) return;
    _dac->setRaw((uint8_t)(ch1 - 1), raw);
    publishDAC(true);
    publishSnapshotJson(true, MQTT_RETAIN_SNAPSHOT);
}

void MQTTManager::cmdRequestFull() {
    publishSnapshot(true);
}


// --- Tiny JSON helpers (very small, no ArduinoJson dependency) ---
static long jsonGetLong(const String& s, const char* key, long defVal) {
    // Looks for: "key":123 (spaces allowed)
    String k = String("\"") + key + "\"";
    int p = s.indexOf(k);
    if (p < 0) return defVal;
    p = s.indexOf(':', p + k.length());
    if (p < 0) return defVal;
    p++;
    while (p < (int)s.length() && (s[p] == ' ' || s[p] == '\t')) p++;
    // parse number (may be negative)
    int e = p;
    while (e < (int)s.length() && (isDigit(s[e]) || s[e] == '-')) e++;
    String num = s.substring(p, e);
    if (!num.length()) return defVal;
    return num.toInt();
}

void MQTTManager::cmdRtcSet(const String& iso) {
    if (!_rtc || !_rtc->isConnected()) {
        publish(topic("resp/rtc/set"), "ERR:RTC_NOT_CONNECTED", false);
        return;
    }
    bool ok = _rtc->setDateTimeFromString(iso.c_str());
    publish(topic("resp/rtc/set"), ok ? "OK" : "ERR:BAD_FORMAT", false);
    if (ok) {
        publishRTC(true);
        publishSnapshotJson(true, MQTT_RETAIN_SNAPSHOT);
    }
}

void MQTTManager::cmdBuzzerBeepJson(const String& json) {
    if (!_rtc || !_rtc->isConnected()) {
        publish(topic("resp/buzzer/beep"), "ERR:RTC_NOT_CONNECTED", false);
        return;
    }
    long freq = jsonGetLong(json, "freq", 2000);
    long ms = jsonGetLong(json, "ms", 200);

    if (freq < 50) freq = 50;
    if (freq > 20000) freq = 20000;
    if (ms < 10) ms = 10;
    if (ms > 60000) ms = 60000;

    _rtc->simpleBeep((uint16_t)freq, (uint32_t)ms);
    publish(topic("resp/buzzer/beep"), "OK", false);
}

void MQTTManager::cmdBuzzerPatternJson(const String& json) {
    if (!_rtc || !_rtc->isConnected()) {
        publish(topic("resp/buzzer/pattern"), "ERR:RTC_NOT_CONNECTED", false);
        return;
    }
    long freq = jsonGetLong(json, "freq", 2000);
    long onMs = jsonGetLong(json, "on", 200);
    long offMs = jsonGetLong(json, "off", 200);
    long rep = jsonGetLong(json, "rep", 5);

    if (freq < 50) freq = 50;
    if (freq > 20000) freq = 20000;

    if (onMs < 10) onMs = 10;
    if (onMs > 60000) onMs = 60000;

    if (offMs < 0) offMs = 0;
    if (offMs > 60000) offMs = 60000;

    if (rep < 1) rep = 1;
    if (rep > 255) rep = 255;

    _rtc->setAlarmPattern((uint16_t)freq, (uint16_t)onMs, (uint16_t)offMs, (uint8_t)rep);
    _rtc->playAlarmPattern();

    publish(topic("resp/buzzer/pattern"), "OK", false);
}

void MQTTManager::cmdBuzzerStop() {
    if (!_rtc) return;
    _rtc->stopBeep();
    publish(topic("resp/buzzer/stop"), "OK", false);
}

String MQTTManager::trimCopy(const String& s) {
    String t = s; t.trim(); return t;
}

int MQTTManager::toIntSafe(const String& s, int def) {
    if (!s.length()) return def;
    return s.toInt();
}

uint16_t MQTTManager::toUInt16Safe(const String& s, uint16_t def) {
    if (!s.length()) return def;
    long v = strtol(s.c_str(), nullptr, 10);
    if (v < 0) v = 0;
    if (v > 65535) v = 65535;
    return (uint16_t)v;
}

String MQTTManager::ipToString(IPAddress ip) {
    char b[24];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return String(b);
}

bool MQTTManager::probeBrokerTCP() {
    INFO_LOG("Probing TCP %s:%u ...", _cfg.broker.c_str(), _cfg.port);
    if (_mode == NET_WIFI) {
        WiFiClient probe;
        probe.setNoDelay(true);
        probe.setTimeout(2000);
        if (!probe.connect(_cfg.broker.c_str(), _cfg.port)) return false;
        probe.stop();
        return true;
    }
    else {
        EthernetClient probe;
        probe.setTimeout(2000);
        if (!probe.connect(_cfg.broker.c_str(), _cfg.port)) return false;
        probe.stop();
        return true;
    }
}