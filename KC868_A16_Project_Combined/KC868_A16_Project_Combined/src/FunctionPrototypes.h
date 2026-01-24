#pragma once
/**
 * FunctionPrototypes.h
 * Global function declarations (kept to avoid Arduino .ino auto-prototype dependency).
 * Generated from original KC868_A16_Controller.ino.
 */

#include "Globals.h"

// Function prototypes
// Main app entry points (implemented in src/core/App.cpp)
void appSetup();
void appLoop();

void initI2C();
void initWiFi();
void initEthernet();
void printMacSummary();
void serviceEthernetDhcp();
void initRTC();
void setupWebServer();
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void broadcastUpdate();
void initRS485();
void initRF();
void saveConfiguration();
void loadConfiguration();
void saveWiFiCredentials(String ssid, String password);
void loadWiFiCredentials();
void saveCommunicationSettings();
void loadCommunicationSettings();
void saveCommunicationConfig();
void loadCommunicationConfig();
bool readInputs();
bool writeOutputs();
void handleWebRoot();
void handleNotFound();
void handleFileUpload();
void handleRelayControl();
void handleSystemStatus();
void handleSchedules();
void handleUpdateSchedule();
void handleConfig();
void handleUpdateConfig();
void handleAnalogTriggers();
void handleUpdateAnalogTriggers();
void handleDebug();
void handleDebugCommand();
void handleReboot();
void handleCommunicationStatus();
void handleSetCommunication();
void handleCommunicationConfig();
void handleUpdateCommunicationConfig();
void handleGetTime();
void handleSetTime();
void handleI2CScan();
void checkSchedules();
void checkAnalogTriggers();
void processRS485Commands();
void processSerialCommands();
void WiFiEvent(WiFiEvent_t event);
void EthEvent(WiFiEvent_t event);
void debugPrintln(String message);
void syncTimeFromNTP();
void syncTimeFromClient(int year, int month, int day, int hour, int minute, int second);
String getTimeString();
String processCommand(String command);
int readAnalogInput(uint8_t index);
float convertAnalogToVoltage(int analogValue);
int calculatePercentage(float voltage);
void printIOStates(); // New function to print I/O states for debugging

// Interrupt handling function prototypes
void initInterruptConfigs();
void saveInterruptConfigs();
void loadInterruptConfigs();
void setupInputInterrupts();
void disableInputInterrupts();
void processInputInterrupts();
void processInputChange(int inputIndex, bool newState);
void pollNonInterruptInputs();
void handleInterrupts();
void handleUpdateInterrupts();

void checkInputBasedSchedules(int changedInputIndex, bool newState);
void executeSchedule(int scheduleIndex);
void handleEvaluateInputSchedules();


// Additional prototypes (not in original prototype block but used across modules)
void startAPMode();
void resetEthernet();
void saveNetworkSettings();
void loadNetworkSettings();
void handleNetworkSettings();
void handleUpdateNetworkSettings();

void executeScheduleAction(int scheduleIndex);
void executeScheduleAction(int scheduleIndex, uint16_t targetId);
void checkInputBasedSchedules();

void initializeDefaultConfig();
void saveSchedulesToEEPROM();
void initializeSensor(uint8_t htIndex);
void readSensor(uint8_t htIndex);
void saveHTSensorConfig();
void loadHTSensorConfig();
void handleHTSensors();
void handleUpdateHTSensor();

String getUptimeString();
String getActiveProtocolName();
