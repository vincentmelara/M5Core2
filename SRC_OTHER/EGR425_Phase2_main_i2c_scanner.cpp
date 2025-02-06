#include <M5Core2.h>
#include <Wire.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
const int SDA_PIN = 32; // 21 for internal; 32 for port A
const int SCL_PIN = 33; // 22 for internal; 33 for port A

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup() {
    // Init device
    M5.begin();

    // Scan variables

    //stores response
    byte i2cResponse;
    int numDevices = 0;
    bool addressFound[128] = {false};

    // Initialize I2C port
                            //clock speed = 400kHz
    Wire.begin(SDA_PIN, SCL_PIN, 400000);

    // Start I2C port scan
    // up to 128 unique devices, so try every address
    Serial.println("STATUS: Scanning for I2C devices...");
    for (int a = 0; a < 128; a++) {
        // Attempt an I2C "hello"
        Wire.beginTransmission(a);
        i2cResponse = Wire.endTransmission();

        //case where a device is present at address
        if (i2cResponse == ESP_OK) {
            //Serial.printf("\tI2C device found at address 0x%02X\n", a);
            addressFound[a] = true;
            numDevices++;
        }        
    }

    // Print total devices found
    if (numDevices == 0)
        Serial.println("\tSTATUS: No I2C devices found\n");
    else {
        Serial.printf("\tSTATUS: Done scanning for I2C device(s). %d devices found at the following address(es): \n", numDevices);
        for (int a = 0; a < 128; a++)
            if (addressFound[a])
                Serial.printf("\t\t0x%02X\n", a);
    }
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{

}