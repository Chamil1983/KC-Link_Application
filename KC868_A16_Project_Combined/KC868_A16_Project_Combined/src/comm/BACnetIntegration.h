#ifndef BACNET_INTEGRATION_H
#define BACNET_INTEGRATION_H

#include <Arduino.h>
#include "BACnetDriver.h"

/**
 * BACnet Integration Layer
 * Bridges between KC868-A16 hardware and BACnet driver
 * Syncs with existing global state from Globals.h
 */

class BACnetIntegration {
public:
    static void initialize();
    static void update();
    static void syncFromHardware();
    static void syncToHardware();

    // Configuration
    static void setEnabled(bool enabled);
    static bool isEnabled();

    // Status
    static String getStatus();
    static uint32_t getRequestCount();
    static uint32_t getResponseCount();

private:
    static bool _enabled;
    static uint32_t _lastSync;
    static const uint32_t SYNC_INTERVAL = 250; // ms

    // Helper methods
    static void updateAnalogInputs();
    static void updateBinaryInputs();
    static void updateBinaryOutputs();
    static void updateSensorValues();
    static void applyBinaryOutputCommands();
};

#endif // BACNET_INTEGRATION_H