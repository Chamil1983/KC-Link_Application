#include "RF433Comm.h"

RF433Comm::RF433Comm() : initialized(false) {
}

bool RF433Comm::begin() {
    // Setup RF transmitter
    rfSwitch.enableTransmit(PIN_RF433_TX);
    
    // Setup RF receiver
    rfSwitch.enableReceive(digitalPinToInterrupt(PIN_RF433_RX));
    
    initialized = true;
    return true;
}

void RF433Comm::sendCode(unsigned long code, unsigned int bitLength) {
    if (!initialized) return;
    
    rfSwitch.send(code, bitLength);
}

void RF433Comm::sendTriState(const char* triStateCode) {
    if (!initialized) return;
    
    rfSwitch.sendTriState(triStateCode);
}

bool RF433Comm::available() {
    if (!initialized) return false;
    
    return rfSwitch.available();
}

unsigned long RF433Comm::getReceivedValue() {
    if (!initialized) return 0;
    
    return rfSwitch.getReceivedValue();
}

unsigned int RF433Comm::getReceivedBitlength() {
    if (!initialized) return 0;
    
    return rfSwitch.getReceivedBitlength();
}

unsigned int RF433Comm::getReceivedProtocol() {
    if (!initialized) return 0;
    
    return rfSwitch.getReceivedProtocol();
}

void RF433Comm::resetReceiver() {
    if (!initialized) return;
    
    rfSwitch.resetAvailable();
}