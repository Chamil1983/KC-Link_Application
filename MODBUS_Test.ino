/**
 * MODBUS RTU Test Program for ESP32
 *
 * This program validates RS-485 MODBUS functionality using ModbusComm driver
 * and integrates with peripheral drivers for comprehensive testing.
 */

#include <Arduino.h>
#include "src/Config.h"
#include "src/Debug.h"
#include "src/ModbusComm.h"
#include "src/AnalogInputs.h"
#include "src/DACControl.h"
#include "src/RelayDriver.h"
#include "src/DigitalInputDriver.h"
#include "src/DHT_Sensors.h"
#include "src/RTCManager.h"

 // ModbusComm instance
ModbusComm modbus;

// Peripheral driver instances
AnalogInputs analogInputs;
DACControl dacControl;
RelayDriver relayDriver;
DHTSensors dhtSensors;

// Serial command handling
String inputString = "";
bool stringComplete = false;
unsigned long lastCharTimeMs = 0;       // For idle-commit fallback
const uint16_t CMD_IDLE_COMMIT_MS = 400; // Commit command if no newline arrives
const size_t CMD_MAX_LEN = 128;          // Guard against excessively long lines

// Status variables
bool modbusInitialized = false;
uint8_t testMode = 0;
uint32_t lastUpdateTime = 0;
const uint32_t UPDATE_INTERVAL = 1000; // Update interval in ms

// Modbus register values
uint16_t inputRegs[16];    // Digital inputs
uint16_t relayRegs[8];     // Relay outputs (logical 0/1)
uint16_t analogRegs[8];    // Analog + current inputs (raw/scaled pairs)
uint16_t tempRegs[8];      // Temperature readings
uint16_t humRegs[8];       // Humidity readings
uint16_t ds18b20Regs[8];   // DS18B20 temperature readings
uint16_t dacRegs[4];       // DAC outputs
uint16_t rtcRegs[8];       // RTC time values

// Function declarations
void initModbus();
void processSerialCommands();
void displayHelp();
void displayStatus();
void updateModbusRegisters();
void updateSensorData();
void testModbusMaster();
void testModbusSlave();
uint16_t modbusInputsCallback(TRegister* reg, uint16_t val);

// New split callbacks for holding registers
uint16_t modbusRelaysGet(TRegister* reg, uint16_t val);
uint16_t modbusRelaysSet(TRegister* reg, uint16_t val);
uint16_t modbusAnalogCallback(TRegister* reg, uint16_t val);
uint16_t modbusDacGet(TRegister* reg, uint16_t val);
uint16_t modbusDacSet(TRegister* reg, uint16_t val);
uint16_t modbusRtcGet(TRegister* reg, uint16_t val);
uint16_t modbusRtcSet(TRegister* reg, uint16_t val);

// Coils (separate GET/SET) mapped to relays
uint16_t modbusRelaysCoilGet(TRegister* reg, uint16_t val);
uint16_t modbusRelaysCoilSet(TRegister* reg, uint16_t val);

// Discrete Inputs (FC 0x02) callback for 8 digital inputs (0..7)
uint16_t modbusDiscreteInputsCallback(TRegister* reg, uint16_t val);

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n\n*** MODBUS RTU Test Program for ESP32 ***");
    Serial.println("Starting initialization...");

    Debug::begin(115200);
    Debug::setLevel(DEBUG_LEVEL_INFO);

    Wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);
    delay(100);
    Serial.println("I2C initialized");

    Serial.println("Initializing peripheral drivers...");

    Serial.println("Initializing digital inputs...");
    bool digitalInputsOk = digitalInputs.begin();
    Serial.println(digitalInputsOk ? "Digital inputs initialized successfully"
        : "Failed to initialize digital inputs - continuing anyway");

    Serial.println("Initializing relays...");
    bool relaysOk = relayDriver.begin();
    if (relaysOk) {
        Serial.println("Relay driver initialized successfully");
        relayDriver.turnOffAll();
        Serial.println("All relays turned OFF");
    }
    else {
        Serial.println("Failed to initialize relay driver - continuing anyway");
    }

    Serial.println("Initializing analog inputs...");
    analogInputs.begin();
    Serial.println("Analog inputs initialized");

    Serial.println("Initializing DAC...");
    bool dacOk = dacControl.begin();
    Serial.println(dacOk ? "DAC controller initialized successfully"
        : "Failed to initialize DAC controller - continuing anyway");

    Serial.println("Initializing DHT sensors...");
    bool dhtOk = dhtSensors.begin();
    Serial.println(dhtOk ? "DHT sensors initialized successfully"
        : "Failed to initialize DHT sensors - continuing anyway");

    Serial.println("Initializing RTC...");
    bool rtcOk = rtcManager.begin();
    Serial.println(rtcOk ? "RTC manager initialized successfully"
        : "Failed to initialize RTC - continuing anyway");

    inputString.reserve(128);

    Serial.println("Initializing MODBUS...");
    initModbus();

    Serial.println("System initialization complete");
    Serial.println("Type 'help' for available commands");
}

void loop() {
    while (Serial.available() > 0) {
        char inChar = (char)Serial.read();
        lastCharTimeMs = millis();

        if (inChar == '\n' || inChar == '\r') {
            if (inputString.length() > 0) stringComplete = true;
        }
        else if (inChar == 0x08 || inChar == 0x7F) {
            if (!inputString.isEmpty()) inputString.remove(inputString.length() - 1);
        }
        else {
            if (inputString.length() < CMD_MAX_LEN - 1) {
                inputString += inChar;
            }
        }
    }

    if (!stringComplete && inputString.length() > 0 &&
        (millis() - lastCharTimeMs) > CMD_IDLE_COMMIT_MS) {
        stringComplete = true;
    }

    if (stringComplete) {
        String echo = inputString; echo.trim();
        if (echo.length() > 0) {
            Serial.print("Command received: ");
            Serial.println(echo);
            processSerialCommands();
        }
        inputString = "";
        stringComplete = false;
    }

    if (millis() - lastUpdateTime > UPDATE_INTERVAL) {
        updateSensorData();
        lastUpdateTime = millis();
    }

    modbus.task();

    if (digitalInputs.isConnected()) {
        digitalInputs.processInterrupts();
    }
    if (rtcManager.isConnected()) {
        rtcManager.update();
    }
}

void initModbus() {
    Serial.println("Initializing MODBUS communication...");

    if (modbus.begin(9600)) {
        modbus.server(1);
        modbusInitialized = true;
        Serial.println("MODBUS initialized at 9600 baud");

        Serial.println("Configuring MODBUS register handlers...");

        // Input registers (FC 04) 0..7 digital inputs (0/1)
        if (!modbus.addInputRegisterHandler(MB_REG_INPUTS_START, NUM_DIGITAL_INPUTS, modbusInputsCallback)) {
            ERROR_LOG("Failed to add input register handlers for digital inputs");
        }

        // Discrete inputs (FC 02) 0..7: same digital inputs (0/1)
        if (!modbus.addDiscreteInputHandler(MB_REG_INPUTS_START, NUM_DIGITAL_INPUTS, modbusDiscreteInputsCallback)) {
            ERROR_LOG("Failed to add discrete input handlers for digital inputs");
        }

        // Holding registers for relays (10..15)
        if (!modbus.addHoldingRegisterHandlers(MB_REG_RELAYS_START, NUM_RELAY_OUTPUTS, modbusRelaysGet, modbusRelaysSet)) {
            ERROR_LOG("Failed to add holding register handlers for relays");
        }

        // Coils for relays (10..15) with separate GET/SET callbacks
        if (!modbus.addCoilHandlers(MB_REG_RELAYS_START, NUM_RELAY_OUTPUTS, modbusRelaysCoilGet, modbusRelaysCoilSet)) {
            ERROR_LOG("Failed to add coil handlers for relays");
        }

        // Input registers for analog + current (from 20)
        const uint16_t analogBlockRegs = (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2; // raw+scaled per channel
        if (!modbus.addInputRegisterHandler(MB_REG_ANALOG_START, analogBlockRegs, modbusAnalogCallback)) {
            ERROR_LOG("Failed to add input register handlers for analog block");
        }

        // DHT temp (30..) and humidity (40..)
        if (!modbus.addInputRegisterHandler(MB_REG_TEMP_START, NUM_DHT_SENSORS,
            [](TRegister* reg, uint16_t) {
                return tempRegs[reg->address.address - MB_REG_TEMP_START];
            })) {
            ERROR_LOG("Failed to add input register handlers for temperature");
        }
        if (!modbus.addInputRegisterHandler(MB_REG_HUM_START, NUM_DHT_SENSORS,
            [](TRegister* reg, uint16_t) {
                return humRegs[reg->address.address - MB_REG_HUM_START];
            })) {
            ERROR_LOG("Failed to add input register handlers for humidity");
        }

        // DS18B20 temps (50..)
        if (!modbus.addInputRegisterHandler(MB_REG_DS18B20_START, MAX_DS18B20_SENSORS,
            [](TRegister* reg, uint16_t) {
                return ds18b20Regs[reg->address.address - MB_REG_DS18B20_START];
            })) {
            ERROR_LOG("Failed to add input register handlers for DS18B20");
        }

        // DAC controls (70..73) as holding registers
        if (!modbus.addHoldingRegisterHandlers(MB_REG_DAC_START, 4, modbusDacGet, modbusDacSet)) {
            ERROR_LOG("Failed to add holding register handlers for DAC");
        }

        // RTC (80..87) as holding registers
        if (!modbus.addHoldingRegisterHandlers(MB_REG_RTC_START, 8, modbusRtcGet, modbusRtcSet)) {
            ERROR_LOG("Failed to add holding register handlers for RTC");
        }

        Serial.println("MODBUS register handlers configured");
    }
    else {
        modbusInitialized = false;
        Serial.println("Failed to initialize MODBUS");
    }
}

void updateSensorData() {
    if (digitalInputs.isConnected()) {
        digitalInputs.updateAllInputs();
        uint8_t st = digitalInputs.getInputState();
        for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) inputRegs[i] = (st & (1 << i)) ? 1 : 0;
    }
    else {
        for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) inputRegs[i] = 0;
    }

    for (uint8_t i = 0; i < NUM_ANALOG_CHANNELS; i++) {
        analogRegs[i * 2] = analogInputs.readRawVoltageChannel(i);
        analogRegs[i * 2 + 1] = (uint16_t)(analogInputs.readVoltage(i) * 100);
    }

    for (uint8_t i = 0; i < NUM_CURRENT_CHANNELS; i++) {
        const uint8_t base = (NUM_ANALOG_CHANNELS + i) * 2;
        analogRegs[base] = analogInputs.readRawCurrentChannel(i);
        analogRegs[base + 1] = (uint16_t)(analogInputs.readCurrent(i) * 100);
    }

    dhtSensors.update();
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
        float t = dhtSensors.getTemperature(i);
        float h = dhtSensors.getHumidity(i);
        tempRegs[i] = isnan(t) ? 0 : (uint16_t)(t * 100);
        humRegs[i] = isnan(h) ? 0 : (uint16_t)(h * 100);
    }
    for (uint8_t i = 0; i < dhtSensors.getDS18B20Count() && i < MAX_DS18B20_SENSORS; i++) {
        float t = dhtSensors.getDS18B20Temperature(i);
        ds18b20Regs[i] = (t < -100.0f) ? 0 : (uint16_t)((t + 100.0f) * 100);
    }

    if (rtcManager.isConnected()) {
        DateTime now = rtcManager.getDateTime(true);
        rtcRegs[0] = now.year();
        rtcRegs[1] = now.month();
        rtcRegs[2] = now.day();
        rtcRegs[3] = now.hour();
        rtcRegs[4] = now.minute();
        rtcRegs[5] = now.second();
    }
}

uint16_t modbusInputsCallback(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_INPUTS_START;
    return (idx < NUM_DIGITAL_INPUTS) ? inputRegs[idx] : 0;
}

// Discrete Inputs (FC 0x02) callback: return 0xFF00/0x0000 per digital input (0..7)
uint16_t modbusDiscreteInputsCallback(TRegister* reg, uint16_t) {
    const uint16_t idx = reg->address.address - MB_REG_INPUTS_START;
    if (idx >= NUM_DIGITAL_INPUTS) {
        reg->value = 0x0000;
        return reg->value;
    }

    // Use the same input state as for input registers (FC 0x04)
    // Map ON->0xFF00, OFF->0x0000 (matches Modbus coil/discrete conventions)
    const bool on = (inputRegs[idx] != 0);
    reg->value = on ? 0xFF00 : 0x0000;

    TRACE_LOG("MB FC02 Get DI %u -> %s (raw=0x%04X)",
        (unsigned)reg->address.address, on ? "ON" : "OFF", (unsigned)reg->value);

    return reg->value;
}

// ===== Relays (Holding Registers 10..15) =====

// Helpers for FC 0x06 relay mapping
static inline bool isRelayOnValue(uint16_t v) {
    return (v == 0x0001) || (v == 0x00FF) || (v == 0xFF00) || (v == 0xFFFF);
}
static inline bool isRelayOffValue(uint16_t v) {
    return (v == 0x0000);
}

uint16_t modbusRelaysGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_RELAYS_START;
    uint16_t val = (idx < NUM_RELAY_OUTPUTS) ? relayRegs[idx] : 0;
    TRACE_LOG("MB FC03 Get Hreg %u (relay %u) -> %u",
        (unsigned)reg->address.address, (unsigned)(idx + 1), (unsigned)val);
    return val;
}

uint16_t modbusRelaysSet(TRegister* reg, uint16_t val) {
    const uint16_t addr = reg->address.address;
    const uint16_t idx = addr - MB_REG_RELAYS_START;
    if (idx >= NUM_RELAY_OUTPUTS) {
        WARNING_LOG("MB FC06 Hreg %u out of range", (unsigned)addr);
        // Echo back regardless to avoid exceptions
        reg->value = val;
        // Return a safe stored value (0) to avoid special sentinel handling
        return 0;
    }

    const uint16_t prev = relayRegs[idx];
    bool desiredState = false;
    bool validPattern = true;

    // For relay 1, which works correctly with standard protocol
    if (idx == 0) {
        if (isRelayOffValue(val)) {
            desiredState = false;  // OFF
        }
        else if (isRelayOnValue(val)) {
            desiredState = true;   // ON
        }
        else {
            // Unsupported value: keep previous state but still echo to satisfy FC06
            validPattern = false;
            desiredState = (prev != 0);
        }
    }
    // For relays 2-6 with the QModMaster protocol issue
    else {
        if (val == 0xFFFF) {
            desiredState = (prev == 0) ? true : false; // toggle
        }
        else if (isRelayOffValue(val)) {
            desiredState = false;  // explicit OFF
        }
        else if (isRelayOnValue(val)) {
            desiredState = true;   // explicit ON
        }
        else {
            validPattern = false;
            desiredState = (prev != 0);
        }
    }

    uint16_t desired = desiredState ? 1 : 0;

    INFO_LOG("MB FC06 Set Hreg %u (relay %u): valIn=0x%04X prev=%u -> desired=%u%s",
        (unsigned)addr, (unsigned)(idx + 1),
        (unsigned)val, (unsigned)prev, (unsigned)desired,
        validPattern ? "" : " (ignored: invalid value)");

    // Apply change to hardware if needed
    bool hwOk = true;
    if (desired != prev) {
        if (relayDriver.isConnected()) {
            hwOk = relayDriver.setState(idx + 1, desired ? RELAY_ON : RELAY_OFF);
        }
        else {
            WARNING_LOG("Relay driver not connected; skipping hardware write");
        }
        relayRegs[idx] = desired;
    }

    // Optional verification
    const uint8_t verify = relayDriver.getState(idx + 1);
    INFO_LOG("MB FC06 verify relay %u: desired=%s, readback=%s, hwOk=%s",
        (unsigned)(idx + 1),
        (desired ? "ON" : "OFF"),
        (verify == RELAY_ON ? "ON" : (verify == RELAY_OFF ? "OFF" : "ERR")),
        (hwOk ? "true" : "false"));

    reg->value = val;   // on-wire echo (FC06 response)
    return (desired ? 1 : 0);
}

// ===== Coils for Relays (map 10..15 as coils for FC05/FC01/FC0F) =====

// Map relayRegs[idx] (0/1) -> Modbus raw echo (0x0000/0xFF00)
static inline uint16_t coilStateToRaw(uint16_t state01) {
    return state01 ? 0xFF00 : 0x0000;
}

uint16_t modbusRelaysCoilGet(TRegister* reg, uint16_t /*val*/) {
    const uint16_t addr = reg->address.address;
    const uint16_t idx = addr - MB_REG_RELAYS_START;
    if (idx >= NUM_RELAY_OUTPUTS) return 0x0000;

    const uint16_t raw = coilStateToRaw(relayRegs[idx]);
    TRACE_LOG("MB FC01 Get Coil %u (relay %u): state=%u -> raw=0x%04X",
        (unsigned)addr, (unsigned)(idx + 1), (unsigned)relayRegs[idx], (unsigned)raw);

    reg->value = raw; // keep stack mirror consistent as raw echo
    return raw;
}

/*
  FC 0x05 (Write Single Coil) requirements:

  - Relay 1 (idx 0): Standard behavior
      0xFF00 -> ON, 0x0000 -> OFF. Ignore/No-op for any other values.

  - Relays 2..6 (idx 1..5): Toggle-capable behavior
      0xFF00 -> ON (do NOT toggle).
      0x0000 -> OFF (do NOT toggle).
      Any other value -> TOGGLE (invert current state).

  - Response: Echo the exact raw value received for FC 0x05 (per Modbus spec).
*/
uint16_t modbusRelaysCoilSet(TRegister* reg, uint16_t val) {
    const uint16_t addr = reg->address.address;
    const uint16_t idx = addr - MB_REG_RELAYS_START;
    if (idx >= NUM_RELAY_OUTPUTS) return reg->value;

    // Log the raw input value for debugging
    Serial.printf("FC05 RAW INPUT: 0x%04X for coil %u (relay %u)\n", val, addr, idx + 1);

    const uint16_t prev = relayRegs[idx];
    uint16_t desired = prev; // default: no change

    if (idx == 0) {
        // Relay 1: strict standard mapping only
        if (val == 0xFF00) desired = 1;       // ON
        else if (val == 0x0000) desired = 0;  // OFF
        else {
            INFO_LOG("MB FC05 relay 1: ignoring unsupported value 0x%04X", (unsigned)val);
        }
    }
    else {
        // Relays 2..6: ON/OFF only for FF00/0000, otherwise toggle
        if (val == 0xFF00) {
            desired = 1; // explicit ON (no toggle)
        }
        else if (val == 0x0000) {
            desired = 0; // explicit OFF (no toggle)
        }
        else {
            // Toggle on any non-standard value
            desired = (prev ? 0 : 1);
            INFO_LOG("MB FC05 relay %u: non-standard value 0x%04X -> TOGGLE (%u->%u)",
                (unsigned)(idx + 1), (unsigned)val, (unsigned)prev, (unsigned)desired);
        }
    }

    INFO_LOG("MB FC05 Set Coil %u (relay %u): raw=0x%04X -> desired=%u, prev=%u",
        (unsigned)addr, (unsigned)(idx + 1), (unsigned)val, (unsigned)desired, (unsigned)prev);

    if (desired != prev) {
        if (relayDriver.isConnected()) {
            bool ok = relayDriver.setState(idx + 1, desired ? RELAY_ON : RELAY_OFF);
            INFO_LOG("MB FC05 apply relay %u -> %s (hw=%s)",
                (unsigned)(idx + 1), desired ? "ON" : "OFF", ok ? "ok" : "fail");
        }
        relayRegs[idx] = desired;
    }
    else {
        INFO_LOG("MB FC05 no change needed for relay %u (already %s)",
            (unsigned)(idx + 1), desired ? "ON" : "OFF");
    }

    // Echo EXACT request value as per Modbus FC05 spec
    reg->value = val;
    return val;
}

// ===== Analog block (Input Registers) =====

uint16_t modbusAnalogCallback(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_ANALOG_START;
    const uint16_t total = (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2;
    return (idx < total) ? analogRegs[idx] : 0;
}

// ===== DAC (Holding Registers 70..73) =====

uint16_t modbusDacGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_DAC_START;
    return (idx < 4) ? dacRegs[idx] : 0;
}

uint16_t modbusDacSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_DAC_START;
    if (idx >= 4) return reg->value;

    dacRegs[idx] = val;

    if (dacControl.isInitialized()) {
        switch (idx) {
        case 0: dacControl.setRaw(0, val); break;
        case 1: dacControl.setRaw(1, val); break;
        case 2: dacControl.setVoltage(0, val / 100.0f); break;
        case 3: dacControl.setVoltage(1, val / 100.0f); break;
        }
    }
    else {
        WARNING_LOG("DAC not initialized; cached write only index=%u val=%u", idx, val);
    }

    reg->value = val;
    INFO_LOG("MB FC06 DAC idx=%u raw=0x%04X -> reg=0x%04X", idx, val, reg->value);
    return reg->value;
}

// ===== RTC (Holding Registers 80..87) =====

uint16_t modbusRtcGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_RTC_START;
    return (idx < 8) ? rtcRegs[idx] : 0;
}

uint16_t modbusRtcSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_RTC_START;
    if (idx >= 8) return reg->value;

    rtcRegs[idx] = val;
    reg->value = val;

    if (idx == 5 && rtcManager.isConnected()) {
        rtcManager.setDateTime(DateTime(
            rtcRegs[0], rtcRegs[1], rtcRegs[2],
            rtcRegs[3], rtcRegs[4], rtcRegs[5]
        ));
        INFO_LOG("RTC updated: %04u-%02u-%02u %02u:%02u:%02u",
            rtcRegs[0], rtcRegs[1], rtcRegs[2],
            rtcRegs[3], rtcRegs[4], rtcRegs[5]);
    }
    INFO_LOG("MB FC06 RTC idx=%u raw=0x%04X -> reg=0x%04X", idx, val, reg->value);
    return reg->value;
}

void processSerialCommands() {
    String command = inputString; command.toLowerCase(); command.trim();

    if (command == "help") displayHelp();
    else if (command == "status") displayStatus();
    else if (command.startsWith("baud ")) {
        unsigned long br = command.substring(5).toInt();
        if (br > 0) { modbus.begin(br); modbus.server(1); Serial.printf("MODBUS baud rate changed to %lu\n", br); }
        else Serial.println("Invalid baud rate");
    }
    else if (command.startsWith("debug ")) {
        int lvl = command.substring(6).toInt();
        if (lvl < DEBUG_LEVEL_NONE) lvl = DEBUG_LEVEL_NONE;
        if (lvl > DEBUG_LEVEL_TRACE) lvl = DEBUG_LEVEL_TRACE;
        Debug::setLevel((uint8_t)lvl);
        Serial.printf("Debug level set to %d\n", lvl);
    }
    else if (command == "master") { modbus.master(); testMode = 1; }
    else if (command == "slave") { initModbus(); testMode = 0; }
    else if (command.startsWith("read ")) {
        int p[3] = { 0 }; int n = 0; String s = command.substring(5);
        int st = 0, sp = s.indexOf(' ');
        while (sp >= 0 && n < 3) { p[n++] = s.substring(st, sp).toInt(); st = sp + 1; sp = s.indexOf(' ', st); }
        if (st < s.length() && n < 3) p[n++] = s.substring(st).toInt();
        if (n != 3) { Serial.println("Usage: read <slaveAddr> <regAddr> <numRegs>"); return; }
        uint16_t data[20] = { 0 }; if (p[2] > 20) p[2] = 20;
        if (modbus.readRegisters(p[0], p[1], p[2], data)) {
            for (int i = 0; i < p[2]; i++) Serial.printf("Reg %d: %u\n", p[1] + i, data[i]);
        }
        else { Serial.println("Failed to read registers"); }
    }
    else if (command.startsWith("write ")) {
        int p[3] = { 0 }; int n = 0; String s = command.substring(6);
        int st = 0, sp = s.indexOf(' ');
        while (sp >= 0 && n < 3) { p[n++] = s.substring(st, sp).toInt(); st = sp + 1; sp = s.indexOf(' ', st); }
        if (st < s.length() && n < 3) p[n++] = s.substring(st).toInt();
        if (n != 3) { Serial.println("Usage: write <slaveAddr> <regAddr> <value>"); return; }
        if (!modbus.writeRegister(p[0], p[1], p[2])) Serial.println("Failed to write register");
    }
    else if (command.startsWith("relay ")) {
        int p[2] = { 0 }; int n = 0; String s = command.substring(6);
        int st = 0, sp = s.indexOf(' ');
        while (sp >= 0 && n < 2) { p[n++] = s.substring(st, sp).toInt(); st = sp + 1; sp = s.indexOf(' ', st); }
        if (st < s.length() && n < 2) p[n++] = s.substring(st).toInt();
        if (n != 2) { Serial.println("Usage: relay <relayNum> <state>"); return; }
        if (p[0] >= 1 && p[0] <= NUM_RELAY_OUTPUTS && relayDriver.isConnected()) {
            uint16_t cmd = (p[1] != 0) ? 1 : 0;
            relayDriver.setState(p[0], cmd ? RELAY_ON : RELAY_OFF);
            relayRegs[p[0] - 1] = cmd;
            Serial.printf("Relay %d set to %s\n", p[0], cmd ? "ON" : "OFF");
        }
        else Serial.println("Invalid relay number or driver not initialized");
    }
    else if (command.startsWith("dac ")) {
        int p[2] = { 0 }; int n = 0; String s = command.substring(4);
        int st = 0, sp = s.indexOf(' ');
        while (sp >= 0 && n < 2) { p[n++] = s.substring(st, sp).toInt(); st = sp + 1; sp = s.indexOf(' ', st); }
        if (st < s.length() && n < 2) p[n++] = s.substring(st).toInt();
        if (n != 2) { Serial.println("Usage: dac <channel> <value>"); return; }
        if (p[0] >= 0 && p[0] <= 1 && dacControl.isInitialized()) {
            dacControl.setRaw(p[0], p[1]);
            dacRegs[p[0]] = p[1];
            Serial.printf("DAC %d raw set to %d\n", p[0], p[1]);
        }
        else Serial.println("Invalid DAC channel or DAC not initialized");
    }
    else if (command.startsWith("dacv ")) {
        int sp = command.indexOf(' ', 5);
        if (sp > 0) {
            int ch = command.substring(5, sp).toInt();
            float v = command.substring(sp + 1).toFloat();
            if (ch >= 0 && ch <= 1 && dacControl.isInitialized()) {
                dacControl.setVoltage(ch, v);
                dacRegs[2 + ch] = (uint16_t)lroundf(v * 100.0f);
                Serial.printf("DAC %d set to %.2fV\n", ch, v);
            }
            else Serial.println("Invalid DAC channel or DAC not initialized");
        }
        else Serial.println("Usage: dacv <channel> <voltage>");
    }
    else if (command == "readinputs") {
        if (digitalInputs.isConnected()) {
            digitalInputs.updateAllInputs();
            uint8_t st = digitalInputs.getInputState();
            for (int i = 0; i < NUM_DIGITAL_INPUTS; i++)
                Serial.printf("Input %d: %s\n", i + 1, (st & (1 << i)) ? "HIGH" : "LOW");
        }
        else Serial.println("Digital inputs not initialized");
    }
    else if (command == "readanalog") {
        Serial.println("Analog Inputs (0-5V):");
        for (int i = 0; i < NUM_ANALOG_CHANNELS; i++) {
            uint16_t raw = analogInputs.readRawVoltageChannel(i);
            float v = analogInputs.readVoltage(i);
            Serial.printf("V%d: Raw=%u, %.3fV\n", i + 1, raw, v);
        }
        Serial.println("Current Inputs (4-20mA):");
        for (int i = 0; i < NUM_CURRENT_CHANNELS; i++) {
            uint16_t raw = analogInputs.readRawCurrentChannel(i);
            float mA = analogInputs.readCurrent(i);
            Serial.printf("I%d: Raw=%u, %.3fmA\n", i + 1, raw, mA);
        }
    }
    else if (command == "readdht") {
        Serial.println("Reading DHT sensors...");
        dhtSensors.readAll(true);
        for (int i = 0; i < NUM_DHT_SENSORS; i++) {
            if (dhtSensors.isSensorConnected(i)) {
                Serial.printf("DHT%d: %.1fC, %.1f%%\n", i + 1,
                    dhtSensors.getTemperature(i), dhtSensors.getHumidity(i));
            }
            else Serial.printf("DHT%d: Not connected\n", i + 1);
        }
    }
    else if (command == "readds18b20") {
        Serial.println("DS18B20 Sensors:");
        for (int i = 0; i < dhtSensors.getDS18B20Count(); i++) {
            if (dhtSensors.isDS18B20Connected(i)) {
                float t = dhtSensors.getDS18B20Temperature(i);
                Serial.printf("DS18B20[%d]: %.1fC\n", i, t);
            }
            else Serial.printf("DS18B20[%d]: Not connected\n", i);
        }
    }
    else if (command == "time") {
        if (rtcManager.isConnected()) {
            DateTime now = rtcManager.getDateTime(true);
            Serial.printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
        }
        else Serial.println("RTC not initialized");
    }
    else if (command.startsWith("settime ")) {
        if (rtcManager.isConnected()) {
            String ts = command.substring(8);
            if (rtcManager.setDateTimeFromString(ts.c_str())) Serial.println("RTC time updated");
            else Serial.println("Failed. Use: YYYY-MM-DD HH:MM:SS");
        }
        else Serial.println("RTC not initialized");
    }
    else if (command == "scan") {
        Serial.println("Scanning I2C bus...");
        Debug::scanI2CDevices();
    }
    else {
        Serial.printf("Unknown command: '%s'\n", command.c_str());
        Serial.println("Type 'help' for available commands");
    }
}

void displayHelp() {
    Serial.println("\n=== MODBUS RTU Test Program Commands ===");
    Serial.println("help                - Display this help");
    Serial.println("status              - Display system status");
    Serial.println("baud <rate>         - Set MODBUS baud rate (re-enters slave mode ID=1)");
    Serial.println("debug <0..5>        - Set debug level (0=NONE,1=ERROR,2=WARN,3=INFO,4=DEBUG,5=TRACE)");
    Serial.println("master              - Switch to MODBUS master mode");
    Serial.println("slave               - Switch to MODBUS slave mode (ID=1)");
    Serial.println("read <addr> <reg> <count> - Master: Read holding regs from slave");
    Serial.println("write <addr> <reg> <val>  - Master: Write single holding register");
    Serial.println("relay <num> <state> - Control relay (1-6, 0=off/1=on)");
    Serial.println("dac <ch> <val>      - Set DAC raw value (0-32767)");
    Serial.println("dacv <ch> <volt>    - Set DAC voltage (0-5V/0-10V)");
    Serial.println("readinputs          - Read digital inputs");
    Serial.println("readanalog          - Read analog & current inputs");
    Serial.println("readdht             - Read DHT temperature/humidity");
    Serial.println("readds18b20         - Read DS18B20 sensors");
    Serial.println("time                - Display current RTC time");
    Serial.println("settime <datetime>  - Set RTC (YYYY-MM-DD HH:MM:SS)");
    Serial.println("scan                - Scan I2C devices");
}

void displayStatus() {
    Serial.println("\n=== System Status ===");
    Serial.printf("MODBUS: %s, Mode: %s, Baud: %lu\n",
        modbusInitialized ? "Initialized" : "Not Initialized",
        testMode == 0 ? "Slave" : "Master",
        modbus.getBaudRate());
    Serial.printf("Debug Level: %u\n", Debug::getLevel());
    Serial.printf("Digital Inputs: %s\n", digitalInputs.isConnected() ? "Connected" : "Disconnected");
    Serial.printf("Relays: %s\n", relayDriver.isConnected() ? "Connected" : "Disconnected");
    Serial.printf("DAC: %s, Range: %s\n",
        dacControl.isInitialized() ? "Initialized" : "Not Initialized",
        dacControl.getOutputRange() == RANGE_0_5V ? "0-5V" : "0-10V");
    Serial.printf("RTC: %s\n", rtcManager.isConnected() ? "Connected" : "Disconnected");
    Serial.printf("DHT Sensors: %d connected\n", dhtSensors.getConnectedCount());
    Serial.printf("DS18B20 Sensors: %d found\n", dhtSensors.getDS18B20Count());

    Serial.println("\n=== MODBUS Register Map ===");
    Serial.printf("Digital Inputs:  %d-%d\n", MB_REG_INPUTS_START, MB_REG_INPUTS_START + NUM_DIGITAL_INPUTS - 1);
    Serial.printf("Analog Values:   %d-%d\n", MB_REG_ANALOG_START, MB_REG_ANALOG_START + (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2 - 1);
    Serial.printf("Temperature:     %d-%d\n", MB_REG_TEMP_START, MB_REG_TEMP_START + NUM_DHT_SENSORS - 1);
    Serial.printf("Humidity:        %d-%d\n", MB_REG_HUM_START, MB_REG_HUM_START + NUM_DHT_SENSORS - 1);
    Serial.printf("DS18B20 Temps:   %d-%d\n", MB_REG_DS18B20_START, MB_REG_DS18B20_START + MAX_DS18B20_SENSORS - 1);
    Serial.printf("Relays (Hreg):   %d-%d\n", MB_REG_RELAYS_START, MB_REG_RELAYS_START + NUM_RELAY_OUTPUTS - 1);
    Serial.printf("Relays (Coils):  %d-%d\n", MB_REG_RELAYS_START, MB_REG_RELAYS_START + NUM_RELAY_OUTPUTS - 1);
    Serial.printf("DAC Controls:    %d-%d\n", MB_REG_DAC_START, MB_REG_DAC_START + 3);
    Serial.printf("RTC Values:      %d-%d\n", MB_REG_RTC_START, MB_REG_RTC_START + 7);


}