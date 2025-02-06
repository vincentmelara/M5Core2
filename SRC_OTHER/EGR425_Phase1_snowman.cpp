#include <M5Core2.h>

///////////////////////////////////////////////////////////////
// CHALLENGE: Upgrade the snowman program so each button press
// of A, B or C will increment a Red, Green or Blue color value,
// respectively. The three distinct RGB color values should be
// used to generate one overall color which will be the
// background color behind the snowman. The background should
// update each time a button is pressed.
//
// LCD Details
// 320 x 240 pixels (width x height)
//      - (0, 0) is the TOP LEFT pixel
//      - Colors seem to be 16-bit as: RRRR RGGG GGGB BBBB (32 Reds, 64 Greens, 32 Blues)
//      TFT_RED         0xF800      /* 255,   0,   0 */     1111 1000 0000 0000
//      TFT_GREEN       0x07E0      /*   0, 255,   0 */     0000 0111 1110 0000
//      TFT_BLUE        0x001F      /*   0,   0, 255 */     0000 0000 0001 1111
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup() {
	// Initialize the device
    M5.begin();
	
    // Set up some variables for use in drawing
    int screenWidth = M5.Lcd.width();
    int screenHeight = M5.Lcd.height();

    //Draw the background
    M5.Lcd.fillScreen(BLACK);

	// TODO 0: Draw the base of the snowman
    // https://github.com/m5stack/m5-docs/blob/master/docs/en/api/lcd.md#fillcircle

    int radius = screenHeight / 4;
    //screenheight /4 = 60
    M5.Lcd.fillCircle(screenWidth / 2, screenHeight - 48, radius, TFT_WHITE);

    // TODO 1: Draw the body and head (2 iterations)
    //body
    M5.Lcd.fillCircle(screenWidth / 2, screenHeight - 130, radius - 15, TFT_WHITE);

    //head
    M5.Lcd.fillCircle(screenWidth / 2, screenHeight - 185, radius - 30, TFT_WHITE);


    // TODO 2: Draw 3 buttons
    M5.Lcd.fillCircle(screenWidth / 2, screenHeight - 48, radius / 7, TFT_BLACK);
    M5.Lcd.fillCircle(screenWidth / 2, screenHeight - 90, radius / 8, TFT_BLACK);
    M5.Lcd.fillCircle(screenWidth / 2, screenHeight - 130, radius / 10, TFT_BLACK);

    // TODO 3: Draw 2 eyes

    // TODO 4: Draw a carrot nose

}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() {}

//////////////////////////////////////////////////////////////////////////////////
// For more documentation see the following link:
// https://github.com/m5stack/m5-docs/blob/master/docs/en/api/
//////////////////////////////////////////////////////////////////////////////////