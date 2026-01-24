// AnalogDriver.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

float convertAnalogToVoltage(int analogValue) {
    // Calibration data: pairs of [ADC value, Actual Voltage]
    // These should be measured using a calibrated reference
    // For example: with 1V input, ADC reads ~820, with 5V input, ADC reads ~4095
    const int calADC[] = { 0, 820, 1640, 2460, 3270, 4095 };
    const float calVolts[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    const int numCalPoints = 6;  // Number of calibration points

    // Bound input to valid range
    if (analogValue <= 0) return 0.0f;
    if (analogValue >= 4095) return 5.0f;

    // Find the right calibration segment
    int segment = 0;
    while (segment < numCalPoints - 1 && analogValue > calADC[segment + 1]) {
        segment++;
    }

    // Linear interpolation within the segment
    float fraction = (float)(analogValue - calADC[segment]) /
        (float)(calADC[segment + 1] - calADC[segment]);

    float voltage = calVolts[segment] + fraction * (calVolts[segment + 1] - calVolts[segment]);

    return voltage;
}

int calculatePercentage(float voltage) {
    // Ensure voltage is in the correct range
    if (voltage > 5.0f) voltage = 5.0f;
    if (voltage < 0.0f) voltage = 0.0f;

    // Calculate percentage based on 0-5V range
    return (int)((voltage / 5.0f) * 100.0f);
}

int readAnalogInput(uint8_t index) {
    int pinMapping[] = { ANALOG_PIN_1, ANALOG_PIN_2, ANALOG_PIN_3, ANALOG_PIN_4 };

    if (index >= 4) return 0;

    // Take multiple readings and average them for better stability
    const int numReadings = 10;  // Increased from 5 to 10 for better accuracy
    int total = 0;

    for (int i = 0; i < numReadings; i++) {
        total += analogRead(pinMapping[index]);
        delay(1);  // Short delay between readings
    }

    return total / numReadings;
}

