// RtcDriver.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"


// ====== NTP servers ======
const char* NTP1 = "pool.ntp.org";
const char* NTP2 = "time.nist.gov";

// ====== Melbourne TZ (Australia/Melbourne) ======
static const char* TZ_MELBOURNE = "AEST-10AEDT-11,M10.1.0/2,M4.1.0/3";

// ====== Helpers ======
static bool waitForSystemTime(int maxRetries = 20, int delayMs = 500) {
    time_t now = time(nullptr);
    int retry = 0;

    while (now < 24 * 3600 && retry < maxRetries) { // "valid" epoch check
        delay(delayMs);
        now = time(nullptr);
        retry++;
    }
    return (now >= 24 * 3600);
}

// Set ESP32 timezone to Melbourne and ensure libc uses it for localtime()
static void setMelbourneTimezone() {
    setenv("TZ", TZ_MELBOURNE, 1);
    tzset();
}

// Update DS3231 from current system time (UTC conversion handled by RTC storing "local"?)
// Best practice: store UTC in RTC to avoid DST ambiguity.
// We will store UTC in RTC and always print local time via system clock.
static void updateRTCFromSystemUTC() {
    if (!rtcInitialized) return;

    time_t now = time(nullptr);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);

    rtc.adjust(DateTime(tm_utc.tm_year + 1900,
        tm_utc.tm_mon + 1,
        tm_utc.tm_mday,
        tm_utc.tm_hour,
        tm_utc.tm_min,
        tm_utc.tm_sec));
    debugPrintln("Updated RTC with system UTC time");
}

// If RTC exists, load system time from RTC (assumes RTC holds UTC)
static void setSystemFromRTCUTC() {
    DateTime r = rtc.now(); // read RTC
    struct tm tm_utc {};
    tm_utc.tm_year = r.year() - 1900;
    tm_utc.tm_mon = r.month() - 1;
    tm_utc.tm_mday = r.day();
    tm_utc.tm_hour = r.hour();
    tm_utc.tm_min = r.minute();
    tm_utc.tm_sec = r.second();

    // Convert this UTC struct tm to epoch seconds (timegm is non-standard, so do manual)
    // We'll use mktime after forcing TZ=UTC temporarily.
    char* oldTZ = getenv("TZ");
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t t = mktime(&tm_utc);
    if (oldTZ) setenv("TZ", oldTZ, 1); else unsetenv("TZ");
    tzset();

    if (t > 24 * 3600) {
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, nullptr);
        debugPrintln("System time loaded from RTC (UTC)");
    }
}



void initRTC() {
    // Always set TZ for Melbourne early so getTimeString() returns local time.
    setMelbourneTimezone();

    rtcInitialized = rtc.begin();
    if (!rtcInitialized) {
        debugPrintln("Couldn't find RTC, using ESP32 internal time");

        // If no RTC, rely on NTP to set ESP32 time
        syncTimeFromNTP();
        return;
    }

    debugPrintln("RTC found");

    if (rtc.lostPower()) {
        debugPrintln("RTC lost power -> setting RTC to compile time (treated as UTC here), then NTP will correct.");

        // NOTE: Compile time is NOT UTC; it's your PC local time when compiled.
        // We set it anyway as a fallback; NTP should overwrite shortly.
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

        // Try to get accurate time from NTP and store to RTC (UTC)
        syncTimeFromNTP();
    }
    else {
        // RTC has time; load ESP32 system clock from RTC first
        setSystemFromRTCUTC();

        // Optionally, still do an NTP sync to correct drift (recommended when WiFi is available)
        syncTimeFromNTP();
    }

    // Print current RTC time (as UTC) and system local time (Melbourne)
    DateTime r = rtc.now();
    String rtcUtc = String(r.year()) + "-" +
        String(r.month()) + "-" +
        String(r.day()) + " " +
        String(r.hour()) + ":" +
        String(r.minute()) + ":" +
        String(r.second());
    debugPrintln("RTC time (stored UTC): " + rtcUtc);

    debugPrintln("System local time (Melbourne): " + getTimeString());
}


void syncTimeFromNTP() {
    debugPrintln("Syncing time from NTP (Melbourne TZ will be applied to localtime) ...");

    // Important:
    // - configTime() sets up SNTP and stores UTC internally.
    // - localtime_r() will apply TZ rules we set with setenv/tzset.
    configTime(0, 0, NTP1, NTP2);

    if (!waitForSystemTime(20, 500)) {
        debugPrintln("NTP time sync failed");
        return;
    }

    debugPrintln("NTP time sync successful");

    // Update RTC if available (store UTC)
    if (rtcInitialized) {
        updateRTCFromSystemUTC();
    }
}

void syncTimeFromClient(int year, int month, int day, int hour, int minute, int second) {
    // Assume "client time" is Melbourne LOCAL time.
    // We'll convert that local time to epoch using Melbourne TZ, set system time,
    // and then store UTC into RTC.

    setMelbourneTimezone();

    struct tm tm_local {};
    tm_local.tm_year = year - 1900;
    tm_local.tm_mon = month - 1;
    tm_local.tm_mday = day;
    tm_local.tm_hour = hour;
    tm_local.tm_min = minute;
    tm_local.tm_sec = second;
    tm_local.tm_isdst = -1; // let TZ rules determine DST

    time_t t = mktime(&tm_local); // interprets as local time (Melbourne because TZ set)
    if (t <= 24 * 3600) {
        debugPrintln("Client time conversion failed");
        return;
    }

    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, nullptr);
    debugPrintln("Updated system time with client LOCAL time (Melbourne)");

    if (rtcInitialized) {
        updateRTCFromSystemUTC();
    }
}

String getTimeString() {
    // Returns Melbourne local time string
    setMelbourneTimezone();

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}





