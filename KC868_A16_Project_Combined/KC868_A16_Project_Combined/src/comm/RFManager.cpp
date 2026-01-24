// RFManager.cpp
// Auto-split from original KC868_A16_Controller.ino

#include "../FunctionPrototypes.h"

void initRF() {
    rfReceiver.enableReceive(RF_RX_PIN);
    rfTransmitter.enableTransmit(RF_TX_PIN);
    debugPrintln("RF receiver/transmitter initialized");
}

