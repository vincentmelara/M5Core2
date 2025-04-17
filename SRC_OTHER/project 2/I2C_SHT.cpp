#include "I2C_SHT.h"

// I2C_SHT Variable inits
int I2C_SHT::i2cAddress = 0;
int I2C_SHT::i2cFrequency = 0;
int I2C_SHT::i2cSdaPin = 0;
int I2C_SHT::i2cSclPin = 0;

// This method sets the i2C address, frequency and GPIO pins and must
// be called once anytime you want to communicate to a different device
// and/or change the parameters for I2C.
void I2C_SHT::initI2C(int i2cAddr, int i2cFreq, int pinSda, int pinScl) {
    // Save parameters in case needed later
    i2cAddress = i2cAddr;
    i2cFrequency = i2cFreq;
    i2cSdaPin = pinSda;
    i2cSclPin = pinScl;

    // Initialize I2C interface
    Wire.begin(i2cSdaPin, i2cSclPin, i2cFrequency);
}

// Scan the I2C bus for any attached periphials and print out the addresses of each line.
void I2C_SHT::scanI2cLinesForAddresses(bool verboseConnectionFailures) {
    byte error, address;
    int nDevices;
    Serial.println("STATUS: Scanning for I2C devices...");
    nDevices = 0;

    // Scan all possible addresses
    bool addressesFound[128] = {false};
    for (address = 0; address < 128; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            // Serial.printf("\tSTATUS: I2C device found at address 0x%02X\n", address);
            addressesFound[address] = true;
            nDevices++;
        } else if (verboseConnectionFailures) {
            char message[20];
            sprintf(message, "on address 0x%02X", address);
            // printI2cReturnStatus(error, 0, message);
        }
    }

    // Print total devices found
    if (nDevices == 0) {
        Serial.println("\tSTATUS: No I2C devices found\n");
    } else {
        Serial.print("\tSTATUS: Done scanning for I2C devices. Devices found at following addresses: \n\t\t");
        for (address = 0; address < 128; address++) {
            if (addressesFound[address])
                Serial.printf("0x%02X   ", address);
        }
        Serial.println("");
    }
    delay(100);
}

// Take in the return status of an I2C endTransmission() call and
// print out the status.
void I2C_SHT::printI2cReturnStatus(byte returnStatus, int bytesWritten, const char action[]) {
    switch (returnStatus) {
        case 0:
            Serial.printf("\tSTATUS (I2C): %d bytes successfully read/written %s\n", bytesWritten, action);
            break;
        case 1:
            Serial.printf("\t***ERROR (I2C): %d bytes failed %s - data too long to fit in transmit buffer***\n", bytesWritten, action);
            break;
        case 2:
            Serial.printf("\t***ERROR (I2C): %d bytes failed %s - received NACK on transmit of address***\n", bytesWritten, action);
            break;
        case 3:
            Serial.printf("\t***ERROR (I2C): %d bytes failed %s - received NACK on transmit of data***\n", bytesWritten, action);
            break;
        default:
            Serial.printf("\t***ERROR (I2C): %d bytes failed %s - unknown error***\n", bytesWritten, action);
            break;
    }
}

// Read register at the 8-bit address via I2C; returns the 16-bit value
uint32_t I2C_SHT::readReg8Addr48Data(byte regAddr, int numBytesToRead, String action, bool verbose) {

    // Attempts a number of retries in case the data is not initially ready
    int maxRetries = 50;
    for (int i = 0; i < maxRetries; i++) {
        // Enable I2C connection
        Wire.beginTransmission(i2cAddress);

        // Prepare and write address - MSB first and then LSB
        int bytesWritten = 0;
        bytesWritten += Wire.write(regAddr);

        // End transmission (not writing...reading)
        byte returnStatus = Wire.endTransmission(true);
        delay(10);
        if (verbose)
            printI2cReturnStatus(returnStatus, bytesWritten, action.c_str());

        // Read data from above address
        Wire.requestFrom(i2cAddress, numBytesToRead);

        // Grab the data from the data line
        if (Wire.available() == numBytesToRead) {
            uint32_t data = 0;
            uint8_t t_msb = Wire.read();
            uint8_t t_lsb = Wire.read();
            uint8_t check1 = Wire.read();
            uint8_t h_msb = Wire.read();
            uint8_t h_lsb = Wire.read();
            uint8_t check2 = Wire.read();
            data = ((uint32_t) t_msb << 24 | t_lsb << 16 | h_msb << 8 | h_lsb);

            if (verbose)
                Serial.printf("\tSTATUS: I2C read at address 0x%02X = data:%d (%s)\n", regAddr, data, action.c_str());
            return data;
        }
    }
}
