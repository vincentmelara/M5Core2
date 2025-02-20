#include <M5Core2.h>
#include <Wire.h>
#include "./I2C_RW.h"
#include <Speaker.h>
#include <Adafruit_VCNL4040.h>

// Variables
int const PIN_SDA = 32;
int const PIN_SCL = 33;
Adafruit_VCNL4040 vcnl;  // Proximity sensor object

// Register defines
#define VCNL_I2C_ADDRESS 0x60
#define VCNL_REG_PROX_DATA 0x08
#define VCNL_REG_ALS_DATA 0x09
#define VCNL_REG_WHITE_DATA 0x0A

#define VCNL_REG_PS_CONFIG 0x03
#define VCNL_REG_ALS_CONFIG 0x00
#define VCNL_REG_WHITE_CONFIG 0x04

Speaker speaker = Speaker();

const int SAMPLE_RATE = 16000;  // Sample rate in Hz
const int BUFFER_SIZE = 512;    // Buffer size for generated sound

uint8_t soundBuffer[BUFFER_SIZE];  // Buffer to hold generated sound wave

void generateSineWave(int frequency, int volume) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        // Generate a sine wave PCM value (8-bit audio: volume-based)
        soundBuffer[i] = (uint8_t)(128 + (volume * sin(2 * M_PI * frequency * i / SAMPLE_RATE)));
    }
}

// Setup function (runs once)
void setup() {
    // Init device
    M5.begin();
    Wire.begin();
    M5.Axp.SetSpkEnable(true);  // Enable speaker amplifier

    if (!vcnl.begin()) {
        Serial.println("Failed to find VCNL4040 sensor!");
        while (1);
    }

    // Connect to VCNL sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);

	// Write registers to initialize/enable VCNL sensors
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_WHITE_CONFIG, 2, 0x0000, " to enable raw white light sensor", true);
}

// Loop function (runs repeatedly)
void loop() {
    uint16_t proximity = vcnl.getProximity();  // Read proximity value

     // If proximity is less than 15, do not play any sound
    if (proximity < 15) {
        Serial.println("Proximity too low, no sound.");
        M5.Axp.SetVibration(0);  // Turn off vibration
        delay(100);  // Small delay to prevent constant polling
        return;  // Skip sound generation and playback
    }

    // Limit proximity to 500 max to prevent exceeding volume limits
    if (proximity > 500) {
        proximity = 500;
    }

    // Map proximity (0-500) to vibration intensity (0-255)
    int vibrationIntensity = map(proximity, 0, 500, 0, 255);

     // Map proximity (0-500) to volume (0-127) for smooth scaling
    int volume = map(proximity, 0, 500, 0, 127);
    
    // Map proximity to frequency (closer = higher pitch)
    int frequency = map(proximity, 0, 500, 100, 2000);

    // Generate new sound wave data with mapped volume
    generateSineWave(frequency, volume);

    // Play the generated sound buffer
    M5.Spk.PlaySound(soundBuffer, sizeof(soundBuffer));
    M5.Axp.SetVibration(vibrationIntensity);

    delay(100);  // Small delay to stabilize readings
}
