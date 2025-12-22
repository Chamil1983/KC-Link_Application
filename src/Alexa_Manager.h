#ifndef ALEXA_MANAGER_H
#define ALEXA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Espalexa.h>

#include "Config.h"
#include "Debug.h"
#include "RelayDriver.h"
#include "DACControl.h"
#include "AnalogInputs.h"
#include "DHT_Sensors.h"
#include "DigitalInputDriver.h"
#include "RTCManager.h"

// Alexa / Espalexa manager with robust WiFi + diagnostics.
class AlexaManager {
public:
    struct Settings {
        String ssid;
        String password;
        float  dacDefault[2] = { 1.000f, 1.000f };
        uint16_t buzzerFreq = 2000;
        uint16_t buzzerOnMs = 300;
        uint16_t buzzerOffMs = 300;
        uint8_t  buzzerRepeats = 4;
        bool     autoSnapshot = true;
        uint32_t autoSnapshotMs = 10000;
        bool     blockingWifi = true;
        bool     allowSoftAPFallback = false;
        uint8_t  maxScanRetries = 3;
        uint32_t connectAttemptTimeoutMs = 20000;
        uint8_t  maxFailedCyclesBeforeAP = 5;
        uint32_t softApRetryMs = 180000;
    };

    AlexaManager();

    void begin(const Settings& cfg,
        RelayDriver* rel,
        DACControl* dac,
        AnalogInputs* ai,
        DHTSensors* dht,
        DigitalInputDriver* di,
        RTCManager* rtc);

    void loop();
    void printStatus(Stream& s);
    void listDevices(Stream& s);

    void setDacDefault(uint8_t ch, float volts);
    void setBuzzerPattern(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t reps);
    void snapshotSensors(Stream& s, bool header = true);

    bool wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
    bool gotIP() const { return _gotIP && WiFi.localIP() != INADDR_NONE; }
    bool alexaReady() const { return _espConfigured && gotIP(); }
    bool pendingEspConfig() const { return _pendingEspConfig && !_espConfigured; }

    void setWifiCredentials(const String& ssid, const String& password);

    // Optional: set host name to something simple for router visibility
    void setHostname(const String& hn) { _hostname = hn; }

private:
    Settings   _cfg;

    // Espalexa
    Espalexa _esp;
    bool _espConfigured = false;
    bool _pendingEspConfig = false;

    // WiFi tracking
    bool _gotIP = false;
    bool _wifiEventAttached = false;
    unsigned long _lastWiFiAttemptStart = 0;
    unsigned long _lastRetryMs = 0;
    uint32_t      _retryBackoffMs = 5000;
    const uint32_t RETRY_BACKOFF_MAX = 60000;
    uint8_t       _scanAttempts = 0;
    bool          _ssidFound = false;
    bool          _softAPMode = false;
    unsigned long _softAPStartMs = 0;
    uint8_t       _failedConnectCycles = 0;
    wifi_err_reason_t _lastDiscReason = WIFI_REASON_UNSPECIFIED;
    wl_status_t       _lastStatus = WL_IDLE_STATUS;

    unsigned long _lastSensorAuto = 0;
    unsigned long _lastGotIpMs = 0;

    // Optional hostname
    String _hostname = "A8RM-ESP32";

    // Driver pointers
    RelayDriver* _rel = nullptr;
    DACControl* _dac = nullptr;
    AnalogInputs* _ai = nullptr;
    DHTSensors* _dht = nullptr;
    DigitalInputDriver* _di = nullptr;
    RTCManager* _rtc = nullptr;

    // Device name constants
    static constexpr const char* DEV_RELAY_PREFIX = "Relay ";
    static constexpr const char* DEV_DAC_PREFIX = "DAC ";
    static constexpr const char* DEV_BUZZER = "Buzzer";
    static constexpr const char* DEV_SENSORS = "Sensors";
    static constexpr const char* DEV_RTC_REFRESH = "RTC Sync";

    // Setup / teardown
    void _attachWifiEvents();

    bool _startWiFiCycle();
    void _evaluateWiFiProgress();
    bool _attemptBlockingWiFi();

    // Espalexa configuration ONLY from loop context
    void _maybeConfigureEspInLoop();
    void _configureEspalexa();
    void _teardownEspalexa();

    // WiFi helpers
    void _scanForSSID();
    void _enterSoftAP();
    void _maybeExitSoftAP();
    void _configureWiFiForStability();

    // Diagnostics
    String _timestamp();
    void _logAlexa(const char* device, bool state, uint8_t brightness = 255);
    const char* _discReasonString(wifi_err_reason_t r);
    const char* _statusString(wl_status_t s);

    // Handlers
    void _handleSetRelay(uint8_t index, bool state);
    void _handleSetDAC(uint8_t ch, bool state, uint8_t bri);
    void _handleBuzzer(bool state);
    void _handleSensorsTrigger();
    void _handleRTCRefresh();

    boolean _connectWifi();

    // WiFi event callback
    static void _wifiEventThunk(WiFiEvent_t event, WiFiEventInfo_t info);
    void _wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);
};

#endif