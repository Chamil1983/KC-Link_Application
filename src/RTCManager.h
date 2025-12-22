#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "Config.h"
#include "Debug.h"
#include "DigitalInputDriver.h"

extern DigitalInputDriver digitalInputs;

/* RTClib square wave enum normalization / fallbacks */
#if defined(DS3231_SquareWave1Hz) && !defined(DS3231_SQW1Hz)
#define DS3231_SQW1Hz DS3231_SquareWave1Hz
#endif
#if defined(DS3231_SquareWave1kHz) && !defined(DS3231_SQW1024Hz)
#define DS3231_SQW1024Hz DS3231_SquareWave1kHz
#endif
#if defined(DS3231_SquareWave4kHz) && !defined(DS3231_SQW4096Hz)
#define DS3231_SQW4096Hz DS3231_SquareWave4kHz
#endif
#if defined(DS3231_SquareWave8kHz) && !defined(DS3231_SQW8192Hz)
#define DS3231_SQW8192Hz DS3231_SquareWave8kHz
#endif
#if defined(DS3231_SquareWaveOff) && !defined(DS3231_OFF)
#define DS3231_OFF DS3231_SquareWaveOff
#endif
#ifndef DS3231_SQW1Hz
#define DS3231_SQW1Hz 0x00
#endif
#ifndef DS3231_SQW1024Hz
#define DS3231_SQW1024Hz 0x08
#endif
#ifndef DS3231_SQW4096Hz
#define DS3231_SQW4096Hz 0x10
#endif
#ifndef DS3231_SQW8192Hz
#define DS3231_SQW8192Hz 0x18
#endif
#ifndef DS3231_OFF
#define DS3231_OFF 0x1C
#endif

class RTCManager {
public:
    RTCManager();

    bool begin(TwoWire& wire = Wire, bool setIfLostPower = true);
    bool isConnected() const { return connected; }
    bool lostPower();
    void adjustToCompileTime();

    DateTime getDateTime(bool forceRefresh = false);
    void refreshTime();
    bool setDateTimeFromString(const char* iso);      // "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS"
    bool setDate(uint16_t y, uint8_t m, uint8_t d);
    bool setClock(uint8_t hh, uint8_t mm, uint8_t ss);
    void setDateTime(const DateTime& dt);

    uint16_t year(); uint8_t month(); uint8_t day();
    uint8_t hour(); uint8_t minute(); uint8_t second();

    bool setAlarm1Absolute(const DateTime& dt);
    bool hasAlarm1() const { return alarm1Active; }
    DateTime getAlarm1() const { return alarm1Time; }
    void clearAlarm1();
    void clearAllAlarms();
    bool checkAndServiceAlarms();

    void simpleBeep(uint16_t freq, uint32_t durationMs);
    void stopBeep();
    bool isBeeping() const { return activeBeep.active; }
    void setVolumeLevel(uint8_t level) { if (level < 1) level = 1; if (level > 5) level = 5; volumeLevel = level; }
    uint8_t getVolumeLevel() const { return volumeLevel; }
    void playAlarmPattern();
    void setAlarmPattern(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t repeats);

    void configureSquareWave(Ds3231SqwPinMode mode, bool enableInterrupt);
    void disableSquareWave();
    uint32_t getSqwPulseCount() const { return sqwPulseCount; }
    void resetSqwPulseCount() { sqwPulseCount = 0; }

    bool startSqwTest(uint32_t freqHz, uint16_t durationSec);
    bool isSqwTestActive() const { return sqwTest.active; }

    float getTemperatureC();

    // Call in loop()
    void update();

    // Optional future hook: attempt NTP (currently just stub returns false)
    bool tryNTPSync();  // safe no-op / stub

private:
    RTC_DS3231 rtc;
    bool initialized;
    bool connected;

    DateTime cachedNow;
    unsigned long lastTimeRead;

    bool sqwEnabled;
    bool sqwInterruptEnabled;
    Ds3231SqwPinMode currentSqwMode;
    bool lastSqwState;
    uint32_t sqwPulseCount;

    bool alarm1Active;
    DateTime alarm1Time;

    struct SimpleBeep {
        bool active;
        uint16_t freq;
        uint32_t endMs;
        bool pattern;
    } activeBeep;

    uint16_t patternFreq;
    uint16_t patternOnMs;
    uint16_t patternOffMs;
    uint8_t  patternRepeats;

    uint8_t patternStep;
    unsigned long patternPhaseStart;
    bool patternRunning;
    bool patternPhaseOn;

    uint8_t volumeLevel;

    struct SqwTest {
        bool active;
        uint32_t targetFreq;
        uint16_t durationSec;
        unsigned long endMs;
        Ds3231SqwPinMode savedMode;
        bool savedInterrupt;
        uint32_t startCount;
    } sqwTest;

    bool parseDate(const char* s, int& y, int& m, int& d);
    bool parseTime(const char* s, int& h, int& m, int& sec);
    void ensureSqwInterrupt(bool en);
    bool readSqwPin();
    Ds3231SqwPinMode modeFromFreq(uint32_t freq);
    void serviceBeepPattern();
    void finishSqwTestIfDue();
};

extern RTCManager rtcManager;

#endif // RTC_MANAGER_H