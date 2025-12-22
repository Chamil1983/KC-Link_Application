#include "Alexa_Manager.h"

static AlexaManager* __alexa_singleton = nullptr;

AlexaManager::AlexaManager() {
    __alexa_singleton = this;
}

void AlexaManager::begin(const Settings& cfg,
    RelayDriver* rel,
    DACControl* dac,
    AnalogInputs* ai,
    DHTSensors* dht,
    DigitalInputDriver* di,
    RTCManager* rtc) {

    _cfg = cfg;
    _rel = rel;
    _dac = dac;
    _ai = ai;
    _dht = dht;
    _di = di;
    _rtc = rtc;

    DEBUG_LOG(DEBUG_LEVEL_INFO, "AlexaManager: begin()");

    _configureWiFiForStability();
    _attachWifiEvents();
    _scanForSSID();

    // Start WiFi connection cycle (non-blocking by default; blockingWifi=false in sketch)
    bool cycleStarted = _startWiFiCycle();

    if (cycleStarted && _cfg.blockingWifi && _ssidFound) {
        DEBUG_LOG(DEBUG_LEVEL_INFO, "AlexaManager: blocking until WiFi connected...");
        if (_attemptBlockingWiFi()) {
            DEBUG_LOG(DEBUG_LEVEL_INFO, "AlexaManager: WiFi connected (blocking phase)");
            _pendingEspConfig = true;
        }
        else {
            WARNING_LOG("AlexaManager: initial blocking WiFi connect timed out; proceeding non-blocking");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.println("WiFi connected successfully!");
            Serial.println("--------------------------------");
            Serial.println("Network Details:");
            Serial.print("Connected SSID:       "); Serial.println(WiFi.SSID());
            Serial.print("IP Address:           "); Serial.println(WiFi.localIP());
            Serial.print("Subnet Mask:          "); Serial.println(WiFi.subnetMask());
            Serial.print("Gateway IP:           "); Serial.println(WiFi.gatewayIP());
            Serial.print("DNS Server:           "); Serial.println(WiFi.dnsIP());
            Serial.print("MAC Address:          "); Serial.println(WiFi.macAddress());
            Serial.print("Signal Strength (RSSI):"); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
            Serial.println("--------------------------------");
        }
    }
}

void AlexaManager::_configureWiFiForStability() {
    // Follow Espalexa examples + a bit of tuning
    WiFi.disconnect(true, true);
    delay(100);

    // Station mode only
    WiFi.mode(WIFI_STA);
    delay(50);

    // Optional hostname can help on some routers
    if (_hostname.length()) {
#ifdef ARDUINO_ARCH_ESP32
        WiFi.setHostname(_hostname.c_str());
#endif
    }

    // Keep it simple: no power-save, no persistence
    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    // Conservative TX power (too high can sometimes upset some APs)
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_ps(WIFI_PS_NONE);

    DEBUG_LOG(DEBUG_LEVEL_INFO, "WiFi configured for stability (STA, sleep disabled, auto-reconnect enabled)");
}

void AlexaManager::setWifiCredentials(const String& ssid, const String& password) {
    INFO_LOG("AlexaManager: runtime WiFi credential update -> '%s'", ssid.c_str());
    _cfg.ssid = ssid;
    _cfg.password = password;

    _gotIP = false;
    _ssidFound = false;
    _scanAttempts = 0;
    _failedConnectCycles = 0;
    _lastDiscReason = WIFI_REASON_UNSPECIFIED;
    _retryBackoffMs = 5000;
    _pendingEspConfig = false;

    if (_softAPMode) {
        WiFi.softAPdisconnect(true);
        _softAPMode = false;
    }

    WiFi.disconnect(true, true);
    delay(200);

    _configureWiFiForStability();
    _scanForSSID();
    _startWiFiCycle();
}

void AlexaManager::_attachWifiEvents() {
    if (_wifiEventAttached) return;
    WiFi.onEvent(AlexaManager::_wifiEventThunk);
    _wifiEventAttached = true;
    INFO_LOG("AlexaManager: WiFi events attached");
}

void AlexaManager::_wifiEventThunk(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (__alexa_singleton) __alexa_singleton->_wifiEventHandler(event, info);
}

void AlexaManager::_wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
        DEBUG_LOG(DEBUG_LEVEL_INFO, "WiFi Event: STA_START");
        break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        INFO_LOG("WiFi Event: STA_CONNECTED (BSSID=%02X:%02X:%02X:%02X:%02X:%02X CH=%u)",
            info.wifi_sta_connected.bssid[0], info.wifi_sta_connected.bssid[1],
            info.wifi_sta_connected.bssid[2], info.wifi_sta_connected.bssid[3],
            info.wifi_sta_connected.bssid[4], info.wifi_sta_connected.bssid[5],
            info.wifi_sta_connected.channel);
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        _gotIP = true;
        _lastGotIpMs = millis();
        _failedConnectCycles = 0;
        _retryBackoffMs = 5000;
        INFO_LOG("WiFi Event: GOT_IP %s", WiFi.localIP().toString().c_str());
        _maybeExitSoftAP();
        _pendingEspConfig = true;   // actual Espalexa setup only in loop()
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        _gotIP = false;
        _lastDiscReason = (wifi_err_reason_t)info.wifi_sta_disconnected.reason;
        INFO_LOG("WiFi Event: DISCONNECTED reason=%d (%s)",
            (int)_lastDiscReason, _discReasonString(_lastDiscReason));
        if (_espConfigured) {
            _teardownEspalexa();
            _espConfigured = false;
        }
        break;
    default:
        TRACE_LOG("WiFi Event: %d", (int)event);
        break;
    }
}

void AlexaManager::_scanForSSID() {
    if (_cfg.ssid.isEmpty()) {
        WARNING_LOG("WiFi SSID empty");
        return;
    }
    if (_scanAttempts >= _cfg.maxScanRetries) return;

    INFO_LOG("Scanning for SSID '%s' (attempt %u)...", _cfg.ssid.c_str(), _scanAttempts + 1);

    int n = WiFi.scanNetworks(false, false, false, 400);
    _ssidFound = false;

    if (n > 0) {
        INFO_LOG("Found %d networks:", n);
        for (int i = 0; i < n; ++i) {
            INFO_LOG("  %d: %s (RSSI: %d CH: %d)", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
            if (WiFi.SSID(i) == _cfg.ssid) {
                _ssidFound = true;
            }
        }
    }

    INFO_LOG("Scan result: %s", _ssidFound ? "SSID FOUND" : "SSID NOT FOUND");
    WiFi.scanDelete();
    _scanAttempts++;
}

bool AlexaManager::_startWiFiCycle() {
    if (_cfg.ssid.isEmpty()) {
        WARNING_LOG("Cannot start WiFi cycle: SSID empty");
        return false;
    }

    if (!_ssidFound) _scanForSSID();

    if (!_ssidFound && _cfg.allowSoftAPFallback) {
        WARNING_LOG("SSID not located after scans -> entering SoftAP fallback");
        _enterSoftAP();
        return false;
    }

    INFO_LOG("Starting WiFi connect cycle to SSID='%s'", _cfg.ssid.c_str());

    WiFi.disconnect(true, true);
    delay(100);
    _configureWiFiForStability();

    _lastWiFiAttemptStart = millis();
    _lastRetryMs = millis();

    WiFi.begin(_cfg.ssid.c_str(), _cfg.password.c_str());
    return true;
}

void AlexaManager::_evaluateWiFiProgress() {
    if (_softAPMode) {
        if (millis() - _softAPStartMs > _cfg.softApRetryMs) {
            INFO_LOG("SoftAP period elapsed -> retrying station connection");
            WiFi.softAPdisconnect(true);
            _softAPMode = false;
            _scanAttempts = 0;
            _ssidFound = false;
            _retryBackoffMs = 5000;
            _startWiFiCycle();
        }
        return;
    }

    if (wifiConnected() && gotIP()) {
        return;
    }

    wl_status_t st = WiFi.status();
    _lastStatus = st;
    unsigned long now = millis();

    // Give each cycle a chance; do not hammer the AP
    if (now - _lastWiFiAttemptStart >= _cfg.connectAttemptTimeoutMs) {
        _failedConnectCycles++;
        INFO_LOG("Connection cycle failed (#%u) status=%d", _failedConnectCycles, (int)st);

        if (_cfg.allowSoftAPFallback && _failedConnectCycles >= _cfg.maxFailedCyclesBeforeAP && !_softAPMode) {
            _enterSoftAP();
            return;
        }

        INFO_LOG("Restarting WiFi cycle...");
        _startWiFiCycle();
        return;
    }

    // Gentle reconnect backoff
    if (st != WL_CONNECTED && (now - _lastRetryMs >= _retryBackoffMs)) {
        _lastRetryMs = now;
        if (_retryBackoffMs < RETRY_BACKOFF_MAX) {
            _retryBackoffMs += 5000;
        }
        INFO_LOG("Retry connecting... (Status=%s, Backoff=%lu ms)", _statusString(st), _retryBackoffMs);
        WiFi.disconnect(true, true);
        delay(150);
        WiFi.begin(_cfg.ssid.c_str(), _cfg.password.c_str());
    }
}

bool AlexaManager::_attemptBlockingWiFi() {
    uint32_t start = millis();
    uint32_t timeout = _cfg.connectAttemptTimeoutMs;

    INFO_LOG("Attempting blocking WiFi connection (timeout: %lu ms)...", timeout);

    while (millis() - start < timeout) {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
            _gotIP = true;
            INFO_LOG("WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
            return true;
        }
        delay(500);
        yield();

        if ((millis() - start) % 5000 < 500) {
            INFO_LOG("WiFi connecting... status=%s elapsed=%lu ms",
                _statusString(WiFi.status()), millis() - start);
        }
    }
    return false;
}

// Only called from loop() to avoid lwIP/tcpip thread asserts
void AlexaManager::_maybeConfigureEspInLoop() {
    if (_espConfigured) return;
    if (!_pendingEspConfig) return;
    if (_softAPMode) return;
    if (!wifiConnected() || !gotIP()) return;

    // Small stabilization delay after GOT_IP to avoid race conditions
    if (millis() - _lastGotIpMs < 500) return;

    _configureEspalexa();
    _pendingEspConfig = false;
}

void AlexaManager::_configureEspalexa() {
    if (_espConfigured) return;

    INFO_LOG("AlexaManager: configuring Espalexa devices");

    // 6 Relays as simple on/off devices (like the example)
    for (uint8_t i = 0; i < NUM_RELAY_OUTPUTS; i++) {
        String name = String(DEV_RELAY_PREFIX) + String(i + 1);
        _esp.addDevice(name.c_str(), [this, i](uint8_t bri) {
            bool state = (bri > 0);
            _logAlexa(String(DEV_RELAY_PREFIX + String(i + 1)).c_str(), state, bri);
            _handleSetRelay(i, state);
            });
        INFO_LOG("Added Alexa device: %s", name.c_str());
    }

    // DAC channels: dimmable
    for (uint8_t ch = 0; ch < 2; ch++) {
        String name = String(DEV_DAC_PREFIX) + String(ch + 1);
        _esp.addDevice(name.c_str(), [this, ch](uint8_t bri) {
            bool state = (bri > 0);
            _logAlexa(String(DEV_DAC_PREFIX + String(ch + 1)).c_str(), state, bri);
            _handleSetDAC(ch, state, bri);
            });
        INFO_LOG("Added Alexa device: %s", name.c_str());
    }

    // Buzzer
    _esp.addDevice(DEV_BUZZER, [this](uint8_t bri) {
        bool state = (bri > 0);
        _logAlexa(DEV_BUZZER, state, bri);
        _handleBuzzer(state);
        });
    INFO_LOG("Added Alexa device: %s", DEV_BUZZER);

    // Sensors snapshot
    _esp.addDevice(DEV_SENSORS, [this](uint8_t bri) {
        bool state = (bri > 0);
        _logAlexa(DEV_SENSORS, state, bri);
        if (state) _handleSensorsTrigger();
        });
    INFO_LOG("Added Alexa device: %s", DEV_SENSORS);

    // RTC refresh
    _esp.addDevice(DEV_RTC_REFRESH, [this](uint8_t bri) {
        bool state = (bri > 0);
        _logAlexa(DEV_RTC_REFRESH, state, bri);
        if (state) _handleRTCRefresh();
        });
    INFO_LOG("Added Alexa device: %s", DEV_RTC_REFRESH);

    // Start Espalexa HTTP/SSDP server AFTER WiFi is stable
    delay(50);
    _esp.begin();

    _espConfigured = true;

    INFO_LOG("Espalexa configured. Ready for Alexa discovery. Say 'Alexa, discover devices'");
    INFO_LOG("Device IP: %s", WiFi.localIP().toString().c_str());
}

void AlexaManager::_teardownEspalexa() {
    _esp = Espalexa();
}

void AlexaManager::_enterSoftAP() {
    if (_softAPMode) return;
    String apName = "A8RM_Config";
    INFO_LOG("Starting SoftAP fallback SSID='%s'", apName.c_str());
    WiFi.softAP(apName.c_str(), "configure123");
    _softAPMode = true;
    _softAPStartMs = millis();
    _gotIP = false;
    _pendingEspConfig = false;
    INFO_LOG("SoftAP IP: %s", WiFi.softAPIP().toString().c_str());
    INFO_LOG("NOTE: Alexa discovery will NOT work in SoftAP mode!");
}

void AlexaManager::_maybeExitSoftAP() {
    if (_softAPMode && wifiConnected() && gotIP()) {
        INFO_LOG("Station connected -> disabling SoftAP");
        WiFi.softAPdisconnect(true);
        _softAPMode = false;
    }
}

String AlexaManager::_timestamp() {
    if (_rtc && _rtc->isConnected()) {
        DateTime dt = _rtc->getDateTime(false);
        char buf[24];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
            dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
        return String(buf);
    }
    return String("millis:") + String(millis());
}

void AlexaManager::_logAlexa(const char* device, bool state, uint8_t brightness) {
    INFO_LOG("Alexa CMD @%s device='%s' state=%s bri=%u",
        _timestamp().c_str(), device, state ? "ON" : "OFF", (unsigned)brightness);
    Serial.printf("[ALEXA] %s -> %s (bri=%u)\n", device, state ? "ON" : "OFF", brightness);
}

const char* AlexaManager::_discReasonString(wifi_err_reason_t r) {
    switch (r) {
    case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "SUPCHAN_BAD";
    case WIFI_REASON_IE_INVALID: return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4W_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GTK_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_4W_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP: return "RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED: return "8021X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_REJECTED";
    case WIFI_REASON_INVALID_PMKID: return "INVALID_PMKID";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    default: return "OTHER";
    }
}

const char* AlexaManager::_statusString(wl_status_t s) {
    switch (s) {
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
#ifdef WL_WRONG_PASSWORD
    case WL_WRONG_PASSWORD: return "WRONG_PASSWORD";
#endif
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
    }
}

void AlexaManager::_handleSetRelay(uint8_t index, bool state) {
    if (!_rel || !_rel->isConnected()) { WARNING_LOG("Relay driver not ready"); return; }
    bool ok = _rel->setState(index + 1, state ? RELAY_ON : RELAY_OFF);
    Serial.printf("[ALEXA] Relay %u %s (%s)\n", index + 1, state ? "ON" : "OFF", ok ? "OK" : "FAIL");
}

void AlexaManager::_handleSetDAC(uint8_t ch, bool state, uint8_t bri) {
    if (!_dac || !_dac->isInitialized()) { WARNING_LOG("DAC not initialized"); return; }
    float maxV = _cfg.dacDefault[ch];
    float volts = state ? (maxV * (bri / 255.0f)) : 0.0f;
    bool ok = _dac->setVoltage(ch, volts);
    Serial.printf("[ALEXA] DAC %u -> %.3f V (bri=%u) (%s)\n", ch + 1, volts, bri, ok ? "OK" : "FAIL");
}

void AlexaManager::_handleBuzzer(bool state) {
    if (!_rtc) { WARNING_LOG("RTCManager (buzzer) not available"); }
    if (state) {
        if (_rtc) {
            _rtc->setAlarmPattern(_cfg.buzzerFreq, _cfg.buzzerOnMs, _cfg.buzzerOffMs, _cfg.buzzerRepeats);
            _rtc->playAlarmPattern();
        }
        else {
            tone(PIN_BUZZER, _cfg.buzzerFreq, _cfg.buzzerOnMs);
        }
        Serial.println("[ALEXA] Buzzer pattern started");
    }
    else {
        if (_rtc) _rtc->stopBeep(); else noTone(PIN_BUZZER);
        Serial.println("[ALEXA] Buzzer stopped");
    }
}

void AlexaManager::_handleSensorsTrigger() {
    Serial.println("[ALEXA] Sensors snapshot requested");
    snapshotSensors(Serial, true);
}

void AlexaManager::_handleRTCRefresh() {
    if (_rtc && _rtc->isConnected()) {
        _rtc->refreshTime();
        Serial.println("[ALEXA] RTC refreshed from hardware");
    }
    else {
        Serial.println("[ALEXA] RTC refresh requested but RTC not connected");
    }
}

void AlexaManager::loop() {
    boolean wifiState = false;

    if (wifiConnected() && gotIP()) {
        _maybeConfigureEspInLoop();

        if (_espConfigured) {

        _esp.loop();

        }
    }

    else {
        wifiState = _connectWifi();

        if (wifiState && gotIP()) {
            _maybeConfigureEspInLoop();
        }
    }


    if (_espConfigured && _cfg.autoSnapshot &&
        (millis() - _lastSensorAuto >= _cfg.autoSnapshotMs)) {
        _lastSensorAuto = millis();
        snapshotSensors(Serial, false);
    }
}


boolean AlexaManager::_connectWifi()
{
    int attempts = 0;
    boolean state = true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(_cfg.ssid.c_str(), _cfg.password.c_str());

    Serial.println("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    // Handle successful connection
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println("WiFi connected successfully!");
        Serial.println("--------------------------------");
        Serial.println("Network Details:");
        Serial.print("Connected SSID:       "); Serial.println(WiFi.SSID());
        Serial.print("IP Address:           "); Serial.println(WiFi.localIP());
        Serial.print("Subnet Mask:          "); Serial.println(WiFi.subnetMask());
        Serial.print("Gateway IP:           "); Serial.println(WiFi.gatewayIP());
        Serial.print("DNS Server:           "); Serial.println(WiFi.dnsIP());
        Serial.print("MAC Address:          "); Serial.println(WiFi.macAddress());
        Serial.print("Signal Strength (RSSI):"); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
        Serial.println("--------------------------------");
    }
    else
    {
        state = false;
        Serial.println("Connection failed.");
    }

    return state;
}

void AlexaManager::printStatus(Stream& s) {
    s.println("\n=== Alexa Status ===");
    wl_status_t st = WiFi.status();
    s.print("WiFi status: "); s.println(_statusString(st));
    s.print("Last disconnect reason: "); s.println(_discReasonString(_lastDiscReason));
    s.print("SSID: "); s.println(_cfg.ssid);
    s.print("SSID found: "); s.println(_ssidFound ? "YES" : "NO");
    s.print("SoftAP mode: "); s.println(_softAPMode ? "YES" : "NO");
    s.print("Got IP: "); s.println(gotIP() ? "YES" : "NO");
    if (wifiConnected()) {
        s.print("IP: "); s.println(WiFi.localIP());
        s.print("RSSI: "); s.println(WiFi.RSSI());
    }
    else if (_softAPMode) {
        s.print("SoftAP IP: "); s.println(WiFi.softAPIP());
    }
    s.print("Failed connect cycles: "); s.println(_failedConnectCycles);
    s.print("Attempt runtime(ms): ");
    s.println(wifiConnected() ? 0 : (unsigned long)(millis() - _lastWiFiAttemptStart));
    s.print("Backoff(ms): "); s.println(_retryBackoffMs);
    s.print("Espalexa Configured: "); s.println(_espConfigured ? "YES" : "NO");
    s.print("Pending Espalexa Config: "); s.println(pendingEspConfig() ? "YES" : "NO");
    s.print("Alexa Ready: "); s.println(alexaReady() ? "YES" : "NO");
    if (_espConfigured) {
        s.print("Devices: "); s.println(NUM_RELAY_OUTPUTS + 2 + 3);
    }
    s.println("====================");
}

void AlexaManager::listDevices(Stream& s) {
    if (!_espConfigured) {
        s.println("Alexa devices not yet configured (waiting for WiFi connection).");
        return;
    }
    s.println("Alexa Devices:");
    for (uint8_t i = 0; i < NUM_RELAY_OUTPUTS; i++)
        s.printf("  Relay %u\n", i + 1);
    for (uint8_t i = 0; i < 2; i++)
        s.printf("  DAC %u (default %.3f V)\n", i + 1, _cfg.dacDefault[i]);
    s.printf("  %s (pattern F=%uHz on=%ums off=%ums reps=%u)\n",
        DEV_BUZZER, _cfg.buzzerFreq, _cfg.buzzerOnMs, _cfg.buzzerOffMs, _cfg.buzzerRepeats);
    s.printf("  %s (snapshot trigger)\n", DEV_SENSORS);
    s.printf("  %s (refresh RTC)\n", DEV_RTC_REFRESH);
}

void AlexaManager::setDacDefault(uint8_t ch, float volts) {
    if (ch >= 2) return;
    if (volts < 0) volts = 0;
    if (volts > 10.0f) volts = 10.0f;
    _cfg.dacDefault[ch] = volts;
    INFO_LOG("AlexaManager: DAC%u default set to %.3f V", ch + 1, volts);
}

void AlexaManager::setBuzzerPattern(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t reps) {
    if (freq) _cfg.buzzerFreq = freq;
    if (onMs) _cfg.buzzerOnMs = onMs;
    _cfg.buzzerOffMs = offMs;
    if (reps) _cfg.buzzerRepeats = reps;
    INFO_LOG("AlexaManager: buzzer pattern updated F=%u on=%u off=%u reps=%u",
        _cfg.buzzerFreq, _cfg.buzzerOnMs, _cfg.buzzerOffMs, _cfg.buzzerRepeats);
}

void AlexaManager::snapshotSensors(Stream& s, bool header) {
    if (header) s.println("\n--- Snapshot @ " + _timestamp() + " ---");

    if (_di && _di->isConnected()) {
        _di->updateAllInputs();
        uint8_t bits = _di->getInputState();
        s.print("DI: ");
        for (uint8_t i = 0; i < NUM_DIGITAL_INPUTS; i++) {
            s.print((bits & (1 << i)) ? "1" : "0");
            if (i + 1 < NUM_DIGITAL_INPUTS) s.print(",");
        }
        s.println();
    }
    else s.println("DI: (not connected)");

    if (_rel && _rel->isConnected()) {
        uint8_t rb = _rel->getAllStates();
        s.print("Relays: ");
        for (uint8_t i = 0; i < NUM_RELAY_OUTPUTS; i++) {
            bool on = ((rb & (1 << i)) == 0);
            s.print(on ? "ON" : "OFF");
            if (i + 1 < NUM_RELAY_OUTPUTS) s.print(",");
        }
        s.println();
    }
    else s.println("Relays: (not connected)");

    if (_dac && _dac->isInitialized()) {
        s.print("DAC: ");
        for (uint8_t ch = 0; ch < 2; ch++)
            s.printf("Ch%u=%.3fV ", ch + 1, _dac->getVoltage(ch));
        s.println();
    }
    else s.println("DAC: (not initialized)");

    if (_ai) {
        s.print("Analog Voltage: ");
        for (uint8_t i = 0; i < NUM_ANALOG_CHANNELS; i++)
            s.printf("V%u=%.3fV ", i + 1, _ai->readVoltage(i));
        s.println();
        s.print("Analog Current: ");
        for (uint8_t i = 0; i < NUM_CURRENT_CHANNELS; i++)
            s.printf("I%u=%.2fmA ", i + 1, _ai->readCurrent(i));
        s.println();
    }
    else s.println("Analog Inputs: (not initialized)");

    if (_dht) {
        _dht->update();
        for (uint8_t ch = 0; ch < NUM_DHT_SENSORS; ch++) {
            float t = _dht->getTemperature(ch);
            float h = _dht->getHumidity(ch);
            s.printf("DHT%u: T=%.2fC H=%s\n", ch + 1, t, isnan(h) ? "N/A" : String(h, 2).c_str());
        }
        uint8_t dsCount = _dht->getDS18B20Count();
        if (dsCount > 0) {
            float t = _dht->getDS18B20Temperature(0);
            s.printf("DS18B20[0]=%.2fC (count=%u)\n", t, dsCount);
        }
        else {
            s.println("DS18B20: none");
        }
    }
    else s.println("Sensors: (not initialized)");

    if (_rtc && _rtc->isConnected()) {
        DateTime dt = _rtc->getDateTime(false);
        s.printf("RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
            dt.year(), dt.month(), dt.day(),
            dt.hour(), dt.minute(), dt.second());
    }
    else s.println("RTC: (not connected)");

    if (header) s.println("---------------------------");
}