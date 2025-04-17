#ifndef I2C_SHT_H
#define I2C_SHT_H

// Includes
#include "Arduino.h"
#include <Wire.h>    // I2C library

class I2C_SHT{
    public:
        // Members
        static int i2cAddress;  // Will only be changed in testing to confirm address
        static int i2cFrequency;
        static int i2cSdaPin;
        static int i2cSclPin;

        // Initialization and debugging methods
        static void initI2C(int i2cAddress, int i2cFreq, int pinSda, int pinScl);
        static void scanI2cLinesForAddresses(bool verboseConnectionFailures);
        static void printI2cReturnStatus(byte returnStatus, int bytesWritten, const char action[]);
        static void readReg8Addr16Data(unsigned char, int, String, bool);
        static void writeReg8Addr16DataWithProof(unsigned char, int, unsigned short, String, bool);

        // 8-bit register methods
        static uint32_t readReg8Addr48Data(byte regAddr, int numBytesToRead, String action, bool verbose);
};
#endif