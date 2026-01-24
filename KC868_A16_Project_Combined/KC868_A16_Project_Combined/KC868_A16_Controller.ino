/*
 * KC868_A16_Controller.ino (Modular)
 * Wrapper sketch that delegates to src/core/App.cpp


    * KC868_A16_Controller.ino
    * Complete control system for Kincony KC868-A16 Smart Home Controller
    * Using Renzo Mischianti's PCF8574 Library

   Features:
   - 16 relay outputs via PCF8574 I2C expanders
   - 16 digital inputs via PCF8574 I2C expanders
   - 4 analog inputs
   - 3 direct GPIO inputs (HT1-HT3)
   - Multiple communication interfaces (USB, WiFi, Ethernet, RS-485)
   - Web interface for control with visual representation
   - Scheduling system with RTC
   - Analog threshold triggering
   - Comprehensive diagnostics
   - WebSocket support for real-time updates
   - Digital input interrupt handling with priority configuration

   Hardware:
   - ESP32 microcontroller
   - PCF8574 I2C expanders (addresses 0x21, 0x22, 0x24, 0x25)
   - LAN8720 Ethernet PHY
   - DS3231 RTC module (optional)

   Pin mapping from ESPHome configuration and schematic:
   - I2C: SDA=GPIO4, SCL=GPIO5
   - RS485: TX=GPIO13, RX=GPIO16
   - Analog inputs: A1=GPIO36, A2=GPIO34, A3=GPIO35, A4=GPIO39
   - Direct inputs: HT1=GPIO32, HT2=GPIO33, HT3=GPIO14
   - RF: RX=GPIO2, TX=GPIO15
   - Ethernet: MDC=GPIO23, MDIO=GPIO18, CLK=GPIO17
*/



#include "src/core/App.h"

void setup() {
	appSetup();
}

void loop() {
	appLoop();
}
