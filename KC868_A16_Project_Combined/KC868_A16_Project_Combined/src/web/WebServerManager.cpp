// WebServerManager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void setupWebServer() {
    // Serve static files from SPIFFS
    server.serveStatic("/", SPIFFS, "/index.html");
    server.serveStatic("/style.css", SPIFFS, "/style.css");
    server.serveStatic("/script.js", SPIFFS, "/script.js");

    // API endpoints
    server.on("/", HTTP_GET, handleWebRoot);
    server.on("/api/status", HTTP_GET, handleSystemStatus);
    server.on("/api/relay", HTTP_POST, handleRelayControl);
    server.on("/api/schedules", HTTP_GET, handleSchedules);
    server.on("/api/schedules", HTTP_POST, handleUpdateSchedule);
    server.on("/api/evaluate-input-schedules", HTTP_GET, handleEvaluateInputSchedules);
    server.on("/api/analog-triggers", HTTP_GET, handleAnalogTriggers);
    server.on("/api/analog-triggers", HTTP_POST, handleUpdateAnalogTriggers);
    server.on("/api/ht-sensors", HTTP_GET, handleHTSensors);
    server.on("/api/ht-sensors", HTTP_POST, handleUpdateHTSensor);
    server.on("/api/config", HTTP_GET, handleConfig);
    server.on("/api/config", HTTP_POST, handleUpdateConfig);
    server.on("/api/debug", HTTP_GET, handleDebug);
    server.on("/api/debug", HTTP_POST, handleDebugCommand);
    server.on("/api/reboot", HTTP_POST, handleReboot);

    // Communication endpoints
    server.on("/api/communication", HTTP_GET, handleCommunicationStatus);
    server.on("/api/communication", HTTP_POST, handleSetCommunication);
    server.on("/api/communication/config", HTTP_GET, handleCommunicationConfig);
    server.on("/api/communication/config", HTTP_POST, handleUpdateCommunicationConfig);

    // Time endpoints
    server.on("/api/time", HTTP_GET, handleGetTime);
    server.on("/api/time", HTTP_POST, handleSetTime);

    // Diagnostic endpoints
    server.on("/api/i2c/scan", HTTP_GET, handleI2CScan);

    // Network settings endpoints
    server.on("/api/network/settings", HTTP_GET, handleNetworkSettings);
    server.on("/api/network/settings", HTTP_POST, handleUpdateNetworkSettings);

    // Add interrupt configuration endpoint
    server.on("/api/interrupts", HTTP_GET, handleInterrupts);
    server.on("/api/interrupts", HTTP_POST, handleUpdateInterrupts);

    // File upload handler
    server.on("/api/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "File upload complete");
        }, handleFileUpload);

    // Not found handler
    server.onNotFound(handleNotFound);

    // Start server
    server.begin();
    debugPrintln("Web server started");
}

void handleWebRoot() {
    server.sendHeader("Location", "/index.html", true);
    server.send(302, "text/plain", "");
}

void handleNotFound() {
    // If in captive portal mode and request is for a domain, redirect to configuration page
    if (WiFi.getMode() == WIFI_AP && !server.hostHeader().startsWith("192.168.")) {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
        return;
    }

    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
}

void handleFileUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        debugPrintln("File upload start: " + filename);
        fsUploadFile = SPIFFS.open(filename, FILE_WRITE);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
            debugPrintln("File upload complete: " + String(upload.totalSize) + " bytes");
        }
    }
}

