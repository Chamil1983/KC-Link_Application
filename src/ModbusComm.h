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

    void setEnabled(bool en) { _enabled = en; }
    bool isEnabled() const { return _enabled; }

    void setFrameGapMs(uint16_t ms) { _frameGapMs = (ms < 2) ? 2 : ms; }

    void serviceFlush() {
        unsigned long now = millis();
        if (_enabled) {
            if (_rxLen && (now - _lastRxMs) >= _frameGapMs) flushRxFrame();
            if (_txLen && (now - _lastTxMs) >= _frameGapMs) flushTxFrame();
        }
    }

    unsigned long baudRate() { return _s ? _s->baudRate() : 0UL; }

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
    }

    using Print::write;

    size_t write(uint8_t b) override {
        if (!_s) return 0;
        size_t n = _s->write(b);
        if (_enabled && n == 1) {
            if (_txLen < sizeof(_txBuf)) _txBuf[_txLen++] = b;
            _lastTxMs = millis();
        }
        return n;
    }

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

    void forceFlush() {
        if (_enabled) {
            if (_rxLen) flushRxFrame();
            if (_txLen) flushTxFrame();
        }
    }

private:
    HardwareSerial* _s = nullptr;

    uint8_t _rxBuf[256] = { 0 };
    uint8_t _txBuf[256] = { 0 };
    size_t  _rxLen = 0;
    size_t  _txLen = 0;
    unsigned long _lastRxMs = 0;
    unsigned long _lastTxMs = 0;
    uint16_t _frameGapMs = 8;
    bool _enabled = true;

    void dumpHex(const char* tag, const uint8_t* buf, size_t len);
    void parseSummary(const char* tag, const uint8_t* buf, size_t len);
    void printFriendly(const char* dir, const uint8_t* buf, size_t len);

    void flushRxFrame();
    void flushTxFrame();
};

class ModbusComm {
public:
    ModbusComm();

    // ===== EXTENDED begin() =====
    // parity: 0=None 1=Odd 2=Even
    // stopBits: 1 or 2
    bool begin(unsigned long baudRate = 9600,
        uint8_t dataBits = 8,
        uint8_t parity = 0,
        uint8_t stopBits = 1);

    void master();
    void server(uint8_t id = 1);

    bool readRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t numRegs, uint16_t* data);
    bool writeRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t value);
    bool writeMultipleRegisters(uint8_t slaveAddr, uint16_t regAddr, uint16_t* data, uint16_t numRegs);

    void task();

    HardwareSerial* getSerial() { return serialPort; }

    bool addHoldingRegisterHandler(uint16_t regAddr, uint16_t numRegs, cbModbus cb);
    bool addHoldingRegisterHandlers(uint16_t regAddr, uint16_t numRegs, cbModbus cbGet, cbModbus cbSet);

    bool addInputRegisterHandler(uint16_t regAddr, uint16_t numRegs, cbModbus cb);

    bool addCoilHandler(uint16_t regAddr, uint16_t numCoils, cbModbus cb);
    bool addCoilHandlers(uint16_t regAddr, uint16_t numCoils, cbModbus cbGet, cbModbus cbSet);

    bool addDiscreteInputHandler(uint16_t regAddr, uint16_t numInputs, cbModbus cb);

    unsigned long getBaudRate() const { return baudRate; }
    uint8_t getDataBits() const { return dataBits; }
    uint8_t getParity() const { return parity; }
    uint8_t getStopBits() const { return stopBits; }

private:
    enum Mode : uint8_t { MODE_IDLE = 0, MODE_MASTER, MODE_SERVER };

    void ensureMaster();
    void ensureServer();

    ModbusRTU mb;
    HardwareSerial* serialPort;
    unsigned long baudRate;
    uint8_t dataBits;
    uint8_t parity;
    uint8_t stopBits;

    bool mbServerEnabled;
    uint8_t slaveId;
    Mode mode;

    ModbusSniffStream* sniff = nullptr;
};

#endif // MODBUS_COMM_H