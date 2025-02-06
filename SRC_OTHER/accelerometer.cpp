#include <M5Core2.h>

// -------Variables-------
float accX;
float accY;
float accZ;
int screenWidth = M5.Lcd.width();
int screenHeight = M5.Lcd.height();
int barWidth = screenWidth / 4;
bool isGraphMode = false;

// -------Put your setup code here, to run once-------
void setup() {
    // TODO 2: Init device and accelerometer
    M5.begin();

    // Initialize text options and black screen
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextWrap(true);
    M5.Lcd.setTextSize(3);
}

// -------Put your main code here, to run repeatedly-------
void loop() {
    // Read in button data and store
    M5.update(); // Reads data

    // If button was just pressed/released, update mode and button state
    if (M5.BtnA.wasPressed()) {
        isGraphMode = !isGraphMode;
    }

    // TODO 1: Get the accelerometer data and adjust to m/s^2 so that 1=>9.8

    // Draw the black screen
    M5.Lcd.fillScreen(BLACK);

    // If in RAW DATA mode, print the raw values
    if (!isGraphMode) {
        // Initialize the cursor/font
        M5.Lcd.setTextSize(3);
        M5.Lcd.setCursor(0, 0);

        // TODO 3: Print out the data (white if positive, red if negative)
        M5.Lcd.printf("Gravity\n(m/s^2)\n\n");
        M5.Lcd.printf("X=%.2f\n\n", accX);
        M5.Lcd.printf("Y=%.2f\n\n", accY);
        M5.Lcd.printf("Z=%.2f\n\n", accZ);
    } else { // If in GRAPH mode, print the XYZ bar graphs

        // TODO 6: Multiply such that each m/s^2 takes up mult pixels

        // TODO 9: Draw x-axis marks
        // M5.Lcd.setTextSize(2);
        // M5.Lcd.setTextColor(TFT_WHITE);

        // TODO 5: Draw the bar graphs - draws down and to right; (x,y) is top-left corner

        // TODO 7: Draw the X/Y/Z labels on the graph at the bottom
        // M5.Lcd.setTextColor(TFT_BLACK);
        // M5.Lcd.print("X");
        // M5.Lcd.print("Y");
        // M5.Lcd.print("Z");

        // TODO 8: Draw the actual values above the graph
        // M5.Lcd.setTextColor(TFT_WHITE);
        // M5.Lcd.setTextSize(2);
    }

    // TODO 4: Arbitrary delay to slow down screen updates
}

//////////////////////////////////////////////////////////////////////////////////
// For more documentation see the following link:
// https://github.com/m5stack/m5-docs/blob/master/docs/en/api/
//////////////////////////////////////////////////////////////////////////////////