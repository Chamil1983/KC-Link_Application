#include "AnalogInputs.h"

AnalogInputs::AnalogInputs() {
    voltageChannelPins[0] = PIN_ANALOG_CH1;
    voltageChannelPins[1] = PIN_ANALOG_CH2;

    currentChannelPins[0] = PIN_CURRENT_CH1;
    currentChannelPins[1] = PIN_CURRENT_CH2;
}

void AnalogInputs::begin() {
    analogReadResolution(12);
}

uint16_t AnalogInputs::readRawVoltageChannel(uint8_t channel) {
    if (channel >= NUM_ANALOG_CHANNELS) return 0;
    return analogRead(voltageChannelPins[channel]);
}

uint16_t AnalogInputs::readRawCurrentChannel(uint8_t channel) {
    if (channel >= NUM_CURRENT_CHANNELS) return 0;
    return analogRead(currentChannelPins[channel]);
}

float AnalogInputs::readVoltage(uint8_t channel) {
    if (channel >= NUM_ANALOG_CHANNELS) return 0.0f;
    uint16_t rawValue = readRawVoltageChannel(channel);
    return rawToVoltage(rawValue);
}

float AnalogInputs::readCurrent(uint8_t channel) {
    if (channel >= NUM_CURRENT_CHANNELS) return 0.0f;
    uint16_t rawValue = readRawCurrentChannel(channel);
    return rawToCurrent(rawValue);
}

int32_t AnalogInputs::readVoltage_mV(uint8_t channel) {
    float v = readVoltage(channel);
    return (int32_t)lroundf(v * 1000.0f);
}

int32_t AnalogInputs::readCurrent_mA(uint8_t channel) {
    float mA = readCurrent(channel);
    return (int32_t)lroundf(mA);
}

float AnalogInputs::getAverageVoltage(uint8_t channel, uint8_t samples) {
    if (channel >= NUM_ANALOG_CHANNELS) return 0.0f;
    float sum = 0.0f;
    for (uint8_t i = 0; i < samples; i++) {
        sum += readVoltage(channel);
        delay(2);
    }
    return sum / samples;
}

float AnalogInputs::getAverageCurrent(uint8_t channel, uint8_t samples) {
    if (channel >= NUM_CURRENT_CHANNELS) return 0.0f;
    float sum = 0.0f;
    for (uint8_t i = 0; i < samples; i++) {
        sum += readCurrent(channel);
        delay(2);
    }
    return sum / samples;
}

float AnalogInputs::rawToVoltage(uint16_t rawValue) {
    // ADC input voltage at ESP32 pin (0..~3.3V). We scale to "logical" range 0..5 or 0..10.
    float v_adc = (rawValue / (float)ADC_RESOLUTION) * ADC_VOLTAGE_REF;

    // Prior code effectively assumed 0-5V. Now we allow 0-10V software scaling.
    float logicalMax = (_voltRange == V_RANGE_10V) ? 10.0f : 5.0f;
    // Map ADC ref span to logical span (software-only scaling)
    // v_adc == ADC_VOLTAGE_REF => logicalMax
    float v = (v_adc / ADC_VOLTAGE_REF) * logicalMax;

    if (v < 0) v = 0;
    if (v > logicalMax) v = logicalMax;
    return v;
}

float AnalogInputs::rawToCurrent(uint16_t rawValue) {
    // Based on calibration points:
    // ADC value 625 = 4mA
    // ADC value 3860 = 20mA
    if (rawValue <= 625) return 4.0f;
    if (rawValue >= 3860) return 20.0f;
    return 0.004946f * rawValue + 0.91f;
}