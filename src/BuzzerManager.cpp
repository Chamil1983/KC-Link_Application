/**
 * @file BuzzerManager.cpp
 * @brief Implementation of the BuzzerManager class
 */

#include "BuzzerManager.h"

 // Initialize static variables
unsigned long BuzzerManager::stopTime = 0;
bool BuzzerManager::useStopTime = false;

void BuzzerManager::begin() {
    // Initialize the EasyBuzzer library with the buzzer pin
    EasyBuzzer.setPin(BUZZER_PIN);
}

void BuzzerManager::update() {
    // Update EasyBuzzer to generate the sound patterns
    EasyBuzzer.update();

    // Check if we need to stop the buzzer after a certain duration
    if (useStopTime && millis() >= stopTime) {
        EasyBuzzer.stopBeep();
        useStopTime = false;
    }
}

void BuzzerManager::stop() {
    EasyBuzzer.stopBeep();
    useStopTime = false;
}

void BuzzerManager::playWarning(WarningLevel_t level, unsigned long durationMs) {
    // Stop any existing pattern first
    stop();

    // Set stop time if duration is specified
    if (durationMs > 0) {
        stopTime = millis() + durationMs;
        useStopTime = true;
    }

    // Play the appropriate pattern based on warning level
    switch (level) {
    case WARNING_INFO:
        playInfoPattern();
        break;
    case WARNING_NOTICE:
        playNoticePattern();
        break;
    case WARNING_CAUTION:
        playCautionPattern();
        break;
    case WARNING_WARNING:
        playWarningPattern();
        break;
    case WARNING_ALARM:
        playAlarmPattern();
        break;
    case WARNING_CRITICAL:
        playCriticalPattern();
        break;
    default:
        // Default to info level if unknown
        playInfoPattern();
        break;
    }
}

void BuzzerManager::playCustomPattern(unsigned int frequency,
    unsigned int onDuration,
    unsigned int offDuration,
    unsigned int beepCount,
    unsigned int pauseDuration,
    unsigned int sequences) {
    stop();
    EasyBuzzer.beep(
        frequency,    // Frequency in Hz
        onDuration,   // On duration in ms
        offDuration,  // Off duration in ms
        beepCount,    // Beep count in a cycle
        pauseDuration,// Pause duration in ms
        sequences     // Number of cycles (0 = infinite)
    );
}

void BuzzerManager::playConfirmation() {
    stop();
    // Play a short rising confirmation beep (rising pitch)
    EasyBuzzer.beep(
        2000,  // Frequency
        100,   // On duration
        0,     // Off duration
        1,     // Beep count
        0,     // Pause
        1      // Sequences
    );
}

void BuzzerManager::playBootSound() {
    stop();
    // Play a startup sound (rising pitch sequence)
    EasyBuzzer.beep(
        1000,  // Starting frequency
        100,   // On duration
        50,    // Off duration
        3,     // Beeps
        200,   // Pause
        1,     // Sequences
        []() { // Callback function for when first sequence completes
            EasyBuzzer.beep(
                2000,  // Higher frequency
                150,   // On duration
                0,     // Off duration
                1,     // Beep count
                0,     // Pause
                1      // Sequences
            );
        }
    );
}

void BuzzerManager::playShutdownSound() {
    stop();
    // Play a shutdown sound (falling pitch sequence)
    EasyBuzzer.beep(
        2000,  // Starting frequency
        150,   // On duration
        50,    // Off duration
        1,     // Beeps
        200,   // Pause
        1,     // Sequences
        []() { // Callback function for when first sequence completes
            EasyBuzzer.beep(
                1000,  // Lower frequency
                300,   // On duration
                0,     // Off duration
                1,     // Beep count
                0,     // Pause
                1      // Sequences
            );
        }
    );
}

bool BuzzerManager::isActive() {
    return EasyBuzzer.isRunning();
}

// Pattern implementations
void BuzzerManager::playInfoPattern() {
    // Single short beep, low frequency
    EasyBuzzer.beep(
        800,   // Low frequency
        200,   // On duration
        0,     // Off duration
        1,     // Beep count
        0,     // Pause
        1      // Sequences
    );
}

void BuzzerManager::playNoticePattern() {
    // Two short beeps, medium-low frequency
    EasyBuzzer.beep(
        1000,  // Medium-low frequency
        200,   // On duration
        200,   // Off duration
        2,     // Beep count
        0,     // Pause
        1      // Sequences
    );
}

void BuzzerManager::playCautionPattern() {
    // Three short beeps, medium frequency
    EasyBuzzer.beep(
        1500,  // Medium frequency
        200,   // On duration
        200,   // Off duration
        3,     // Beep count
        1000,  // Pause
        0      // Sequences (infinite until stopped)
    );
}

void BuzzerManager::playWarningPattern() {
    // Intermittent medium-high frequency beeps
    EasyBuzzer.beep(
        2000,  // Medium-high frequency
        300,   // On duration
        300,   // Off duration
        2,     // Beep count
        600,   // Pause
        0      // Sequences (infinite until stopped)
    );
}

void BuzzerManager::playAlarmPattern() {
    // Rapid high frequency beeps
    EasyBuzzer.beep(
        2500,  // High frequency
        200,   // On duration
        200,   // Off duration
        5,     // Beep count
        500,   // Pause
        0      // Sequences (infinite until stopped)
    );
}

void BuzzerManager::playCriticalPattern() {
    // Continuous SOS pattern with high frequency
    EasyBuzzer.beep(
        3000,  // High frequency
        200,   // On duration
        200,   // Off duration
        3,     // Three short beeps
        400,   // Pause
        1,     // Sequence
        []() { // Callback function for when first sequence completes
            EasyBuzzer.beep(
                3000,  // Same frequency
                600,   // Longer on duration
                200,   // Off duration
                3,     // Three long beeps
                400,   // Pause
                1,     // Sequence
                []() { // Callback function for when second sequence completes
                    EasyBuzzer.beep(
                        3000,  // Same frequency
                        200,   // On duration
                        200,   // Off duration
                        3,     // Three short beeps again
                        1000,  // Long pause
                        0      // Sequences (infinite until stopped)
                    );
                }
            );
        }
    );
}