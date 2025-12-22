#include "RTCManager.h"

RTCManager rtcManager;

RTCManager::RTCManager()
    : initialized(false),
    connected(false),
    cachedNow(DateTime(F(__DATE__), F(__TIME__))),
    lastTimeRead(0),
    sqwEnabled(false),
    sqwInterruptEnabled(false),
    currentSqwMode((Ds3231SqwPinMode)DS3231_OFF),
    lastSqwState(false),
    sqwPulseCount(0),
    alarm1Active(false),
    alarm1Time(DateTime(2000, 1, 1, 0, 0, 0)),
    patternFreq(2000),
    patternOnMs(200),
    patternOffMs(200),
    patternRepeats(5),
    patternStep(0),
    patternPhaseStart(0),
    patternRunning(false),
    patternPhaseOn(false),
    volumeLevel(5)
{
    activeBeep = { false, 0, 0, false };
    sqwTest = { false, 0, 0, 0, (Ds3231SqwPinMode)DS3231_OFF, false, 0 };
}

bool RTCManager::begin(TwoWire& wire, bool setIfLostPower) {
    if (initialized) return connected;
    wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);
    delay(10);

    if (!rtc.begin(&wire)) {
        ERROR_LOG("RTC DS3231 not found");
        initialized = true;
        connected = false;
        return false;
    }
    connected = true;
    initialized = true;

    if (lostPower()) {
        INFO_LOG("RTC lost power");
        if (setIfLostPower) {
            adjustToCompileTime();
            INFO_LOG("Adjusted to compile time");
        }
    }
    else {
        cachedNow = rtc.now();
        lastTimeRead = millis();
    }

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    disableSquareWave();

    return true;
}

bool RTCManager::lostPower() {
    return connected ? rtc.lostPower() : true;
}

void RTCManager::adjustToCompileTime() {
    if (!connected) return;
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    cachedNow = rtc.now();
    lastTimeRead = millis();
}

bool RTCManager::parseDate(const char* s, int& y, int& m, int& d) {
    return sscanf(s, "%d-%d-%d", &y, &m, &d) == 3 &&
        y >= 2000 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}
bool RTCManager::parseTime(const char* s, int& h, int& m, int& sec) {
    return sscanf(s, "%d:%d:%d", &h, &m, &sec) == 3 &&
        h >= 0 && h < 24 && m >= 0 && m < 60 && sec >= 0 && sec < 60;
}

DateTime RTCManager::getDateTime(bool forceRefresh) {
    if (!connected) return cachedNow;
    unsigned long nowMs = millis();
    if (forceRefresh || (nowMs - lastTimeRead) > 1000) {
        cachedNow = rtc.now();
        lastTimeRead = nowMs;
    }
    return cachedNow;
}

void RTCManager::refreshTime() {
    if (!connected) return;
    cachedNow = rtc.now();
    lastTimeRead = millis();
}

bool RTCManager::setDateTimeFromString(const char* iso) {
    if (!connected || !iso) return false;
    char buf[25];
    strncpy(buf, iso, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* sep = strchr(buf, 'T');
    if (!sep) sep = strchr(buf, ' ');
    if (!sep) return false;
    *sep = 0;
    const char* dp = buf;
    const char* tp = sep + 1;
    int Y, M, D, h, m, s;
    if (!parseDate(dp, Y, M, D)) return false;
    if (!parseTime(tp, h, m, s)) return false;
    setDateTime(DateTime(Y, M, D, h, m, s));
    return true;
}

bool RTCManager::setDate(uint16_t y, uint8_t m, uint8_t d) {
    if (!connected) return false;
    DateTime c = getDateTime(true);
    setDateTime(DateTime(y, m, d, c.hour(), c.minute(), c.second()));
    return true;
}

bool RTCManager::setClock(uint8_t hh, uint8_t mm, uint8_t ss) {
    if (!connected) return false;
    DateTime c = getDateTime(true);
    setDateTime(DateTime(c.year(), c.month(), c.day(), hh, mm, ss));
    return true;
}

void RTCManager::setDateTime(const DateTime& dt) {
    if (!connected) return;
    rtc.adjust(dt);
    cachedNow = dt;
    lastTimeRead = millis();
}

uint16_t RTCManager::year() { return getDateTime().year(); }
uint8_t  RTCManager::month() { return getDateTime().month(); }
uint8_t  RTCManager::day() { return getDateTime().day(); }
uint8_t  RTCManager::hour() { return getDateTime().hour(); }
uint8_t  RTCManager::minute() { return getDateTime().minute(); }
uint8_t  RTCManager::second() { return getDateTime().second(); }

bool RTCManager::setAlarm1Absolute(const DateTime& dt) {
    if (!connected) return false;
    bool ok = rtc.setAlarm1(dt, DS3231_A1_Date);
    if (ok) {
        alarm1Active = true;
        alarm1Time = dt;
        configureSquareWave((Ds3231SqwPinMode)DS3231_OFF, true);
    }
    return ok;
}

void RTCManager::clearAlarm1() {
    if (!connected) return;
    rtc.clearAlarm(1);
    rtc.disableAlarm(1);
    alarm1Active = false;
}

void RTCManager::clearAllAlarms() {
    clearAlarm1();
    rtc.clearAlarm(2);
    rtc.disableAlarm(2);
}

bool RTCManager::checkAndServiceAlarms() {
    if (!connected) return false;
    if (alarm1Active && rtc.alarmFired(1)) {
        rtc.clearAlarm(1);
        alarm1Active = false;
        playAlarmPattern();
        return true;
    }
    return false;
}

void RTCManager::simpleBeep(uint16_t freq, uint32_t durationMs) {
    if (freq == 0) freq = 1000;
    if (durationMs == 0) durationMs = 200;
    activeBeep.active = true;
    activeBeep.freq = freq;
    activeBeep.endMs = millis() + durationMs;
    activeBeep.pattern = false;
    tone(PIN_BUZZER, freq);
}

void RTCManager::stopBeep() {
    noTone(PIN_BUZZER);
    activeBeep.active = false;
}

void RTCManager::setAlarmPattern(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t repeats) {
    if (freq) patternFreq = freq;
    if (onMs) patternOnMs = onMs;
    patternOffMs = offMs;
    patternRepeats = repeats ? repeats : 1;
}

void RTCManager::playAlarmPattern() {
    patternRunning = true;
    patternStep = 0;
    patternPhaseOn = true;
    patternPhaseStart = millis();
    simpleBeep(patternFreq, patternOnMs);
    activeBeep.pattern = true;
}

void RTCManager::serviceBeepPattern() {
    if (!patternRunning) {
        if (activeBeep.active && !activeBeep.pattern && millis() >= activeBeep.endMs)
            stopBeep();
        return;
    }
    unsigned long nowMs = millis();
    if (activeBeep.active && nowMs >= activeBeep.endMs)
        stopBeep();

    if (patternPhaseOn) {
        if (!activeBeep.active) {
            patternPhaseOn = false;
            patternPhaseStart = nowMs;
        }
    }
    else {
        if (nowMs - patternPhaseStart >= patternOffMs) {
            patternStep++;
            if (patternStep >= patternRepeats) {
                patternRunning = false;
                return;
            }
            patternPhaseOn = true;
            patternPhaseStart = nowMs;
            simpleBeep(patternFreq, patternOnMs);
            activeBeep.pattern = true;
        }
    }
}

void RTCManager::ensureSqwInterrupt(bool en) {
    digitalInputs.enableRtcSqwInterrupt(en);
    sqwInterruptEnabled = en;
}

Ds3231SqwPinMode RTCManager::modeFromFreq(uint32_t freq) {
    switch (freq) {
    case 1:    return (Ds3231SqwPinMode)DS3231_SQW1Hz;
    case 1024: return (Ds3231SqwPinMode)DS3231_SQW1024Hz;
    case 4096: return (Ds3231SqwPinMode)DS3231_SQW4096Hz;
    case 8192: return (Ds3231SqwPinMode)DS3231_SQW8192Hz;
    default:   return (Ds3231SqwPinMode)DS3231_OFF;
    }
}

void RTCManager::configureSquareWave(Ds3231SqwPinMode mode, bool enableInterrupt) {
    if (!connected) return;
    rtc.writeSqwPinMode(mode);
    currentSqwMode = mode;
    sqwEnabled = (mode != (Ds3231SqwPinMode)DS3231_OFF);
    ensureSqwInterrupt(enableInterrupt && sqwEnabled);
}

void RTCManager::disableSquareWave() {
    configureSquareWave((Ds3231SqwPinMode)DS3231_OFF, false);
}

bool RTCManager::startSqwTest(uint32_t freqHz, uint16_t durationSec) {
    if (durationSec == 0) durationSec = 3;
    Ds3231SqwPinMode mode = modeFromFreq(freqHz);
    if (mode == (Ds3231SqwPinMode)DS3231_OFF) {
        Serial.println(F("ERR: Unsupported SQW freq"));
        return false;
    }
    if (sqwTest.active) {
        Serial.println(F("ERR: SQW test running"));
        return false;
    }
    sqwTest.active = true;
    sqwTest.targetFreq = freqHz;
    sqwTest.durationSec = durationSec;
    sqwTest.endMs = millis() + (uint32_t)durationSec * 1000UL;
    sqwTest.savedMode = currentSqwMode;
    sqwTest.savedInterrupt = sqwInterruptEnabled;
    resetSqwPulseCount();
    configureSquareWave(mode, true);
    Serial.print(F("OK: SQW test "));
    Serial.print(freqHz);
    Serial.print(F("Hz for "));
    Serial.print(durationSec);
    Serial.println(F("s"));
    return true;
}

void RTCManager::finishSqwTestIfDue() {
    if (!sqwTest.active) return;
    if (millis() >= sqwTest.endMs) {
        float measured = (float)sqwPulseCount / (float)sqwTest.durationSec;
        Serial.print(F("SQWTEST RESULT: "));
        Serial.print(measured, 2);
        Serial.print(F("Hz (req "));
        Serial.print(sqwTest.targetFreq);
        Serial.print(F("Hz) pulses="));
        Serial.println(sqwPulseCount);
        configureSquareWave(sqwTest.savedMode, sqwTest.savedInterrupt);
        resetSqwPulseCount();
        sqwTest.active = false;
    }
}

float RTCManager::getTemperatureC() {
    if (!connected) return NAN;
    return rtc.getTemperature(); // Your RTClib version
}

bool RTCManager::readSqwPin() {
    return digitalInputs.readPortAPin(MCP_RTC_SQW_PIN);
}

void RTCManager::update() {
    if (!connected) {
        serviceBeepPattern();
        return;
    }

    if (sqwEnabled && sqwInterruptEnabled) {
        bool state = readSqwPin();
        if (!lastSqwState && state) {
            sqwPulseCount++;
            if (currentSqwMode == (Ds3231SqwPinMode)DS3231_SQW1Hz) {
                cachedNow = rtc.now();
                lastTimeRead = millis();
            }
        }
        lastSqwState = state;
    }
    else {
        if (millis() - lastTimeRead > 1000) {
            cachedNow = rtc.now();
            lastTimeRead = millis();
        }
    }

    checkAndServiceAlarms();
    finishSqwTestIfDue();
    serviceBeepPattern();

    if (activeBeep.active && !activeBeep.pattern && millis() >= activeBeep.endMs)
        stopBeep();
}

bool RTCManager::tryNTPSync() {
    // Stub – place holder if you later add WiFi/Ethernet & call configTime().
    return false;
}