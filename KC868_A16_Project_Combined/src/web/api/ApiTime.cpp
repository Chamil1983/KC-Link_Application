// ApiTime.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../../FunctionPrototypes.h"

void handleGetTime() {
    DynamicJsonDocument doc(256);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    doc["year"] = timeinfo.tm_year + 1900;
    doc["month"] = timeinfo.tm_mon + 1;
    doc["day"] = timeinfo.tm_mday;
    doc["hour"] = timeinfo.tm_hour;
    doc["minute"] = timeinfo.tm_min;
    doc["second"] = timeinfo.tm_sec;
    doc["formatted"] = getTimeString();
    doc["rtc_available"] = rtcInitialized;
    doc["timestamp"] = now; // Add Unix timestamp for client-side comparison

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetTime() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("year") && doc.containsKey("month") && doc.containsKey("day") &&
                doc.containsKey("hour") && doc.containsKey("minute") && doc.containsKey("second")) {

                int year = doc["year"];
                int month = doc["month"];
                int day = doc["day"];
                int hour = doc["hour"];
                int minute = doc["minute"];
                int second = doc["second"];

                // Update system time and RTC if available
                syncTimeFromClient(year, month, day, hour, minute, second);

                response = "{\"status\":\"success\",\"message\":\"Time updated\"}";
            }
            else if (doc.containsKey("ntp_sync") && doc["ntp_sync"].as<bool>()) {
                // Sync time from NTP
                syncTimeFromNTP();
                response = "{\"status\":\"success\",\"message\":\"NTP sync initiated\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

