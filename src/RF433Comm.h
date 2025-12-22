#ifndef RF433_COMM_H
#define RF433_COMM_H

#include <Arduino.h>
#include <RCSwitch.h>
#include "Config.h"

class RF433Comm {
public:
    RF433Comm();
    bool begin();
    
    // Transmit functions
    void sendCode(unsigned long code, unsigned int bitLength = 24);
    void sendTriState(const char* triStateCode);
    
    // Receive functions
    bool available();
    unsigned long getReceivedValue();
    unsigned int getReceivedBitlength();
    unsigned int getReceivedProtocol();
    void resetReceiver();

private:
    RCSwitch rfSwitch;
    bool initialized;
};

#endif // RF433_COMM_H