// App.cpp - main orchestrator split from original sketch
#include "../FunctionPrototypes.h"
#include "../comm/ModbusRtuManager.h"
#include "../comm/BACnetDriver.h"
#include "../comm/BACnetIntegration.h"
#include <new>

static void reinitWebPortsIfNeeded() {
    // Reconstruct servers (before begin/handlers) if user changed ports.
    // This is safe here because we haven't registered handlers nor called begin().
    if (httpPort < 1 || httpPort > 65535) httpPort = 80;
    if (wsPort < 1 || wsPort > 65535) wsPort = 81;
    new (&server) WebServer((uint16_t)httpPort);
    new (&webSocket) WebSocketsServer((uint16_t)wsPort);
}

void appSetup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    Serial.println("\nKC868-A16 Controller starting up...");

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // Initialize file system
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
    }
    else {
        Serial.println("SPIFFS Mounted SUCCESSFULLY...");
    }

    // Initialize I2C with custom pins
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);  // Lower to 50kHz for more reliable communication

    // Initialize PCF8574 expanders
    initI2C();

    // Initialize direct GPIO inputs
    pinMode(HT1_PIN, INPUT_PULLUP);
    pinMode(HT2_PIN, INPUT_PULLUP);
    pinMode(HT3_PIN, INPUT_PULLUP);

    // Load configuration from EEPROM
    loadConfiguration();
    loadWiFiCredentials();
    loadCommunicationSettings();
    loadCommunicationConfig();
    loadNetworkSettings(); // Load persisted Ethernet/WiFi/port settings
    reinitWebPortsIfNeeded();

    // Initialize HT sensor configuration
    loadHTSensorConfig();


    // Add a reset for the Ethernet controller
    resetEthernet();

    // Initialize Ethernet first to check if wired connection is available
    initEthernet();

    // Initialize WiFi only if Ethernet is not connected
    if (!ethConnected) {
        initWiFi();
    }

    // Print MAC summary (Board/ETH/STA/AP)
    printMacSummary();

    if (WiFi.status() == WL_CONNECTED || ethConnected) {
        // Initialize RTC
        initRTC();

        debugPrintln("Current time (Melbourne): " + getTimeString());
    }


    // Initialize RS485 serial with current configuration
    initRS485();

    // Initialize MODBUS RTU slave on RS485 whenever RS485 protocol is set to Modbus.
    // NOTE: The VB.NET master talks to the board over RS485 regardless of the active
    //       "currentCommunicationProtocol" (usb/wifi/ethernet). To avoid a deadlock
    //       where Modbus can't be enabled because Modbus isn't running, we always
    //       start the Modbus RTU slave when rs485Protocol indicates Modbus.
    if (rs485Protocol.indexOf("Modbus") >= 0) {
        initModbusRtu();
    }

    // Initialize RF receiver and transmitter
    initRF();

    // Start DNS server for captive portal if in AP mode
    if (apMode) {
        dnsServer.start(53, "*", WiFi.softAPIP());
    }

    // Initialize WebSocket server
    webSocket.begin();
    webSocket.onEvent(handleWebSocketEvent);
    Serial.println("WebSocket server started");

    // Setup web server endpoints
    setupWebServer();

    // Initialize output states (All relays OFF)
    writeOutputs();

    // Read initial input states
    readInputs();

    // Read initial analog values
    for (int i = 0; i < 4; i++) {
        analogValues[i] = readAnalogInput(i);
        analogVoltages[i] = convertAnalogToVoltage(analogValues[i]);
    }

    // Initialize BACnet/IP server on Ethernet/WiFi (BACnet over UDP/IP)
    if ((WiFi.status() == WL_CONNECTED || ethConnected)) {
        Serial.println("[App] Initializing BACnet/IP...");

        // Start BACnet driver with Ethernet settings
        if (bacnetDriver.begin(ip, gateway, subnet)) {
            // Set device information from global variables
            bacnetDriver.setDeviceName(deviceName.c_str());
            bacnetDriver.setDescription("KC868-A16 Smart Controller");
            bacnetDriver.setLocation("Building Automation");

            // Enable BACnet integration
            BACnetIntegration::initialize();
            BACnetIntegration::setEnabled(true);

            Serial.printf("[App] BACnet/IP started on %s:%d\n",
                ip.toString().c_str(), bacnetDriver.getPort());
        }
        else {
            Serial.println("[App] BACnet/IP initialization failed");
        }
    }

    // Initialize WebSocket client array
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        webSocketClients[i] = false;
    }

    // Initialize interrupt configurations
    initInterruptConfigs();

    // Setup input interrupts
    setupInputInterrupts();

    // Print initial I/O states for debugging
    printIOStates();

    Serial.println("KC868-A16 Controller initialization complete");

    // Print network status
    if (ethConnected) {
        Serial.println("Using Ethernet connection");
        Serial.print("IP: ");
        Serial.println(ETH.localIP());
    }
    else if (wifiClientMode) {
        Serial.println("Using WiFi Client connection");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else if (apMode) {
        Serial.println("Running in Access Point mode");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    }
}

void appLoop() {
    // Handle DNS requests for captive portal if in AP mode
    if (apMode) {
        dnsServer.processNextRequest();
    }

    // Rest of the original loop function...
    // Handle web server clients
    server.handleClient();

    // Handle WebSocket events
    webSocket.loop();


    // Update BACnet
    BACnetIntegration::update();

    // Static variables to track last updates
    static unsigned long lastWebSocketUpdate = 0;
    static unsigned long lastInputsCheck = 0;
    static unsigned long lastAnalogCheck = 0;
    static unsigned long lastSensorCheck = 0;
    static unsigned long lastNetworkCheck = 0;  // Add network check timer
    static unsigned long lastNetTimeCheck = 0;  // Add network check timer
    unsigned long currentMillis = millis();

    // Process any input interrupts with priorities
    processInputInterrupts();

    // Poll any non-interrupt inputs
    pollNonInterruptInputs();

    // Read digital inputs more frequently (every 100ms) if interrupts are not enabled
    if (!inputInterruptsEnabled && (currentMillis - lastInputsCheck >= 100)) {
        lastInputsCheck = currentMillis;
        bool inputsChanged = readInputs();

        // If inputs changed, broadcast immediately
        if (inputsChanged) {
            broadcastUpdate();
            lastWebSocketUpdate = currentMillis;
        }
    }

    // Read HT sensors periodically
    if (currentMillis - lastSensorCheck >= 1000) { // Check sensors every second
        lastSensorCheck = currentMillis;

        // Read each HT sensor
        for (int i = 0; i < 3; i++) {
            readSensor(i);
        }
    }

    // Read analog inputs more frequently - reduced to 100ms (from 500ms) for better responsiveness
    if (currentMillis - lastAnalogCheck >= 100) {
        lastAnalogCheck = currentMillis;
        bool analogChanged = false;

        for (int i = 0; i < 4; i++) {
            int newValue = readAnalogInput(i);
            if (abs(newValue - analogValues[i]) > 10) { // Reduced threshold for more sensitivity
                analogValues[i] = newValue;
                analogVoltages[i] = convertAnalogToVoltage(newValue); // Update voltage
                analogChanged = true;
            }
        }

        // If analog values changed significantly, check triggers
        if (analogChanged) {
            checkAnalogTriggers();

            // Broadcast immediately if analog values changed
            broadcastUpdate();
            lastWebSocketUpdate = currentMillis;
        }
    }

    // Periodically check network status (every 5 seconds)
    if (currentMillis - lastNetworkCheck >= 5000) {
        lastNetworkCheck = currentMillis;


        // Maintain DHCP client state for Ethernet (LAN8720.ino proven logic)
        serviceEthernetDhcp();
        // If Ethernet was connected but now disconnected, try to reconnect WiFi
        if (wiredMode && !ETH.linkUp()) {
            wiredMode = false;
            ethConnected = false;

            // Try to reconnect WiFi if we have credentials
            if (wifiSSID.length() > 0 && !wifiClientMode && !apMode) {
                WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
                debugPrintln("Ethernet disconnected, trying WiFi...");
            }
        }

        // If WiFi client was connected but now disconnected, try to reconnect
        if (wifiClientMode && WiFi.status() != WL_CONNECTED) {
            wifiClientMode = false;
            wifiConnected = false;

            // Try reconnecting to WiFi
            if (!ethConnected && !apMode) {
                WiFi.reconnect();
                debugPrintln("WiFi disconnected, trying to reconnect...");
            }
        }
    }

    // Broadcast periodic updates every 1 second even if no changes (reduced from 2 seconds)
    if (currentMillis - lastWebSocketUpdate >= 1000) {
        broadcastUpdate();
        lastWebSocketUpdate = currentMillis;
    }

    // Process commands based on active communication protocol
    if (currentCommunicationProtocol == "usb") {
        processSerialCommands();
    }
    else if (currentCommunicationProtocol == "rs485") {
        // If Modbus is enabled, Modbus owns the RS485 port.
        if (rs485Protocol.indexOf("Modbus") >= 0 && isModbusRtuRunning()) {
            taskModbusRtu();
        } else {
            processRS485Commands();
        }
    }

    // Even when the active UI protocol is not RS485 (e.g. WiFi/Ethernet/USB),
    // keep Modbus RTU responsive on RS485 when enabled.
    // Avoid calling twice in the same loop when RS485 is already the active protocol.
    if (currentCommunicationProtocol != "rs485" &&
        rs485Protocol.indexOf("Modbus") >= 0 &&
        isModbusRtuRunning()) {
        taskModbusRtu();
    }
// Check RF receiver for any signals
    if (rfReceiver.available()) {
        unsigned long rfCode = rfReceiver.getReceivedValue();
        debugPrintln("RF code received: " + String(rfCode));
        rfReceiver.resetAvailable();
    }

    // Check schedules every second
    if (currentMillis - lastTimeCheck >= 1000) {
        lastTimeCheck = currentMillis;
        checkSchedules();
    }

    // Check Time stamp every 10 second
    if (currentMillis - lastNetTimeCheck >= 10000) {
        if (WiFi.status() == WL_CONNECTED || ethConnected) {

            lastNetTimeCheck = currentMillis;
            // Initialize RTC
            debugPrintln("Now (Melbourne): " + getTimeString());
        }
    }

    // Update system uptime (for diagnostics)
    if (currentMillis - lastSystemUptime >= 60000) {
        lastSystemUptime = currentMillis;
        debugPrintln("System uptime: " + String(millis() / 60000) + " minutes");
    }
}
