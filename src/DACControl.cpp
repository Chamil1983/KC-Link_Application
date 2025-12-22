/**
 * DACControl.cpp - Implementation with calibration, improved scaling, and robust I2C probing
 * Modified to handle initialization errors more gracefully
 */

#include "DACControl.h"
#include <Wire.h>
#include <math.h>

 // Tunables for I2C robustness
static constexpr uint32_t I2C_SAFE_CLOCK_HZ = 100000;  // 100 kHz to be conservative
static constexpr uint8_t  I2C_RETRY_COUNT = 3;
static constexpr uint16_t I2C_RETRY_DELAY_MS = 20;
static constexpr uint16_t POWERUP_DELAY_MS = 50;

static bool probeI2C(uint8_t addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission(true); // send STOP
    return (err == 0);
}

DACControl::DACControl()
    : _device(GP8413_DAC_ADDR)
{
    _initialized = false;
}

bool DACControl::begin(uint8_t address) {
    if (_initialized) return true;

    // Initialize I2C if not already initialized
    Wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);

    // Ensure the bus is at a safe speed and the device had time to power up
    Wire.setClock(I2C_SAFE_CLOCK_HZ);
    delay(POWERUP_DELAY_MS);

    // Step 1: try the provided address first
    uint8_t selectedAddr = address;
    INFO_LOG("Trying GP8413 at address 0x%02X", selectedAddr);

    if (!probeI2C(selectedAddr)) {
        INFO_LOG("GP8413 not found at primary address, trying alternative addresses");
        // Step 2: auto-discover across valid GP8413 ranges 0x58..0x5F
        bool found = false;
        for (uint8_t a = 0x58; a <= 0x5F; ++a) {
            if (probeI2C(a)) {
                selectedAddr = a;
                found = true;
                INFO_LOG("GP8413 found at alternative address 0x%02X", selectedAddr);
                break;
            }
        }

        if (!found) {
            WARNING_LOG("GP8413 DAC not found on I2C bus - will proceed without DAC support");
            // Allow program to continue without DAC
            return false;
        }
    }

    // Reconstruct at the detected address
    {
        DFRobot_GP8413 tmp(selectedAddr);
        _device = tmp;
    }

    // Step 3: robust begin with retries (library returns 0 on success)
    bool ok = false;
    for (uint8_t i = 0; i < I2C_RETRY_COUNT; ++i) {
        INFO_LOG("Attempting DAC initialization, attempt %d of %d", i + 1, I2C_RETRY_COUNT);
        if (_device.begin() == 0) {
            ok = true;
            INFO_LOG("GP8413 successfully initialized");
            break;
        }
        delay(I2C_RETRY_DELAY_MS);
    }

    if (!ok) {
        WARNING_LOG("GP8413 initialization failed - will proceed without DAC support");
        return false;
    }

    // Default range 0-5V
    _device.setDACOutRange(_device.eOutputRange5V);
    _currentRange = RANGE_0_5V;

    // Initialize outputs to zero
    _device.setDACOutVoltage(0, 2);
    _raw[0] = _raw[1] = 0;
    _volts[0] = _volts[1] = 0.0f;

    _initialized = true;
    INFO_LOG("GP8413 initialized @0x%02X", selectedAddr);
    return true;
}

bool DACControl::setOutputRange(DACOutputRange range) {
    if (!_initialized) { ERROR_LOG("Range change before init"); return false; }
    if (range == _currentRange) return true;

    if (range == RANGE_0_5V) {
        _device.setDACOutRange(_device.eOutputRange5V);
    }
    else {
        _device.setDACOutRange(_device.eOutputRange10V);
    }
    _currentRange = range;

    // Re-apply voltages (will be recalculated with new full-scale)
    setVoltage(0, _volts[0]);
    setVoltage(1, _volts[1]);

    INFO_LOG("Range set -> %s", (range == RANGE_0_5V) ? "0-5V" : "0-10V");
    return true;
}

float DACControl::activeFullScale() const {
    return _measuredFS[rangeIndex(_currentRange)];
}

uint16_t DACControl::voltageToRaw(float v) const {
    float fs = activeFullScale();
    if (fs <= 0.001f) fs = (_currentRange == RANGE_0_5V) ? NOMINAL_FS_5V : NOMINAL_FS_10V;

    if (v < 0.0f) v = 0.0f;

    float ratio = v / fs;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    uint32_t raw = (uint32_t)lroundf(ratio * (float)MAX_DAC_VALUE);
    if (raw > MAX_DAC_VALUE) raw = MAX_DAC_VALUE;
    return (uint16_t)raw;
}

float DACControl::rawToVoltage(uint16_t raw) const {
    if (raw > MAX_DAC_VALUE) raw = MAX_DAC_VALUE;
    float fs = activeFullScale();
    if (fs <= 0.001f) fs = (_currentRange == RANGE_0_5V) ? NOMINAL_FS_5V : NOMINAL_FS_10V;
    return (float)raw / (float)MAX_DAC_VALUE * fs;
}

bool DACControl::setVoltage(uint8_t channel, float voltage) {
    if (!_initialized) { ERROR_LOG("setVoltage before init"); return false; }
    if (channel > 2) { ERROR_LOG("Bad channel %u", channel); return false; }

    uint16_t raw = voltageToRaw(voltage);

    if (channel == 2) {
        _raw[0] = _raw[1] = raw;
        _volts[0] = _volts[1] = rawToVoltage(raw);
        _device.setDACOutVoltage(raw, 2);
        INFO_LOG("Both -> %.4f V (raw=%u)", _volts[0], raw);
    }
    else {
        _raw[channel] = raw;
        _volts[channel] = rawToVoltage(raw);
        _device.setDACOutVoltage(raw, channel);
        INFO_LOG("Ch%u -> %.4f V (raw=%u)", channel, _volts[channel], raw);
    }
    return true;
}

bool DACControl::setRaw(uint8_t channel, uint16_t rawCode) {
    if (!_initialized) return false;
    if (channel > 2) return false;
    if (rawCode > MAX_DAC_VALUE) rawCode = MAX_DAC_VALUE;

    if (channel == 2) {
        _raw[0] = _raw[1] = rawCode;
        _volts[0] = _volts[1] = rawToVoltage(rawCode);
        _device.setDACOutVoltage(rawCode, 2);
        INFO_LOG("Both raw=%u (%.4f V)", rawCode, _volts[0]);
    }
    else {
        _raw[channel] = rawCode;
        _volts[channel] = rawToVoltage(rawCode);
        _device.setDACOutVoltage(rawCode, channel);
        INFO_LOG("Ch%u raw=%u (%.4f V)", channel, rawCode, _volts[channel]);
    }
    return true;
}

float DACControl::getVoltage(uint8_t channel) const {
    if (channel > 1) return 0.0f;
    return _volts[channel];
}

uint16_t DACControl::getRaw(uint8_t channel) const {
    if (channel > 1) return 0;
    return _raw[channel];
}

bool DACControl::storeSettings() {
    if (!_initialized) return false;
    _device.store();
    INFO_LOG("DAC internal store invoked");
    return true;
}

void DACControl::setMeasuredFullScale(DACOutputRange range, float measuredFS) {
    if (measuredFS <= 0.05f) {
        WARNING_LOG("Reject FS %.3f (too small)", measuredFS);
        return;
    }
    int idx = rangeIndex(range);
    _measuredFS[idx] = measuredFS;
    INFO_LOG("Cal FS for range %s set to %.4f V",
        (range == RANGE_0_5V ? "0-5" : "0-10"), measuredFS);

    // Re-sync cached voltages (recalculate display values)
    _volts[0] = rawToVoltage(_raw[0]);
    _volts[1] = rawToVoltage(_raw[1]);
}

float DACControl::getMeasuredFullScale(DACOutputRange range) const {
    return _measuredFS[rangeIndex(range)];
}

void DACControl::resetCalibration() {
    _measuredFS[0] = NOMINAL_FS_5V;
    _measuredFS[1] = NOMINAL_FS_10V;
    INFO_LOG("Calibration reset to nominal");
    _volts[0] = rawToVoltage(_raw[0]);
    _volts[1] = rawToVoltage(_raw[1]);
}