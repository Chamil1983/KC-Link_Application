#ifndef ANALOG_INPUTS_H
#define ANALOG_INPUTS_H

#include <Arduino.h>
#include "Config.h"

class AnalogInputs {
public:
    enum VoltRange : uint8_t { V_RANGE_5V = 5, V_RANGE_10V = 10 };

    AnalogInputs();
    void begin();

    // Global voltage scaling selection (software scaling only)
    void setVoltageRange(VoltRange r) { _voltRange = r; }
    VoltRange getVoltageRange() const { return _voltRange; }

    // Read raw ADC values (0..4095)
    uint16_t readRawVoltageChannel(uint8_t channel);
    uint16_t readRawCurrentChannel(uint8_t channel);

    // Read scaled values
    float readVoltage(uint8_t channel);     // volts (0..5 or 0..10 depending on setVoltageRange)
    float readCurrent(uint8_t channel);     // mA (4..20)

    // Convenience helpers (mV/mA)
    int32_t readVoltage_mV(uint8_t channel);   // 0..5000 or 0..10000
    int32_t readCurrent_mA(uint8_t channel);   // 0.. (typically 4..20)

    // Advanced functions
    float getAverageVoltage(uint8_t channel, uint8_t samples = 10);
    float getAverageCurrent(uint8_t channel, uint8_t samples = 10);

private:
    uint8_t voltageChannelPins[NUM_ANALOG_CHANNELS];
    uint8_t currentChannelPins[NUM_CURRENT_CHANNELS];
    VoltRange _voltRange = V_RANGE_5V;

    float rawToVoltage(uint16_t rawValue);
    float rawToCurrent(uint16_t rawValue);
};

#endif // ANALOG_INPUTS_H