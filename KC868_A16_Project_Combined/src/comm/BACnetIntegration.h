#ifndef BACNET_INTEGRATION_H
#define BACNET_INTEGRATION_H

#include <Arduino.h>
#include "BACnetDriver.h"

/**
 * BACnet Integration Layer
 * ------------------------------------------------------------
 * Bridges between KC868-A16 hardware state (Globals.h) and the BACnetDriver.
 *
 * - Starts BACnet/IP automatically once Ethernet/WiFi is connected
 * - Keeps BI/BO/AI objects updated from hardware
 * - Applies BO write commands back to hardware (MOSFET outputs)
 *
 * IMPORTANT:
 *   This file does NOT modify any MODBUS code paths.
 */

class BACnetIntegration {
public:
    static void initialize();
    static void update();

    static void setEnabled(bool enabled);
    static bool isEnabled();

private:
    static bool _enabled;
    static bool _started;
    static uint32_t _lastSync;
    static const uint32_t SYNC_INTERVAL = 250; // ms (BACnet value refresh to client)

    // Internal helpers
    static bool isNetworkReady(IPAddress& ip, IPAddress& gw, IPAddress& mask);
    static void startIfNeeded();

    static void applyBinaryOutputCommands();

    static void updateAnalogInputs();
    static void updateBinaryInputs();
    static void updateBinaryOutputs();
    static void updateSensorValues();
};

#endif // BACNET_INTEGRATION_H
