#ifndef MODBUS_COMM_H
#define MODBUS_COMM_H

#include <Arduino.h>
#include <ModbusRTU.h>
#include "Config.h"
#include "Debug.h"

// Callback function type for Modbus register operations
typedef std::function<uint16_t(TRegister* reg, uint16_t val)> cbModbus;

// Transparent sniffer stream that wraps a HardwareSerial to log raw RX/TX bytes.
class ModbusSniffStream : public Stream {
public:
    explicit ModbusSniffStream(HardwareSerial* s)
        : _s(s) {}

    // Enable/disable logging at runtime
    void setEnabled(bool en) { _enabled = en; }
    bool isEnabled() const { return _enabled; }

    // Idle gap in milliseconds to consider one frame complete
    void setFrameGapMs(uint16_t ms) { _frameGapMs = (ms < 2) ? 2 : ms; }

    // Call frequently (e.g., from ModbusComm::task())
    void serviceFlush() {
        unsigned long now = millis();
        if (_enabled) {
            if (_rxLen && (now - _lastRxMs) >= _frameGapMs) flushRxFrame();
            if (_txLen && (now - _lastTxMs) >= _frameGapMs) flushTxFrame();
        }
    }

    // Required by ModbusRTU: expose current baud rate of the underlying serial
    unsigned long baudRate() { return _s ? _s->baudRate() : 0UL; }

    // Stream interface
    int available() override { return _s ? _s->available() : 0; }

    int read() override {
        if (!_s) return -1;
        int c = _s->read();
        if (_enabled && c >= 0) {
            if (_rxLen < sizeof(_rxBuf)) _rxBuf[_rxLen++] = static_cast<uint8_t>(c);
            _lastRxMs = millis();
        }
        return c;
    }

    int peek() override { return _s ? _s->peek() : -1; }

    void flush() override {
        if (_s) _s->flush();
        // Don't force log flush here; frames end on inter-char gap
    }

    using Print::write;

    // Byte-wise write
    size_t write(uint8_t b) override {
        if (!_s) return 0;
        size_t n = _s->write(b);
        if (_enabled && n == 1) {
            if (_txLen < sizeof(_txBuf)) _txBuf[_txLen++] = b;
            _lastTxMs = millis();
        }
        return n;
    }

    // Buffer write for efficiency
    size_t write(const uint8_t* buffer, size_t size) override {
        if (!_s || !buffer || size == 0) return 0;
        size_t n = _s->write(buffer, size);
        if (_enabled && n > 0) {
            size_t toCopy = (size <= (sizeof(_txBuf) - _txLen)) ? size : (sizeof(_txBuf) - _txLen);
            if (toCopy > 0) {
                memcpy(_txBuf + _txLen, buffer, toCopy);
                _txLen += toCopy;
            }
            _lastTxMs = millis();
        }
        return n;
    }

    // Helpers to force flush (optional)
    void forceFlush() {
        if (_enabled) {
            if (_rxLen) flushRxFrame();
            if (_txLen) flushTxFrame();
        }
    }

private:
    HardwareSerial* _s = nullptr;

    // Logging buffers
    uint8_t _rxBuf[256] = { 0 };
    uint8_t _txBuf[256] = { 0 };
    size_t  _rxLen = 0;
    size_t  _txLen = 0;
    unsigned long _lastRxMs = 0;
    unsigned long _lastTxMs = 0;
    uint16_t _frameGapMs = 8;    // default ~8ms gap (>= 3.5 chars @ 9600)
    bool _enabled = true;

    // Dump helpers
    void dumpHex(const char* tag, const uint8_t* buf, size_t len);
    void parseSummary(const char* tag, const uint8_t* buf, size_t len);

    // NEW: user-friendly command printer
    void printFriendly(const char* dir, const uint8_t* buf, size_t len);

    void flushRxFrame();
    void flushTxFrame();
};

class ModbusComm {
public:
    ModbusComm();
    bool begin(unsigned long baudRate = 9600);

    // Explicit mode controls
    void master();                 // Switch to master mode (for issuing requests)
    void server(uint8_t id = 1);   // Switch to server/slave mode with given ID

    // Modbus master functions (only valid in master mode)
    bool readRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t numRegs, uint16_t* data);
    bool writeRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t value);
    bool writeMultipleRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t* data, uint16_t numRegs);

    // Process Modbus messages
    void task();

    // Serial port access for direct communication
    HardwareSerial* getSerial() { return serialPort; }

    // Register callback handlers for Modbus server functionality
    // Single-callback variant: registers per-register handlers for reliability.
    bool addHoldingRegisterHandler(uint16_t regAddr, uint16_t numRegs, cbModbus cb);
    // Separate getters and setters (registered per-register).
    bool addHoldingRegisterHandlers(uint16_t regAddr, uint16_t numRegs, cbModbus cbGet, cbModbus cbSet);

    bool addInputRegisterHandler(uint16_t regAddr, uint16_t numRegs, cbModbus cb);

    // Coils
    bool addCoilHandler(uint16_t regAddr, uint16_t numCoils, cbModbus cb); // legacy single callback (get + set)
    bool addCoilHandlers(uint16_t regAddr, uint16_t numCoils, cbModbus cbGet, cbModbus cbSet); // preferred

    bool addDiscreteInputHandler(uint16_t regAddr, uint16_t numInputs, cbModbus cb);

    // Getters
    unsigned long getBaudRate() const { return baudRate; }

private:
    enum Mode : uint8_t { MODE_IDLE = 0, MODE_MASTER, MODE_SERVER };

    void ensureMaster();
    void ensureServer();

    ModbusRTU mb;
    HardwareSerial* serialPort;
    unsigned long baudRate;
    bool mbServerEnabled;
    uint8_t slaveId;
    Mode mode;

    // Sniffer stream wrapper
    ModbusSniffStream* sniff = nullptr;
};

#endif // MODBUS_COMM_H