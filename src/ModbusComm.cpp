#include "ModbusComm.h"
#include <Arduino.h>

ModbusComm::ModbusComm()
    : baudRate(9600),
    dataBits(8),
    parity(0),
    stopBits(1),
    mbServerEnabled(false),
    slaveId(1),
    mode(MODE_IDLE) {
    serialPort = &Serial2;
}

static uint32_t serialConfigFrom(uint8_t dataBits, uint8_t parity, uint8_t stopBits) {
    // ESP32 Arduino supports SERIAL_8N1, SERIAL_8E1, SERIAL_8O1, etc.
    // We'll support 7/8 data bits, N/E/O parity, 1/2 stop bits.

    if (dataBits != 7 && dataBits != 8) dataBits = 8;
    if (parity > 2) parity = 0;
    if (stopBits != 2) stopBits = 1;

    // Map to Arduino macros
    if (dataBits == 8) {
        if (stopBits == 1) {
            if (parity == 0) return SERIAL_8N1;
            if (parity == 1) return SERIAL_8O1;
            return SERIAL_8E1;
        }
        else {
            if (parity == 0) return SERIAL_8N2;
            if (parity == 1) return SERIAL_8O2;
            return SERIAL_8E2;
        }
    }
    else {
        if (stopBits == 1) {
            if (parity == 0) return SERIAL_7N1;
            if (parity == 1) return SERIAL_7O1;
            return SERIAL_7E1;
        }
        else {
            if (parity == 0) return SERIAL_7N2;
            if (parity == 1) return SERIAL_7O2;
            return SERIAL_7E2;
        }
    }
}

bool ModbusComm::begin(unsigned long baud, uint8_t db, uint8_t par, uint8_t sb) {
    baudRate = baud;
    dataBits = db;
    parity = par;
    stopBits = sb;

    pinMode(PIN_MAX485_TXRX, OUTPUT);
    digitalWrite(PIN_MAX485_TXRX, LOW);

    uint32_t cfg = serialConfigFrom(dataBits, parity, stopBits);

    serialPort->begin(baudRate, cfg, PIN_MAX485_RO, PIN_MAX485_DI);
    delay(100);

    if (sniff) { delete sniff; sniff = nullptr; }
    sniff = new ModbusSniffStream(serialPort);

    mb.begin(sniff, PIN_MAX485_TXRX);

    mbServerEnabled = false;
    mode = MODE_IDLE;

    INFO_LOG("ModbusComm::begin baud=%lu db=%u par=%u sb=%u", baudRate, dataBits, parity, stopBits);
    return true;
}

void ModbusComm::master() {
    if (mode != MODE_MASTER) {
        mb.master();
        mode = MODE_MASTER;
        mbServerEnabled = false;
        Serial.println("Switched to MODBUS master mode");
    }
}

void ModbusComm::server(uint8_t id) {
    slaveId = id ? id : 1;
    mb.server(slaveId);
    mbServerEnabled = true;
    mode = MODE_SERVER;
    Serial.printf("Switched to MODBUS slave mode (ID=%u)\n", slaveId);
}

void ModbusComm::ensureMaster() {
    if (mode != MODE_MASTER) {
        mb.master();
        mode = MODE_MASTER;
        mbServerEnabled = false;
    }
}

void ModbusComm::ensureServer() {
    if (!mbServerEnabled || mode != MODE_SERVER) {
        server(slaveId);
    }
}

bool ModbusComm::readRegisters(uint8_t sAddr, uint16_t regAddr, uint16_t numRegs, uint16_t* data) {
    ensureMaster();

    Serial.printf("MB master: read %u regs @%u from slave %u\n", numRegs, regAddr, sAddr);
    if (!mb.readHreg(sAddr, regAddr, data, numRegs)) {
        return false;
    }

    uint32_t start = millis();
    while (millis() - start < 500) {
        mb.task();
        if (sniff) sniff->serviceFlush();
        delay(1);
    }
    return true;
}

bool ModbusComm::writeRegister(uint8_t sAddr, uint16_t regAddr, uint16_t value) {
    ensureMaster();

    Serial.printf("MB master: write %u -> Hreg %u of slave %u\n", value, regAddr, sAddr);
    if (!mb.writeHreg(sAddr, regAddr, value)) {
        return false;
    }

    uint32_t start = millis();
    while (millis() - start < 500) {
        mb.task();
        if (sniff) sniff->serviceFlush();
        delay(1);
    }
    return true;
}

bool ModbusComm::writeMultipleRegisters(uint8_t sAddr, uint16_t regAddr, uint16_t* data, uint16_t numRegs) {
    ensureMaster();

    Serial.printf("MB master: write %u regs starting @%u to slave %u\n", numRegs, regAddr, sAddr);
    if (!mb.writeHreg(sAddr, regAddr, data, numRegs)) {
        return false;
    }

    uint32_t start = millis();
    while (millis() - start < 500) {
        mb.task();
        if (sniff) sniff->serviceFlush();
        delay(1);
    }
    return true;
}

void ModbusComm::task() {
    mb.task();
    if (sniff) sniff->serviceFlush();
}

bool ModbusComm::addHoldingRegisterHandler(uint16_t regAddr, uint16_t numRegs, cbModbus cb) {
    ensureServer();

    if (!mb.addHreg(regAddr, 0, numRegs)) {
        ERROR_LOG("addHreg block failed @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numRegs - 1));
        return false;
    }
    mb.onGetHreg(regAddr, cb, numRegs);
    mb.onSetHreg(regAddr, cb, numRegs);
    DEBUG_LOG_MSG("Hreg block handler registered @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numRegs - 1));
    return true;
}

bool ModbusComm::addHoldingRegisterHandlers(uint16_t regAddr, uint16_t numRegs, cbModbus cbGet, cbModbus cbSet) {
    ensureServer();

    if (!mb.addHreg(regAddr, 0, numRegs)) {
        ERROR_LOG("addHreg block failed @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numRegs - 1));
        return false;
    }
    mb.onGetHreg(regAddr, cbGet, numRegs);
    mb.onSetHreg(regAddr, cbSet, numRegs);
    DEBUG_LOG_MSG("Hreg block get/set handlers registered @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numRegs - 1));
    return true;
}

bool ModbusComm::addInputRegisterHandler(uint16_t regAddr, uint16_t numRegs, cbModbus cb) {
    ensureServer();

    if (!mb.addIreg(regAddr, 0, numRegs)) {
        ERROR_LOG("addIreg block failed @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numRegs - 1));
        return false;
    }
    mb.onGetIreg(regAddr, cb, numRegs);
    DEBUG_LOG_MSG("Ireg block handler registered @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numRegs - 1));
    return true;
}

bool ModbusComm::addCoilHandler(uint16_t regAddr, uint16_t numCoils, cbModbus cb) {
    ensureServer();

    bool ok = true;
    for (uint16_t i = 0; i < numCoils; i++) {
        uint16_t a = regAddr + i;
        if (!mb.addCoil(a, false)) {
            ERROR_LOG("addCoil failed @%u", (unsigned)a);
            ok = false;
            break;
        }
        mb.onGetCoil(a, cb, 1);
        mb.onSetCoil(a, cb, 1);
        DEBUG_LOG_MSG("Coil handler (single) registered @%u", (unsigned)a);
    }
    return ok;
}

bool ModbusComm::addCoilHandlers(uint16_t regAddr, uint16_t numCoils, cbModbus cbGet, cbModbus cbSet) {
    ensureServer();

    bool ok = true;
    for (uint16_t i = 0; i < numCoils; i++) {
        uint16_t a = regAddr + i;
        if (!mb.addCoil(a, false)) {
            ERROR_LOG("addCoil failed @%u", (unsigned)a);
            ok = false;
            break;
        }
        mb.onGetCoil(a, cbGet, 1);
        mb.onSetCoil(a, cbSet, 1);
        DEBUG_LOG_MSG("Coil handlers (get/set) registered @%u", (unsigned)a);
    }
    return ok;
}

bool ModbusComm::addDiscreteInputHandler(uint16_t regAddr, uint16_t numInputs, cbModbus cb) {
    ensureServer();

    if (!mb.addIsts(regAddr, false, numInputs)) {
        ERROR_LOG("addIsts block failed @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numInputs - 1));
        return false;
    }
    mb.onGetIsts(regAddr, cb, numInputs);
    DEBUG_LOG_MSG("DI block handler registered @%u..%u", (unsigned)regAddr, (unsigned)(regAddr + numInputs - 1));
    return true;
}

/* =========================
   ModbusSniffStream helpers
   ========================= */

void ModbusSniffStream::dumpHex(const char* tag, const uint8_t* buf, size_t len) {
    String line;
    line.reserve(3 * len + 32);
    line += tag;
    line += " ";
    line += String(len);
    line += " bytes: ";
    for (size_t i = 0; i < len; i++) {
        if (buf[i] < 16) line += "0";
        line += String(buf[i], HEX);
        if (i + 1 < len) line += " ";
    }
    line.toUpperCase();
    INFO_LOG("%s", line.c_str());
}

static inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)p[0] << 8 | p[1];
}

void ModbusSniffStream::printFriendly(const char* dir, const uint8_t* buf, size_t len) {
    if (len < 2) return;
    uint8_t slave = buf[0];
    uint8_t fc = buf[1];

    uint16_t crc = 0;
    if (len >= 4) {
        crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    }

    auto onoff_fc05 = [](uint16_t v) -> const char* {
        if (v == 0xFF00) return "ON";
        if (v == 0x0000) return "OFF";
        return "INVALID";
    };
    auto onoff_fc06 = [](uint16_t v) -> const char* {
        if (v == 0x0000) return "OFF";
        if (v == 0x0001 || v == 0x00FF || v == 0xFF00 || v == 0xFFFF) return "ON";
        return "DATA";
    };

    switch (fc) {
    case 0x05: {
        if (len >= 8) {
            uint16_t addr = rd16(&buf[2]);
            uint16_t val = rd16(&buf[4]);
            INFO_LOG("[MB-CMD %s] Slave=%u FC=05 WriteSingleCoil addr=%u (0x%04X) value=0x%04X -> %s CRC=0x%04X",
                dir, slave, addr, addr, val, onoff_fc05(val), crc);
        }
        break;
    }
    case 0x06: {
        if (len >= 8) {
            uint16_t addr = rd16(&buf[2]);
            uint16_t val = rd16(&buf[4]);
            INFO_LOG("[MB-CMD %s] Slave=%u FC=06 WriteSingleRegister addr=%u (0x%04X) value=0x%04X (%u) -> %s CRC=0x%04X",
                dir, slave, addr, addr, val, (unsigned)val, onoff_fc06(val), crc);
        }
        break;
    }
    case 0x0F: {
        if (len >= 9) {
            uint16_t addr = rd16(&buf[2]);
            uint16_t qty = rd16(&buf[4]);
            uint8_t  bc = buf[6];
            String dataBytes;
            for (uint8_t i = 0; i < bc && i < 4; i++) {
                char tmp[6];
                snprintf(tmp, sizeof(tmp), "%s%02X", (i ? " " : ""), buf[7 + i]);
                dataBytes += tmp;
            }
            INFO_LOG("[MB-CMD %s] Slave=%u FC=0F WriteMultipleCoils addr=%u qty=%u byteCount=%u data[0..]=%s%s CRC=0x%04X",
                dir, slave, addr, qty, bc, dataBytes.c_str(), (bc > 4 ? " ..." : ""), crc);
        }
        break;
    }
    case 0x10: {
        if (len >= 9) {
            uint16_t addr = rd16(&buf[2]);
            uint16_t qty = rd16(&buf[4]);
            uint8_t  bc = buf[6];
            String dataBytes;
            for (uint8_t i = 0; i < bc && i < 4; i++) {
                char tmp[6];
                snprintf(tmp, sizeof(tmp), "%s%02X", (i ? " " : ""), buf[7 + i]);
                dataBytes += tmp;
            }
            INFO_LOG("[MB-CMD %s] Slave=%u FC=10 WriteMultipleRegisters addr=%u qty=%u byteCount=%u data[0..]=%s%s CRC=0x%04X",
                dir, slave, addr, qty, bc, dataBytes.c_str(), (bc > 4 ? " ..." : ""), crc);
        }
        break;
    }
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04: {
        if (len >= 8) {
            uint16_t addr = rd16(&buf[2]);
            uint16_t qty = rd16(&buf[4]);
            INFO_LOG("[MB-CMD %s] Slave=%u FC=0x%02X Read addr=%u qty=%u CRC=0x%04X",
                dir, slave, fc, addr, qty, crc);
        }
        break;
    }
    default:
        break;
    }
}

void ModbusSniffStream::parseSummary(const char* tag, const uint8_t* buf, size_t len) {
    if (len < 2) return;
    uint8_t slave = buf[0];
    uint8_t fc = buf[1];

    switch (fc) {
    case 0x05:
        if (len >= 6) {
            uint16_t addr = (uint16_t)buf[2] << 8 | buf[3];
            uint16_t val = (uint16_t)buf[4] << 8 | buf[5];
            INFO_LOG("%s PARSE] Slave=%u FC=05 CoilAddr=0x%04X Value=0x%04X", tag, slave, addr, val);
        }
        break;
    case 0x06:
        if (len >= 6) {
            uint16_t addr = (uint16_t)buf[2] << 8 | buf[3];
            uint16_t val = (uint16_t)buf[4] << 8 | buf[5];
            INFO_LOG("%s PARSE] Slave=%u FC=06 HReg=0x%04X Value=0x%04X", tag, slave, addr, val);
        }
        break;
    case 0x03:
    case 0x04:
    case 0x01:
    case 0x02:
        if (len >= 6) {
            uint16_t addr = (uint16_t)buf[2] << 8 | buf[3];
            uint16_t qty = (uint16_t)buf[4] << 8 | buf[5];
            INFO_LOG("%s PARSE] Slave=%u FC=0x%02X Addr=0x%04X Qty=0x%04X", tag, slave, fc, addr, qty);
        }
        break;
    case 0x0F:
    case 0x10:
        if (len >= 7) {
            uint16_t addr = (uint16_t)buf[2] << 8 | buf[3];
            uint16_t qty = (uint16_t)buf[4] << 8 | buf[5];
            uint8_t  bc = buf[6];
            INFO_LOG("%s PARSE] Slave=%u FC=0x%02X Addr=0x%04X Qty=0x%04X ByteCount=%u",
                tag, slave, fc, addr, qty, bc);
        }
        break;
    default:
        INFO_LOG("%s PARSE] Slave=%u FC=0x%02X (len=%u)", tag, slave, fc, (unsigned)len);
        break;
    }

    const bool isRx = strstr(tag, "RX") != nullptr;
    printFriendly(isRx ? "RX" : "TX", buf, len);
}

void ModbusSniffStream::flushRxFrame() {
    if (_rxLen == 0) return;
    dumpHex("[MB-RX]", _rxBuf, _rxLen);
    parseSummary("[MB-RX", _rxBuf, _rxLen);
    _rxLen = 0;
}

void ModbusSniffStream::flushTxFrame() {
    if (_txLen == 0) return;
    dumpHex("[MB-TX]", _txBuf, _txLen);
    parseSummary("[MB-TX", _txBuf, _txLen);
    _txLen = 0;
}