/**
 * DACControl.h - Improved & Calibrated GP8413 15-bit DAC driver
 * Cortex Link A8R-M ESP32 Smart Relay Board
 *
 * Features:
 *  - 0-5V / 0-10V range selection
 *  - Per-range measured full-scale calibration (compensate HW gain error)
 *  - Voltage <-> raw 15-bit conversion using calibrated full scale
 *  - Per-channel / both-channel updates
 *  - Persistent store() passthrough (chip internal, NOT calibration)
 *  - Raw direct setting
 */

#ifndef DAC_CONTROL_H
#define DAC_CONTROL_H

#include <Arduino.h>
#include <DFRobot_GP8XXX.h>
#include "Debug.h"
#include "Config.h"

enum DACOutputRange : uint8_t {
    RANGE_0_5V = 0,
    RANGE_0_10V = 1
};

class DACControl {
public:
    DACControl();

    bool begin(uint8_t address = GP8413_DAC_ADDR);
    bool setOutputRange(DACOutputRange range);

    // Set user voltage (channel=0|1|2=both). Applies calibration.
    bool setVoltage(uint8_t channel, float voltage);
    // Set raw 15-bit code directly
    bool setRaw(uint8_t channel, uint16_t rawCode);

    float getVoltage(uint8_t channel) const;   // cached (calculated) voltage
    uint16_t getRaw(uint8_t channel) const;    // cached raw code

    DACOutputRange getOutputRange() const { return _currentRange; }

    bool storeSettings();                      // chip EEPROM (NOT calibration)

    // Calibration interface
    void  setMeasuredFullScale(DACOutputRange range, float measuredFS);
    float getMeasuredFullScale(DACOutputRange range) const;
    void  resetCalibration();                  // restore nominal 5.0 / 10.0

    // Helpers
    static constexpr uint16_t MAX_DAC_VALUE = 32767;
    static constexpr uint8_t  DAC_BITS = 15;

    // Nominal (design) full-scale values
    static constexpr float NOMINAL_FS_5V = 5.0f;
    static constexpr float NOMINAL_FS_10V = 10.0f;

    // Active (may be calibrated) full-scale for current range
    float activeFullScale() const;

    // Conversions (public in case caller wants to use directly)
    uint16_t voltageToRaw(float v) const;
    float    rawToVoltage(uint16_t raw) const;

    // Public initialization status accessor (used by displayStatus())
    bool isInitialized() const { return _initialized; }

private:
    int rangeIndex(DACOutputRange r) const { return (r == RANGE_0_5V) ? 0 : 1; }

    DFRobot_GP8413 _device;
    bool           _initialized = false;
    DACOutputRange _currentRange = RANGE_0_5V;

    // Cached state
    uint16_t _raw[2] = { 0, 0 };
    float    _volts[2] = { 0.0f, 0.0f };

    // Calibration (measured full-scale per range)
    float _measuredFS[2] = { NOMINAL_FS_5V, NOMINAL_FS_10V };
};

#endif // DAC_CONTROL_H