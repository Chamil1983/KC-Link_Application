/**
 * @file BuzzerManager.h
 * @brief Buzzer management library for Cortex Link A8R-M ESP32 Smart Relay Board
 * @author Copilot
 * @version 1.0
 * @date 2023-09-28
 *
 * This library provides functions to control the onboard buzzer
 * for different warning levels and notification patterns.
 * Uses ESP32 GPIO2 connected to the buzzer.
 */

#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>
#include <EasyBuzzer.h>

 // Buzzer pin definition - from pin configuration table (GPIO2)
#define BUZZER_PIN 2

// Warning levels
typedef enum {
    WARNING_INFO,        // Low level - informational alerts
    WARNING_NOTICE,      // Notice level - something requires attention
    WARNING_CAUTION,     // Caution level - potential issue
    WARNING_WARNING,     // Warning level - issue detected
    WARNING_ALARM,       // Alarm level - serious issue
    WARNING_CRITICAL     // Critical level - critical system failure
} WarningLevel_t;

class BuzzerManager {
public:
    /**
     * @brief Initialize the buzzer
     */
    static void begin();

    /**
     * @brief Update function, must be called in the main loop
     */
    static void update();

    /**
     * @brief Stop any active buzzer pattern
     */
    static void stop();

    /**
     * @brief Play a warning pattern based on warning level
     * @param level Warning level from WarningLevel_t enum
     * @param durationMs Optional duration in milliseconds (0 = repeat until stop is called)
     */
    static void playWarning(WarningLevel_t level, unsigned long durationMs = 0);

    /**
     * @brief Play a custom beep pattern
     * @param frequency Frequency in Hz
     * @param onDuration On duration in milliseconds
     * @param offDuration Off duration in milliseconds
     * @param beepCount Number of beeps to play (0 = infinite)
     * @param pauseDuration Pause between sequences in milliseconds
     * @param sequences Number of sequences to play (0 = infinite)
     */
    static void playCustomPattern(unsigned int frequency,
        unsigned int onDuration,
        unsigned int offDuration,
        unsigned int beepCount,
        unsigned int pauseDuration,
        unsigned int sequences);

    /**
     * @brief Play a short confirmation beep
     */
    static void playConfirmation();

    /**
     * @brief Play a boot-up sound
     */
    static void playBootSound();

    /**
     * @brief Play a shutdown sound
     */
    static void playShutdownSound();

    /**
     * @brief Check if the buzzer is currently active
     * @return True if active, false otherwise
     */
    static bool isActive();

private:
    static unsigned long stopTime;
    static bool useStopTime;

    // Pattern definitions
    static void playInfoPattern();
    static void playNoticePattern();
    static void playCautionPattern();
    static void playWarningPattern();
    static void playAlarmPattern();
    static void playCriticalPattern();
};

#endif // BUZZER_MANAGER_H