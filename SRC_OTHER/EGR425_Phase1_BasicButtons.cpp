#include <Arduino.h>
#include <M5Core2.h>

// Program wide Variables
static bool ledIsOn = false;
static unsigned long buttonAPresses = 0;
static unsigned long lastTS = 0; //last timestamp

//variable to increment how many seconds the device has been on 
static unsigned long secondsOn = 0; 

//create the area that the user is allowed to give touch inputs in
static HotZone hz(0, 0, 320, 240);

void setup() {
  // Initialize the device so it is responsive
  M5.begin();

  //Do some intitial stuff
  M5.Lcd.setTextSize(3);
  M5.Lcd.fillScreen(BLUE);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println("Hello World! Dr. Dan is the best. ");
  Serial.println("Hello World! Dr. Dan is the best. ");

  //initialize LED
  M5.Axp.SetLed(ledIsOn);
  ledIsOn = !ledIsOn;
  lastTS = millis();
}

void loop() {
  //check for button inut
  M5.update();
  if (M5.BtnA.wasPressed()){
    Serial.printf("\tYou pressed button A %lu times.\n", ++buttonAPresses);

  } 

  if(M5.Touch.ispressed()){
    TouchPoint tp = M5.Touch.getPressPoint();
    if (hz.inHotZone(tp)) {
      Serial.printf("\t You touched the screen at (%d, %d). \n", tp.x, tp.y);
    }
  }
  if(millis() >= lastTS + 1000) {
    M5.Axp.SetLed(ledIsOn);
    ledIsOn = !ledIsOn;
    lastTS = millis();
    Serial.printf("Hello again; the device has been on for %lu seconds. \n", ++secondsOn);
    //delay(1000);
  }
}